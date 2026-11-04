#!/usr/bin/env python3
"""Differential MAME execution of published IC29 *global* VCF arithmetic.

Exercises actual published bytes at 0x04CB..0x04F5, beginning at 0x04D5.
These bytes select signed LFO and bend, update FF6A carry/borrow state and
leave EA containing the unclamped global VCF coordinate. No Roland ROM is
shipped; synthetic startup injects already-computed cutoff/LFO/bend inputs.

This is MAME's stock JUNO-106 UPD7810 device, not original UPD7811
silicon or a whole-ROM / ADC / interrupt-timing / audio test.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import zipfile

from run_mame_cpu_probe import mame_binary

START, ENTRY, EXIT = 0x04CB, 0x04D5, 0x04F6
WINDOW_SHA256 = "08a0491367f316ed72992cfe3ecb2cef241efcdeb38b292c0b8eaa58b721ffc2"
# (cutoff7, lfo_depth7, lfo_onset, lfo_magnitude, negative LFO,
#  bend magnitude, bend sensitivity, negative bend)
CASE_EDGES: tuple[tuple[int, int, int, int, int, int, int, int], ...] = (
    (0, 0, 0, 0, 0, 0, 0, 0),
    (127, 0, 0, 0, 0, 0, 0, 0),
    (1, 0, 0, 0, 0, 0, 0, 0),
    (0, 127, 255, 8191, 0, 0, 0, 0),
    (0, 127, 255, 8191, 1, 0, 0, 0),
    (127, 127, 255, 8191, 0, 0, 0, 0),
    (127, 127, 255, 8191, 1, 0, 0, 0),
    (0, 0, 0, 0, 0, 255, 255, 0),
    (0, 0, 0, 0, 0, 255, 255, 1),
    (127, 0, 0, 0, 0, 255, 255, 0),
    (127, 0, 0, 0, 0, 255, 255, 1),
    (0, 127, 255, 8191, 0, 255, 255, 0),
    (0, 127, 255, 8191, 0, 255, 255, 1),
    (0, 127, 255, 8191, 1, 255, 255, 0),
    (0, 127, 255, 8191, 1, 255, 255, 1),
    (127, 127, 255, 8191, 0, 255, 255, 0),
    (127, 127, 255, 8191, 0, 255, 255, 1),
    (127, 127, 255, 8191, 1, 255, 255, 0),
    (127, 127, 255, 8191, 1, 255, 255, 1),
    (1, 127, 255, 8191, 1, 255, 255, 0),
    (1, 127, 255, 8191, 0, 255, 255, 1),
    (2, 127, 255, 8191, 1, 255, 255, 0),
    (2, 127, 255, 8191, 0, 255, 255, 1),
    (3, 127, 255, 8191, 1, 255, 255, 0),
    (3, 127, 255, 8191, 0, 255, 255, 1),
    (32, 127, 255, 8191, 1, 255, 255, 0),
    (32, 127, 255, 8191, 0, 255, 255, 1),
    (64, 127, 255, 8191, 1, 255, 255, 0),
    (64, 127, 255, 8191, 0, 255, 255, 1),
    (127, 1, 1, 1, 0, 1, 1, 0),
    (0, 1, 1, 1, 1, 1, 1, 1),
    (32, 64, 127, 4096, 0, 128, 128, 1),
    (32, 64, 127, 4096, 1, 128, 128, 0),
    (32, 64, 127, 4096, 1, 128, 128, 1),
    (32, 64, 127, 4096, 0, 128, 128, 0),
    (64, 127, 0, 8191, 1, 255, 255, 1),
    (64, 0, 255, 8191, 1, 255, 255, 1),
    (64, 127, 255, 0, 1, 255, 255, 1),
    (64, 127, 255, 8191, 1, 0, 255, 1),
    (64, 127, 255, 8191, 1, 255, 0, 1),
)


def cases() -> tuple[tuple[int, int, int, int, int, int, int, int], ...]:
    rng = random.Random(0x10604)
    unique = dict.fromkeys(CASE_EDGES)
    while len(unique) < 64:
        item = (
            rng.choice((0, 1, 2, 3, 32, 64, 126, 127)),
            rng.choice((0, 1, 2, 63, 64, 126, 127)),
            rng.choice((0, 1, 2, 127, 254, 255)),
            rng.choice((0, 1, 255, 1024, 4095, 8190, 8191)),
            rng.randrange(2),
            rng.choice((0, 1, 2, 127, 128, 254, 255)),
            rng.choice((0, 1, 2, 127, 128, 254, 255)),
            rng.randrange(2),
        )
        unique[item] = None
    result = tuple(unique)
    if len(result) != 64 or len({(c[4], c[7]) for c in result}) != 4:
        raise AssertionError("VCF global polarity/size coverage changed")
    return result


def published_window(source: Path) -> bytes:
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    mem: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match is None:
            continue
        address = int(match.group(1), 16)
        for byte in match.group(2).split():
            if START <= address < EXIT:
                value = int(byte, 16)
                if address in mem and mem[address] != value:
                    raise ValueError("conflicting public ROM byte at %04X" % address)
                mem[address] = value
            address += 1
    if len(mem) != EXIT - START:
        raise ValueError("incomplete global VCF source opcode window")
    data = bytes(mem[address] for address in range(START, EXIT))
    digest = hashlib.sha256(data).hexdigest()
    if digest != WINDOW_SHA256:
        raise ValueError("global VCF opcode hash changed: " + digest)
    return data


def production_values(exe: Path, items: tuple[tuple[int, ...], ...]
                      ) -> list[tuple[int, int, int, int]]:
    payload = "".join(" ".join(map(str, item)) + "\n" for item in items)
    output = subprocess.run(
        [str(exe)], input=payload, capture_output=True,
        text=True, timeout=15, check=True)
    parsed = [tuple(int(x) for x in line.split(","))
              for line in output.stdout.splitlines() if line.strip()]
    if len(parsed) != len(items) or any(len(p) != 4 for p in parsed):
        raise AssertionError("invalid compiled global VCF responses")
    for cutoff, lfo, bend, signed in parsed:
        if not (0 <= cutoff <= 0x3fff and 0 <= lfo <= 0x3fff
                and 0 <= bend <= 0x3fff and -32768 <= signed <= 32767):
            raise AssertionError("global VCF case exceeded bounded CPU domain")
    return parsed


def boot_image(window: bytes, item: tuple[int, ...],
               source: tuple[int, int, int, int]) -> bytes:
    cutoff, lfo, bend, _ = source
    # MVI V,FF; LXI SP,FFF0, MVIW working-byte inputs.
    boot = bytearray((0x68, 0xFF, 0x04, 0xF0, 0xFF))
    # FF4A bit 1 flags LFO negative; FF1E bit 5 flags bend negative.
    flags = (
        (0x4A, 0x02 if item[4] else 0),
        (0x1E, 0x20 if item[7] else 0),
        (0x3D, cutoff & 255), (0x3E, cutoff >> 8),
        (0x53, lfo & 255), (0x54, lfo >> 8),
        (0x65, bend & 255), (0x66, bend >> 8),
    )
    for address, value in flags:
        boot.extend((0x71, address, value))
    boot.extend((0x54, ENTRY & 255, ENTRY >> 8))
    if len(boot) >= START or len(window) != EXIT - START:
        raise AssertionError("incorrect synthetic ROM layout")
    rom = bytearray(0x2000)
    rom[:len(boot)] = boot
    rom[START:EXIT] = window
    rom[EXIT:EXIT+3] = bytes((0x54, EXIT & 255, EXIT >> 8))
    return bytes(rom)


def debug_script() -> str:
    return (
        "focus :modulecpu\n"
        'bp 04f6:modulecpu,1,{logerror "FM1JUNO_VCF_GLOBAL:%04X:%02X\\n",'
        'ea,b@ff6a; quit}\n'
        "g\n"
    )


def run(source: Path, native_cli: Path, mame: str, timeout: int) -> None:
    window = published_window(source)
    items = cases()
    compiled = production_values(native_cli, items)
    cpu = mame_binary(mame)
    print("MAME IC29 global VCF CPU chain: %d cases, core=%s" %
          (len(items), cpu), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-vcf-global-") as folder:
        root = Path(folder)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        debugger = root / "global-vcf.cmd"
        debugger.write_text(debug_script(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for index, (item, expected) in enumerate(zip(items, compiled)):
            rom = boot_image(window, item, expected)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", assigner)
                archive.writestr("v2 b_2.ic37", rom)
            argv = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(debugger),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                proc = subprocess.run(
                    argv, text=True, capture_output=True, cwd=root,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
                    timeout=timeout)
            except subprocess.TimeoutExpired as error:
                raise AssertionError(
                    "VCF global CPU timeout at case %d %r" %
                    (index, item)) from error
            diagnostics = proc.stdout + proc.stderr
            errorlog = root / "error.log"
            if errorlog.exists():
                diagnostics += "\n" + errorlog.read_text(
                    encoding="utf-8", errors="replace")
                errorlog.unlink()
            trace = re.search(
                r"FM1JUNO_VCF_GLOBAL:([0-9a-fA-F]{4}):([0-9a-fA-F]{2})",
                diagnostics)
            if trace is None:
                raise AssertionError(
                    "global VCF checkpoint missing case %d, exit %d: %s" %
                    (index, proc.returncode, diagnostics[-1400:]))
            observed = int(trace.group(1), 16)
            overflow_flag = int(trace.group(2), 16)
            signed = expected[3]
            expected_flag = int(signed < 0)
            if observed != (signed & 0xffff) or overflow_flag != expected_flag:
                raise AssertionError(
                    "global VCF mismatch case %d %r: MAME EA=%04X FF6A=%02X "
                    "compiled C signed=%d expected flag=%d, inputs=%r" %
                    (index, item, observed, overflow_flag, signed,
                     expected_flag, expected[:3]))
            if proc.returncode != 0:
                raise AssertionError(
                    "MAME process exited %d at global VCF case %d" %
                    (proc.returncode, index))
            print(
                "MAME_VCF_GLOBAL %02d/%02d PASS EA=%04X signflag=%d" %
                (index + 1, len(items), observed, overflow_flag),
                flush=True)
    print("IC29_VCF_GLOBAL_MAME_CORE: PASS (%d cases)" % len(items), flush=True)
    print("Scope: original 04CB..04F5 bytes; synthetic precomputed LFO/bend "
          "inputs; MAME UPD7810, not original uPD7811/full ROM", flush=True)


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
        print("IC29_VCF_GLOBAL_MAME_CORE: FAIL: " + str(error),
              file=sys.stderr)
        sys.exit(1)
