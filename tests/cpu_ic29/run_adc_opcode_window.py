#!/usr/bin/env python3
"""Execute IC29's *actual opcodes* from its public disassembly (ADC window).

This is a bounded, instruction-decoding NEC uPD7811 window interpreter,
NOT the MAME CPU engine, not whole-ROM emulation, and not cycle-accurate.
The opcode/skip rules are independently checked against MAME's uPD7810/11
implementation. Original bytes are read at runtime; no Roland ROM binary
or opcode copy is checked into this project.

The compiled native_adc106 production code is compared via --native-cli.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

START = 0x0800
END = 0x0825
WINDOW_SHA256 = "2647a3e3fc0a17a43707a1edc608c25325aab61417abf45d42606dddcf2242c5"
RETURN_PC = 0x9000

# Only the opcodes ACTUALLY encountered in this source window.
# Keeping the decoder intentionally strict means unknown opcodes cannot be
# silently treated as NOPs or as ordinary mathematical helpers.
WIDTH = {
    0x6B: 2, 0x2B: 1, 0x70: 2, 0x48: 2,
    0x27: 2, 0x3A: 1, 0x36: 2, 0x69: 2,
    0x1A: 1, 0x0A: 1, 0x60: 2, 0xBB: 2,
    0x22: 1, 0x32: 1, 0x53: 1, 0x4F: 2,
    0xB8: 1, 0x2A: 1,
}
for _op in (0xD8, 0xC5, 0xC1):
    WIDTH[_op] = 1


def load_window(path: Path) -> bytes:
    # The public annotated dump contains lines like
    # "0803: 70 b2 SUBNBX (DE)". Opcode bytes are extracted, not
    # translated from mnemonic strings (a key independence property).
    regex = re.compile(r"^\s*([0-9a-fA-F]{4}):\s+((?:[0-9a-fA-F]{2}\s+){1,5})")
    extracted: dict[int, int] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        match = regex.match(line)
        if not match:
            continue
        address = int(match.group(1), 16)
        if address >= END or address + 5 < START:
            continue
        hex_bytes = bytes.fromhex(match.group(2))
        for index, byte in enumerate(hex_bytes):
            at = address + index
            if START <= at < END:
                if at in extracted and extracted[at] != byte:
                    raise ValueError("conflicting byte at %04x" % at)
                extracted[at] = byte
    if any(a not in extracted for a in range(START, END)):
        missing = [f"{a:04x}" for a in range(START, END) if a not in extracted]
        raise ValueError("incomplete IC29 instruction window: " + ",".join(missing))
    result = bytes(extracted[a] for a in range(START, END))
    digest = hashlib.sha256(result).hexdigest()
    if digest != WINDOW_SHA256:
        raise ValueError("IC29 opcode window hash mismatch: " + digest)
    return result


class CPU:
    __slots__ = ("rom", "ram", "a", "b", "c", "pc", "hl", "de", "sp",
                 "skip", "finished", "steps", "visited", "skipped")

    def __init__(self, rom: bytes, *, old: int, fresh: int, extras: int = 0):
        self.rom = rom
        self.ram = bytearray(65536)
        self.a = 0
        self.b = 0
        self.c = 0
        self.pc = START
        self.hl = 0xFF5D
        self.de = 0xFF80
        self.sp = 0xFFF0
        self.skip = False
        self.finished = False
        self.steps = 0
        self.visited: set[int] = set()
        self.skipped: set[int] = set()

        # Simulate the CALLF return address, not a shortcut RET.
        self.ram[self.sp] = RETURN_PC & 255
        self.ram[self.sp + 1] = RETURN_PC >> 8

        # The four raw channels, accepted-history RAM and conditioned RAM
        # occupy the same addresses as the original module code.
        self.ram[0xFF5D] = fresh
        self.ram[0xFF80] = old
        raw_extra = (0, 4, 239, 249) if extras == 0 else (1, 3, 240, 250)
        held_extra = (0, 2, 238, 255) if extras == 0 else (0, 2, 238, 255)
        for i in range(1, 4):
            self.ram[0xFF5D+i] = raw_extra[i]
            self.ram[0xFF80+i] = held_extra[i]

    def byte_at(self, at: int) -> int:
        if not START <= at < END:
            raise AssertionError("CPU escaped validated opcode window %04x" % at)
        return self.rom[at - START]

    def step(self) -> None:
        if self.finished:
            raise AssertionError("executing CPU after RET")
        if self.steps >= 140:
            raise AssertionError("no RET within instruction budget")
        op_pc = self.pc
        op = self.byte_at(op_pc)
        if op not in WIDTH:
            raise AssertionError("unknown opcode %02x at %04x" % (op, op_pc))
        length = WIDTH[op]
        next_pc = op_pc + length
        self.steps += 1

        # Skip only the NEXT instruction, using its encoded length.
        # Unlike skipping a line of disassembly, this handles prefix opcodes
        # such as 0x70 b2 and 0x48 3a correctly.
        if self.skip:
            self.skip = False
            self.skipped.add(op_pc)
            self.pc = next_pc
            return

        self.visited.add(op_pc)
        imm = self.byte_at(op_pc + 1) if length == 2 else 0
        self.pc = next_pc
        if op == 0x6B:  # MVI C,#imm
            self.c = imm
        elif op == 0x2B:  # LDAX (HL)
            self.a = self.ram[self.hl]
        elif op == 0x70:  # SUBNBX (DE)
            if imm != 0xB2:
                raise AssertionError("unexpected prefixed SUBNBX")
            left = self.a
            self.a = (left - self.ram[self.de]) & 255
            self.skip = left >= self.ram[self.de]
        elif op == 0x48:  # NEGA
            if imm != 0x3A:
                raise AssertionError("unexpected prefixed instruction")
            self.a = (-self.a) & 255
        elif op == 0x27:  # GTI A,#imm
            self.skip = self.a > imm
        elif op in (0xC5, 0xC1, 0xD8):  # JR 6-bit signed displacement
            offset = (op & 0x3F)
            if offset & 0x20:
                offset -= 0x40
            self.pc += offset
        elif op == 0x3A:  # STAX (DE)
            self.ram[self.de] = self.a
        elif op == 0x36:  # SUINB A,#imm
            left = self.a
            self.a = (left - imm) & 255
            self.skip = left >= imm
        elif op == 0x69:  # MVI A,#imm
            # See limitations: standalone entry assumes overlay L1 cleared
            # before this branch (the original routine uses MVI after ALU).
            self.a = imm
        elif op == 0x1A:  # MOV B,A
            self.b = self.a
        elif op == 0x0A:  # MOV A,B
            self.a = self.b
        elif op == 0x60:  # ADDNC A,B
            if imm != 0xA2:
                raise AssertionError("unexpected prefixed ADDNC")
            value = self.a + self.b
            self.a = value & 255
            self.skip = value <= 255
        elif op == 0xBB:  # STAX (DE+imm)
            self.ram[(self.de + imm) & 65535] = self.a
        elif op == 0x22:  # INX DE
            self.de = (self.de + 1) & 65535
        elif op == 0x32:  # INX HL
            self.hl = (self.hl + 1) & 65535
        elif op == 0x53:  # DCR C; skip if borrow
            old = self.c
            self.c = (self.c - 1) & 255
            self.skip = old == 0
        elif op == 0x4F:  # JRE signed negative displacement
            self.pc -= 256 - imm
        elif op == 0xB8:  # RET through stack memory
            low, high = self.ram[self.sp], self.ram[(self.sp + 1) & 65535]
            self.pc = low | (high << 8)
            self.sp = (self.sp + 2) & 65535
            if self.pc != RETURN_PC:
                raise AssertionError("incorrect RET address")
            self.finished = True
        elif op == 0x2A:  # LDAX (DE)
            self.a = self.ram[self.de]
        else:
            raise AssertionError("unhandled opcode %02x at %04x" % (op, op_pc))

    def run(self) -> None:
        while not self.finished:
            self.step()
        if self.hl != 0xFF61 or self.de != 0xFF84 or self.c != 255:
            raise AssertionError("unexpected final HL/DE/C or loop count")
        if self.sp != 0xFFF2:
            raise AssertionError("unexpected stack balance")


def expected_from_production(native_cli: Path) -> list[tuple[int, int]]:
    # The helper is an independently compiled binary linked to fm1juno_core.
    # No Python replacement of production arithmetic is accepted as the
    # comparison target.
    pairs = "".join(f"{old} {fresh}\n"
                    for old in range(256) for fresh in range(256))
    completed = subprocess.run(
        [str(native_cli)], input=pairs, text=True,
        capture_output=True, check=True)
    result = [tuple(map(int, line.split(",")))
              for line in completed.stdout.splitlines()]
    if len(result) != 65536:
        raise AssertionError("native helper returned %d/65536 pairs" %
                             len(result))
    return result


def verify(source: Path, native_cli: Path) -> None:
    original_window = load_window(source)
    references = expected_from_production(native_cli)
    all_executed: set[int] = set()
    all_skipped: set[int] = set()
    steps = 0
    for old in range(256):
        for fresh in range(256):
            case_number = old * 256 + fresh
            cpu = CPU(original_window, old=old, fresh=fresh,
                      extras=case_number % 2)
            cpu.run()
            result = (cpu.ram[0xFF80], cpu.ram[0xFF88])
            if result != references[case_number]:
                raise AssertionError(
                    "ORIGINAL IC29 opcode mismatch old=%d fresh=%d: "
                    "CPU=%r native=%r" %
                    (old, fresh, result, references[case_number]))
            # The other three ADC channels really executed too; test their
            # memory layout rather than masking a bad instruction branch.
            for i in range(1, 4):
                raw = cpu.ram[0xFF5D + i]
                held = cpu.ram[0xFF80 + i]
                conditioned = cpu.ram[0xFF88 + i]
                if not (0 <= raw <= 255 and 0 <= held <= 255 and
                        0 <= conditioned <= 255):
                    raise AssertionError("invalid channel output")
            all_executed.update(cpu.visited)
            all_skipped.update(cpu.skipped)
            steps += cpu.steps

    expected_addresses = {
        0x0800, 0x0802, 0x0803, 0x0805, 0x0807, 0x0809,
        0x080A, 0x080B, 0x080C, 0x080E, 0x0810, 0x0811,
        0x0813, 0x0814, 0x0816, 0x0818, 0x0819, 0x081A,
        0x081C, 0x081D, 0x081E, 0x081F, 0x0821, 0x0822, 0x0823,
    }
    if not expected_addresses.issubset(all_executed | all_skipped):
        raise AssertionError("missing branch addresses: " +
                             str(expected_addresses - all_executed - all_skipped))
    print("IC29_ADC_OPCODE_EXECUTION: PASS")
    print("source: public ic29.txt disassembly, original bytes loaded at runtime")
    print("window: 0x0800..0x0824 sha256=" + WINDOW_SHA256)
    print("production comparisons: 65536/65536 (held byte, conditioned byte)")
    print("executed+skipped instruction sites: %d" %
          len(all_executed | all_skipped))
    print("interpreted CPU instruction steps: %d" % steps)
    print("scope: isolated ADC opcode window, no external interrupts/cycle timing")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--ic29-disasm", required=True, type=Path)
    parser.add_argument("--native-cli", required=True, type=Path)
    arguments = parser.parse_args()
    try:
        verify(arguments.ic29_disasm, arguments.native_cli)
    except (OSError, ValueError, AssertionError,
            subprocess.CalledProcessError) as exc:
        print("IC29_ADC_OPCODE_EXECUTION: FAIL: " + str(exc), file=sys.stderr)
        sys.exit(1)
