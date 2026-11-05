#!/usr/bin/env python3
"""Original IC29 ADC follow-on through DI/SKIT FAD/MKH/EI and POP DE.

36 real-internal-ADC IRQ -> original ISR/RETI -> six-voice -> original
ADC bank-service cases are extended WITHOUT modifying original bytes:
through 065F..0737 published bend/portamento/LFO path, through original
06B3 DI, 06B4 SKIT FAD, 06B7 MVI MKH,04, 06BA EI, 06BB JRE 0737,
and actual 0737 POP DE. The only synthetic scheduler remains between
the real ISR RETI and 04D5; this probe does not assert cycle timing,
UPD7811 chip/ROM identity or original Roland audio/analogue calibration.
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
from run_mame_vcf_original_attack_probe import original_parts
from run_mame_vcf_original_decay_release_probe import original_decay_helper
from run_mame_six_voice_original_loop import (
    original_loop_extension, compiled_expectations, check_case as check_six,
)
from run_mame_six_voice_adc_handoff import (
    source_slice, POST_START, POST_END, POST_SHA256, BANK0_START,
    BANK0_END, BANK0_SHA256,
    verify_adc,
)
from run_adc_opcode_window import load_window as load_adc_window
from run_mame_adc_isr_direct_probe import (
    original_slice, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA,
    BODY_BEGIN, BODY_END, BODY_SHA,
)
from run_mame_adc_irq_to_six_voice_service import (
    build_rom, test_cases, native_adc_expected, verify_irq, raw_for_bank,
    debugger as source_debugger,
    OLD_PATTERN, SCENARIOS, TOTAL,
)

FOLLOW_START, FOLLOW_END = 0x065F, 0x0738
FOLLOW_SHA = "b9a57a701bb31daeecc064843efc8db94faacfba27c6649b0820eb45dc514bb2"
PORTA_START, PORTA_END = 0x0A00, 0x0A80
PORTA_SHA = "06d1c862622b5aaa2b7e42d561dbdf2cd424620a8e46cfa0c2c9deb5c484984e"


def source_table(source: Path) -> bytes:
    # Original published lookup table lines contain up to sixteen
    # contiguous byte tokens, unlike 1..5-byte instruction lines.
    data: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^([0-9a-fA-F]{4}):\s+(.+)$", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if not PORTA_START <= addr < PORTA_END:
            continue
        for token in m.group(2).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            if addr in data and data[addr] != int(token, 16):
                raise ValueError("published portamento table conflict")
            data[addr] = int(token, 16)
            addr += 1
    if any(a not in data for a in range(PORTA_START, PORTA_END)):
        raise ValueError("missing published portamento lookup bytes")
    table = bytes(data[a] for a in range(PORTA_START, PORTA_END))
    actual = hashlib.sha256(table).hexdigest()
    if actual != PORTA_SHA:
        raise ValueError("source-pinned original portamento table drift " + actual)
    return table


def debugger() -> str:
    # Reuse the FULL genuine ADC interrupt + RETI debugger contract,
    # not only the six-voice/ADC checkpoints. The latter would miss
    # proof of real IRQ entry and register/stack return.
    base = source_debugger()
    if not base.endswith("g\n"):
        raise AssertionError("downstream ADC debugger contract changed")
    # Existing checkpoints at 0652, 065F and 06E7 MUST continue,
    # not quit, so all prior differential voice and ADC assertions
    # remain meaningful.
    prior = base[:-2].replace("; quit}", "; g}")
    checks = (
        'bp 06b3:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_ENTER:%04X:%02X:%02X:%02X\\n",'
        'sp,mkh,b@ff5c,b@ff0f; g}\n'
        'bp 06b6:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_NOP:%04X\\n",sp; g}\n'
        'bp 06b7:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_AFTER_SKIT:%04X:%02X\\n",sp,mkh; g}\n'
        'bp 06ba:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_PRE_EI:%04X:%02X\\n",sp,mkh; g}\n'
        'bp 0737:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_BEFORE_POP:%04X:%02X\\n",sp,mkh; g}\n'
        'bp 0738:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FAD_AFTER_POP:%04X:%04X:%02X:%02X:%02X:%02X\\n",'
        'sp,de,mkh,b@ff5c,b@ff0f,b@ff7d; quit}\n'
    )
    return prior + checks + "g\n"


def verify_original_fad(log: str, serial: int, bank: int) -> None:
    def record(name: str, fields: int) -> tuple[int, ...]:
        hits = re.findall(r"FM1JUNO_ORIG_FAD_" + name +
                          r":([0-9a-fA-F:]+)", log)
        if len(hits) != 1:
            raise AssertionError(
                "case %d bank %d original FAD %s had %d hits; tail=%s"
                % (serial, bank, name, len(hits), log[-2000:]))
        values = tuple(int(v, 16) for v in hits[0].split(":"))
        if len(values) != fields:
            raise AssertionError("original FAD debugger width %s %r" %
                                 (name, values))
        return values

    enter = record("ENTER", 4)
    after = record("AFTER_SKIT", 2)
    preei = record("PRE_EI", 2)
    beforepop = record("BEFORE_POP", 2)
    pop = record("AFTER_POP", 6)
    nops = re.findall(r"FM1JUNO_ORIG_FAD_NOP:([0-9a-fA-F]+)", log)
    if len(nops) > 1:
        raise AssertionError("case %d reentered original FAD boundary" %
                             serial)
    if enter != (0xFFEE, 0x05, bank << 3, 6):
        raise AssertionError(
            "original DI entry wrong: %s bank %d" % (enter, bank))
    if (after != (0xFFEE, 0x05)
            or preei != (0xFFEE, 0x04)
            or beforepop != (0xFFEE, 0x04)):
        raise AssertionError(
            "original SKIT/MKH/EI register/stack invariant: %s/%s/%s" %
            (after, preei, beforepop))
    sp, de, mkh, mode, voice, porta = pop
    if (sp, de, mkh, mode, voice) != (
            0xFFF0, 0xFF31, 0x04, bank << 3, 6):
        raise AssertionError(
            "original 0737 POP DE failed to restore saved envelope "
            "cursor: %s" % (pop,))
    print("MAME_ADC_POST_FAD %02d/%02d PASS bank=%d "
          "original_06B3_to_0738=continuous FF7D=%02X "
          "SKIT_FAD_NOP=%d" %
          (serial + 1, TOTAL, bank, porta, len(nops)), flush=True)


def run(disasm: Path, attack_cli: Path, decay_cli: Path, global_cli: Path,
        voice_cli: Path, adc_cli: Path, mame: str, timeout: int,
        input_profile: str) -> None:
    original, dac, table = original_parts(disasm)
    continuation = original_loop_extension(disasm)
    multiplier = original_decay_helper(disasm)
    post = source_slice(disasm, POST_START, POST_END, POST_SHA256)
    bank0 = source_slice(disasm, BANK0_START, BANK0_END, BANK0_SHA256)
    follow = source_slice(disasm, FOLLOW_START, FOLLOW_END, FOLLOW_SHA)
    porta = source_table(disasm)
    adc = load_adc_window(disasm)
    vector = original_slice(
        disasm, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA)
    isr = original_slice(disasm, BODY_BEGIN, BODY_END, BODY_SHA)
    gcases = global_cases()
    globals_input = tuple(gcases[i % len(gcases)]
                          for i in range(len(SCENARIOS)))
    globals_ = global_production(global_cli, globals_input)
    voices = compiled_expectations(
        SCENARIOS, attack_cli, decay_cli, voice_cli, globals_)
    cases = test_cases()
    adcs = native_adc_expected(adc_cli, cases, input_profile)
    binary = mame_binary(mame)
    print("Published 065F..0737 downstream control, original "
          "DI/SKIT FAD/MKH/EI, 0737 POP DE, %d cases profile=%s" %
          (TOTAL, input_profile), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-original-fad-") as td:
        root = Path(td)
        romdir = root / "roms"
        romdir.mkdir()
        (root / "cfg").mkdir()
        script = root / "follow.cmd"
        script.write_text(debugger(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000 - 3)
        for serial, ((i, bank), adc_expect) in enumerate(zip(cases, adcs)):
            rom = bytearray(build_rom(
                original, continuation, dac, multiplier, table,
                post, bank0, adc, vector, isr,
                SCENARIOS[i], globals_input[i], globals_[i],
                voices[i], i, bank))
            rom[FOLLOW_START:FOLLOW_END] = follow
            rom[PORTA_START:PORTA_END] = porta
            with zipfile.ZipFile(romdir / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", bytes(rom))
            cmd = [
                binary, "juno106", "-rompath", str(romdir),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(script),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                p = subprocess.run(cmd, cwd=root, text=True,
                                   capture_output=True, timeout=timeout,
                                   env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as e:
                log = e.stdout or b""
                if isinstance(log, bytes):
                    log = log.decode("utf-8", errors="replace")
                error = root / "error.log"
                if error.exists():
                    log += error.read_text(encoding="utf-8",
                                           errors="replace")
                    error.unlink()
                raise AssertionError(
                    "original post-ADC FAD path timeout "
                    "case=%d bank=%d logtail=%s" %
                    (serial, bank, log[-3200:])) from e
            log = p.stdout + p.stderr
            err = root / "error.log"
            if err.exists():
                text = err.read_text(encoding="utf-8", errors="replace")
                err.unlink()
                log = text if "FM1JUNO_REAL_ISR_ENTRY:" in text else log + text
            verify_irq(log, bank, serial, input_profile)
            check_six(log, voices[i], globals_[i][3], SCENARIOS[i], i)
            verify_adc(log, (i, bank, OLD_PATTERN[0],
                             raw_for_bank(bank, input_profile)[0]),
                       adc_expect)
            verify_original_fad(log, serial, bank)
            if p.returncode:
                raise AssertionError(
                    "MAME original FAD post-ADC exit=%d case=%d" %
                    (p.returncode, serial))
    print("IC29_ADC_POST_FAD_MAME: PASS (%d real-IRQ CPU cases "
          "through original 065F..0737 and original FAD scheduler "
          "boundary, 216 voices, 144 ADC channels)" % TOTAL, flush=True)
    print("Scope: original post-ADC section with synthetic initial "
          "state and ISR->04D5 dispatch; NOT original whole-ROM "
          "scheduler, original uPD7811 silicon or physical cycles.",
          flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--attack-cli", type=Path, required=True)
    parser.add_argument("--decay-cli", type=Path, required=True)
    parser.add_argument("--global-cli", type=Path, required=True)
    parser.add_argument("--voice-cli", type=Path, required=True)
    parser.add_argument("--adc-cli", type=Path, required=True)
    parser.add_argument("--mame", default="mame")
    parser.add_argument("--case-timeout", type=int, default=14)
    parser.add_argument("--input-profile", choices=("stock-zero", "fixed-eight"),
                        default="stock-zero")
    args = parser.parse_args()
    try:
        run(args.ic29_disasm, args.attack_cli, args.decay_cli,
            args.global_cli, args.voice_cli, args.adc_cli,
            args.mame, args.case_timeout, args.input_profile)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_ADC_POST_FAD_MAME: FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
