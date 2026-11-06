#!/usr/bin/env python3
"""Validate the published IC29 VCF clamp bytes using MAME's real CPU core.

This is *not* a separate local instruction decoder. MAME's existing JUNO-106
driver instantiates the uPD7810 CPU device, and we inject a synthetic boot
program plus the real 0x0609..0x0618 machine-byte window from the annotated
IC29 listing. The probe stops at 0x0400 and captures MAME's actual EA/FF6B.
MAME UPD7810 is not full-original-ROM or cycle-accurate uPD7811 certification.
"""
from __future__ import annotations

from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import tempfile
import zipfile

from run_mame_cpu_probe import mame_binary
from run_vcf_opcode_window import load_bytes, execute

EXIT = 0x0400
VCF_START = 0x0609
VCF_END = 0x0619
# Four flag states (including unrelated bit 1) test original ONIW mask.
STATES: tuple[tuple[int, int], ...] = tuple(
    (word, flag)
    for word in (0, 1, 0x0fff, 0x3ffe, 0x3fff, 0x4000,
                 0x4001, 0x7fff, 0x8000, 0xbfff, 0xc000,
                 0xfffe, 0xffff)
    for flag in (0, 1, 2, 3)
)


def make_rom(window: bytes, word: int, flag: int) -> bytes:
    if len(window) != (VCF_END - VCF_START):
        raise AssertionError("original VCF window length mismatch")
    # MVI V,ff; LXI SP,fff0; MVIW ff6b,flag;
    # LXI EA,word; JMP 0609.
    startup = bytes((
        0x68, 0xFF,
        0x04, 0xF0, 0xFF,
        0x71, 0x6B, flag,
        0x44, word & 255, (word >> 8) & 255,
        0x54, 0x09, 0x06,
    ))
    memory = bytearray(0x2000)
    memory[0:len(startup)] = startup
    memory[VCF_START:VCF_END] = window
    # Both positive/no-clamp and negative saturation branches reach $063C.
    memory[0x063C:0x063F] = bytes((0x54, 0x00, 0x04))
    memory[EXIT:EXIT+3] = bytes((0x54, 0x00, 0x04))
    return bytes(memory)


def debug_script() -> str:
    return (
        "focus :modulecpu\n"
        'bp 0400:modulecpu,1,{logerror "FM1JUNO_MAME_VCF:%04X:%02X\\n",'
        'ea,b@ff6b; quit}\n'
        "g\n"
    )


def production_values(exe: Path) -> dict[tuple[int, int], int]:
    # The signed model represents a wrapped negative result as EA-65536
    # only when the underflow-selection flag is 1 AND EA is in the clip
    # region. An in-range EA is observable unchanged regardless of the
    # flag, so its signed input is simply EA.
    sent: list[str] = []
    for word, flag in STATES:
        signed = (word-65536 if (flag & 1 and word >= 0x4000) else word)
        sent.append(str(signed))
    out = subprocess.run(
        [str(exe)], input="\n".join(sent)+"\n", capture_output=True,
        text=True, timeout=15, check=True)
    numbers = [int(s) for s in out.stdout.splitlines() if s.strip()]
    if len(numbers) != len(STATES):
        raise AssertionError("compiled VCF reference response count mismatch")
    return dict(zip(STATES, numbers))


def run(source: Path, native_cli: Path, mame: str, timeout: int) -> None:
    window = load_bytes(source)
    compiler = production_values(native_cli)
    cpu = mame_binary(mame)
    print("MAME independent VCF clamp core probe: " + cpu, flush=True)

    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-vcf-") as directory:
        root = Path(directory)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        script = root / "vcf.cmd"
        script.write_text(debug_script(), encoding="ascii")
        synthetic_assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for index, (word, flag) in enumerate(STATES):
            image = make_rom(window, word, flag)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", synthetic_assigner)
                z.writestr("v2 b_2.ic37", image)
            command = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(script),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                outcome = subprocess.run(
                    command, cwd=root, env={
                        **os.environ, "QT_QPA_PLATFORM": "offscreen"},
                    text=True, capture_output=True, timeout=timeout)
            except subprocess.TimeoutExpired as error:
                raise AssertionError(
                    "MAME VCF timed out word=%04x flag=%d" %
                    (word, flag)) from error
            diagnostics = outcome.stdout + outcome.stderr
            log = root / "error.log"
            if log.exists():
                diagnostics += "\n" + log.read_text(
                    encoding="utf-8", errors="replace")
                log.unlink()
            trace = re.search(
                r"FM1JUNO_MAME_VCF:([0-9a-fA-F]{4}):([0-9a-fA-F]{2})",
                diagnostics)
            if not trace:
                raise AssertionError(
                    "MAME VCF breakpoint not reached word=%04x flag=%d "
                    "exit=%d, output:\n%s" %
                    (word, flag, outcome.returncode, diagnostics[-1800:]))
            observed = int(trace.group(1), 16)
            observed_flag = int(trace.group(2), 16)
            oracle, _, _ = execute(window, word, flag)
            expected = compiler[(word, flag)]
            if observed != expected or observed != oracle:
                raise AssertionError(
                    "VCF disagreement EA=%04x flag=%d "
                    "MAME=%04x compiledC=%04x boundedOriginal=%04x" %
                    (word, flag, observed, expected, oracle))
            if observed_flag != flag:
                raise AssertionError(
                    "MAME VCF modified unrelated FF6B flag byte: %02x != %02x" %
                    (observed_flag, flag))
            if outcome.returncode != 0:
                raise AssertionError(
                    "MAME process exited %d after reaching VCF breakpoint" %
                    outcome.returncode)
            print(
                "MAME_CORE_VCF %02d/%02d PASS EA=%04x flag=%02x -> %04x" %
                (index+1, len(STATES), word, flag, observed), flush=True)

    print("IC29_VCF_MAME_CORE: PASS (%d original-opcode MAME CPU cases)" %
          len(STATES), flush=True)
    print("Scope: published original VCF clamp bytes under MAME UPD7810, "
          "synthetic boot; NOT full original ROM/uPD7811", flush=True)


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
            subprocess.CalledProcessError) as exception:
        print("IC29_VCF_MAME_CORE: FAIL: " + str(exception), file=sys.stderr)
        sys.exit(1)
