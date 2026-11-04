#!/usr/bin/env python3
"""MAME ADC peripheral-driven interrupt execution of original IC29 ISR.

Unlike run_mame_adc_isr_direct_probe.py, synthetic startup does NOT
jump to the interrupt vector. It configures the stock MAME UPD7810
ANM scan mode, MKH mask and IFF, then idles in a tiny main loop until
the CPU's own ADC converter raises INTFAD and the CPU accepts vector
0x0020. Original published code 0020 and 0070..008D executes through
the ORIGINAL RETI; we check return to the original idle main-loop PC.

MAME JUNO-106 driver has no AN0..AN7 callbacks; upstream upd7810
defaults disconnected analog callbacks to zero. Thus all CR0..CR3
values are synthetic default-zero, NOT verified Roland analog input,
electrical timing, µPD7811 ROM mapping or calibrated ADC conversion.
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
from run_mame_adc_isr_direct_probe import (
    original_slice, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA,
    BODY_BEGIN, BODY_END, BODY_SHA,
)

# Two scanning ranges, both 192- and 144-CPU-cycle conversion modes,
# as configured in MAME upd7810.cpp's ADC timer. This tests CPU
# peripheral interrupt acceptance/return, NOT wall-clock timing.
MODES = (0x00, 0x08, 0x10, 0x18)
MAIN_IDLE = 0x0010


def image(vector: bytes, body: bytes, mode: int) -> bytes:
    # MVI V FF ; LXI SP FFF0 ; MVIW FF5C AA sentinel
    # MVI MKH 04 (unmask FAD on bit0, mask serial transmit)
    # MVI A,mode ; MOV ANM,A ; EI ;
    # MAIN_IDLE: JMP MAIN_IDLE (never jumps to ISR)
    bootstrap = bytes((
        0x68, 0xFF,
        0x04, 0xF0, 0xFF,
        0x71, 0x5C, 0xAA,
        0x64, 0x06, 0x04,
        0x69, mode,
        0x4D, 0xC8,
        0xAA,
        0x54, MAIN_IDLE & 0xFF, MAIN_IDLE >> 8,
    ))
    if len(bootstrap) >= VECTOR_BEGIN or bootstrap[MAIN_IDLE] != 0x54:
        raise AssertionError("synthetic idle overlaps original ADC vector")
    rom = bytearray(0x2000)
    rom[:len(bootstrap)] = bootstrap
    rom[VECTOR_BEGIN:VECTOR_END] = vector
    rom[BODY_BEGIN:BODY_END] = body
    return bytes(rom)


def debug() -> str:
    return (
        "focus :modulecpu\n"
        'bp 0020:modulecpu,1,{logerror '
        '"FM1JUNO_ISR_REAL_ENTRY:%04X:%02X:%02X\\n",sp,anm,mkh; g}\n'
        'bp 008d:modulecpu,1,{logerror '
        '"FM1JUNO_ISR_REAL_PRE_RETI:%04X:%02X:%02X:%02X:'
        '%02X:%02X:%02X:%02X:%02X\\n",sp,anm,mkh,'
        'b@ff5c,b@ff5d,b@ff5e,b@ff5f,b@ff60,a; g}\n'
        # Condition prevents counting ordinary pre-interrupt idle-loop
        # iterations as RETI return. The only writer to FF5C is the
        # executed original ISR STAX at 007A.
        'bp 0010:modulecpu,b@ff5c == 0x00 || b@ff5c == 0x08 || '
        'b@ff5c == 0x10 || b@ff5c == 0x18,{logerror '
        '"FM1JUNO_ISR_REAL_RETURN:%04X:%02X:%02X:%02X\\n",'
        'sp,anm,mkh,b@ff5c; quit}\n'
        "g\n"
    )


def run(source: Path, mame: str, timeout: int) -> None:
    vector = original_slice(
        source, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA)
    body = original_slice(
        source, BODY_BEGIN, BODY_END, BODY_SHA)
    binary = mame_binary(mame)
    print("MAME UPD7810 actual ADC peripheral-generated interrupt "
          "gate: %d scan-mode fixtures" % len(MODES), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-real-adc-irq-"
                                     ) as folder:
        root = Path(folder)
        roms = root / "roms"
        roms.mkdir()
        (root / "cfg").mkdir()
        script = root / "interrupt.cmd"
        script.write_text(debug(), encoding="ascii")
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        for index, mode in enumerate(MODES):
            with zipfile.ZipFile(roms / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", image(vector, body, mode))
            argv = [
                binary, "juno106", "-rompath", str(roms),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(script),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                p = subprocess.run(
                    argv, cwd=root, capture_output=True, text=True,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"},
                    timeout=timeout)
            except subprocess.TimeoutExpired as e:
                raise AssertionError(
                    "MAME ADC internal converter did not cause complete "
                    "IRQ/RETI round trip, mode=%02X" % mode) from e
            text = p.stdout + p.stderr
            errorlog = root / "error.log"
            if errorlog.exists():
                log = errorlog.read_text(
                    encoding="utf-8", errors="replace")
                errorlog.unlink()
                text = log if "FM1JUNO_ISR_REAL_ENTRY:" in log else (
                    text + "\n" + log)
            e = re.findall(
                r"FM1JUNO_ISR_REAL_ENTRY:([0-9a-fA-F:]+)", text)
            pre = re.findall(
                r"FM1JUNO_ISR_REAL_PRE_RETI:([0-9a-fA-F:]+)", text)
            ret = re.findall(
                r"FM1JUNO_ISR_REAL_RETURN:([0-9a-fA-F:]+)", text)
            if (len(e) != 1 or len(pre) != 1 or len(ret) != 1):
                raise AssertionError(
                    "real peripheral ADC IRQ trace missing/duplicated "
                    "mode=%02X entry=%s pre=%s returned=%s rc=%d "
                    "tail=%s" %
                    (mode, e, pre, ret, p.returncode, text[-1700:]))
            actual_e = tuple(int(v, 16) for v in e[0].split(":"))
            actual_pre = tuple(int(v, 16) for v in pre[0].split(":"))
            actual_return = tuple(int(v, 16) for v in ret[0].split(":"))
            if actual_e != (0xFFED, mode, 4):
                raise AssertionError(
                    "original IRQ vector did not use CPU IRQ stack "
                    "mode=%02X entry=%r" % (mode, actual_e))
            if actual_pre != (
                0xFFED, mode ^ 8, 5, mode,
                0, 0, 0, 0, mode,
            ):
                raise AssertionError(
                    "original ADC ISR register/CR behavior unexpected "
                    "mode=%02X actual=%r" % (mode, actual_pre))
            if actual_return != (0xFFF0, mode ^ 8, 5, mode):
                raise AssertionError(
                    "real RETI did not resume interrupted idle PC "
                    "mode=%02X return=%r" % (mode, actual_return))
            if p.returncode != 0:
                raise AssertionError("MAME exited nonzero: %d" %
                                     p.returncode)
            print("MAME_ADC_ISR_PERIPHERAL %d/%d PASS "
                  "initial_ANM=%02X captured_bank=%d ANM_next=%02X "
                  "SP_IRQ=FFED SP_RETI=FFF0 CR=00000000" %
                  (index+1, len(MODES), mode,
                   (mode >> 3) & 1, mode ^ 8), flush=True)
    print("IC29_ADC_ISR_PERIPHERAL_MAME: PASS (%d original-byte "
          "internal-ADC-triggered CPU interrupt/RETI paths)" %
          len(MODES), flush=True)
    print("Scope: MAME UPD7810 ADC IRQ core with unconnected AN0-AN7 "
          "default-zero callbacks. No Roland voltage calibration, "
          "CPU UPD7811 mapping, sampled analog data or 8253 timing.",
          flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--ic29-disasm", type=Path, required=True)
    p.add_argument("--mame", default="mame")
    p.add_argument("--case-timeout", type=int, default=14)
    a = p.parse_args()
    try:
        run(a.ic29_disasm, a.mame, a.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as e:
        print("IC29_ADC_ISR_PERIPHERAL_MAME: FAIL: " + str(e),
              file=sys.stderr)
        sys.exit(1)
