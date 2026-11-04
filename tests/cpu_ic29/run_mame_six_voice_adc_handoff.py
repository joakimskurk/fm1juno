#!/usr/bin/env python3
"""Test the original IC29 six-voice -> ADC bank-service handoff in MAME.

The public IC29 opcodes execute continuously from 04D5 through the original
six-voice loop, original 0652 PUSH DE and 0656 bank-selection instruction,
then the original 0800..0824 four-channel ADC hysteresis/conditioning helper.
A debug breakpoint stops at the *first instruction after the real CALF
returns*: 06E7 for bank 0 or 065F for bank 1.

This establishes **program-order and bank/stack causality only**, not elapsed
microseconds, an actually fired ADC interrupt, ADC hardware conversion
clock, an original UPD7811 ROM map or physical Roland voltage fidelity.
The CPU is MAME's existing JUNO-106 UPD7810 skeleton; initial state/ROM
are synthetic and each published source byte-window is SHA-256 pinned.
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

from run_adc_opcode_window import load_window as load_original_adc
from run_mame_cpu_probe import mame_binary, CASES as ADC_CASES
from run_mame_vcf_global_probe import cases as global_cases
from run_mame_vcf_global_probe import production_values as global_production
from run_mame_vcf_original_attack_probe import original_parts
from run_mame_vcf_original_decay_release_probe import original_decay_helper
from run_mame_six_voice_original_loop import (
    original_loop_extension, vectors as six_vectors,
    compiled_expectations, original_image, debugger_script as six_debugger,
    check_case as check_six,
)

# Disjoint branches of the authentic published post-six-voice code.
# At 0656, BIT 3 FF5C + JRE 06E2 selects bank 0 if bit 3 is clear;
# otherwise 065A selects bank 1. The original 0800 helper is shared.
POST_START, POST_END = 0x0652, 0x065F
BANK0_START, BANK0_END = 0x06E2, 0x06E7
POST_SHA256 = "6ae873b5cc79f06f63dbcad6da2515d0a7ee9f20b01e588c3cdea9494a989400"
BANK0_SHA256 = "381f5e495486c447526db3d58e6e8884bcabec0a48e2631290e264a809423105"
# 0800..0824 SHA validated by run_adc_opcode_window.load_window().
BANK_START, BANK_END = 0x0800, 0x0825
BANKS = (0, 1)
# Deterministic cross-product: each original six-voice phase fixture
# reaches ADC helper through BOTH original bank-selection branches.
SIX_CASES = six_vectors()
TOTAL = len(SIX_CASES) * 2


def source_slice(source: Path, start: int, end: int, sha: str) -> bytes:
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    memory: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        m = pattern.match(line)
        if m is None:
            continue
        addr = int(m.group(1), 16)
        for val in m.group(2).split():
            if start <= addr < end:
                b = int(val, 16)
                if addr in memory and memory[addr] != b:
                    raise ValueError("conflicting public opcode at %04X" % addr)
                memory[addr] = b
            addr += 1
    if any(a not in memory for a in range(start, end)):
        raise ValueError("incomplete original ADC branch %04X..%04X" %
                         (start, end-1))
    result = bytes(memory[a] for a in range(start, end))
    actual = hashlib.sha256(result).hexdigest()
    if actual != sha:
        raise ValueError("published ADC-branch SHA mismatch at %04X: %s" %
                         (start, actual))
    return result


def inputs() -> tuple[tuple[int, int, int, int], ...]:
    # case=(six_voice_fixture_index, selected_bank, old_held, incoming_raw)
    out = []
    for i in range(len(SIX_CASES)):
        for bank in BANKS:
            old, fresh = ADC_CASES[(i*2 + bank) % len(ADC_CASES)]
            out.append((i, bank, old, fresh))
    if len(out) != TOTAL or len(set((i, b) for i, b, _, _ in out)) != TOTAL:
        raise AssertionError("post-six-voice ADC coverage not stable")
    return tuple(out)


def compiled_adc(native_cli: Path,
                 items: tuple[tuple[int, int, int, int], ...]
                 ) -> list[tuple[int, ...]]:
    query = "".join("%d %d\n" % (old, raw) for _, _, old, raw in items)
    proc = subprocess.run([str(native_cli)], input=query, text=True,
                          capture_output=True, check=True, timeout=15)
    data = [tuple(map(int, l.strip().split(",")))
            for l in proc.stdout.splitlines() if l.strip()]
    if len(data) != len(items) or any(
            len(row) != 8 or any(not 0 <= n <= 255 for n in row)
            for row in data):
        raise AssertionError("compiled production ADC response invalid")
    return data


def boot_adc(image: bytes, post: bytes, bank0_code: bytes,
             adc_helper: bytes, bank: int, old: int, fresh: int
             ) -> bytes:
    rom = bytearray(image)
    # 0652 contains an artificial JMP trap in the previous test fixture;
    # replace it COMPLETELY with untouched original published bytes.
    rom[POST_START:POST_END] = post
    rom[BANK0_START:BANK0_END] = bank0_code
    rom[BANK_START:BANK_END] = adc_helper
    # Everything used for the bank input is synthetic RAM setup *before*
    # original code at 04CB. The incoming raw FF5D..FF60 is the same
    # 4-channel input for either selected bank; FF5C selects the destination.
    jump = bytes((0x54, 0xD5, 0x04))
    start = rom.find(jump, 0, 0x04CB)
    if start < 0 or rom.find(jump, start + 1, 0x04CB) != -1:
        raise AssertionError("synthetic six-voice boot jump not unique")
    raw = (fresh, 3 if fresh & 1 else 4,
           240 if fresh & 1 else 239,
           250 if fresh & 1 else 249)
    setup = [(0x5C, 0x08 if bank else 0x00)]
    setup.extend((0x5D + k, raw[k]) for k in range(4))
    for b in (0, 1):
        for k in range(4):
            held = (old, 2, 238, 255)[k] if b == bank \
                else (11, 77, 131, 239)[k]
            cond = 0xB0 + k if b == bank else 0xA0 + k
            setup.append((0x80 + 4*b + k, held))
            setup.append((0x88 + 4*b + k, cond))
    inserted = bytearray()
    for addr, value in setup:
        inserted.extend((0x71, addr, value))
    inserted.extend(jump)
    if start + len(inserted) >= 0x04CB:
        raise AssertionError("original published code overwritten by RAM setup")
    rom[start:start + len(inserted)] = inserted
    return bytes(rom)


def debugger_script() -> str:
    base = six_debugger()
    if not base.endswith("g\n") or base.count("; quit}") != 1:
        raise AssertionError("six-voice debugger contract changed")
    # Prior six-voice final breakpoint at 0652 now RECORDS and continues;
    # original CPU executes PUSH DE, original BIT/JRE and original CALF.
    preamble = base[:-2].replace("; quit}", "; g}")
    words = [("b@ff5c", "%02X"), ("sp", "%04X"), ("hl", "%04X"),
             ("de", "%04X"), ("b@ffee", "%02X"), ("b@ffef", "%02X"),
             ("b@ff0f", "%02X")]
    words.extend(("b@%04x" % addr, "%02X") for addr in range(0xFF80, 0xFF90))
    fields = ":".join(fmt for _, fmt in words)
    regs = ",".join(name for name, _ in words)
    return preamble + (
        'bp 065d:modulecpu,1,{logerror "FM1JUNO_ADC_CALF:B1\\n"; g}\n'
        'bp 06e5:modulecpu,1,{logerror "FM1JUNO_ADC_CALF:B0\\n"; g}\n'
        'bp 065f:modulecpu,1,{logerror "FM1JUNO_ADC_EXIT:B1:' +
        fields + '\\n",' + regs + '; quit}\n'
        'bp 06e7:modulecpu,1,{logerror "FM1JUNO_ADC_EXIT:B0:' +
        fields + '\\n",' + regs + '; quit}\n'
        "g\n"
    )


def verify_adc(output: str, case: tuple[int, int, int, int],
               expected: tuple[int, ...]) -> None:
    i, bank, old, fresh = case
    calls = re.findall(r"FM1JUNO_ADC_CALF:B([01])", output)
    returns = re.findall(r"FM1JUNO_ADC_EXIT:B([01]):([0-9a-fA-F:]+)",
                         output)
    if calls != [str(bank)] or len(returns) != 1 or returns[0][0] != str(bank):
        raise AssertionError(
            "case=%d bank=%d old=%d raw=%d original ADC control "
            "flow violated: CALFs=%s returns=%s; tail=%s" %
            (i, bank, old, fresh, calls, returns, output[-2000:]))
    values = [int(v, 16) for v in returns[0][1].split(":")]
    if len(values) != 23:
        raise AssertionError("post-ADC debugger record size mismatch: %r" %
                             values)
    mode, sp, hl, de, stack_lo, stack_hi, voice_idx = values[:7]
    ram = values[7:]
    if (mode != (8 if bank else 0) or sp != 0xFFEE or
            hl != 0xFF61 or de != (0xFF84 + 4*bank) or
            stack_lo != 0x31 or stack_hi != 0xFF or voice_idx != 6):
        raise AssertionError(
            "case %d bank=%d post-CALF register/stack mismatch "
            "FF5C=%02X SP=%04X HL=%04X DE=%04X "
            "stack=%02X%02X voice=%02X" %
            (i, bank, mode, sp, hl, de, stack_hi, stack_lo, voice_idx))
    for b in (0, 1):
        for k in range(4):
            held = ram[4*b+k]
            condition = ram[8+4*b+k]
            if b == bank:
                correct_held = expected[2*k]
                correct_cond = expected[2*k+1]
            else:
                correct_held = (11, 77, 131, 239)[k]
                correct_cond = 0xA0 + k
            if held != correct_held or condition != correct_cond:
                raise AssertionError(
                    "case=%d bank=%d channel=%d MAME held=%02X "
                    "conditioned=%02X; production=%02X,%02X" %
                    (i, b, k, held, condition,
                     correct_held, correct_cond))
    print("MAME_SIX_ADC %02d/%02d PASS sixvoices=%d bank=%d "
          "old=%3d fresh=%3d RAM=%s" %
          (i*2 + bank + 1, TOTAL, i, bank, old, fresh,
           ",".join("%02X" % n for n in expected)), flush=True)


def run(disasm: Path, attack_cli: Path, dr_cli: Path,
        global_cli: Path, voice_cli: Path, adc_cli: Path,
        mame: str, timeout: int) -> None:
    prefix, dac, table = original_parts(disasm)
    helper = original_decay_helper(disasm)
    loop = original_loop_extension(disasm)
    post = source_slice(disasm, POST_START, POST_END, POST_SHA256)
    bank0 = source_slice(disasm, BANK0_START, BANK0_END, BANK0_SHA256)
    original_adc = load_original_adc(disasm)
    source = global_cases()
    globals_input = tuple(source[i % len(source)]
                          for i in range(len(SIX_CASES)))
    globals_ = global_production(global_cli, globals_input)
    six_expected = compiled_expectations(
        SIX_CASES, attack_cli, dr_cli, voice_cli, globals_)
    test_inputs = inputs()
    adc_expected = compiled_adc(adc_cli, test_inputs)
    exe = mame_binary(mame)
    print("Original 6-voice -> selected ADC bank handoff: %d cases, "
          "216 per-voice comparisons, 144 physical ADC-channel checks" %
          TOTAL, flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-six-adc-") as tmp:
        root = Path(tmp)
        (root / "roms").mkdir()
        (root / "cfg").mkdir()
        script = root / "six-adc.cmd"
        script.write_text(debugger_script(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for j, ((i, bank, old, fresh), actual_adc) in enumerate(
                zip(test_inputs, adc_expected)):
            image = original_image(
                prefix, loop, dac, helper, table, SIX_CASES[i],
                globals_input[i], globals_[i], six_expected[i], i)
            image = boot_adc(image, post, bank0, original_adc,
                             bank, old, fresh)
            with zipfile.ZipFile(root / "roms" / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as archive:
                archive.writestr("v5 a_5.ic2", assigner)
                archive.writestr("v2 b_2.ic37", image)
            args = [
                exe, "juno106", "-rompath", str(root / "roms"),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(script),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                proc = subprocess.run(
                    args, cwd=root, text=True, capture_output=True,
                    timeout=timeout,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as e:
                raise AssertionError(
                    "MAME 6->ADC timeout case %d bank %d" %
                    (j, bank)) from e
            text = proc.stdout + proc.stderr
            log = root / "error.log"
            if log.exists():
                body = log.read_text(encoding="utf-8", errors="replace")
                log.unlink()
                # Do not concatenate duplicate debugger logerror() records
                # emitted to both MAME stdout and error.log.
                text = body if "FM1JUNO_ADC_CALF:" in body else text + body
            # Preserve every original voice comparison, DAC return and
            # original stack+pointer check at 0652 from the previous gate.
            check_six(text, six_expected[i], globals_[i][3], SIX_CASES[i], i)
            verify_adc(text, (i, bank, old, fresh), actual_adc)
            if proc.returncode != 0:
                raise AssertionError(
                    "MAME original 6->ADC nonzero exit code=%d case=%d" %
                    (proc.returncode, j))
    print("IC29_SIX_TO_ADC_MAME_CORE: PASS (%d complete original "
          "sixvoice+ADC-bank paths, %d per-voice and %d ADC-channel "
          "comparisons)" % (TOTAL, TOTAL*6, TOTAL*4), flush=True)
    print("Scope: MAME UPD7810 synthetic bootstrap/ADC snapshots; "
          "NO actual ADC interrupt firing, no cycle calibration, "
          "no original UPD7811 whole ROM.", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--ic29-disasm", type=Path, required=True)
    p.add_argument("--attack-cli", type=Path, required=True)
    p.add_argument("--decay-cli", type=Path, required=True)
    p.add_argument("--global-cli", type=Path, required=True)
    p.add_argument("--voice-cli", type=Path, required=True)
    p.add_argument("--adc-cli", type=Path, required=True)
    p.add_argument("--mame", default="mame")
    p.add_argument("--case-timeout", type=int, default=14)
    a = p.parse_args()
    try:
        run(a.ic29_disasm, a.attack_cli, a.decay_cli, a.global_cli,
            a.voice_cli, a.adc_cli, a.mame, a.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as err:
        print("IC29_SIX_TO_ADC_MAME_CORE: FAIL: " + str(err),
              file=sys.stderr)
        sys.exit(1)
