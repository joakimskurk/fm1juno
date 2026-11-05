#!/usr/bin/env python3
"""Generate a TEST-ONLY MAME juno106.cpp with deterministic ADC callbacks.

Input: upstream MAME mame0285 src/mame/roland/juno106.cpp.
Output: separately built TEST-ONLY MAME C++ source, not production code.

This generator and --input-profile fixed-eight in the real ADC IRQ gate
prepare a legitimate nonzero ADC-source test. It is NOT itself proof
that a rebuilt MAME binary passed, nor permission to claim Roland
analogue conversion or original clock timing.
"""
from __future__ import annotations

import argparse
from pathlib import Path

PROFILE = (
    (0, 0x11), (1, 0x2D), (2, 0x49), (3, 0x65),
    (4, 0x81), (5, 0x9D), (6, 0xB9), (7, 0xD5),
)
ANCHOR = (
    "\tUPD7810(config, m_modulecpu, 12_MHz_XTAL); "
    "// µPD7810G or µPD7811G-102-36\n"
    "\tm_modulecpu->set_addrmap("
    "AS_PROGRAM, &juno106_state::module_map);\n"
)
LABEL = "// FM1JUNO TEST-ONLY ANALOG INPUT FIXTURE"


def patch_upstream(source: str) -> str:
    if source.count(ANCHOR) != 1:
        raise ValueError(
            "upstream MAME juno106 module CPU anchor changed; "
            "stop rather than patching the wrong machine")
    if LABEL in source:
        raise ValueError("analogue fixture already patched")
    lines = [
        "\t" + LABEL + ": not Roland calibration data.",
        "\t// Values are eight distinct, fixed 8-bit MAME AN callbacks.",
        "\t// TEST BINARY ONLY; never ship this in user firmware or MAME.",
    ]
    lines += [
        "\tm_modulecpu->an%d_func().set(" % channel
        + "[] { return u8(0x%02x); });" % value
        for channel, value in PROFILE
    ]
    patched = source.replace(
        ANCHOR, ANCHOR + "\n" + "\n".join(lines) + "\n",
        1)
    if patched.count(LABEL) != 1 or any(
            patched.count("m_modulecpu->an%d_func().set(" % i) != 1
            for i, _ in PROFILE):
        raise AssertionError("incomplete analog callback bindings")
    return patched


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    original = args.source.read_text(encoding="utf-8")
    new_source = patch_upstream(original)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(new_source, encoding="utf-8")
    print("FM1JUNO_TEST_ONLY_MAME_ADCSOURCE: PASS "
          "(8 pinned AN0..AN7 callback bindings, source only)")
    print("Next: compile and run this changed MAME juno106 driver "
          "with --input-profile fixed-eight; do NOT claim verified "
          "nonzero peripheral input before that CPU run passes.")


if __name__ == "__main__":
    main()
