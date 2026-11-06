#!/usr/bin/env python3
"""Differential original-byte VCF *global-to-voice* handoff in MAME.

The real published bytes run continuously from 0x04D5 through 0x0502
and then 0x05C0 through 0x063B, inside ONE MAME CPU invocation per case.
A clearly marked synthetic seven-byte seam at 0x0503 replaces the original
envelope evolution (0x0503..0x05BF). It preserves the global EA stack
save, puts the already-computed envelope at FF27 and transfers to the
original POP HL at 0x05C0. Thus this is not unmodified whole-ROM execution,
even though the signed global result is carried through the voice-control
instructions via the *original* PUSH/POP and FF6A/FF6B state transfers.

Original opcode bytes are hash-pinned to the published IC29 disassembly.
The stock MAME JUNO-106 skeleton uses UPD7810, not an UPD7811 device.
No Roland ROM binary or synthetic data file is committed.
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
from run_mame_vcf_global_probe import cases as global_cases
from run_mame_vcf_global_probe import production_values as global_production

FIRST, SEAM, LAST_START, EXIT = 0x04CB, 0x0503, 0x05C0, 0x063C
ENTRY = 0x04D5
# SHA-256 of exact two ORIGINAL source byte segments concatenated in order:
# 0x04CB..0x0502 (56 bytes), 0x05C0..0x063B (124 bytes).
SOURCE_SHA256 = "545631d0b7a24a39babc7e1b95dbf29be2c2a042b77e1d1a0fad382d6df89ce5"
# The seam does not pretend to be a published Roland opcode window.
# Original 0503 PUSH HL; synthetic LXI DE,FF27 (post-envelope cursor);
# JMP 05C0 original POP HL. All other fetched VCF bytes remain original.
SEAM_BYTES = bytes((0xB3, 0x24, 0x27, 0xFF, 0x54, 0xC0, 0x05))


def published_segments(source: Path) -> tuple[bytes, bytes]:
    expression = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    memory: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = expression.match(line)
        if match is None:
            continue
        address = int(match.group(1), 16)
        for octet in match.group(2).split():
            if FIRST <= address < SEAM or LAST_START <= address < EXIT:
                value = int(octet, 16)
                if address in memory and memory[address] != value:
                    raise ValueError("conflicting published byte at %04X" % address)
                memory[address] = value
            address += 1
    if len(memory) != (SEAM-FIRST) + (EXIT-LAST_START):
        raise ValueError("published global-to-voice source segments incomplete")
    prefix = bytes(memory[p] for p in range(FIRST, SEAM))
    suffix = bytes(memory[p] for p in range(LAST_START, EXIT))
    digest = hashlib.sha256(prefix + suffix).hexdigest()
    if digest != SOURCE_SHA256:
        raise ValueError("published global-to-voice byte hash changed: " + digest)
    # Protect the crucial original PUSH EA / setup and POP HL endpoints.
    if prefix[-13:] != bytes.fromhex("b4710f007134013471ff2425ff"):
        raise AssertionError("changed original global-stack/voice setup")
    if suffix[0] != 0xA3:
        raise AssertionError("changed original POP HL at seam exit")
    return prefix, suffix


def voice_inputs(index: int) -> tuple[int, int, int, bool, int]:
    # Stable, broad independent combinations rather than
    # feeding every case the same envelope or note pitch.
    env = (0, 1, 255, 4096, 8192, 16383, 16382, 127)[index % 8]
    pitch = (0, 48 << 8, 60 << 8, 72 << 8,
             84 << 8, 127 << 8, (60 << 8) + 15,
             (60 << 8) - 15)[(index // 2) % 8]
    depth = (0, 1, 37, 64, 127, 127, 120, 2)[(index // 3) % 8]
    inverted = (index % 2) != 0
    key = (0, 1, 2, 64, 127, 127, 29, 90)[(index // 7) % 8]
    return env, pitch, depth, inverted, key


def compiled_voice_values(
    executable: Path, globals_: list[tuple[int, int, int, int]]
) -> list[int]:
    lines: list[str] = []
    for index, (_, _, _, global_raw) in enumerate(globals_):
        env, pitch, depth, inverted, key = voice_inputs(index)
        lines.append("%d %d %d %d %d %d" %
                     (global_raw, env, pitch, depth, int(inverted), key))
    response = subprocess.run(
        [str(executable)], input="\n".join(lines)+"\n",
        text=True, capture_output=True, check=True, timeout=15)
    values = [int(line) for line in response.stdout.splitlines()
              if line.strip()]
    if len(values) != len(globals_) or any(v < 0 or v > 0x3FFF for v in values):
        raise AssertionError("invalid production C voice response")
    return values


def boot_image(prefix: bytes, suffix: bytes,
               global_case: tuple[int, ...],
               amounts: tuple[int, int, int, int],
               voice: tuple[int, int, int, bool, int]) -> bytes:
    cutoff, lfo, bend, global_raw = amounts
    env, pitch, depth, inverted, key = voice
    if not (0 <= env <= 0x3FFF and 0 <= pitch <= 0xFFFF and
            0 <= depth <= 127 and 0 <= key <= 127 and
            -32768 <= global_raw <= 32767):
        raise AssertionError("injected CPU state out of bounded domain")
    # MVI V,FF; LXI SP,FFF0. MVIW FFxx,imm writes synthetic input
    # bytes to the real MAME module CPU's working RAM.
    bootstrap = bytearray((0x68, 0xFF, 0x04, 0xF0, 0xFF))
    registers = (
        (0x4A, 2 if global_case[4] else 0),  # LFO sign, bit 1
        (0x1E, 0x20 if global_case[7] else 0),  # Bend sign, bit 5
        (0x3D, cutoff & 255), (0x3E, cutoff >> 8),
        (0x53, lfo & 255), (0x54, lfo >> 8),
        (0x65, bend & 255), (0x66, bend >> 8),
        (0x41, depth * 2), (0x42, key * 2),
        (0x37, 0 if inverted else 2),  # ENV polarity switch
        (0x27, env & 255), (0x28, env >> 8),
        (0x71, pitch & 255), (0x72, pitch >> 8),
    )
    for address, value in registers:
        bootstrap.extend((0x71, address, value))
    bootstrap.extend((0x54, ENTRY & 255, ENTRY >> 8))  # JMP 04D5
    if (len(bootstrap) >= FIRST or len(prefix) != SEAM-FIRST or
            len(suffix) != EXIT-LAST_START):
        raise AssertionError("invalid synthetic VCF linked-ROM layout")
    image = bytearray(0x2000)
    image[:len(bootstrap)] = bootstrap
    image[FIRST:SEAM] = prefix
    image[SEAM:SEAM+len(SEAM_BYTES)] = SEAM_BYTES
    image[LAST_START:EXIT] = suffix
    image[EXIT:EXIT+3] = bytes((0x54, EXIT & 255, EXIT >> 8))
    return bytes(image)


def debug_script() -> str:
    # Final RAM FF6A proves the global sign was transferred via original
    # instructions; FF6B is the per-voice flag. SP/HL expose stack and
    # 16-bit pitch cursor behavior. We require a real MAME breakpoint.
    return (
        "focus :modulecpu\n"
        'bp 063c:modulecpu,1,{logerror '
        '"FM1JUNO_LINKED_VCF:%04X:%02X:%02X:%04X:%04X\\n",'
        'ea,b@ff6a,b@ff6b,sp,hl; quit}\n'
        "g\n"
    )


def run(disasm: Path, global_cli: Path, voice_cli: Path,
        mame: str, timeout: int) -> None:
    prefix, suffix = published_segments(disasm)
    cases = global_cases()
    if len(cases) != 64:
        raise AssertionError("global scenario set unexpectedly changed")
    globals_ = global_production(global_cli, cases)
    voices = compiled_voice_values(voice_cli, globals_)
    cpu = mame_binary(mame)
    print("MAME original-byte global-to-voice VCF: %d cases" %
          len(cases), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-vcf-linked-") as folder:
        root = Path(folder)
        romdir = root/"roms"
        romdir.mkdir()
        (root/"cfg").mkdir()
        commandfile = root/"vcf-linked.cmd"
        commandfile.write_text(debug_script(), encoding="ascii")
        synthetic_assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for index, (case, amounts, expected_voice) in enumerate(
                zip(cases, globals_, voices)):
            voice = voice_inputs(index)
            rom = boot_image(prefix, suffix, case, amounts, voice)
            with zipfile.ZipFile(romdir/"juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", synthetic_assigner)
                archive.writestr("v2 b_2.ic37", rom)
            argv = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root/"cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(commandfile),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                result = subprocess.run(
                    argv, cwd=root, text=True, capture_output=True,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
                    timeout=timeout)
            except subprocess.TimeoutExpired as err:
                raise AssertionError(
                    "linked VCF MAME case %d timed out: %r / %r" %
                    (index, case, voice)) from err
            diagnostics = result.stdout+result.stderr
            log = root/"error.log"
            if log.exists():
                diagnostics += "\n"+log.read_text(
                    encoding="utf-8", errors="replace")
                log.unlink()
            match = re.search(
                r"FM1JUNO_LINKED_VCF:([0-9a-fA-F]{4}):([0-9a-fA-F]{2}):"
                r"([0-9a-fA-F]{2}):([0-9a-fA-F]{4}):([0-9a-fA-F]{4})",
                diagnostics)
            if match is None:
                raise AssertionError(
                    "MAME linked VCF checkpoint missing case %d, rc=%d: %s" %
                    (index, result.returncode, diagnostics[-1800:]))
            ea, global_flag, voice_flag, sp, hl = (
                int(text, 16) for text in match.groups())
            expected_global_sign = int(amounts[3] < 0)
            if (ea != expected_voice or global_flag != expected_global_sign or
                    sp != 0xFFEE or hl != 0xFF73):
                raise AssertionError(
                    "linked VCF mismatch case %d: global=%r voice=%r "
                    "MAME EA=%04X FF6A=%02X FF6B=%02X SP=%04X HL=%04X, "
                    "expected EA=%04X FF6A=%02X SP=FFEE HL=FF73" %
                    (index, case, voice, ea, global_flag, voice_flag,
                     sp, hl, expected_voice, expected_global_sign))
            if result.returncode != 0:
                raise AssertionError(
                    "MAME exited nonzero (%d) after linked VCF checkpoint" %
                    result.returncode)
            print("MAME_LINKED_VCF %02d/%02d PASS global=%d EA=%04X" %
                  (index+1, len(cases), amounts[3], ea), flush=True)
    print("IC29_LINKED_VCF_MAME_CORE: PASS (%d cases)" % len(cases), flush=True)
    print("Scope: original 04CB..0502 + 05C0..063B; SYNTHETIC 0503 "
          "envelope-state bridge; MAME UPD7810 only, no original full ROM",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--global-cli", type=Path, required=True)
    parser.add_argument("--voice-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    args = parser.parse_args()
    try:
        run(args.ic29_disasm, args.global_cli, args.voice_cli,
            args.mame, args.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as error:
        print("IC29_LINKED_VCF_MAME_CORE: FAIL: " + str(error), file=sys.stderr)
        sys.exit(1)
