#!/usr/bin/env python3
"""Differential independent MAME CPU proof of original first-voice DECAY/RELEASE.

Executes SHA-256 pinned original IC29 04CB..063B, 082F..0850 and
0B60..0C5F (ATTACK table included as non-executed provenance). Unlike the
older linked seam fixture, the DECAY/RELEASE envelope instructions and the
original 083D multiplication subroutine execute without synthetic jumps.

Original synthesis control input RAM and ROM startup are still injected:
this is not an original whole-ROM cold boot / UPD7811 / IRQ timing test.
A compiled production C oracle (envelope106_decay_tick,
envelope106_release_tick, vcf106_global_raw, vcf106_voice_code14) supplies
the expected response; no new Python arithmetic clone is used.
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
from run_mame_vcf_original_attack_probe import (
    ROM_BEGIN, ROM_END, ENTRY, EXIT, original_parts, bootstrap_rom,
)

HELPER_START, HELPER_END = 0x083D, 0x0851
HELPER_SHA256 = "bb924bf1d0d80ad60bc14860a2830dd2ce4cd6b9f42bad25064cb5a097319b6c"

# The test exercises *every* coefficient index on both original code paths;
# extra exact boundary pairs stress sustain clamping, zero and max levels.
EXTRA_CASES: tuple[tuple[int, int, int, int], ...] = (
    (0, 0, 0, 0), (0, 0, 0x3FFF, 0),
    (0, 0, 0x3FFF, 0x3FFF), (0, 0, 0, 0x3FFF),
    (0, 1, 0x3FFF, 0), (0, 1, 1, 0x3FFF),
    (0, 4, 1, 0), (0, 4, 0x3FFF, 0x3FFE),
    (0, 43, 0x3FFF, 0x3FFF), (0, 43, 0x3FFE, 0x3FFF),
    (0, 65, 1, 0), (0, 65, 0x3FFE, 0x3FFD),
    (0, 123, 0x3FFF, 0), (0, 123, 0, 0x3FFF),
    (0, 127, 0x3FFF, 0x3FFE), (0, 127, 0x3FFE, 0x3FFF),
    (1, 0, 0, 0), (1, 0, 0x3FFF, 0),
    (1, 0, 0x3FFE, 0x3FFF), (1, 0, 1, 0),
    (1, 1, 0x3FFF, 0), (1, 1, 1, 0),
    (1, 4, 0x3FFF, 0), (1, 4, 0, 0),
    (1, 43, 0x3FFF, 0), (1, 43, 1, 0x3FFF),
    (1, 65, 0x3FFE, 0), (1, 65, 0x3FFF, 0x3FFF),
    (1, 123, 0x3FFF, 0), (1, 123, 1, 0),
    (1, 127, 0x3FFF, 0), (1, 127, 0x3FFE, 0),
)


def original_decay_helper(source: Path) -> bytes:
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    memory: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match is None:
            continue
        address = int(match.group(1), 16)
        for octet in match.group(2).split():
            if HELPER_START <= address < HELPER_END:
                value = int(octet, 16)
                if address in memory and memory[address] != value:
                    raise ValueError("original decay helper byte conflict")
                memory[address] = value
            address += 1
    if any(address not in memory
           for address in range(HELPER_START, HELPER_END)):
        raise ValueError("original decay helper incomplete")
    result = bytes(memory[address] for address in
                   range(HELPER_START, HELPER_END))
    if hashlib.sha256(result).hexdigest() != HELPER_SHA256:
        raise ValueError("original decay helper SHA-256 mismatch")
    if result[-1] != 0xB8:
        raise AssertionError("original helper no longer ends in RET")
    return result


def cases() -> tuple[tuple[int, int, int, int], ...]:
    # 0 = original DECAY path, 1 = original RELEASE path.
    # Each index is tested for both paths, with sustain above/below the
    # envelope and nontrivial/zero/max envelope values.
    levels = (0, 1, 127, 255, 256, 1024, 4096, 8192,
              0x2001, 0x3F00, 0x3FFE, 0x3FFF)
    sustains = (0, 1, 127, 256, 1024, 4096, 8192, 0x3FFE, 0x3FFF)
    result = [(mode, index,
               levels[(index * 7 + mode * 5) % len(levels)],
               sustains[(index * 5 + mode * 3) % len(sustains)])
              for mode in (0, 1) for index in range(128)]
    result.extend(EXTRA_CASES)
    if (len(result) != 288 or len({(m, idx) for m, idx, _, _ in result})
            != 256 or not any(m == 0 and level < sustain
                              for m, _, level, sustain in result)
            or not any(m == 0 and level > sustain
                       for m, _, level, sustain in result)):
        raise AssertionError("original DECAY/RELEASE coverage changed")
    return tuple(result)


def production_envelopes(
    native_cli: Path, vectors: tuple[tuple[int, int, int, int], ...]
) -> list[tuple[int, int]]:
    inp = "".join(" ".join(map(str, vector)) + "\n" for vector in vectors)
    reply = subprocess.run([str(native_cli)], input=inp, text=True,
                           capture_output=True, check=True, timeout=15)
    results = [tuple(map(int, line.split(","))) for line in
               reply.stdout.splitlines() if line.strip()]
    if len(results) != len(vectors) or any(len(r) != 2 for r in results):
        raise AssertionError("compiled envelope reference count/shape")
    if any(not 0 <= r[0] <= 0xFFFF or not 0 <= r[1] <= 0x3FFF
           for r in results):
        raise AssertionError("compiled envelope response out of range")
    return results


def production_voice(
    native_cli: Path, globals_: list[tuple[int, int, int, int]],
    envelopes: list[tuple[int, int]],
) -> list[int]:
    inp = []
    for i, (amounts, envelope) in enumerate(zip(globals_, envelopes)):
        _, pitch, env_depth, inverted, key = voice_inputs(i)
        inp.append("%d %d %d %d %d %d" % (
            amounts[3], envelope[1], pitch, env_depth, int(inverted), key))
    reply = subprocess.run(
        [str(native_cli)], input="\n".join(inp) + "\n",
        text=True, capture_output=True, check=True, timeout=15)
    numbers = [int(line) for line in reply.stdout.splitlines()
               if line.strip()]
    if len(numbers) != len(globals_) or any(
            not 0 <= x <= 0x3FFF for x in numbers):
        raise AssertionError("compiled VCF voice reference count/value")
    return numbers


def phase_image(original: bytes, dac: bytes, attack_table: bytes,
                helper: bytes, global_case: tuple[int, ...],
                amounts: tuple[int, int, int, int],
                vector: tuple[int, int, int, int],
                coefficient: int, index: int) -> bytes:
    mode, _, current, sustain = vector
    base = bytearray(bootstrap_rom(
        original, dac, attack_table, global_case, amounts,
        0, current, index))
    # The synthetic ATTACK bootstrap is replaced/extended solely inside
    # the synthetic startup address space below original 0x04CB. Original
    # instructions at 04CB..063B remain BYTE-IDENTICAL to the published
    # bytes. State is set to the appropriate original branch:
    # DECAY = voiceRun/rlsPhase/dcyPhase bits 1, attack bit 0.
    # RELEASE = voiceRun 0, initial rlsPhase/dcyPhase 1, attack bit 0.
    jmp = bytes((0x54, ENTRY & 255, ENTRY >> 8))
    position = base.find(jmp, 0, ROM_BEGIN)
    if position < 0 or base.find(jmp, position + 1, ROM_BEGIN) >= 0:
        raise AssertionError("synthetic bootstrap jump not unique")
    additions = (
        (0x11, 1 if mode == 0 else 0),
        (0x33, 1),  # original FF33 release-stage state
        (0x08, 1),  # original FF08 decay-stage state
        (0x07, 0),  # no attack phase
        (0x21, coefficient & 0xFF),
        (0x22, (coefficient >> 8) & 0xFF),
        (0x23, sustain & 0xFF),
        (0x24, sustain >> 8),
        (0x25, coefficient & 0xFF),
        (0x26, (coefficient >> 8) & 0xFF),
    )
    boot = bytearray()
    for address, value in additions:
        boot.extend((0x71, address, value))
    boot.extend(jmp)
    if position + len(boot) >= ROM_BEGIN:
        raise AssertionError("phase input startup overlaps original code")
    base[position:position+len(boot)] = boot
    base[HELPER_START:HELPER_END] = helper
    return bytes(base)


def debugger_commands() -> str:
    return (
        "focus :modulecpu\n"
        'bp 063c:modulecpu,1,{logerror '
        '"FM1JUNO_ORIGINAL_DR:%04X:%02X:%02X:%02X:%02X:%02X:'
        '%04X:%04X:%02X:%02X:%02X\\n",'
        'ea,b@ff27,b@ff28,b@ff33,b@ff08,b@ff07,'
        'sp,hl,b@ff0f,b@ff34,b@ff6a; quit}\n'
        "g\n"
    )


def run(source: Path, envelope_cli: Path, global_cli: Path,
        voice_cli: Path, mame: str, timeout: int) -> None:
    original, dac, attack_table = original_parts(source)
    helper = original_decay_helper(source)
    vectors = cases()
    compiled = production_envelopes(envelope_cli, vectors)
    seeds = global_cases()
    global_inputs = tuple(seeds[i % len(seeds)] for i in range(len(vectors)))
    globals_ = global_production(global_cli, global_inputs)
    voices = production_voice(voice_cli, globals_, compiled)
    cpu = mame_binary(mame)
    print("Original IC29 DECAY/RELEASE and VCF: %d cases (%d per phase "
          "plus boundaries)" % (len(vectors), 128), flush=True)
    print("Original code 04CB..063B, original DAC 082F..083C, "
          "original multiplier 083D..0850, synthetic input RAM", flush=True)

    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-original-dr-"
                                     ) as folder:
        root = Path(folder)
        roms = root / "roms"
        roms.mkdir()
        (root / "cfg").mkdir()
        debugger = root / "decay-release.cmd"
        debugger.write_text(debugger_commands(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000 - 3)
        for i, (vector, expected_env, amounts, expected_voice) in enumerate(
                zip(vectors, compiled, globals_, voices)):
            mode, idx, current, sustain = vector
            rom = phase_image(
                original, dac, attack_table, helper, global_inputs[i],
                amounts, vector, expected_env[0], i)
            with zipfile.ZipFile(roms / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", rom)
            argv = [
                cpu, "juno106", "-rompath", str(roms),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(debugger),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                proc = subprocess.run(
                    argv, text=True, capture_output=True,
                    cwd=root, timeout=timeout,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as exc:
                raise AssertionError(
                    "MAME original %s timed out index=%d case=%r" %
                    ("RELEASE" if mode else "DECAY", i, vector)) from exc
            diagnostic = proc.stdout + proc.stderr
            log = root / "error.log"
            if log.exists():
                diagnostic += "\n" + log.read_text(
                    encoding="utf-8", errors="replace")
                log.unlink()
            marker = re.search(
                r"FM1JUNO_ORIGINAL_DR:([0-9a-fA-F]{4}):"
                r"([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):"
                r"([0-9a-fA-F]{2}):([0-9a-fA-F]{2}):"
                r"([0-9a-fA-F]{2}):([0-9a-fA-F]{4}):"
                r"([0-9a-fA-F]{4}):([0-9a-fA-F]{2}):"
                r"([0-9a-fA-F]{2}):([0-9a-fA-F]{2})", diagnostic)
            if marker is None:
                raise AssertionError(
                    "MAME original %s checkpoint missing case=%d "
                    "idx=%d rc=%d: %s" %
                    ("RELEASE" if mode else "DECAY", i, idx,
                     proc.returncode, diagnostic[-1800:]))
            (ea, env_lo, env_hi, rls_bits, dcy_bits, atk_bits,
             sp, hl, voice_idx, voice_mask, sign) = (
                int(t, 16) for t in marker.groups())
            env = env_lo | (env_hi << 8)
            expected_mask = 1 if mode == 0 else 0
            if (ea != expected_voice or env != expected_env[1] or
                    rls_bits != expected_mask or
                    dcy_bits != expected_mask or atk_bits != 0 or
                    sp != 0xFFEE or hl != 0xFF73 or voice_idx != 0 or
                    voice_mask != 1 or sign != int(amounts[3] < 0)):
                raise AssertionError(
                    "original %s mismatch case=%d idx=%d before=%04X "
                    "sustain=%04X coeff=%04X global=%d: "
                    "MAME EA=%04X ENV=%04X FF33=%02X FF08=%02X "
                    "FF07=%02X SP=%04X HL=%04X voice=%02X mask=%02X "
                    "FF6A=%02X; production EA=%04X ENV=%04X" %
                    ("RELEASE" if mode else "DECAY",
                     i, idx, current, sustain, expected_env[0],
                     amounts[3], ea, env, rls_bits, dcy_bits,
                     atk_bits, sp, hl, voice_idx, voice_mask, sign,
                     expected_voice, expected_env[1]))
            if proc.returncode != 0:
                raise AssertionError(
                    "MAME original DECAY/RELEASE process nonzero "
                    "exit=%d case=%d" % (proc.returncode, i))
            print("MAME_ORIGINAL_DR %03d/%03d PASS %s idx=%03d "
                  "ENV=%04X VCF=%04X" %
                  (i+1, len(vectors),
                   "RELEASE" if mode else "DECAY", idx, env, ea),
                  flush=True)
    print("IC29_ORIGINAL_DR_MAME_CORE: PASS (%d cases)" % len(vectors),
          flush=True)
    print("Scope: independent MAME UPD7810 original DECAY/RELEASE "
          "and 083D multiplier; injected startup/inputs; no original "
          "full ROM, real UPD7811 or IRQ timing.", flush=True)


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
        print("IC29_ORIGINAL_DR_MAME_CORE: FAIL: " + str(error),
              file=sys.stderr)
        sys.exit(1)
