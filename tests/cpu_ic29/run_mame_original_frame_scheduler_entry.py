#!/usr/bin/env python3
"""Run original 02EC..03A0 first next-frame scheduler slice in MAME.

A real MAME UPD7810 ADC conversion IRQ enters the published IC29 vector,
and original ISR/RETI, six original voices, four original ADC channels,
FAD guard, voice-6 VCA, LFO/PWM/noise and original 07B5->02EC JMP
all run as in the already verified post-tail fixture. Unlike that
fixture, this runner executes the source-pinned ORIGINAL 02EC..03A0
frame scheduler up to 03A1: voice-run/sustain, resonance DAC, and
LFO holdoff/attack/modulation branches.

The startup and FF5C poll remain *synthetic*, but have been relocated
into documented public-ROM NOP holes at 0863 and 09E0 so they cannot
overwrite the new 02EC original code. This does NOT authenticate a
full original ROM, UPD7811 CPU or physical PIT/ADC timing.
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
from run_mame_vcf_global_probe import cases as global_cases
from run_mame_vcf_global_probe import production_values as global_production
from run_mame_vcf_original_attack_probe import original_parts
from run_mame_vcf_original_decay_release_probe import original_decay_helper
from run_mame_six_voice_original_loop import (
    original_loop_extension, compiled_expectations, check_case as check_six,
)
from run_mame_six_voice_adc_handoff import (
    source_slice, POST_START, POST_END, POST_SHA256,
    BANK0_START, BANK0_END, BANK0_SHA256, verify_adc,
)
from run_adc_opcode_window import load_window as load_adc_window
from run_mame_adc_isr_direct_probe import (
    original_slice, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA,
    BODY_BEGIN, BODY_END, BODY_SHA,
)
from run_mame_adc_irq_to_six_voice_service import (
    build_rom, test_cases, native_adc_expected,
    raw_for_bank, OLD_PATTERN, SCENARIOS,
)
from run_mame_adc_post_service_fad import (
    FOLLOW_START, FOLLOW_END, FOLLOW_SHA,
    PORTA_START, PORTA_END, source_table,
    verify_irq_interleaving, verify_original_fad,
)
from run_mame_original_post_adc_vca_lfo_tail import (
    TAIL_START, TAIL_END, TAIL_SHA, tail_seed,
    verify_tail, verify_tail_irq, debugger as tail_debugger,
)

FRAME_START, FRAME_END = 0x02EC, 0x03A1
FRAME_SHA = "3cd0206778e6fa8650fd8d803ff4d7665cd15f9963803d1ff98a9118be70c7df"
BOOT_NEW = 0x0863  # published NOP-only 0863..09DF ROM hole
POLL_NEW = 0x09E0  # published NOP-only 09E0..09E9 ROM hole
OLD_BOOT = 0x0200
OLD_POLL = 0x0400
BOOT_SUFFIX = bytes((0x48, 0x48, 0x00, 0x64, 0x06, 0x04,
                     0xAA, 0x54, 0x00, 0x04))
RESONANCE = 0x1234
HOLD_RATE = 0x0020
ATTACK_RATE = 0x0020
ATTACK_SEED = 0x1000
SCALE_PITCH = 0x30
SCALE_VCF = 0x40
CASES = tuple((i, bank) for i, bank in test_cases() if i < 18)


def seed_and_relocate(rom: bytearray, case: int,
                      pattern: str, frame: bytes) -> int:
    """Only rehome *synthetic* bytes. Published code is never patched."""
    tail_seed(rom, case)
    start = rom.find(BOOT_SUFFIX, OLD_BOOT, OLD_POLL)
    if start < 0 or rom.find(BOOT_SUFFIX, start + 1, OLD_POLL) >= 0:
        raise AssertionError("test-only synthetic startup suffix not unique")
    run_mask = sum(1 << i for i, ch in enumerate(pattern) if ch != "R")
    # 3 source-level branches at 0323:
    # no bit1 => original holdoff branch at 0370
    # bit1 but not bit2 => original delay attack at 0388
    # bit1 and bit2 => original full modulation at 032B
    phase = case % 3
    flag1 = bytes((0x71, 0x1E))
    flag_at = rom.find(flag1, OLD_BOOT, start)
    if flag_at < 0 or rom.find(flag1, flag_at + 1, start) >= 0:
        raise AssertionError("synthetic FF1E original flags seed missing")
    rom[flag_at + 2] |= (0, 2, 6)[phase]

    vals = (
        (0x10, run_mask),
        (0x3F, RESONANCE & 0xFF), (0x40, RESONANCE >> 8),
        (0x48, SCALE_VCF), (0x49, SCALE_PITCH),
        (0x56, 0), (0x57, 0),
        (0x58, HOLD_RATE & 0xFF), (0x59, HOLD_RATE >> 8),
        (0x5A, ATTACK_SEED & 0xFF),
        (0x5B, ATTACK_SEED >> 8),
        (0x6C, ATTACK_RATE & 0xFF),
        (0x6D, ATTACK_RATE >> 8),
    )
    extra = bytes(byte for a, v in vals for byte in (0x71, a, v))
    if start + len(extra) + len(BOOT_SUFFIX) >= OLD_POLL:
        raise AssertionError("frame fixture seed exceeds former boot area")
    rom[start:start+len(extra)+len(BOOT_SUFFIX)] = extra + BOOT_SUFFIX
    boot_end = start + len(extra) + len(BOOT_SUFFIX)
    synthetic = bytearray(rom[OLD_BOOT:boot_end])
    if synthetic[-3:] != bytes((0x54, 0x00, 0x04)):
        raise AssertionError("synthetic frame boot has different JMP")
    synthetic[-2:] = bytes((POLL_NEW & 0xFF, POLL_NEW >> 8))
    if BOOT_NEW + len(synthetic) >= POLL_NEW:
        raise AssertionError("relocated synthetic boot overlaps new polling")
    # Existing synthetic polling loop checks FF5C sentinel after RETI:
    # LDAW FF5C; EQI A,AA; JMP 04D5; JMP 0400.
    poll = bytearray(rom[OLD_POLL:OLD_POLL + 10])
    if poll != bytes((0x01, 0x5C, 0x77, 0xAA,
                      0x54, 0xD5, 0x04, 0x54, 0x00, 0x04)):
        raise AssertionError("old synthetic IRQ polling contract changed")
    poll[-2:] = bytes((POLL_NEW & 0xFF, POLL_NEW >> 8))
    # Reset executes synthetic ROM code via a JMP outside the public
    # instruction source window. This creates room for original frame
    # instructions at the newly covered source 02EC..03A0.
    rom[OLD_BOOT:boot_end] = bytes(boot_end - OLD_BOOT)
    rom[OLD_POLL:OLD_POLL+10] = bytes(10)
    rom[BOOT_NEW:BOOT_NEW+len(synthetic)] = synthetic
    rom[POLL_NEW:POLL_NEW+len(poll)] = poll
    rom[:3] = bytes((0x54, BOOT_NEW & 0xFF, BOOT_NEW >> 8))
    rom[FRAME_START:FRAME_END] = frame
    if rom[FRAME_START:FRAME_END] != frame:
        raise AssertionError("authentic scheduler source overwritten")
    return run_mask


def debugger() -> str:
    base = tail_debugger()
    if not base.endswith("g\n"):
        raise AssertionError("original tail debugger format changed")
    if base.count("bp 0400:modulecpu") != 1:
        raise AssertionError("old synthetic polling probe missing")
    base = base.replace("bp 0400:modulecpu",
                        "bp 09e0:modulecpu", 1)
    if base.count("; quit}") != 1:
        raise AssertionError("original 02EC tail stop contract changed")
    base = base.replace("; quit}", "; g}")
    checks = (
        'bp 0301:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_VOICES:%02X:%02X:%04X\\n",'
        'b@ff10,b@ff11,sp; g}\n'
        'bp 0308:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_RESONANCE:%02X:%04X:%04X\\n",a,ea,sp; g}\n'
        'bp 030a:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_RESONANCE_RETURN:%02X:%04X:%04X\\n",'
        'a,ea,sp; g}\n'
        'bp 0370:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_HOLDOFF:%04X\\n",sp; g}\n'
        'bp 0388:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_ATTACK:%04X\\n",sp; g}\n'
        'bp 032b:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_FULL_MOD:%04X\\n",sp; g}\n'
        'bp 03a1:modulecpu,1,{logerror '
        '"FM1JUNO_ORIG_FRAME_END:%04X:%04X:%02X:%02X:%04X:'
        '%04X:%04X:%04X\\n",'
        'sp,de,b@ff11,b@ff1e,w@ff51,w@ff53,w@ff56,w@ff5a; quit}\n'
    )
    return base[:-2] + checks + "g\n"


def check_frame(log: str, case: int, bank: int, run_mask: int) -> None:
    def single(kind: str, n: int) -> tuple[int, ...]:
        hits = re.findall(
            r"FM1JUNO_ORIG_FRAME_" + kind + r":([0-9a-fA-F:]+)", log)
        if len(hits) != 1:
            raise AssertionError(
                "case %d bank %d original scheduler %s hits %d tail=%s" %
                (case, bank, kind, len(hits), log[-2000:]))
        values = tuple(int(x, 16) for x in hits[0].split(":"))
        if len(values) != n:
            raise AssertionError("original scheduler debugger width "
                                 "%s=%r" % (kind, values))
        return values
    mode = case % 3
    gate, active, sp = single("VOICES", 3)
    if (gate, active, sp) != (run_mask, run_mask, 0xFFF0):
        raise AssertionError("original scheduler voice-run/gate mismatch "
                             "%r expected=%02x" %
                             ((gate,active,sp),run_mask))
    resonance = single("RESONANCE", 3)
    returned = single("RESONANCE_RETURN", 3)
    # Original CALF 082F preserves EA, but the helper's final
    # MOV A,EAL at 0839 returns the DAC's low 8-bit value in A.
    # A=06 is only the source-side channel selector *before* CALF.
    if resonance != (6, RESONANCE, 0xFFF0) or (
            returned != (RESONANCE & 0xFF, RESONANCE, 0xFFF0)):
        raise AssertionError("original resonance DAC original 082F "
                             "call/return mismatch %r/%r" %
                             (resonance,returned))
    # Each case takes exactly one of three original LFO-delay branches,
    # triggered by distinct synthetic FF1E bit1/bit2 inputs.
    labels = ("HOLDOFF", "ATTACK", "FULL_MOD")
    counts = tuple(len(re.findall(
        "FM1JUNO_ORIG_FRAME_" + kind + ":", log)) for kind in labels)
    if counts != tuple(1 if i==mode else 0 for i in range(3)):
        raise AssertionError(
            "case %d expected scheduler LFO phase %d not %r" %
            (case, mode, counts))
    (endsp, de, voice, flags, to_pitch, to_vcf, hold, attack) = (
        single("END", 8))
    if (endsp, de, voice) != (0xFFF0, 0xFF31, run_mask):
        raise AssertionError(
            "original scheduler exit SP/DE/voice-run wrong %r" %
            ((endsp,de,voice),))
    if (hold,attack) != (
            (HOLD_RATE, ATTACK_SEED) if mode==0 else
            (0, ATTACK_SEED + ATTACK_RATE) if mode==1 else
            (0, ATTACK_SEED)):
        raise AssertionError(
            "original scheduler holdoff/attack envelope mismatch "
            "mode %d actual %r" % (mode,(hold,attack)))
    if not (0 <= to_pitch <= 0xFFFF and 0 <= to_vcf <= 0xFFFF):
        raise AssertionError("original LFO modulation output overflow")
    print("MAME_ORIGINAL_FRAME %02d/%02d PASS bank=%d "
          "gate=%02X res=%04X branch=%s toPitch=%04X toVCF=%04X "
          "SP=%04X" %
          (case*2+bank+1, len(CASES),bank,run_mask,
           RESONANCE,labels[mode],to_pitch,to_vcf,endsp),
          flush=True)


def run(disasm: Path, attack_cli: Path, decay_cli: Path,
        global_cli: Path, voice_cli: Path, adc_cli: Path,
        mame: str, timeout: int, input_profile: str) -> None:
    original,dac,attack_table = original_parts(disasm)
    continuation = original_loop_extension(disasm)
    multiplier = original_decay_helper(disasm)
    post = source_slice(disasm,POST_START,POST_END,POST_SHA256)
    bank0 = source_slice(disasm,BANK0_START,BANK0_END,BANK0_SHA256)
    follow = source_slice(disasm,FOLLOW_START,FOLLOW_END,FOLLOW_SHA)
    frame = source_slice(disasm,FRAME_START,FRAME_END,FRAME_SHA)
    porta = source_table(disasm)
    adc = load_adc_window(disasm)
    vector = original_slice(disasm,VECTOR_BEGIN,VECTOR_END,VECTOR_SHA)
    isr = original_slice(disasm,BODY_BEGIN,BODY_END,BODY_SHA)
    tail = source_slice(disasm,TAIL_START,TAIL_END,TAIL_SHA)
    gcases=global_cases()
    g_inputs=tuple(gcases[i%len(gcases)] for i in range(len(SCENARIOS)))
    globals_=global_production(global_cli,g_inputs)
    voices=compiled_expectations(SCENARIOS,attack_cli,decay_cli,
                                 voice_cli,globals_)
    cases=CASES
    adcs=native_adc_expected(adc_cli,cases,input_profile)
    binary=mame_binary(mame)
    print("MAME original 02EC..03A0 frame scheduler from genuine "
          "ADC IRQ and original 07B5 jump: %d cases input=%s" %
          (len(cases),input_profile),flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-original-frame-") as td:
        root=Path(td)
        romdir=root/"roms"
        romdir.mkdir()
        (root/"cfg").mkdir()
        script=root/"frame.cmd"
        script.write_text(debugger(),encoding="ascii")
        assigner=bytes((0x54,0,0))+bytes(0x2000-3)
        for serial,((i,bank),adc_expect) in enumerate(zip(cases,adcs)):
            rom=bytearray(build_rom(
                original,continuation,dac,multiplier,attack_table,
                post,bank0,adc,vector,isr,
                SCENARIOS[i],g_inputs[i],globals_[i],voices[i],i,bank))
            rom[FOLLOW_START:FOLLOW_END]=follow
            rom[PORTA_START:PORTA_END]=porta
            rom[TAIL_START:TAIL_END]=tail
            run_mask=seed_and_relocate(rom,i,SCENARIOS[i]["pattern"],frame)
            with zipfile.ZipFile(romdir/"juno106.zip","w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2",assigner)
                z.writestr("v2 b_2.ic37",bytes(rom))
            cmd=[
                binary,"juno106","-rompath",str(romdir),
                "-cfg_directory",str(root/"cfg"),
                "-debug","-debugger","qt","-oslog","-log",
                "-debugscript",str(script),
                "-video","none","-sound","none",
                "-nothrottle","-skip_gameinfo",
            ]
            try:
                p=subprocess.run(
                    cmd,cwd=root,text=True,capture_output=True,
                    timeout=timeout,
                    env={**os.environ,"QT_QPA_PLATFORM":"offscreen"})
            except subprocess.TimeoutExpired as ex:
                text=ex.stdout or b""
                if isinstance(text,bytes):
                    text=text.decode("utf-8",errors="replace")
                err=root/"error.log"
                if err.exists():
                    text+=err.read_text(encoding="utf-8",errors="replace")
                    err.unlink()
                raise AssertionError(
                    "published 02EC scheduler CPU timeout "
                    "case=%d bank=%d tail=%s" %
                    (i,bank,text[-3000:])) from ex
            log=p.stdout+p.stderr
            err=root/"error.log"
            if err.exists():
                data=err.read_text(encoding="utf-8",errors="replace")
                err.unlink()
                log=data if "FM1JUNO_REAL_ISR_ENTRY:" in data else log+data
            # Full earlier bounded proven chain remains mandatory.
            fad=log.find("FM1JUNO_ORIG_FAD_AFTER_POP:")
            if fad<0:raise AssertionError("no original POP DE")
            fad=log.find("\n",fad)
            if fad<0:raise AssertionError("truncated original POP DE")
            head=log.find("FM1JUNO_ORIG_TAIL_FRAME_HEAD:")
            if head<0:raise AssertionError("no original 07B5 JMP")
            head=log.find("\n",head)
            if head<0:raise AssertionError("truncated 02EC JMP destination")
            pre_frame=log[:head+1]
            second_irq=verify_irq_interleaving(
                pre_frame[:fad+1],bank,serial,input_profile)
            verify_tail_irq(pre_frame,fad+1,bank,serial,input_profile)
            check_six(log,voices[i],globals_[i][3],SCENARIOS[i],i)
            verify_adc(log,(i,bank,OLD_PATTERN[0],
                           raw_for_bank(bank,input_profile)[0]),adc_expect)
            verify_original_fad(pre_frame,serial,bank,second_irq)
            verify_tail(pre_frame,i,SCENARIOS[i],voices[i])
            check_frame(log,i,bank,run_mask)
            if p.returncode:
                raise AssertionError("MAME 02EC original frame CPU rc=%d"
                                     "case=%d" %(p.returncode,i))
    print("IC29_ORIGINAL_FRAME_MAME: PASS (%d actual ADC IRQ CPU cases "
          "and published 02EC..03A0 frame scheduler entries, "
          "%d voice and %d ADC comparisons)" %
          (len(cases),len(cases)*6,len(cases)*4),flush=True)
    print("SCOPE: synthetic boot/poll relocated into pinned public "
          "NOP holes; no original full ROM scheduler provenance, "
          "no physical clock/UPD7811 silicon proof.",flush=True)


if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--ic29-disasm",type=Path,required=True)
    p.add_argument("--attack-cli",type=Path,required=True)
    p.add_argument("--decay-cli",type=Path,required=True)
    p.add_argument("--global-cli",type=Path,required=True)
    p.add_argument("--voice-cli",type=Path,required=True)
    p.add_argument("--adc-cli",type=Path,required=True)
    p.add_argument("--mame",default="mame")
    p.add_argument("--case-timeout",type=int,default=14)
    p.add_argument("--input-profile",choices=("stock-zero","fixed-eight"),
                   default="stock-zero")
    a=p.parse_args()
    try:
        run(a.ic29_disasm,a.attack_cli,a.decay_cli,a.global_cli,
            a.voice_cli,a.adc_cli,a.mame,a.case_timeout,a.input_profile)
    except (OSError,ValueError,AssertionError,subprocess.CalledProcessError) as ex:
        print("IC29_ORIGINAL_FRAME_MAME: FAIL: "+str(ex),file=sys.stderr)
        sys.exit(1)
