# P1.1 — independent uPD7811 ALU flag oracle: ADC and VCF

Updated: 2026-11-03
Status: **instruction-semantic subroutine oracle passed in host CI; original full-CPU execution is still an OPEN gate**.

This document describes the next fidelity slice after `PHASE1_NATIVE_PANEL_SERIAL_ADC.md`. It adds **no copied firmware ROM tables** and does not run original ROM bytecode.

## Evidence / implementation authorities

1. Original module CPU control-flow and operands: public IC29 disassembly at https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt
2. Independent instruction-level meaning of `SUBNBX`, `SUINB`, `GTI`, `ADDNC`, `DADDNC`, `DSUBNB`, and `DGT`: NEC uPD7810/11 / 78C10/11 technical documentation; corroborated against the independent uPD7810/11 emulator in MAME (https://github.com/mamedev/mame/blob/master/src/devices/cpu/upd7810/upd7810_opcodes.cpp). This source is consulted for **semantic behavior only**; its implementation is not linked or included.
3. Production DSP arithmetic under test: `src/control106/native_panel106.c` and `src/control106/vcf_control106.c`. The oracle is kept in **tests only**: `tests/unit/test_upd7811_flag_oracle.c`.

## Separate test implementation

The oracle does **not** call the production `control106_mul16x8_hi` / `vcf106_*` / ADC conditioner helpers to calculate its expected values. Instead, it uses:

- explicit 8-bit subtraction, unsigned borrow and “skip next” state for `SUBNBX/SUINB`;
- explicit >1 comparison for the `GTI` threshold;
- explicit 8-bit addition and carry for `ADDNC`;
- independent 8x8 partial products for 16x8 high-word multiplication;
- a wrapped 16-bit `EA` with a separate accumulated underflow-selection flag, changed only by the relevant `DADDNC/DSUBNB` overflow/underflow branches;
- a final `EA & 0xc000` saturation decision, using the retained underflow flag to choose 0 versus 0x3fff.

It thereby checks the same observable arithmetic through structurally different reference code.

## ADC $0800..$0824

The original path first subtracts the previously accepted **raw** byte from the current raw byte (with unsigned no-borrow flag), conditionally negates the absolute difference, and only accepts new raw samples for differences **strictly greater than one**.

The conditioned byte is then computed by two `SUINB` skip branches and an `ADDNC` carry clamp:

`n = max(0, accepted_raw - 4)`

- for `n < 235`: use `n`;
- for `n >= 235`: use `min(255, 2*n - 235)`.

The independent test executes every possible pair of old accepted raw byte and incoming raw byte: **256×256 = 65,536 pairs**. Every pair checks both updated accepted raw and the final conditioned value. Additional direct threshold vectors cover accepted raw 238, 239, 240, 248 and 249.

Outcome: **all pairwise comparisons matched the production conditioner**.

This verifies this subroutine's **modeled integer/skip arithmetic**. It does *not* prove physical ADC conversion clock, sample noise, timing of automatic bank alternation, initial raw RAM state across original firmware power-up, or exact uPD7811 ISR instruction-cycle phase.

## VCF $04D5..$0640

The ROM operates on a 16-bit working `EA` register:

1. start at patch cutoff, shifted left 7;
2. apply positive/negative LFO and bend (16-bit arithmetic, with underflow state retained on negative paths);
3. add/subtract the independently truncated ENV contribution;
4. compute Key Follow from separately truncated `pitch/4 + pitch/8` relative to the middle-C pivot;
5. add/subtract Key Follow, updating the low/negative overflow state when a subtraction borrows or an addition carries;
6. saturate **once at final DAC output**, not after each stage. The 16-bit wrapped value's upper two bits and the accumulated flag determine whether the clipped result is 0 or 0x3fff.

Testing includes every combination of the three sign flags across 6 structured parameter configurations, 7 pitch boundaries and 6 envelope boundaries (2,016 structured cases), plus **90,000 deterministic seeded combinations** of valid 7/8/16-bit parameter states including out-of-range LFO magnitude values (which the production path clamps at its defined 0x1fff boundary).

Each case checks:

- unclamped global signed cutoff/LFO/bend;
- full per-voice 14-bit code against the independent **wrapped 16-bit, carry/borrow** oracle;
- the final physical 12-bit DAC reduction;
- signed Key Follow contribution against independent split-byte multiplication.

The test requires its generated cases to cover at least one add-carry and one subtract-borrow branch, not just common non-overflow values.

Outcome: **all comparisons matched**, including the wrap/saturation cases explored. This supports keeping the current signed-widened production VCF implementation as a behaviorally equivalent *macro-control* calculation over the tested parameter domains. The test does **not** certify every possible interleaved CPU interrupt or an independently captured full original-ROM trace.

## CI / quality gate

- New file: `tests/unit/test_upd7811_flag_oracle.c`.
- New CTest target: `fm1juno_upd7811_flag_oracle` with strict C11 `-Wall -Wextra -Wpedantic -Werror`.
- GitHub Actions host-tests **#240**: **18 / 18** passed, including the new oracle.
- Existing callback invariance and native serial/ADC smoke remain unchanged and green.

## Remaining strict digital fidelity gate

**DO NOT mark P1.1 closed** based solely on this oracle.

- Execute original IC29 ROM (or a relevant extracted instruction-window fixture) in a separately validated uPD7811 CPU engine, and compare external-state/flag traces, not merely arithmetic results.
- Confirm ADC bank hardware scan cadence and ISR collisions.
- Obtain independent original 8253 gate/prescaler phase evidence.
- Keep derived 8253 divisor and DCO-CV laws classified **DERIVED_APPROXIMATE**, not original-ROM bit-identical.
- Resolve/control original microsecond macro/S&H offsets through measured or cycle-accurate data, and make active calibration gaps visible to the host.

The recommended next isolated task is an **executable instruction-window fixture** for the small ADC subroutine and VCF flag boundaries, driven by independent CPU core and isolated ROM test memory, not the final audio renderer.
