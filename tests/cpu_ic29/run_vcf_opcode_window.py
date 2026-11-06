#!/usr/bin/env python3
"""Execute ORIGINAL IC29 VCF saturation bytes in a bounded uPD7811 decoder.

This follows ROM $0609-$0618, independently extracted from the published
disassembly and hash-pinned. It does not emulate full VCF math, the full
uPD7811 CPU, original interrupt interleaving or cycle clocks.

The execution is compared with a separately compiled *production C*
vcf106_voice_code14 executable at reachable signed-input boundaries.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

START, END, EXIT_PC = 0x0609, 0x0619, 0x063C
EXPECTED_SHA256 = "998e3790863ac4f1aca79f78c7bec54de371fdd14ab11aafa24c3ae480fd86a8"
WIDTHS = {0x08: 1, 0x47: 2, 0x4E: 2, 0x44: 3, 0x45: 3}
SITES = {0x0609, 0x060A, 0x060C, 0x060E, 0x0611, 0x0614, 0x0617}


def load_bytes(source: Path) -> bytes:
    match_line = re.compile(
        r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    memory: dict[int, int] = {}
    for line in source.read_text(encoding="utf-8").splitlines():
        match = match_line.match(line)
        if match:
            base = int(match.group(1), 16)
            for delta, octet in enumerate(bytes.fromhex(match.group(2))):
                address = base + delta
                if START <= address < END:
                    if address in memory and memory[address] != octet:
                        raise ValueError("conflicting source byte at %04x" % address)
                    memory[address] = octet
    if len(memory) != END-START or any(a not in memory for a in range(START, END)):
        raise ValueError("missing original VCF opcode bytes")
    data = bytes(memory[a] for a in range(START, END))
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError("VCF opcode hash differs: " + digest)
    return data


def execute(data: bytes, ea: int, overflow_flag: int
            ) -> tuple[int, set[int], set[int]]:
    a = 0
    pc = START
    skip = False
    steps = 0
    visited: set[int] = set()
    skipped: set[int] = set()
    while pc != EXIT_PC:
        if not START <= pc < END:
            raise AssertionError("VCF CPU jumped beyond instrumented window")
        if steps > 9:
            raise AssertionError("nonterminating VCF opcode window")
        op = data[pc - START]
        if op not in WIDTHS:
            raise AssertionError("unhandled original opcode at %04x" % pc)
        origin = pc
        length = WIDTHS[op]
        operand = data[pc - START + 1: pc - START + length]
        pc += length
        steps += 1
        if skip:
            skipped.add(origin)
            skip = False
            continue
        visited.add(origin)
        if op == 0x08:  # MOV A,EAH
            a = (ea >> 8) & 255
        elif op == 0x47:  # ONI A,imm
            skip = (a & operand[0]) != 0
        elif op == 0x4E:  # JRE positive 8-bit displacement
            pc += operand[0]
        elif op == 0x44:  # LXI EA,imm16 (little endian)
            ea = operand[0] | (operand[1] << 8)
        elif op == 0x45:  # ONIW FFxx, imm
            wa, mask = operand
            if wa != 0x6B or mask != 0x01:
                raise AssertionError("unexpected ROM flag memory operand")
            skip = (overflow_flag & mask) != 0
        else:
            raise AssertionError("internal decoder bug")
    return ea, visited, skipped


def verify(source: Path, native_cli: Path) -> None:
    data = load_bytes(source)
    cases = []
    commands = []
    # Every 16-bit register value and all combinations of flag bit 0
    # and unrelated bit 1; the flag is allowed to remain set with an
    # in-range EA, but that particular state is *not* modeled as a valid
    # signed global control input when comparing production C.
    for ea in range(65536):
        for flag in range(4):
            signed_input = None
            if (flag & 1) == 0:
                signed_input = ea
            elif ea >= 0x4000:
                signed_input = ea - 65536
            cases.append((ea, flag, signed_input))
            if signed_input is not None:
                commands.append(str(signed_input))
    output = subprocess.run(
        [str(native_cli)], input="\n".join(commands)+"\n",
        text=True, capture_output=True, check=True)
    production = [int(line) for line in output.stdout.splitlines()]
    if len(production) != len(commands):
        raise AssertionError("VCF production C result count mismatch")

    cursor = 0
    executed: set[int] = set()
    skipped: set[int] = set()
    compare_count = 0
    for ea, flag, signed_input in cases:
        actual, sites, skipped_sites = execute(data, ea, flag)
        expected = ea if ea <= 0x3FFF else (0 if flag & 1 else 0x3FFF)
        if actual != expected:
            raise AssertionError("VCF opcode mismatch ea=%04x flag=%d cpu=%04x expected=%04x" %
                                 (ea, flag, actual, expected))
        executed.update(sites)
        skipped.update(skipped_sites)
        if signed_input is not None:
            observed = production[cursor]
            cursor += 1
            compare_count += 1
            if observed != actual:
                raise AssertionError("VCF CPU vs production C mismatch "
                                     "ea=%04x flag=%d CPU=%04x C=%04x" %
                                     (ea, flag, actual, observed))
    if not SITES.issubset(executed | skipped):
        raise AssertionError("unvisited VCF opcode sites")
    if len(executed) < 6:
        raise AssertionError("VCF branches not covered")
    print("IC29_VCF_OPCODE_EXECUTION: PASS")
    print("window: 0x0609..0x0618 sha256=" + EXPECTED_SHA256)
    print("executed original opcode cases: %d" % len(cases))
    print("matched compiled production VCF code: %d" % compare_count)
    print("instruction sites executed or skipped: %d" %
          len(executed | skipped))
    print("scope: VCF final overflow/underflow saturation only; no full 7811 CPU")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", type=Path, required=True)
    parser.add_argument("--native-cli", type=Path, required=True)
    args = parser.parse_args()
    try:
        verify(args.ic29_disasm, args.native_cli)
    except (ValueError, OSError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_VCF_OPCODE_EXECUTION: FAIL: "+str(exc), file=sys.stderr)
        sys.exit(1)
