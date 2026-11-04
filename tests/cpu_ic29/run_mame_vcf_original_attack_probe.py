#!/usr/bin/env python3
"""Run the original IC29 first-voice ATTACK path in the independent MAME CPU.

Unlike run_mame_vcf_linked_probe.py this runner has NO synthetic bridge
inside 0x0503..0x05BF. Published bytes are installed continuously at
0x04CB..0x063B, with the original CALF 0x082F DAC helper and the published
128-entry 0x0B60 attack lookup table. SHA-256 independently pins all three
areas. Only synthetic boot/input RAM and unvisited ROM holes are generated.

The authentic sequence uses global cutoff +/- LFO +/- bend, PUSH EA,
original envelope attack update (including 0x3FFF saturation and phase
flags), PWM DAC output, POP HL, ENV mod/Key Follow and VCF clamp.
Checks use compiled production envelope and VCF functions, not a Python
reimplementation of the algorithms.

MAME's juno106 skeleton is UPD7810. This is a bounded first-voice CPU
test, not an UPD7811 silicon, six-voice, interrupt-timing, or audio proof.
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
from run_mame_vcf_linked_probe import voice_inputs

ROM_BEGIN, ROM_END = 0x04CB, 0x063C
DAC_BEGIN, DAC_END = 0x082F, 0x083D
TABLE_BEGIN, TABLE_END = 0x0B60, 0x0C60
ENTRY, EXIT = 0x04D5, 0x063C

# 369 consecutive bytes: all original global, envelope and per-voice VCF
# instructions, including the original ATTACK path and release/decay code
# that these test cases do not execute.
ROM_SHA256 = "e3550dc3c7731630b737ede97a47d6029d33929d5b480540d876ce99c42ac365"
# 14 original DAC handler bytes; no stub or branch shortcut.
DAC_SHA256 = "3357c20d1baeed42f251020e287554626d94b061da5e8bd2f59e7db41c0f8310"
# Original published 128 16-bit ATTACK coefficients in little-endian memory
# order. The original listing writes each numeric word most-significant
# digit first, e.g. 4000 is value 0x4000 stored as 00 40 in UPD7810 RAM.
TABLE_SHA256 = "faef5ad5666a501bfe373f0af4cb345cae8ec6c569821873bb15f69f71ec3eea"

# Every attack index, plus 16 intentionally high-envelope saturation
# vectors. Each runs a fresh synthetic ROM under real MAME modulecpu.
BOUNDARY_ATTACKS = (
    0, 1, 2, 3, 4, 5, 7, 15,
    31, 63, 64, 85, 86, 108, 121, 127,
)


def original_parts(path: Path) -> tuple[bytes, bytes, bytes]:
    code_pattern = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    line_pattern = re.compile(r"^\s*([0-9a-fA-F]{4}):\s+(.+)$")
    machine: dict[int, int] = {}
    table: dict[int, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        code = code_pattern.match(line)
        if code:
            location = int(code.group(1), 16)
            for octet in code.group(2).split():
                if (ROM_BEGIN <= location < ROM_END or
                        DAC_BEGIN <= location < DAC_END):
                    number = int(octet, 16)
                    if location in machine and machine[location] != number:
                        raise ValueError("conflicting source opcode at %04X" %
                                         location)
                    machine[location] = number
                location += 1

        row = line_pattern.match(line)
        if row:
            location = int(row.group(1), 16)
            if TABLE_BEGIN <= location < TABLE_END:
                words = row.group(2).strip().split()
                if len(words) != 8 or any(
                        re.fullmatch(r"[0-9a-fA-F]{4}", w) is None
                        for w in words):
                    raise ValueError("invalid published table row at %04X" %
                                     location)
                for word in words:
                    number = int(word, 16)
                    for address, value in ((location, number & 0xFF),
                                           (location + 1, number >> 8)):
                        if address in table and table[address] != value:
                            raise ValueError("conflicting table byte")
                        table[address] = value
                    location += 2

    def segment(blob: dict[int, int], start: int, end: int,
                expected_hash: str) -> bytes:
        if any(address not in blob for address in range(start, end)):
            raise ValueError("missing published IC29 byte at %04X..%04X" %
                             (start, end - 1))
        result = bytes(blob[address] for address in range(start, end))
        actual = hashlib.sha256(result).hexdigest()
        if actual != expected_hash:
            raise ValueError("IC29 byte range %04X..%04X SHA mismatch: %s" %
                             (start, end - 1, actual))
        return result

    original_rom = segment(machine, ROM_BEGIN, ROM_END, ROM_SHA256)
    dac = segment(machine, DAC_BEGIN, DAC_END, DAC_SHA256)
    attack_table = segment(table, TABLE_BEGIN, TABLE_END, TABLE_SHA256)
    if (original_rom[0x0503 - ROM_BEGIN] != 0xB3 or
            original_rom[0x05C0 - ROM_BEGIN] != 0xA3 or
            dac[-1] != 0xB8):
        raise AssertionError("published attack path/stack/DAC RET changed")
    return original_rom, dac, attack_table


def scenarios() -> tuple[tuple[int, int], ...]:
    initial_levels = (0, 1, 255, 4096, 8192, 0x3F00, 0x3FFE, 0x3FFF)
    result = [(attack, initial_levels[attack % len(initial_levels)])
              for attack in range(128)]
    result += [(attack, (0x3FFF, 0x3FFE, 0x3F80, 0x3F00)[i % 4])
               for i, attack in enumerate(BOUNDARY_ATTACKS)]
    if (len(result) != 144 or len({a for a, _ in result}) != 128 or
            not any(x == 0 for _, x in result) or
            not any(x == 0x3FFF for _, x in result)):
        raise AssertionError("ATTACK regression coverage changed")
    return tuple(result)


def native_attack_values(executable: Path, cases: tuple[tuple[int, int], ...]
                         ) -> list[tuple[int, int, int]]:
    data = "".join("%d %d\n" % case for case in cases)
    result = subprocess.run(
        [str(executable)], text=True, input=data, capture_output=True,
        timeout=15, check=True)
    rows = [tuple(map(int, line.strip().split(",")))
            for line in result.stdout.splitlines() if line.strip()]
    if len(rows) != len(cases) or any(len(row) != 3 for row in rows):
        raise AssertionError("compiled envelope attack response count/format")
    if any(not 0 <= row[1] <= 0x3FFF or row[2] not in (0, 1)
           for row in rows):
        raise AssertionError("compiled envelope attack result out of range")
    return rows


def native_voice_values(executable: Path,
                        globals_: list[tuple[int, int, int, int]],
                        envs: list[tuple[int, int, int]]
                        ) -> list[int]:
    inputs = []
    for index, (amounts, attack) in enumerate(zip(globals_, envs)):
        _, pitch, env_depth, inverted, key = voice_inputs(index)
        inputs.append("%d %d %d %d %d %d" % (
            amounts[3], attack[1], pitch, env_depth, int(inverted), key))
    output = subprocess.run(
        [str(executable)], input="\n".join(inputs) + "\n",
        text=True, capture_output=True, check=True, timeout=15)
    values = [int(s) for s in output.stdout.splitlines() if s.strip()]
    if len(values) != len(globals_) or any(v < 0 or v > 0x3FFF for v in values):
        raise AssertionError("compiled VCF reference count or value invalid")
    return values


def bootstrap_rom(original: bytes, dac: bytes, lookup: bytes,
                  global_case: tuple[int, ...],
                  amounts: tuple[int, int, int, int],
                  attack: int, envelope_before: int, index: int) -> bytes:
    cutoff, lfo, bend, _ = amounts
    _, pitch, depth, inverted, key = voice_inputs(index)
    # Standard synthetic input setup: MVI V,FF / LXI SP,FFF0.
    startup = bytearray((0x68, 0xFF, 0x04, 0xF0, 0xFF))
    register_bytes = (
        (0x4A, 0x02 if global_case[4] else 0),
        (0x1E, 0x20 if global_case[7] else 0),
        (0x3D, cutoff & 0xFF), (0x3E, cutoff >> 8),
        (0x53, lfo & 0xFF), (0x54, lfo >> 8),
        (0x65, bend & 0xFF), (0x66, bend >> 8),
        (0x41, depth * 2), (0x42, key * 2),
        (0x37, 0 if inverted else 2),
        (0x11, 1),  # FF11: voice1 gate present, original ATTACK route
        (0x33, 0),  # FF33: no release-phase bit for voice1
        (0x08, 0),  # FF08: decay-phase not yet set
        (0x07, 1),  # FF07: voice1 attack bit
        (0x45, attack),  # FF45: original 7-bit attack index
        (0x27, envelope_before & 0xFF),
        (0x28, envelope_before >> 8),
        (0x71, pitch & 0xFF), (0x72, pitch >> 8),
        (0x4F, 0x34), (0x50, 0x12),  # PWM DAC channel input
    )
    for address, value in register_bytes:
        startup.extend((0x71, address, value))
    startup.extend((0x54, ENTRY & 0xFF, ENTRY >> 8))
    if len(startup) >= ROM_BEGIN:
        raise AssertionError("bootstrap overlaps published original ROM")
    rom = bytearray(0x2000)
    rom[:len(startup)] = startup
    rom[ROM_BEGIN:ROM_END] = original
    rom[DAC_BEGIN:DAC_END] = dac
    rom[TABLE_BEGIN:TABLE_END] = lookup
    rom[EXIT:EXIT+3] = bytes((0x54, EXIT & 0xFF, EXIT >> 8))
    return bytes(rom)


def mame_debug_commands() -> str:
    # No synthetic debugger state patching, all observations after original
    # 0503..063B branch and the original 082F DAC subroutine have executed.
    return (
        "focus :modulecpu\n"
        'bp 063c:modulecpu,1,{logerror '
        '"FM1JUNO_ORIGINAL_ATTACK:%04X:%02X:%02X:%02X:%02X:%02X:'
        '%04X:%04X:%02X:%02X\\n",'
        'ea,b@ff27,b@ff28,b@ff33,b@ff08,b@ff6a,sp,hl,b@ff0f,b@ff34; quit}\n'
        "g\n"
    )


def run(disasm: Path, envelope_cli: Path, global_cli: Path,
        voice_cli: Path, mame: str, timeout: int) -> None:
    original, dac, lookup = original_parts(disasm)
    cases = scenarios()
    global_vector = global_cases()
    globals_ = global_production(global_cli, tuple(
        global_vector[i % len(global_vector)] for i in range(len(cases))))
    envelopes = native_attack_values(envelope_cli, cases)
    # Authenticate all 128 published coefficient words against compiled
    # production code; do not use a Python clone of the coefficient law.
    for index, (case, expected) in enumerate(zip(cases, envelopes)):
        actual_word = int.from_bytes(lookup[case[0]*2:case[0]*2+2],
                                     byteorder="little")
        if expected[0] != actual_word:
            raise AssertionError(
                "source attack table != compiled C at case %d, index=%d: "
                "original=%04X, production=%04X" %
                (index, case[0], actual_word, expected[0]))
    voice_results = native_voice_values(voice_cli, globals_, envelopes)
    binary = mame_binary(mame)
    print("Original IC29 ATTACK + VCF MAME: %d original-byte cases" %
          len(cases), flush=True)
    print("Source: contiguous 04CB..063B, original 082F..083C DAC "
          "and original 128 x 16-bit ATTACK table", flush=True)

    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-original-attack-"
                                     ) as folder:
        root = Path(folder)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        commandfile = root / "attack.cmd"
        commandfile.write_text(mame_debug_commands(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for index, ((attack, before), amounts, expected_env,
                    expected_voice) in enumerate(
                        zip(cases, globals_, envelopes, voice_results)):
            global_case = global_vector[index % len(global_vector)]
            image = bootstrap_rom(
                original, dac, lookup, global_case, amounts,
                attack, before, index)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", assigner)
                archive.writestr("v2 b_2.ic37", image)
            argv = [
                binary, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
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
                    "original attack CPU case %d timed out (index=%d)" %
                    (index, attack)) from err
            diagnostics = result.stdout + result.stderr
            error_log = root / "error.log"
            if error_log.exists():
                diagnostics += "\n" + error_log.read_text(
                    encoding="utf-8", errors="replace")
                error_log.unlink()
            match = re.search(
                r"FM1JUNO_ORIGINAL_ATTACK:([0-9A-Fa-f]{4}):"
                r"([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):"
                r"([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4}):"
                r"([0-9A-Fa-f]{4}):([0-9A-Fa-f]{2}):([0-9A-Fa-f]{2})",
                diagnostics)
            if match is None:
                raise AssertionError(
                    "original attack checkpoint missing case %d idx %d rc=%d: %s" %
                    (index, attack, result.returncode, diagnostics[-2000:]))
            (ea, env_low, env_high, release_bits, decay_bits,
             global_sign, sp, hl, voice_index, voice_mask) = (
                int(v, 16) for v in match.groups())
            env = env_low | (env_high << 8)
            expected_phase = expected_env[2]
            if (ea != expected_voice or env != expected_env[1] or
                    release_bits != expected_phase or
                    decay_bits != expected_phase or
                    global_sign != int(amounts[3] < 0) or
                    sp != 0xFFEE or hl != 0xFF73 or
                    voice_index != 0 or voice_mask != 1):
                raise AssertionError(
                    "original ATTACK mismatch case %d idx=%d start=%04X "
                    "global=%d: MAME EA=%04X ENV=%04X RELEASE=%02X "
                    "DECAY=%02X FF6A=%02X SP=%04X HL=%04X voice=%02X "
                    "mask=%02X; C EA=%04X ENV=%04X SAT=%d" %
                    (index, attack, before, amounts[3], ea, env,
                     release_bits, decay_bits, global_sign, sp, hl,
                     voice_index, voice_mask, expected_voice,
                     expected_env[1], expected_phase))
            if result.returncode != 0:
                raise AssertionError(
                    "MAME exited %d after original ATTACK checkpoint" %
                    result.returncode)
            print("MAME_ORIGINAL_ATTACK %03d/%03d PASS idx=%03d "
                  "ENV=%04X EA=%04X SAT=%d" %
                  (index+1, len(cases), attack, env, ea, expected_phase),
                  flush=True)
    print("IC29_ORIGINAL_ATTACK_MAME_CORE: PASS (%d cases)" %
          len(cases), flush=True)
    print("Scope: original first voice ATTACK path with real lookup + DAC "
          "routine; MAME UPD7810; not full 6-voice/IRQ/original UPD7811",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--envelope-cli", type=Path, required=True)
    parser.add_argument("--global-cli", type=Path, required=True)
    parser.add_argument("--voice-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    args = parser.parse_args()
    try:
        run(args.ic29_disasm, args.envelope_cli, args.global_cli,
            args.voice_cli, args.mame, args.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as error:
        print("IC29_ORIGINAL_ATTACK_MAME_CORE: FAIL: " + str(error),
              file=sys.stderr)
        sys.exit(1)
