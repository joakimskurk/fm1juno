#!/usr/bin/env python3
"""Probe IC29 ADC opcodes using MAME's own uPD7810 CPU device.

Unlike run_adc_opcode_window.py, this executes the independently maintained
MAME CPU core, not a locally authored instruction decoder. MAME's juno106
driver currently instantiates UPD7810 for the module controller; both uPD7810
and uPD7811 share the examined instructions but this does NOT by itself certify
full uPD7811 interrupt, internal-ROM, or hardware behavior.

A *synthetic*, not original, boot ROM populates memory and calls a SHA-256-
pinned excerpt from the public IC29 annotated disassembly. No original Roland
ROM blob is bundled or committed. This harness is intentionally ROM-limited
and must NOT be advertised as a whole-ROM JUNO-106 reproduction.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

# Import only the original-public-byte extractor/validator, not its CPU or
# mathematical reference implementation.
from run_adc_opcode_window import load_window

ADC_FIRST, ADC_LAST = 0x0800, 0x0825
CPU_EXIT = 0x0400

# Exercise the boundaries most likely to detect PSW skip/borrow differences.
CASES: tuple[tuple[int, int], ...] = (
    (0, 0), (0, 1), (0, 2), (0, 255),
    (1, 0), (1, 1), (1, 2),
    (4, 0), (4, 5), (4, 6),
    (100, 98), (100, 99), (100, 100), (100, 101), (100, 102),
    (238, 239), (238, 240), (239, 238), (239, 240),
    (240, 239), (240, 240), (240, 241),
    (249, 249), (249, 250), (250, 249),
    (255, 0), (255, 254), (255, 255),
)

# This machine cannot run the actual original Roland module ROM here. The
# 8KiB files below are intentionally artificial and fail the original ROM
# manifest checksum (MAME correctly warns). We never suppress or relabel that.
SYNTHETIC_ROM_PREFIX = "FM1JUNO_SYNTHETIC_MAME_PROBE"


def boot_image(public_adc_opcodes: bytes, old: int, fresh: int) -> bytes:
    # Verified uPD7810/11 opcodes per MAME opcode implementation:
    # MVI V,#ff ; LXI SP,fff0 ; LXI HL,ff5d ; LXI DE,ff80 ;
    # MVIW [ffxx],#imm x8 ; CALF 0800 ; JMP 0400.
    # MVIW uses 8-bit working-address low byte and V as page FF.
    bootstrap = bytearray((
        0x68, 0xFF, 0x04, 0xF0, 0xFF,
        0x34, 0x5D, 0xFF, 0x24, 0x80, 0xFF,
    ))
    other_raw = (4, 239, 249) if (fresh & 1) == 0 else (3, 240, 250)
    raw = (fresh,) + other_raw
    held = (old, 2, 238, 255)
    for i, value in enumerate(raw):
        bootstrap.extend((0x71, 0x5D + i, value))
    for i, value in enumerate(held):
        bootstrap.extend((0x71, 0x80 + i, value))
    bootstrap.extend((0x78, 0x00, 0x54, 0x00, 0x04))

    if len(bootstrap) >= CPU_EXIT or len(public_adc_opcodes) != ADC_LAST - ADC_FIRST:
        raise AssertionError("bad synthetic program layout")
    result = bytearray(0x2000)
    result[:len(bootstrap)] = bootstrap
    result[ADC_FIRST:ADC_LAST] = public_adc_opcodes
    # A real 0x0400 JMP to itself, for an unambiguous breakpoint after CALF.
    result[CPU_EXIT:CPU_EXIT+3] = bytes((0x54, 0x00, 0x04))
    return bytes(result)


def debugger_script() -> str:
    # Console backend and CPU-qualified bpset are documented MAME features.
    # b@ff80 reads the *real* MAME CPU program address space at FF80.
    # Explicitly stop and quit from the breakpoint action to avoid depending
    # on timers, UI input, or a permanently running fake assigner ROM.
    fields = ",".join(
        "b@%04x" % address
        for i in range(4)
        for address in (0xFF80 + i, 0xFF88 + i)
    )
    return (
        "focus :modulecpu\n"
        'bp 0400:modulecpu,1,{logerror "FM1JUNO_MAME_ADC:%02X:%02X:%02X:%02X:'
        '%02X:%02X:%02X:%02X\\n",' + fields + "; quit}\n"
        "g\n"
    )


def expected(native_cli: Path) -> dict[tuple[int, int], tuple[int, ...]]:
    lines = "".join(f"{old} {fresh}\n" for old, fresh in CASES)
    result = subprocess.run(
        [str(native_cli)], input=lines, capture_output=True,
        text=True, check=True, timeout=15)
    data = [tuple(map(int, line.split(",")))
            for line in result.stdout.splitlines() if line.strip()]
    if len(data) != len(CASES) or any(len(row) != 8 for row in data):
        raise AssertionError("compiled production reference output malformed")
    return dict(zip(CASES, data))


def mame_binary(path: str) -> str:
    executable = shutil.which(path)
    if executable:
        return executable
    candidate = Path(path)
    if candidate.is_file():
        return str(candidate.resolve())
    raise OSError("MAME executable not found: " + path)


def run_probe(source: Path, native_cli: Path, mame: str, timeout: int) -> None:
    adc = load_window(source)
    compiled_results = expected(native_cli)
    cpu = mame_binary(mame)
    print("MAME independent uPD7810 CPU probe: " + cpu, flush=True)

    # Per-case tempdir is intentional. The MAME unverified-ROM warning does
    # not pollute any existing user's games/roms directory.
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-cpu-") as folder:
        base = Path(folder)
        romdir = base / "roms"
        romdir.mkdir()
        cmdfile = base / "probe.cmd"
        cmdfile.write_text(debugger_script(), encoding="ascii")
        (base / "cfg").mkdir()
        for index, (old, fresh) in enumerate(CASES):
            image = boot_image(adc, old, fresh)
            # Assigners are NOT being tested; synthetic JMP 0000 loop.
            assigner = bytes((0x54, 0x00, 0x00)) + bytes(0x2000 - 3)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", assigner)
                archive.writestr("v2 b_2.ic37", image)
            argv = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(base / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(cmdfile),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                proc = subprocess.run(
                    argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, timeout=timeout, cwd=base,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as exc:
                raise AssertionError(
                    "MAME probe timed out case %d old=%d fresh=%d" %
                    (index, old, fresh)) from exc
            text = proc.stdout
            # MAME can route logerror through error.log instead of stderr.
            logfile = base / "error.log"
            if logfile.exists():
                text += "\n" + logfile.read_text(encoding="utf-8", errors="replace")
                logfile.unlink()
            match = re.search(
                r"FM1JUNO_MAME_ADC:((?:[0-9a-fA-F]{2}:){7}[0-9a-fA-F]{2})",
                text)
            if match is None:
                raise AssertionError(
                    "MAME produced no debugger breakpoint trace in case "
                    f"{index} old={old} fresh={fresh}, exit={proc.returncode}; "
                    "output tail:\n" + text[-2500:])
            observed = tuple(int(part, 16)
                             for part in match.group(1).split(":"))
            required = compiled_results[(old, fresh)]
            if observed != required:
                raise AssertionError(
                    "MAME core disagrees with production ADC old=%d fresh=%d: "
                    "MAME=%r native=%r\n%s" %
                    (old, fresh, observed, required, text[-1200:]))
            if proc.returncode:
                # Printed breakpoint without clean shutdown is NOT a pass.
                raise AssertionError(
                    "MAME returned nonzero status %d after breakpoint" %
                    proc.returncode)
            print("MAME_CORE_ADC %02d/%02d PASS old=%3d fresh=%3d "
                  "RAM=%s" % (
                      index+1, len(CASES), old, fresh,
                      ",".join("%02x" % x for x in observed)), flush=True)

    print("IC29_ADC_MAME_CORE: PASS (%d original-opcode MAME CPU cases, "
          "all four channels)" % len(CASES), flush=True)
    print("Scope: MAME juno106 driver UPD7810 core, synthetic boot ROM + "
          "original ADC machine-byte window; NOT full uPD7811/full original ROM",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--native-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    options = parser.parse_args()
    try:
        run_probe(options.ic29_disasm, options.native_cli,
                  options.mame, options.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_ADC_MAME_CORE: FAIL: " + str(exc), file=sys.stderr)
        sys.exit(1)
