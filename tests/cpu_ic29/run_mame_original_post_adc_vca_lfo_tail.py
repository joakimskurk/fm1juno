#!/usr/bin/env python3
"""Original IC29 continuous post-ADC VCA, LFO, PWM and noise tail.

36 real-internal-ADC IRQ -> original ISR/RETI -> six-voice -> original
ADC bank-service cases are extended WITHOUT modifying original bytes:
through 065F..0737 published bend/portamento/LFO path, through original
06B3 DI, 06B4 SKIT FAD, 06B7 MVI MKH,04, 06BA EI, 06BB JRE 0737,
and actual 0737 POP DE; then executes published 0738..07B7 through\nvoice-six VCA, LFO/PWM, noise VCA and JMP 02EC. The only synthetic scheduler remains between
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

TAIL_START, TAIL_END = 0x0738, 0x07B8
TAIL_SHA = "d9dd161d9b0d3985b4c3e816cef9197c542aa7d02f27cd63a9bf3ef6da4ba191"
# Artificial RAM fixtures; all original-byte source windows remain unchanged.
LFO_SEED = (0x0100, 0x0020)  # level, increment
NOISE_LEVEL = 0x1367
PWM_DEPTH = 0x60


def tail_seed(rom: bytearray, case: int) -> bool:
    """Add deterministic RAM setup to existing SYNTHETIC boot, not ROM code.

    Odd scenario indices select original switch2 ENV VCA mode (bit 2);
    even scenarios retain the original GATE VCA selection.
    FF37 bit1 is preserved for previously proven inverted VCF tests.
    """
    suffix = bytes((0x48, 0x48, 0x00, 0x64, 0x06, 0x04,
                    0xAA, 0x54, 0x00, 0x04))
    start = rom.find(suffix, 0x0200, 0x0400)
    if start < 0 or rom.find(suffix, start+1, 0x0400) >= 0:
        raise AssertionError("synthetic IRQ-clearing boot boundary changed")
    switch2 = bytes((0x71, 0x37))
    at = rom.find(switch2, 0x0200, start)
    if at < 0 or rom.find(switch2, at+1, start) >= 0:
        raise AssertionError("synthetic original VCA mode RAM seed absent")
    mode_env = bool(case & 1)
    if rom[at+2] & 4:
        raise AssertionError("six-voice VCA switch bit unexpectedly set")
    if mode_env:
        rom[at+2] |= 4
    # Independent PWM source selection: FF37 bit 0 high allows
    # modulated LFO PWM; low replaces BC with manual 3FFF.
    if case & 8:
        rom[at+2] |= 1
    # Oscillator enabled/disabled source branch at published 0783:
    # FF1E bit6 set skips "LXI EA,0000" so computed PW survives.
    flag1 = bytes((0x71, 0x1e))
    flag_at = rom.find(flag1, 0x0200, start)
    if flag_at < 0 or rom.find(flag1, flag_at+1, start) >= 0:
        raise AssertionError("original oscillator flag RAM fixture missing")
    if case & 2:
        rom[flag_at+2] |= 0x40
    # FF4A bit0: rising/descending LFO; preserve original polarity
    # bit1 so this exercises both signs without changing VCF setup.
    lfo_flag = bytes((0x71, 0x4a))
    lfo_at = rom.find(lfo_flag, 0x0200, start)
    if lfo_at < 0 or rom.find(lfo_flag, lfo_at+1, start) >= 0:
        raise AssertionError("original LFO flag RAM fixture missing")
    if case & 4:
        rom[lfo_at+2] |= 1
    values = (
        (0x39, NOISE_LEVEL & 0xff),
        (0x3a, NOISE_LEVEL >> 8),
        (0x4b, LFO_SEED[1] & 0xff),
        (0x4c, LFO_SEED[1] >> 8),
        (0x4d, LFO_SEED[0] & 0xff),
        (0x4e, LFO_SEED[0] >> 8),
        (0x47, PWM_DEPTH),
    )
    extra = bytes(b for a,v in values for b in (0x71, a, v))
    # Shift only existing SYNTHETIC boot suffix to make room.
    if start+len(extra)+len(suffix) >= 0x0400:
        raise AssertionError("tail RAM setup overlaps synthetic polling")
    rom[start:start+len(extra)+len(suffix)] = extra + suffix
    return mode_env


def verify_tail(log: str, case: int, scenario: dict,
                expected: dict) -> None:
    def checkpoint(name: str, n: int) -> tuple[int, ...]:
        hits = re.findall(r"FM1JUNO_ORIG_TAIL_" + name +
                          r":([0-9a-fA-F:]+)", log)
        if len(hits) != 1:
            raise AssertionError(
                "case %d tail %s hits=%d; log tail=%s" %
                (case, name, len(hits), log[-2300:]))
        values = tuple(int(z,16) for z in hits[0].split(":"))
        if len(values) != n:
            raise AssertionError("wrong original tail record width %s %r" %
                                 (name, values))
        return values

    vca = checkpoint("VOICE6", 4)  # FF37 switch, FF11 voice mask, EA, SP
    vca_call = checkpoint("VOICE6_DAC", 2)
    vca_return = checkpoint("VOICE6_RETURN", 2)
    lfo = checkpoint("LFO_PWM", 7)
    noise = checkpoint("NOISE_DAC", 2)
    noise_return = checkpoint("NOISE_RETURN", 2)
    jump = checkpoint("JUMP", 4)
    restart = checkpoint("FRAME_HEAD", 4)
    sw, running, ea, sp = vca
    mode_env = bool(case & 1)
    expected_ea = (expected["voices"][5]["envelope"] if mode_env
                   else (0x3FFF if running & 0x20 else 0))
    if bool(sw & 4) != mode_env or ea != expected_ea or sp != 0xFFF0:
        raise AssertionError(
            "case %d published voice6 VCA ENV/GATE selection "
            "SW=%02x RUN=%02x EA=%04x expected=%04x SP=%04x" %
            (case, sw, running, ea, expected_ea, sp))
    if vca_call != (5, expected_ea) or vca_return[1] != expected_ea:
        raise AssertionError(
            "case %d voice6 VCA native DAC helper call/return mismatch %r/%r"
            % (case, vca_call, vca_return))
    # Original 078B->075F rising branch stores 0100+0020=0120;
    # the 075B descending DSUBNB branch stores 0100-0020=00E0.
    # In both cases it preserves the original FF4A polarity bit1.
    lfo_value, coefficient, pwm, flags, depth, switch, sp_lfo = lfo
    expected_lfo = 0x00e0 if case & 4 else 0x0120
    square_enabled = bool(case & 2)
    if (lfo_value != expected_lfo or coefficient != 0x0020 or
            depth != PWM_DEPTH or not 0 <= pwm <= 0x3FFF or
            pwm == 0x1234 or bool(flags & 1) != bool(case & 4) or
            bool(switch & 1) != bool(case & 8) or
            (pwm != 0) != square_enabled or sp_lfo != 0xFFF0):
        raise AssertionError(
            "case %d original 0738 LFO/PWM flags/direction/output "
            "invalid %r expected LFO=%04X square=%d source=%s" %
            (case, lfo, expected_lfo, square_enabled,
             "LFO" if case & 8 else "MANUAL"))
    if noise != (7, NOISE_LEVEL) or noise_return[1] != NOISE_LEVEL:
        raise AssertionError(
            "case %d original noise VCA DAC call/return %r/%r" %
            (case, noise, noise_return))
    if jump[0] != 0xFFF0 or restart[0] != 0xFFF0 or (
            jump[1:] != restart[1:] or jump[2] != 6):
        raise AssertionError(
            "case %d original 07B5 JMP 02EC continuation "
            "SP/DE/voice mismatch %r %r" %
            (case, jump, restart))
    print("MAME_POST_ADC_TAIL %02d/%02d PASS pattern=%s "
          "VCA=%s:%04X LFO=%s:%04X PWM=%s:%s:%04X "
          "NOISE=%04X JMP=07B5->02EC" %
          (case + 1, TOTAL, scenario["pattern"],
           "ENV" if mode_env else "GATE", ea,
           "DOWN" if case & 4 else "UP", lfo_value,
           "ON" if square_enabled else "OFF",
           "LFO" if case & 8 else "MANUAL", pwm,
           NOISE_LEVEL), flush=True)


def verify_tail_irq(log: str, window_end: int, bank: int,
                    case: int, input_profile: str) -> None:
    """Audit genuine late ADC interrupts separately from pre-0738 FAD.

    The original CPU may continue scanning after original EI 06BA.
    Counting only the first 0738-boundary records as all interrupts
    would falsely reject a real 0749/07A9 IRQ; ignoring it would
    conceal a possible pointer/stack corruption. Reject unpaired,
    unexpected, duplicate or pre-expected-mode IRQ events.
    """
    arrivals = [
        m for m in re.finditer(
            r"FM1JUNO_REAL_ISR_ENTRY:([0-9a-fA-F:]+)", log)
        if m.start() >= window_end]
    snapshots = [
        m for m in re.finditer(
            r"FM1JUNO_REAL_ISR_PRE_RETI:([0-9a-fA-F:]+)", log)
        if m.start() >= window_end]
    # Independently capture interrupted PC from original MAME IRQ stack.
    # The saved return PC is at [FFED] when the main loop was
    # interrupted (SP FFED), or [FFEB] inside the original DAC helper
    # (CALL return already pushed two bytes, SP FFEB).
    saved_pc = [
        m for m in re.finditer(
            r"FM1JUNO_TAIL_IRQ_SAVED_PC:([0-9a-fA-F:]+)", log)
        if m.start() >= window_end]
    if (len(arrivals) != len(snapshots)
            or len(arrivals) != len(saved_pc)
            or len(arrivals) > 1):
        raise AssertionError(
            "case %d late VCA/LFO ADC IRQ trace count "
            "entry/pre-RETI/saved-PC unsupported %d/%d/%d"
            % (case, len(arrivals), len(snapshots), len(saved_pc)))
    for arrival, snap, saved in zip(arrivals, snapshots, saved_pc):
        e = tuple(int(x, 16) for x in arrival.group(1).split(":"))
        p = tuple(int(x, 16) for x in snap.group(1).split(":"))
        mode = (bank << 3) ^ 8
        raw = raw_for_bank(1-bank, input_profile)
        if (e[0] not in (0xFFED, 0xFFEB) or e[1:] != (
                mode, 0x04) or
                p != (e[0], mode ^ 8, 0x05, mode, *raw)):
            raise AssertionError(
                "case %d late true ADC IRQ/RETI register state "
                "unexpected e=%r p=%r" % (case, e, p))
        # MAME's IRQ code pushes PSW, PCH, PCL. The saved PC at
        # SP=FFED or FFEB is the *interrupted instruction address*,
        # independent of the timing of a debugger breakpoint.
        words = tuple(int(x, 16) for x in saved.group(1).split(":"))
        if len(words) != 3 or words[0] != e[0]:
            raise AssertionError(
                "case %d IRQ saved PC marker mismatched SP %r %r" %
                (case, e, words))
        interrupted_pc = words[1] if e[0] == 0xFFED else words[2]
        # A transient IRQ at the original CALF call site (0749/07B0)
        # or its return site (074B/07B2) is SP=FFED even though its
        # debugger log appears *between* CALL and RETURN checkpoints.
        # Therefore testing only checkpoint order incorrectly rejects
        # a genuine IRQ, as observed in compiled nonzero case 17.
        if e[0] == 0xFFEB:
            if not 0x082F <= interrupted_pc <= 0x083C:
                raise AssertionError(
                    "case %d nested DAC IRQ from unknown ROM PC %04X" %
                    (case, interrupted_pc))
            if not any(
                (log.find("FM1JUNO_ORIG_TAIL_" + name + "_DAC:")
                 < arrival.start() <
                 log.find("FM1JUNO_ORIG_TAIL_" + name + "_RETURN:"))
                for name in ("VOICE6", "NOISE")
            ):
                raise AssertionError(
                    "case %d nested DAC helper IRQ missing enclosing "
                    "original CALL/RETURN checkpoint" % case)
        elif not 0x0738 <= interrupted_pc <= 0x07B7:
            raise AssertionError(
                "case %d main-loop IRQ saved PC outside original "
                "published tail: %04X" % (case, interrupted_pc))
    if arrivals:
        words = tuple(int(x, 16) for x in saved_pc[0].group(1).split(":"))
        interrupted_pc = words[1] if words[0] == 0xFFED else words[2]
        print("MAME_POST_ADC_TAIL_REAL_IRQ case=%02d "
              "irq_after_0738=1 SP_IRQ=%04X SAVED_PC=%04X" %
              (case, words[0], interrupted_pc), flush=True)


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
    # MAME UPD7810 pushes interrupted PC LOW then HIGH to the IRQ
    # stack immediately before dispatching to vector 0020. Capture
    # both potential stack slots. This resolves genuine IRQ acceptance
    # at 0749 *before* its CALF opcode versus during the DAC helper:
    # both appear between the debugger CALL and RETURN checkpoints,
    # but they have different SP and interrupted PCs.
    match = 'sp,anm,mkh; g}'
    if base.count(match) != 1:
        raise AssertionError("original IRQ entry debugger contract changed")
    base = base.replace(
        match,
        'sp,anm,mkh; logerror '
        '"FM1JUNO_TAIL_IRQ_SAVED_PC:%04X:%04X:%04X\\n",'
        'sp,w@ffed,w@ffeb; g}',
        1)
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
    extra = (
        'bp 0747:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_VOICE6:%02X:%02X:%04X:%04X\\n",'
        'b@ff37,b@ff11,ea,sp; g}\n'
        'bp 0749:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_VOICE6_DAC:%02X:%04X\\n",a,ea; g}\n'
        'bp 074b:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_VOICE6_RETURN:%02X:%04X\\n",a,ea; g}\n'
        'bp 07a9:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_LFO_PWM:%04X:%04X:%04X:%02X:%02X:%02X:%04X\\n",'
        'w@ff4d,w@ff4b,w@ff4f,b@ff4a,b@ff47,b@ff37,sp; g}\n'
        'bp 07b0:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_NOISE_DAC:%02X:%04X\\n",a,ea; g}\n'
        'bp 07b2:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_NOISE_RETURN:%02X:%04X\\n",a,ea; g}\n'
        'bp 07b5:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_JUMP:%04X:%04X:%02X:%04X\\n",'
        'sp,de,b@ff0f,w@ff4d; g}\n'
        'bp 02ec:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_TAIL_FRAME_HEAD:%04X:%04X:%02X:%04X\\n",'
        'sp,de,b@ff0f,w@ff4d; quit}\n'
    )
    return prior + checks.replace("; quit}", "; g}") + extra + "g\n"


def verify_irq_interleaving(log: str, bank: int, case: int,
                            input_profile: str) -> int:
    # The initial genuine IRQ must be complete BEFORE original 6-voice
    # code executes. A *second* real ADC interrupt may occur after
    # original 06BA EI and while 0652's DE cursor is still saved
    # at stack SP=FFEE. Its CPU IRQ frame must then reach FFEB.
    start = log.find("FM1JUNO_SIX_VCF:00:")
    if start == -1:
        raise AssertionError("missing first voice checkpoint")
    verify_irq(log[:start], bank, case, input_profile)
    events = list(re.finditer(
        r"FM1JUNO_REAL_ISR_ENTRY:([0-9a-fA-F:]+)", log))
    pre = list(re.finditer(
        r"FM1JUNO_REAL_ISR_PRE_RETI:([0-9a-fA-F:]+)", log))
    if len(events) not in (1, 2) or len(events) != len(pre):
        raise AssertionError(
            "case %d unexpected asynchronous IRQ count %d/%d" %
            (case, len(events), len(pre)))
    if len(events) == 1:
        return 0
    marker = log.find("FM1JUNO_ORIG_FAD_PRE_EI:")
    beforepop = log.find("FM1JUNO_ORIG_FAD_BEFORE_POP:")
    endmarker = log.find("FM1JUNO_ORIG_FAD_AFTER_POP:")
    if not (0 <= marker < events[1].start() <
            pre[1].start() < endmarker):
        raise AssertionError(
            "second IRQ did not occur within genuine original "
            "06BA EI -> 0737 POP DE window")
    e = tuple(int(v, 16) for v in events[1].group(1).split(":"))
    p = tuple(int(v, 16) for v in pre[1].group(1).split(":"))
    mode = bank << 3
    raw = raw_for_bank(1-bank, input_profile)
    # A second ADC interrupt can be accepted at three boundaries:
    # (1) before 0737; (2) after debugger stops at 0737 but before
    # POP DE; (3) after POP DE but before debugger stops at 0738.
    # In case (3), FF31 was already popped and the IRQ frame
    # starts from FFF0 instead of FFEE.
    phase = (1 if events[1].start() < beforepop else
             2 if e[0] == 0xFFEB else 3)
    irq_sp = 0xFFED if phase == 3 else 0xFFEB
    if e != (irq_sp, mode ^ 8, 0x04):
        raise AssertionError(
            "second IRQ frame/bank/MKH mismatch: " + repr(e))
    if p != (irq_sp, mode, 0x05, mode ^ 8, *raw):
        raise AssertionError(
            "second ADC IRQ did not snapshot alternate bank: " + repr(p))
    return phase


def verify_original_fad(log: str, serial: int, bank: int,
                        second_irq: int) -> None:
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
    expected_post_mkh = 0x05 if second_irq else 0x04
    expected_before_mkh = 0x05 if second_irq == 1 else 0x04
    expected_post_mode = (bank << 3) ^ (8 if second_irq else 0)
    if (after != (0xFFEE, 0x05)
            or preei != (0xFFEE, 0x04)
            or beforepop != (0xFFEE, expected_before_mkh)):
        raise AssertionError(
            "original SKIT/MKH/EI register/stack invariant: %s/%s/%s" %
            (after, preei, beforepop))
    sp, de, mkh, mode, voice, porta = pop
    if (sp, de, mkh, mode, voice) != (
            0xFFF0, 0xFF31, expected_post_mkh, expected_post_mode, 6):
        raise AssertionError(
            "original 0737 POP DE failed to restore saved envelope "
            "cursor: %s" % (pop,))
    print("MAME_ADC_POST_FAD %02d/%02d PASS bank=%d "
          "original_06B3_to_0738=continuous FF7D=%02X "
          "SKIT_FAD_NOP=%d SECOND_IRQ_PHASE=%d" %
          (serial + 1, TOTAL, bank, porta, len(nops), second_irq),
          flush=True)


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
    tail = source_slice(disasm, TAIL_START, TAIL_END, TAIL_SHA)
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
    print( "Published original 065F..07B7 post-ADC control, original "
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
            rom[TAIL_START:TAIL_END] = tail
            # This changes only the synthetic RAM/boot fixture and never
            # a source-pinned original 0738..07B7 instruction.
            tail_seed(rom, i)
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
                    "original post-ADC VCA/LFO/PWM/noise path timeout "
                    "case=%d bank=%d logtail=%s" %
                    (serial, bank, log[-3200:])) from e
            log = p.stdout + p.stderr
            err = root / "error.log"
            if err.exists():
                text = err.read_text(encoding="utf-8", errors="replace")
                err.unlink()
                log = text if "FM1JUNO_REAL_ISR_ENTRY:" in text else log + text
            # Earlier source-pinned FAD test ENDS at 0738. This new
            # tail adds CPU cycles in which a later real ADC IRQ may
            # occur; do not incorrectly attribute those events to the
            # earlier 06BA EI -> 0737 POP interval.
            fad_stop = log.find("FM1JUNO_ORIG_FAD_AFTER_POP:")
            if fad_stop == -1:
                raise AssertionError("missing original 0738 POP return")
            fad_stop = log.find("\n", fad_stop)
            if fad_stop == -1:
                raise AssertionError("original FAD breakpoint truncated")
            fade_window = log[:fad_stop + 1]
            second_irq = verify_irq_interleaving(
                fade_window, bank, serial, input_profile)
            verify_tail_irq(log, fad_stop + 1, bank, serial, input_profile)
            check_six(log, voices[i], globals_[i][3], SCENARIOS[i], i)
            verify_adc(log, (i, bank, OLD_PATTERN[0],
                             raw_for_bank(bank, input_profile)[0]),
                       adc_expect)
            verify_original_fad(log, serial, bank, second_irq)
            verify_tail(log, i, SCENARIOS[i], voices[i])
            if p.returncode:
                raise AssertionError(
                    "MAME original FAD post-ADC exit=%d case=%d" %
                    (p.returncode, serial))
    print("IC29_ADC_POST_TAIL_MAME: PASS (%d real-IRQ CPU cases "
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
        print("IC29_ADC_POST_TAIL_MAME: FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
