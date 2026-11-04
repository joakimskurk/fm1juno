#!/usr/bin/env python3
"""Bounded authentic IC29 six-voice envelope/VCF loop under independent MAME.

Published original 0x04CB..0x0651 bytes execute once from 0x04D5 through
all six naturally repeated 0x0503 envelope/VCF paths (including DAC
calls at 0x063E) and original 0x0651 POP EA, with no inserted jump in
any of those original regions. Original 0x082F DAC, 0x083D decay
multiplication and 128-entry 0x0B60 ATTACK table are loaded byte-pinned.

CPU startup, six per-voice state inputs, common panel coefficient RAM,
precomputed global LFO/bend and original ROM holes are synthesized.
This does NOT prove real JUNO-106 six-voice peripheral, IRQ, UPD7811
silicon, exact timing or full-ROM behaviour.
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
    original_parts, native_attack_values, ROM_BEGIN, ROM_END
)
from run_mame_vcf_original_decay_release_probe import (
    original_decay_helper, production_envelopes,
)

EXT_START, EXT_END, STOP = 0x063C, 0x0652, 0x0652
EXT_SHA256 = "839530afc63bf2e980c9f9d4a51df5a0527a092504ad228117f76724ce8fcf25"

# A = original ATTACK path, D = original DECAY path, R = original RELEASE.
# The cases deliberately include all-one-phase and mixed phases, so FF07,
# FF08, FF11, FF33 masks change in different ways across the same CPU pass.
PHASE_PATTERNS = (
    "AAAAAA", "DDDDDD", "RRRRRR", "ADRADR", "RDAADR", "AARDDR",
    "RRADDA", "DRAARD", "ARRDDA", "DARARD", "RDRDAA", "DADARR",
    "AADDDR", "DRRAAD", "RADRDA", "DRAADR", "ARDADR", "RADDRA",
)
DECAY_INDICES = (
    0, 1, 2, 3, 4, 5, 15, 31, 43, 63, 64, 65, 85, 86, 123, 127, 108, 121,
)
LEVELS = (0, 1, 127, 255, 1024, 4096, 8192,
          0x2001, 0x3F00, 0x3F80, 0x3FFE, 0x3FFF)
SUSTAINS = (0, 127, 256, 4096, 8192, 0x3FFE, 0x3FFF)
PITCHES = (0, 48 << 8, 60 << 8, 72 << 8, 84 << 8, 127 << 8,
           (60 << 8) + 15, (60 << 8) - 15)


def original_loop_extension(source: Path) -> bytes:
    pattern = re.compile(r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    memory: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        for octet in match.group(2).split():
            if EXT_START <= addr < EXT_END:
                byte = int(octet, 16)
                if addr in memory and memory[addr] != byte:
                    raise ValueError("published six-voice opcode conflict")
                memory[addr] = byte
            addr += 1
    if any(addr not in memory for addr in range(EXT_START, EXT_END)):
        raise ValueError("missing original six-voice opcode bytes")
    segment = bytes(memory[addr] for addr in range(EXT_START, EXT_END))
    if hashlib.sha256(segment).hexdigest() != EXT_SHA256:
        raise ValueError("original six-voice loop SHA-256 mismatch")
    if segment[-4:] != bytes.fromhex("540305a4"):
        raise AssertionError("original six-voice loop/POP opcode changed")
    return segment


def vectors() -> tuple[dict, ...]:
    out = []
    for i, pattern in enumerate(PHASE_PATTERNS):
        attack = 64 if i == 14 else (i * 13 + 3) % 128
        coefficient = DECAY_INDICES[i]
        sustain = SUSTAINS[(i * 3) % len(SUSTAINS)]
        initial = [LEVELS[(i * 7 + voice * 5) % len(LEVELS)]
                   for voice in range(6)]
        if i == 14:
            # Exact ATTACK equality regression during mixed-voice looping.
            attack = 64
            initial[1] = 0x3F80
        pitch = [PITCHES[(i + v * 3) % len(PITCHES)]
                 for v in range(6)]
        out.append({
            "pattern": pattern, "attack": attack, "decay_index": coefficient,
            "sustain": sustain, "levels": tuple(initial),
            "pitches": tuple(pitch),
        })
    if (len(out) != 18 or any(len(v["pattern"]) != 6 for v in out) or
            not all(set(p["pattern"]) <= set("ADR") for p in out)):
        raise AssertionError("mixed-six-voice vector definition changed")
    return tuple(out)


def compiled_expectations(
    scenarios: tuple[dict, ...], attack_cli: Path, dr_cli: Path,
    voice_cli: Path, globals_: list[tuple[int, int, int, int]]
) -> list[dict]:
    indexed = [(i, voice) for i in range(len(scenarios)) for voice in range(6)]
    attack_inputs = tuple(
        (scenarios[i]["attack"], scenarios[i]["levels"][v])
        for i, v in indexed)
    dr_inputs = tuple(
        (0 if scenarios[i]["pattern"][v] == "D" else 1,
         scenarios[i]["decay_index"], scenarios[i]["levels"][v],
         scenarios[i]["sustain"])
        for i, v in indexed)
    attacks = native_attack_values(attack_cli, attack_inputs)
    drs = production_envelopes(dr_cli, dr_inputs)
    records = []
    inputs = []
    for i, v in indexed:
        p = scenarios[i]
        phase = p["pattern"][v]
        k = 6 * i + v
        if phase == "A":
            envelope = attacks[k][1]
            phase_flag = attacks[k][2]
            # Original attack code only ORs FF33/FF08 when sum exceeds 3fff,
            # preserving FF07 attack bit during this one-pass check.
            flags = (1, phase_flag, phase_flag)
        elif phase == "D":
            envelope = drs[k][1]
            flags = (0, 1, 1)
        else:
            envelope = drs[k][1]
            flags = (0, 0, 0)
        _, _, env_depth, inverted, key = voice_inputs(i)
        inputs.append("%d %d %d %d %d %d" % (
            globals_[i][3], envelope, p["pitches"][v],
            env_depth, int(inverted), key))
        records.append({"case": i, "voice": v, "envelope": envelope,
                        "flags": flags})
    reply = subprocess.run([str(voice_cli)], text=True,
                           input="\n".join(inputs) + "\n",
                           capture_output=True, check=True, timeout=15)
    vcfs = [int(v) for v in reply.stdout.splitlines() if v.strip()]
    if len(vcfs) != len(records) or any(not 0 <= v <= 0x3FFF for v in vcfs):
        raise AssertionError("invalid compiled production six-voice VCF")
    for row, vcf in zip(records, vcfs):
        row["vcf"] = vcf
    return [dict(
        voices=records[6*i:6*(i+1)],
        attack_increment=attacks[6*i][0],
        decay_coefficient=drs[6*i][0],
    ) for i in range(len(scenarios))]


def original_image(
    prefix: bytes, extension: bytes, dac: bytes, helper: bytes,
    attack_lookup: bytes, scenario: dict, global_case: tuple[int, ...],
    amounts: tuple[int, int, int, int], expected: dict, case: int
) -> bytes:
    cutoff, lfo, bend, _ = amounts
    _, _, env_depth, inverted, key = voice_inputs(case)
    # Original CPU register and RAM setup, separate from pinned
    # executed code. MVI V,FF and LXI SP,FFF0.
    boot = bytearray((0x68, 0xFF, 0x04, 0xF0, 0xFF))
    run_mask = sum(1 << v for v, ch in enumerate(scenario["pattern"])
                   if ch != "R")
    attack_mask = sum(1 << v for v, ch in enumerate(scenario["pattern"])
                      if ch == "A")
    # Release stage markers are set before DECAY and RELEASE; the latter
    # clears them during the original 0538..0556 instruction path.
    decay_start = sum(1 << v for v, ch in enumerate(scenario["pattern"])
                      if ch != "A")
    state = [
        (0x4A, 2 if global_case[4] else 0),
        (0x1E, 0x20 if global_case[7] else 0),
        (0x3D, cutoff & 0xFF), (0x3E, cutoff >> 8),
        (0x53, lfo & 0xFF), (0x54, lfo >> 8),
        (0x65, bend & 0xFF), (0x66, bend >> 8),
        (0x41, env_depth * 2), (0x42, key * 2),
        (0x37, 0 if inverted else 2),
        (0x11, run_mask),
        (0x07, attack_mask),
        (0x08, decay_start),
        (0x33, decay_start),
        (0x45, scenario["attack"]),
        (0x21, expected["decay_coefficient"] & 0xFF),
        (0x22, expected["decay_coefficient"] >> 8),
        (0x23, scenario["sustain"] & 0xFF),
        (0x24, scenario["sustain"] >> 8),
        (0x25, expected["decay_coefficient"] & 0xFF),
        (0x26, expected["decay_coefficient"] >> 8),
        (0x4F, 0x34), (0x50, 0x12),
    ]
    for voice in range(6):
        level = scenario["levels"][voice]
        pitch = scenario["pitches"][voice]
        state.extend((
            (0x27 + 2*voice, level & 0xFF),
            (0x28 + 2*voice, level >> 8),
            (0x71 + 2*voice, pitch & 0xFF),
            (0x72 + 2*voice, pitch >> 8),
        ))
    for address, val in state:
        if not 0 <= val <= 0xFF:
            raise ValueError("synthetic six-voice RAM seed out of range")
        boot.extend((0x71, address, val))
    boot.extend((0x54, 0xD5, 0x04))  # JMP published global VCF entry
    if len(boot) >= ROM_BEGIN:
        raise AssertionError("synthetic input setup overlaps original ROM")
    image = bytearray(0x2000)
    image[:len(boot)] = boot
    image[ROM_BEGIN:ROM_END] = prefix
    image[EXT_START:EXT_END] = extension
    image[0x082F:0x083D] = dac
    image[0x083D:0x0851] = helper
    image[0x0B60:0x0C60] = attack_lookup
    # Synthetic trap BEYOND the real original 0651 POP EA, never fetched.
    image[STOP:STOP+3] = bytes((0x54, STOP & 0xFF, STOP >> 8))
    return bytes(image)


def debugger_script() -> str:
    # Every VCF channel is captured both immediately before the original
    # 063E DAC call and at its 0640 return; this proves the six 082F
    # helper invocations completed rather than assuming they did.
    captures = [
        ("b@ff0f", "%02X"), ("b@ff34", "%02X"),
        ("sp", "%04X"), ("hl", "%04X"), ("de", "%04X"),
        ("b@ff07", "%02X"), ("b@ff08", "%02X"),
        ("b@ff33", "%02X"), ("b@ff6a", "%02X"),
        ("b@ff6b", "%02X"),
    ] + [("b@%04x" % (0xFF27+i), "%02X") for i in range(12)]
    # Final saved 12 envelope bytes and all masks must be read *after*
    # the sixth voice and actual original POP EA, at 0x0652.
    fmt = ":".join(f for _, f in captures)
    args = ",".join(k for k, _ in captures)
    return (
        "focus :modulecpu\n"
        'bp 063c:modulecpu,1,{logerror '
        '"FM1JUNO_SIX_VCF:%02X:%04X\\n",b@ff0f,ea; g}\n'
        'bp 0640:modulecpu,1,{logerror '
        '"FM1JUNO_SIX_DAC_RETURN:%02X:%04X\\n",b@ff0f,ea; g}\n'
        'bp 0652:modulecpu,1,{logerror '
        '"FM1JUNO_SIX_FINAL:' + fmt + '\\n",' + args + '; quit}\n'
        "g\n"
    )


def check_case(
    text: str, expected: dict, global_raw: int, scenario: dict, case: int
) -> None:
    vcf_matches = re.findall(
        r"FM1JUNO_SIX_VCF:([0-9a-fA-F]{2}):([0-9a-fA-F]{4})", text)
    return_matches = re.findall(
        r"FM1JUNO_SIX_DAC_RETURN:([0-9a-fA-F]{2}):([0-9a-fA-F]{4})",
        text)
    for label, records in (("pre-DAC", vcf_matches),
                           ("DAC-return", return_matches)):
        if len(records) != 6:
            raise AssertionError(
                "case %d %s checkpoint count %d (expected six): %s" %
                (case, label, len(records), text[-2200:]))
        for v, (channel, value) in enumerate(records):
            if int(channel, 16) != v or int(value, 16) !=                     expected["voices"][v]["vcf"]:
                raise AssertionError(
                    "case %d voice %d %s VCF mismatch: channel=%s EA=%s "
                    "compiled=%04X" %
                    (case, v, label, channel, value,
                     expected["voices"][v]["vcf"]))
    checkpoints = re.findall(
        r"FM1JUNO_SIX_FINAL:([0-9a-fA-F:]+)", text)
    if len(checkpoints) != 1:
        raise AssertionError(
            "case %d original six-voice final checkpoint count=%d: %s" %
            (case, len(checkpoints), text[-2200:]))
    parts = [int(w, 16) for w in checkpoints[0].split(":")]
    if len(parts) != 22:
        raise AssertionError("case %d malformed final MAME checkpoint: %r" %
                             (case, parts))
    (voice_index, voice_mask, sp, hl, de, attack_flags,
     decay_flags, release_flags, global_flag, voice_flag) = parts[:10]
    envelopes = [parts[10+i*2] + (parts[11+i*2] << 8) for i in range(6)]
    expected_attack = sum(
        v["flags"][0] << i for i, v in enumerate(expected["voices"]))
    expected_decay = sum(
        v["flags"][1] << i for i, v in enumerate(expected["voices"]))
    expected_release = sum(
        v["flags"][2] << i for i, v in enumerate(expected["voices"]))
    for v, (actual, row) in enumerate(zip(envelopes, expected["voices"])):
        if actual != row["envelope"]:
            raise AssertionError(
                "case %d voice %d original ENV=%04X production=%04X" %
                (case, v, actual, row["envelope"]))
    if (voice_index != 6 or voice_mask != 0x40 or
            sp != 0xFFF0 or hl != 0xFF7D or de != 0xFF31 or
            attack_flags != expected_attack or
            decay_flags != expected_decay or
            release_flags != expected_release or
            global_flag != int(global_raw < 0)):
        raise AssertionError(
            "case %d pattern=%s final CPU mismatch: "
            "index=%02X bit=%02X SP=%04X HL=%04X DE=%04X "
            "FF07=%02X/%02X FF08=%02X/%02X FF33=%02X/%02X FF6A=%02X" %
            (case, scenario["pattern"], voice_index, voice_mask,
             sp, hl, de, attack_flags, expected_attack,
             decay_flags, expected_decay, release_flags,
             expected_release, global_flag))
    print("MAME_SIX_VOICE %02d/%02d PASS phases=%s "
          "envelopes=%s vcf=%s" %
          (case+1, len(PHASE_PATTERNS), scenario["pattern"],
           ",".join("%04X" % v for v in envelopes),
           ",".join("%04X" % r["vcf"] for r in expected["voices"])),
          flush=True)


def run(disasm: Path, attack_cli: Path, dr_cli: Path,
        global_cli: Path, voice_cli: Path, mame: str, timeout: int) -> None:
    prefix, dac, attack_lookup = original_parts(disasm)
    helper = original_decay_helper(disasm)
    extension = original_loop_extension(disasm)
    scenarios = vectors()
    globals_source = global_cases()
    globals_inputs = tuple(
        globals_source[i % len(globals_source)]
        for i in range(len(scenarios)))
    globals_ = global_production(global_cli, globals_inputs)
    expected = compiled_expectations(
        scenarios, attack_cli, dr_cli, voice_cli, globals_)
    cpu = mame_binary(mame)
    print("MAME original six-voice ENV/VCF: %d full CPU loop scenarios "
          "(%d per-voice calculations)" %
          (len(scenarios), len(scenarios)*6), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-six-voice-"
                                     ) as folder:
        root = Path(folder)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        debugger = root / "six-voice.cmd"
        debugger.write_text(debugger_script(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000 - 3)
        for i, scenario in enumerate(scenarios):
            image = original_image(
                prefix, extension, dac, helper, attack_lookup,
                scenario, globals_inputs[i], globals_[i], expected[i], i)
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", image)
            args = [
                cpu, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(debugger),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                result = subprocess.run(
                    args, text=True, capture_output=True, cwd=root,
                    timeout=timeout,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as err:
                raise AssertionError(
                    "six-voice original CPU loop timeout case=%d "
                    "phase=%s" % (i, scenario["pattern"])) from err
            output = result.stdout + result.stderr
            logfile = root / "error.log"
            if logfile.exists():
                output += "\n" + logfile.read_text(
                    encoding="utf-8", errors="replace")
                logfile.unlink()
            check_case(output, expected[i], globals_[i][3], scenario, i)
            if result.returncode != 0:
                raise AssertionError(
                    "six-voice MAME process returned %d case %d" %
                    (result.returncode, i))
    print("IC29_SIX_VOICE_MAME_CORE: PASS (%d full original-loop cases, "
          "%d compared voices)" % (len(scenarios), 6*len(scenarios)),
          flush=True)
    print("Scope: original 04CB..0651, 082F..0850, 0B60 attack "
          "table, MAME UPD7810; synthetic initial RAM and boot, "
          "no original IRQ/ADC/full ROM/UPD7811/audio.", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--attack-cli", type=Path, required=True)
    parser.add_argument("--decay-cli", type=Path, required=True)
    parser.add_argument("--global-cli", type=Path, required=True)
    parser.add_argument("--voice-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    args = parser.parse_args()
    try:
        run(args.ic29_disasm, args.attack_cli, args.decay_cli,
            args.global_cli, args.voice_cli, args.mame, args.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_SIX_VOICE_MAME_CORE: FAIL: " + str(exc), file=sys.stderr)
        sys.exit(1)
