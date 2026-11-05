#!/usr/bin/env python3
"""Causal original ADC ISR -> six voices -> selected-bank ADC main-loop test.

The STOCK MAME juno106 UPD7810 CPU's internal ADC conversion timer actually
raises INTFAD. CPU stacks PC/PSW, enters the published IC29 0020->0070 ISR,
copies four REAL MAME CR registers (all zero because the skeleton's AN0..AN7
callbacks are unbound), returns via original RETI, and only THEN reaches six
original envelope/VCF passes + original 0652/06E2 -> 0800 ADC service.

The tiny SYNTHETIC bootstrap waits for the ISR's FF5C publication and then
jumps to original 04D5. The official program's own control scheduler is NOT
executed between the ISR and 04D5. Neither ADC analog input voltages nor
original uPD7811 ROM, physical cycles or audio have been authenticated.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import zipfile

from run_mame_cpu_probe import mame_binary
from run_adc_opcode_window import load_window as original_adc_helper
from run_mame_vcf_global_probe import cases as global_cases
from run_mame_vcf_global_probe import production_values as global_production
from run_mame_vcf_original_attack_probe import original_parts
from run_mame_vcf_original_decay_release_probe import original_decay_helper
from run_mame_six_voice_original_loop import (
    original_loop_extension, vectors as six_vectors,
    compiled_expectations, original_image, check_case as check_six,
)
from run_mame_six_voice_adc_handoff import (
    source_slice, POST_START, POST_END, POST_SHA256,
    BANK0_START, BANK0_END, BANK0_SHA256,
    debugger_script as downstream_debugger,
    verify_adc,
)
from run_mame_adc_isr_direct_probe import (
    original_slice, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA,
    BODY_BEGIN, BODY_END, BODY_SHA,
)

SCENARIOS = six_vectors()
BANKS = (0, 1)
TOTAL = len(SCENARIOS)*len(BANKS)
POLL_PC = 0x0400
PHASE_ENTRY = 0x04D5
OLD_PATTERN = (0, 2, 238, 255)
OTHER_PATTERN = (11, 77, 131, 239)
# A deterministic, visibly distinct TEST-ONLY MAME callback profile.
# Requires a separately compiled MAME driver with AN0..AN7 bound
# to these constants; stock juno106 provides all-zero callbacks.
ANALOG_FIXTURE = (0x11, 0x2D, 0x49, 0x65,
                  0x81, 0x9D, 0xB9, 0xD5)
# Original six-voice fixture covers ATTACK/DECAY/RELEASE and mixed masks.


def test_cases() -> tuple[tuple[int, int], ...]:
    return tuple((i, bank) for i in range(len(SCENARIOS)) for bank in BANKS)


def raw_for_bank(bank: int, input_profile: str) -> tuple[int, ...]:
    if input_profile == "stock-zero":
        return (0, 0, 0, 0)
    if input_profile == "fixed-eight":
        return ANALOG_FIXTURE[bank*4:bank*4+4]
    raise ValueError("unknown ADC input profile: " + input_profile)


def native_adc_expected(
    cli: Path, cases: tuple[tuple[int, int], ...], input_profile: str
) -> list[tuple[int, ...]]:
    # Source bytes here must match *physical emulator ADC callbacks*.
    # No runtime CPU CR patching is allowed in either profile.
    query = "".join(
        " ".join(map(str, (bank, *OLD_PATTERN,
                           *raw_for_bank(bank, input_profile)))) + "\n"
        for _, bank in cases)
    p = subprocess.run([str(cli)], input=query, text=True,
                       capture_output=True, check=True, timeout=15)
    rows = [tuple(map(int, l.split(","))) for l in
            p.stdout.splitlines() if l.strip()]
    if len(rows) != len(cases) or any(len(x) != 8 for x in rows):
        raise AssertionError("compiled production full-bank ADC oracle invalid")
    return rows


def build_rom(original: bytes, continuation: bytes, dac: bytes,
              multiplier: bytes, table: bytes,
              post: bytes, bank0: bytes, adc: bytes,
              vector: bytes, isr: bytes, scenario: dict,
              global_case: tuple[int, ...],
              global_values: tuple[int, int, int, int],
              expected: dict, case: int, bank: int) -> bytes:
    rom = bytearray(original_image(
        original, continuation, dac, multiplier, table,
        scenario, global_case, global_values, expected, case))
    # The standard six-voice fixture boots at 0000 and its synthetic
    # initialization instructions extend through addresses 0020 and
    # 0070. Those addresses are *authentic interrupt vectors/ISR* in
    # this probe. Relocate the complete original fixture bootstrap to
    # 0200 and install a tiny synthetic reset JMP there. Keep all
    # original IRQ opcode regions byte-identical and executable.
    initial_jump = bytes((0x54, 0xD5, 0x04))
    old_end = rom.find(initial_jump, 0, 0x04CB)
    if old_end < 0 or rom.find(initial_jump, old_end+1, 0x04CB) >= 0:
        raise AssertionError("original six-voice fixture JMP not unique")
    synthetic_prefix = bytes(rom[:old_end])
    bootstrap_address = 0x0200
    if bootstrap_address + len(synthetic_prefix) + 96 >= POLL_PC:
        raise AssertionError("relocated bootstrap overlaps synthetic poll")
    rom[:POLL_PC] = bytes(POLL_PC)
    rom[:3] = bytes((0x54, 0x00, 0x02))
    rom[bootstrap_address:bootstrap_address+len(synthetic_prefix)] = (
        synthetic_prefix)
    # Overwrite only the *old synthetic STOP* at 0652; never alter
    # any original published envelope/VCF, ISR or ADC instruction.
    rom[POST_START:POST_END] = post
    rom[BANK0_START:BANK0_END] = bank0
    rom[0x0800:0x0825] = adc
    rom[VECTOR_BEGIN:VECTOR_END] = vector
    rom[BODY_BEGIN:BODY_END] = isr

    # Add a deterministic 8-channel initially accepted/conditioned state
    # to synthetic boot. FF5C AA is a handshake sentinel, not a bank.
    offset = bootstrap_address + len(synthetic_prefix)
    setup = [(0x5C, 0xAA)]
    for k in range(4):
        setup.extend((
            (0x80 + bank*4+k, OLD_PATTERN[k]),
            (0x88 + bank*4+k, 0xB0+k),
            (0x80 + (1-bank)*4+k, OTHER_PATTERN[k]),
            (0x88 + (1-bank)*4+k, 0xA0+k),
        ))
    bootstrap = bytearray()
    for addr, value in setup:
        bootstrap.extend((0x71, addr, value))
    mode = bank << 3
    # MVI MKH,04 -> MVI A,mode -> MOV ANM,A -> EI -> JMP 0400.
    # No direct jump to the original 0020 interrupt vector.
    bootstrap.extend((0x64, 0x06, 0x04, 0x69, mode,
                      0x4D, 0xC8, 0xAA, 0x54, 0x00, 0x04))
    if offset + len(bootstrap) >= POLL_PC:
        raise AssertionError("ADC synthetic bootstrap overlaps poll region")
    rom[offset:offset+len(bootstrap)] = bootstrap
    # Synthetic polling loop (not original Roland scheduler):
    # LDAW FF5C; EQI A,AA (skip next JMP if sentinel unchanged)
    # JMP 04D5 (ISR published FF5C); JMP 0400 (not yet published)
    rom[POLL_PC:POLL_PC+10] = bytes((
        0x01, 0x5C, 0x77, 0xAA,
        0x54, 0xD5, 0x04,
        0x54, 0x00, 0x04,
    ))
    return bytes(rom)


def debugger() -> str:
    original = downstream_debugger()
    if not original.endswith("g\n"):
        raise AssertionError("downstream debugger contract changed")
    extra = (
        'bp 0020:modulecpu,1,{logerror '
        '"FM1JUNO_REAL_ISR_ENTRY:%04X:%02X:%02X\\n",sp,anm,mkh; g}\n'
        'bp 008d:modulecpu,1,{logerror '
        '"FM1JUNO_REAL_ISR_PRE_RETI:%04X:%02X:%02X:%02X:'
        '%02X:%02X:%02X:%02X\\n",'
        'sp,anm,mkh,b@ff5c,b@ff5d,b@ff5e,b@ff5f,b@ff60; g}\n'
        'bp 0400:modulecpu,b@ff5c != 0xaa,{logerror '
        '"FM1JUNO_REAL_ISR_RESUMED:%04X:%02X:%02X:%02X\\n",'
        'sp,anm,mkh,b@ff5c; g}\n'
    )
    return original[:-2] + extra + "g\n"


def verify_irq(log: str, bank: int, case: int,
               input_profile: str) -> None:
    events = [
        re.findall(r"FM1JUNO_REAL_ISR_ENTRY:([0-9a-fA-F:]+)", log),
        re.findall(r"FM1JUNO_REAL_ISR_PRE_RETI:([0-9a-fA-F:]+)", log),
        re.findall(r"FM1JUNO_REAL_ISR_RESUMED:([0-9a-fA-F:]+)", log),
    ]
    if any(len(xs) != 1 for xs in events):
        raise AssertionError("case %d bank %d real ISR/RETI event count "
                             "%s log_tail=%s" %
                             (case, bank, tuple(len(x) for x in events),
                              log[-1700:]))
    entry, pre, resumed = (
        tuple(int(x, 16) for x in data[0].split(":"))
        for data in events)
    mode = bank << 3
    if entry != (0xFFED, mode, 4):
        raise AssertionError(
            "case %d no hardware-stacked ADC vector: %r" %
            (case, entry))
    if pre != (0xFFED, mode ^ 8, 5, mode,
               *raw_for_bank(bank, input_profile)):
        raise AssertionError(
            "case %d original ISR/ANM/CR snapshot wrong: %r" %
            (case, pre))
    if resumed != (0xFFF0, mode ^ 8, 5, mode):
        raise AssertionError(
            "case %d real RETI stack/poll return incorrect: %r" %
            (case, resumed))


def run(disasm: Path, attack_cli: Path, decay_cli: Path, global_cli: Path,
        voice_cli: Path, adc_cli: Path, mame: str, timeout: int,
        input_profile: str) -> None:
    original, dac, table = original_parts(disasm)
    continuation = original_loop_extension(disasm)
    multiplier = original_decay_helper(disasm)
    post = source_slice(disasm, POST_START, POST_END, POST_SHA256)
    bank0 = source_slice(disasm, BANK0_START, BANK0_END, BANK0_SHA256)
    adc = original_adc_helper(disasm)
    vector = original_slice(
        disasm, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA)
    isr = original_slice(
        disasm, BODY_BEGIN, BODY_END, BODY_SHA)
    globals_source = global_cases()
    globals_input = tuple(globals_source[i % len(globals_source)]
                          for i in range(len(SCENARIOS)))
    globals_ = global_production(global_cli, globals_input)
    expect_six = compiled_expectations(
        SCENARIOS, attack_cli, decay_cli, voice_cli, globals_)
    cases = test_cases()
    expect_adc = native_adc_expected(adc_cli, cases, input_profile)
    binary = mame_binary(mame)
    print("Original internal ADC timer IRQ -> RETI -> all 6 original "
          "voices -> original bank-select ADC service: %d cases "
          "input_profile=%s" %
          (len(cases), input_profile), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-real-isr-to-adc-"
                                     ) as directory:
        root = Path(directory)
        roms = root / "roms"
        roms.mkdir()
        (root / "cfg").mkdir()
        debugfile = root / "irq-to-adc.cmd"
        debugfile.write_text(debugger(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for serial, ((i, bank), adc_reference) in enumerate(
                zip(cases, expect_adc)):
            rom = build_rom(
                original, continuation, dac, multiplier, table,
                post, bank0, adc, vector, isr,
                SCENARIOS[i], globals_input[i], globals_[i],
                expect_six[i], i, bank)
            with zipfile.ZipFile(roms / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", rom)
            argv = [
                binary, "juno106", "-rompath", str(roms),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(debugfile),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                p = subprocess.run(
                    argv, cwd=root, text=True, capture_output=True,
                    timeout=timeout,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as e:
                timeout_text = e.stdout or b""
                if isinstance(timeout_text, bytes):
                    timeout_text = timeout_text.decode(
                        "utf-8", errors="replace")
                error = root / "error.log"
                if error.exists():
                    timeout_text += "\n" + error.read_text(
                        encoding="utf-8", errors="replace")
                    error.unlink()
                raise AssertionError(
                    "real internal ADC IRQ to 6-voice service timed out "
                    "case %d bank %d; diagnostic tail=%s" %
                    (i, bank, timeout_text[-2600:])) from e
            log = p.stdout + p.stderr
            error = root / "error.log"
            if error.exists():
                candidate = error.read_text(
                    encoding="utf-8", errors="replace")
                error.unlink()
                log = candidate if "FM1JUNO_REAL_ISR_ENTRY:" in candidate \
                    else log + "\n" + candidate
            verify_irq(log, bank, serial, input_profile)
            check_six(log, expect_six[i], globals_[i][3], SCENARIOS[i], i)
            verify_adc(log, (i, bank, OLD_PATTERN[0],
                             raw_for_bank(bank, input_profile)[0]),
                       adc_reference)
            if p.returncode != 0:
                raise AssertionError(
                    "MAME returned nonzero %d case %d" %
                    (p.returncode, serial))
            print("MAME_IRQ_TO_SIX_ADC %02d/%02d PASS pattern=%s bank=%d "
                  "ISR=real ADC timer CR=%s" %
                  (serial+1, TOTAL, SCENARIOS[i]["pattern"], bank,
                   "".join("%02X" % x for x in
                           raw_for_bank(bank, input_profile))),
                  flush=True)
    print("IC29_IRQ_TO_SIX_ADC_MAME: PASS (%d causal CPU scenarios, "
          "%d voices, %d ADC channel comparisons)" %
          (TOTAL, 6*TOTAL, 4*TOTAL), flush=True)
    print("Scope: UPD7810 MAME ADC peripheral with %s callback data. "
          "SYNTHETIC handoff poll/JMP between original ISR and original "
          "main loop; NOT original scheduler or calibrated physical "
          "timing." % input_profile, flush=True)


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
    parser.add_argument(
        "--input-profile", choices=("stock-zero", "fixed-eight"),
        default="stock-zero",
        help="fixed-eight requires a separately rebuilt TEST-ONLY MAME "
             "driver with eight exact AN0..AN7 callbacks; stock-zero "
             "works with official MAME and MUST NOT accept nonzero data")
    a = parser.parse_args()
    try:
        run(a.ic29_disasm, a.attack_cli, a.decay_cli, a.global_cli,
            a.voice_cli, a.adc_cli, a.mame, a.case_timeout,
            a.input_profile)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_IRQ_TO_SIX_ADC_MAME: FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
