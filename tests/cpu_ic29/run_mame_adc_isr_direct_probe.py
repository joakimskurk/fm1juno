#!/usr/bin/env python3
"""Execute original published IC29 ADC ISR opcodes on MAME UPD7810.

This is intentionally a DIRECT-ENTRY CPU instruction gate: a synthetic
bootstrap JMPs to the published 0020 vector, rather than a real ADC IRQ.
MAME debugger assigns independent CPU ANM and CR0..CR3 registers
before execution. The breakpoint at 008D is BEFORE the original RETI,
so neither hardware interrupt acceptance nor RETI timing is claimed.
Original source windows are SHA-256-pinned.
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

VECTOR_BEGIN, VECTOR_END = 0x0020, 0x0022
BODY_BEGIN, BODY_END = 0x0070, 0x008E
VECTOR_SHA = "15c841dff8407a197a8be456d43283d973c0ba690724b24da5e5d0d6dbc70423"
BODY_SHA = "d4578a044eed77c142a2bb1c9f252643e4a95e15a22833a7879b7ea63551ae61"
ENTRY = 0x0020
CHECKPOINT = 0x008D
# 0/8 scan mode bank bit; 0x10/0x18 are fast scan variants.
MODES = (0x00, 0x08, 0x10, 0x18)
# Conversion registers are supplied by independent CPU debugger fixture,
# NOT a physical ADC port or a converted sample from Roland hardware.
CHANNELS = (
    (0x00, 0x01, 0x02, 0x03),
    (0x04, 0x7F, 0x80, 0xFF),
    (0xFF, 0x80, 0x01, 0x00),
    (0x11, 0x22, 0x44, 0x88),
    (0xFE, 0xFD, 0xFC, 0xFB),
)


def original_slice(source: Path, begin: int, end: int, expected: str) -> bytes:
    regex = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    data: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        m = regex.match(line)
        if not m:
            continue
        adr = int(m.group(1), 16)
        for val in m.group(2).split():
            if begin <= adr < end:
                byte = int(val, 16)
                if adr in data and data[adr] != byte:
                    raise ValueError("conflicting original opcode %04X" % adr)
                data[adr] = byte
            adr += 1
    if any(a not in data for a in range(begin, end)):
        raise ValueError("missing original ADC ISR source bytes")
    values = bytes(data[a] for a in range(begin, end))
    if hashlib.sha256(values).hexdigest() != expected:
        raise ValueError("published ADC ISR source SHA mismatch %04X" % begin)
    return values


def boot_image(vector: bytes, body: bytes) -> bytes:
    # Primary register state must survive original EXA, EXX pair.
    # MVI V,FF ; LXI SP,FFF0 ; LXI HL,A123 ; LXI DE,B234 ;
    # LXI BC,C345 ; MVI A,5A ; JMP 0020.
    startup = bytes((
        0x68, 0xFF, 0x04, 0xF0, 0xFF,
        0x34, 0x23, 0xA1,
        0x24, 0x34, 0xB2,
        0x14, 0x45, 0xC3,
        0x69, 0x5A,
        0x54, 0x20, 0x00,
    ))
    if len(startup) >= VECTOR_BEGIN:
        raise AssertionError("synthetic boot overlaps original vector")
    rom = bytearray(0x2000)
    rom[:len(startup)] = startup
    rom[VECTOR_BEGIN:VECTOR_END] = vector
    rom[BODY_BEGIN:BODY_END] = body
    # If the original RETI somehow executes instead of our gate,
    # the debugger fails because it never hits the explicit 008D stop.
    return bytes(rom)


def debugger_script(mode: int, values: tuple[int, ...]) -> str:
    # MAME's UPD7810 state_add exposes ANM, CR0..CR3 as CPU registers.
    # These are independent controlled inputs, not handwritten register
    # emulation. The original opcode routine reads/toggles them.
    setup = ["focus :modulecpu", "do anm=%d" % mode]
    setup.extend("do cr%d=%d" % (i, n) for i, n in enumerate(values))
    fields = [
        ("anm", "%02X"), ("mkh", "%02X"),
        ("b@ff5c", "%02X"), ("b@ff5d", "%02X"),
        ("b@ff5e", "%02X"), ("b@ff5f", "%02X"),
        ("b@ff60", "%02X"),
        ("a", "%02X"), ("bc", "%04X"), ("de", "%04X"),
        ("hl", "%04X"), ("sp", "%04X"),
    ]
    fmt = ":".join(f for _, f in fields)
    terms = ",".join(reg for reg, _ in fields)
    setup.append(
        'bp 008d:modulecpu,1,{logerror "FM1JUNO_ADC_ISR_DIRECT:' +
        fmt + '\\n",' + terms + '; quit}')
    setup.append("g")
    return "\n".join(setup) + "\n"


def run(source: Path, mame: str, timeout: int) -> None:
    vector = original_slice(
        source, VECTOR_BEGIN, VECTOR_END, VECTOR_SHA)
    body = original_slice(source, BODY_BEGIN, BODY_END, BODY_SHA)
    if vector != bytes((0x4E, 0x4E)) or body[-2:] != bytes((0xAA, 0x62)):
        raise AssertionError("original ISR entry or EI/RETI changed")
    cpu = mame_binary(mame)
    cases = [(mode, values) for mode in MODES for values in CHANNELS]
    print("Original IC29 ADC direct-entry ISR CPU opcode gate: "
          "%d cases (hardware interrupt/RETI NOT included)" %
          len(cases), flush=True)
    with tempfile.TemporaryDirectory(prefix="fm1juno-mame-adc-isr-"
                                     ) as folder:
        root = Path(folder)
        roms = root / "roms"
        roms.mkdir()
        (root / "cfg").mkdir()
        assigner = bytes((0x54, 0, 0)) + bytes(0x2000-3)
        image = boot_image(vector, body)
        for case, (mode, values) in enumerate(cases):
            with zipfile.ZipFile(roms / "juno106.zip", "w",
                                 compression=zipfile.ZIP_STORED) as z:
                z.writestr("v5 a_5.ic2", assigner)
                z.writestr("v2 b_2.ic37", image)
            debugger = root / "isr.cmd"
            debugger.write_text(debugger_script(mode, values),
                                encoding="ascii")
            argv = [
                cpu, "juno106", "-rompath", str(roms),
                "-cfg_directory", str(root / "cfg"),
                "-debug", "-debugger", "qt", "-oslog", "-log",
                "-debugscript", str(debugger),
                "-video", "none", "-sound", "none",
                "-nothrottle", "-skip_gameinfo",
            ]
            try:
                result = subprocess.run(
                    argv, text=True, capture_output=True, cwd=root,
                    timeout=timeout,
                    env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
            except subprocess.TimeoutExpired as e:
                raise AssertionError(
                    "direct ADC ISR CPU timed out case=%d mode=%02X" %
                    (case, mode)) from e
            output = result.stdout + result.stderr
            log = root / "error.log"
            if log.exists():
                data = log.read_text(encoding="utf-8", errors="replace")
                log.unlink()
                output = data if "FM1JUNO_ADC_ISR_DIRECT:" in data else (
                    output + "\n" + data)
            found = re.findall(
                r"FM1JUNO_ADC_ISR_DIRECT:([0-9a-fA-F:]+)", output)
            if len(found) != 1:
                raise AssertionError(
                    "ISR breakpoint count=%d case=%d mode=%02X "
                    "exit=%d; log=%s" %
                    (len(found), case, mode, result.returncode,
                     output[-2300:]))
            observed = tuple(int(x, 16) for x in found[0].split(":"))
            expected = (
                mode ^ 0x08, 0x05, mode, *values,
                0x5A, 0xC345, 0xB234, 0xA123, 0xFFF0,
            )
            if observed != expected:
                raise AssertionError(
                    "original ISR disagrees with register snapshot "
                    "case=%d mode=%02X CR=%s: CPU=%s expected=%s" %
                    (case, mode, values, observed, expected))
            if result.returncode != 0:
                raise AssertionError(
                    "MAME exit nonzero after ISR checkpoint=%d" %
                    result.returncode)
            print("MAME_ADC_ISR_DIRECT %02d/%02d PASS "
                  "ANM=%02X->%02X CR=%s" %
                  (case+1, len(cases), mode, mode ^ 0x08,
                   ",".join("%02X" % v for v in values)), flush=True)
    print("IC29_ADC_ISR_DIRECT_MAME: PASS (%d bounded original-byte "
          "direct-vector CPU scenarios)" % len(cases), flush=True)
    print("STRICT LIMIT: synthetic JMP into vector and debugger-injected "
          "ANM/CR values; stops before RETI. No actual IRQ, hardware "
          "ADC conversion, UPD7811 silicon or cycle timing.", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--ic29-disasm", type=Path, required=True)
    p.add_argument("--mame", default="mame")
    p.add_argument("--case-timeout", type=int, default=14)
    args = p.parse_args()
    try:
        run(args.ic29_disasm, args.mame, args.case_timeout)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_ADC_ISR_DIRECT_MAME: FAIL: " + str(exc),
              file=sys.stderr)
        sys.exit(1)
