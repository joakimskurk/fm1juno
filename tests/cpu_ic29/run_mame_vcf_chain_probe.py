#!/usr/bin/env python3
"""Execute published IC29 VCF voice-control bytes in MAME's uPD7810 CPU.

Covers the per-voice ENV scaling, envelope polarity, Key Follow and final
saturation from 0x05C1 to 0x063B, *not* the preceding global cutoff/LFO/
bend computation or the preceding envelope/scheduler code. Original bytes
are downloaded, checked and executed; no Roland ROM is distributed here.

The stock MAME juno106 skeleton uses UPD7810 (not UPD7811). A synthetic
bootstrap supplies working RAM, registers and the original voice-control
entry stack. This is an independent CPU differential test, not full-chip,
interrupt-timing or original-hardware certification.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import zipfile

from run_mame_cpu_probe import mame_binary

START, END, EXIT = 0x05C1, 0x063C, 0x063C
WINDOW_SHA256 = "3b77378007f1fb96c884d707263af7a5cf9984dbe6df00b86d6cdf62c979be04"

# (signed global VCF base, envelope14, pitch 8.8, ENV depth7,
#  ENV inverted, Key Follow7). Explicit 0/1/127 and signed-bounds vectors
#  keep all polarities, both sides of key pivot and clamp outcomes in scope.
CASES: tuple[tuple[int, int, int, int, bool, int], ...] = (
    (0, 0, 60 << 8, 0, False, 0),
    (8192, 0, 60 << 8, 127, False, 127),
    (16383, 0, 60 << 8, 127, False, 0),
    (16384, 0, 60 << 8, 0, False, 0),
    (-1, 0, 60 << 8, 0, False, 0),
    (-8192, 0, 60 << 8, 0, True, 0),
    (0, 16383, 60 << 8, 127, False, 0),
    (0, 16383, 60 << 8, 127, True, 0),
    (17000, 4096, 60 << 8, 127, True, 0),
    (-100, 4096, 60 << 8, 127, False, 0),
    (8192, 8192, 60 << 8, 64, False, 0),
    (8192, 8192, 60 << 8, 64, True, 0),
    (8192, 8192, 60 << 8, 0, True, 0),
    (8192, 8192, 60 << 8, 1, False, 0),
    (8192, 8192, 60 << 8, 127, False, 1),
    (8192, 8192, 60 << 8, 127, False, 127),
    (8192, 8192, 48 << 8, 0, False, 127),
    (8192, 8192, 72 << 8, 0, False, 127),
    (8192, 8192, 48 << 8, 127, True, 127),
    (8192, 8192, 72 << 8, 127, False, 127),
    (8192, 8192, (60 << 8) - 1, 0, False, 127),
    (8192, 8192, (60 << 8) + 1, 0, False, 127),
    (8192, 8192, (60 << 8) + 15, 0, False, 127),
    (8192, 8192, (60 << 8) - 15, 0, False, 127),
    (0, 0, 0, 0, False, 127),
    (0, 0, 127 << 8, 0, False, 127),
    (16383, 16383, 127 << 8, 127, False, 127),
    (-8192, 16383, 127 << 8, 127, False, 127),
    (17000, 16383, 0, 127, True, 127),
    (0, 1, 60 << 8, 127, False, 0),
    (0, 255, 60 << 8, 127, False, 0),
    (0, 256, 60 << 8, 127, False, 0),
    (0, 0x3FFE, 60 << 8, 127, False, 0),
    (-1, 1, 60 << 8, 127, False, 0),
    (16384, 1, 60 << 8, 127, True, 0),
    (16382, 1, 60 << 8, 127, False, 0),
    (1, 1, 59 << 8, 1, False, 1),
    (1, 1, 61 << 8, 1, False, 1),
    (7000, 12345, 57 << 8, 37, False, 29),
    (7000, 12345, 57 << 8, 37, True, 29),
    (20000, 8000, 83 << 8, 120, True, 90),
    (-5000, 8000, 37 << 8, 120, False, 90),
)


def original_window(source: Path) -> bytes:
    line_pattern = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    ram: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = line_pattern.match(line)
        if not match:
            continue
        address = int(match.group(1), 16)
        for part in match.group(2).split():
            if START <= address < END:
                octet = int(part, 16)
                if address in ram and ram[address] != octet:
                    raise ValueError("conflicting source byte at %04x" % address)
                ram[address] = octet
            address += 1
    if len(ram) != END - START or any(a not in ram for a in range(START, END)):
        raise ValueError("incomplete VCF voice-control original-byte window")
    data = bytes(ram[a] for a in range(START, END))
    if hashlib.sha256(data).hexdigest() != WINDOW_SHA256:
        raise ValueError("VCF voice-control original-byte SHA-256 mismatch")
    return data


def synthetic_rom(window: bytes, case: tuple[int, int, int, int, bool, int]) -> bytes:
    global_raw, env, pitch, env_depth, inverted, key_depth = case
    if (not -32768 <= global_raw <= 32767 or
            not 0 <= env <= 0x3fff or not 0 <= pitch <= 0xffff or
            not 0 <= env_depth <= 127 or not 0 <= key_depth <= 127):
        raise ValueError("case outside defined ROM / C control domain")
    # MVI V,FF; LXI SP,FFF0; LXI HL,FFB0; LXI DE,FFA0.
    # DE addresses the 16-bit envelope; HL addresses 16-bit pitch.
    boot = bytearray((
        0x68, 0xFF, 0x04, 0xF0, 0xFF,
        0x34, 0xB0, 0xFF, 0x24, 0xA0, 0xFF,
    ))
    # Input bytes are *original* firmware-domain (doubled 7-bit knobs).
    # SW2 bit1 selects normal ENV; clear bit1 selects inverted ENV.
    # FF6A is the carried-over global underflow sign.
    working = (
        (0x41, env_depth * 2),
        (0x42, key_depth * 2),
        (0x37, 0 if inverted else 2),
        (0x6A, 1 if global_raw < 0 else 0),
        (0xA0, env & 255), (0xA1, env >> 8),
        (0xB0, pitch & 255), (0xB1, pitch >> 8),
    )
    for offset, value in working:
        boot.extend((0x71, offset, value))
    # Initial full-width base VCF is stacked exactly where the original
    # path expects POP EA at 0x05D5. Set EA then PUSH EA before entry.
    word = global_raw & 0xffff
    boot.extend((0x44, word & 255, word >> 8, 0xB4,
                 0x54, START & 255, START >> 8))
    if len(boot) >= EXIT or len(window) != END - START:
        raise AssertionError("bad synthetic VCF program layout")
    rom = bytearray(0x2000)
    rom[:len(boot)] = boot
    rom[START:END] = window
    rom[EXIT:EXIT + 3] = bytes((0x54, EXIT & 255, EXIT >> 8))
    return bytes(rom)


def debugger_script() -> str:
    return (
        "focus :modulecpu\n"
        'bp 063c:modulecpu,1,{logerror "FM1JUNO_VCF_CHAIN:%04X:%02X\\n",'
        'ea,b@ff6b; quit}\n'
        "g\n"
    )


def compiled_results(native_cli: Path) -> list[int]:
    lines = [
        "%d %d %d %d %d %d" % (
            base, env, pitch, env_depth, int(inverted), key_depth)
        for base, env, pitch, env_depth, inverted, key_depth in CASES
    ]
    result = subprocess.run(
        [str(native_cli)], input="\n".join(lines) + "\n",
        capture_output=True, text=True, check=True, timeout=15)
    numbers = [int(line) for line in result.stdout.splitlines() if line.strip()]
    if len(numbers) != len(CASES):
        raise AssertionError("compiled VCF chain response count mismatch")
    return numbers


def run(source: Path, native_cli: Path, mame: str, timeout: int) -> None:
    if len(CASES) != 42 or len(set(CASES)) != len(CASES):
        raise AssertionError("VCF chain coverage cases accidentally changed")
    if ({False, True} != {case[4] for case in CASES} or
            not any(case[0] < 0 for case in CASES) or
            not any(case[0] >= 0x4000 for case in CASES) or
            not any(case[2] < (60 << 8) for case in CASES) or
            not any(case[2] > (60 << 8) for case in CASES)):
        raise AssertionError("VCF chain polarity / overflow coverage incomplete")
    window = original_window(source)
    expected = compiled_results(native_cli)
    cpu = mame_binary(mame)
    print("MAME VCF voice-control chain: %s, %d cases" %
          (cpu, len(CASES)), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-vcf-chain-") as folder:
        root = Path(folder)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        script = root / "vcf-chain.cmd"
        script.write_text(debugger_script(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000 - 3)
        for index, case in enumerate(CASES):
            image = synthetic_rom(window, case)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", assigner)
                archive.writestr("v2 b_2.ic37", image)
            argv = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(script),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                proc = subprocess.run(
                    argv, cwd=root, env={
                        **os.environ, "QT_QPA_PLATFORM": "offscreen"},
                    text=True, capture_output=True, timeout=timeout)
            except subprocess.TimeoutExpired as exc:
                raise AssertionError(
                    "VCF chain timeout case %d: %r" % (index, case)) from exc
            diagnostics = proc.stdout + proc.stderr
            log = root / "error.log"
            if log.exists():
                diagnostics += "\n" + log.read_text(
                    encoding="utf-8", errors="replace")
                log.unlink()
            trace = re.search(
                r"FM1JUNO_VCF_CHAIN:([0-9a-fA-F]{4}):([0-9a-fA-F]{2})",
                diagnostics)
            if not trace:
                raise AssertionError(
                    "MAME VCF chain checkpoint missing case %d %r, exit=%d:\n%s" %
                    (index, case, proc.returncode, diagnostics[-1800:]))
            observed = int(trace.group(1), 16)
            flag = int(trace.group(2), 16)
            if observed != expected[index]:
                raise AssertionError(
                    "VCF chain mismatch case %d %r: MAME=%04x FF6B=%02x "
                    "compiledC=%04x" %
                    (index, case, observed, flag, expected[index]))
            if proc.returncode:
                raise AssertionError(
                    "MAME VCF chain nonzero exit %d at case %d" %
                    (proc.returncode, index))
            print(
                "MAME_VCF_CHAIN %02d/%02d PASS EA=%04x flag=%02x" %
                (index + 1, len(CASES), observed, flag), flush=True)
    print("IC29_VCF_CHAIN_MAME_CORE: PASS (%d cases)" % len(CASES), flush=True)
    print("Scope: published 05c1..063b voice-control bytes, stock MAME "
          "UPD7810, synthetic boot; NOT original full ROM/uPD7811",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--native-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    args = parser.parse_args()
    try:
        run(args.ic29_disasm, args.native_cli, args.mame, args.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as error:
        print("IC29_VCF_CHAIN_MAME_CORE: FAIL: " + str(error),
              file=sys.stderr)
        sys.exit(1)
