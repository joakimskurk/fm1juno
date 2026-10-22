# JUNO-106 module-ROM control notes

Status: Phase 0 evidence extraction from the public, manually decoded `ErroneousBosh/j106roms` module-CPU disassembly (`ic29.txt`).

Important caveat: the disassembly's author explicitly describes it as partial/inaccurate/incomplete. We therefore distinguish **instruction/data facts** from **human labels/comments**. Raw instructions, addresses and literal table data are strong evidence; inferred variable names and purpose comments are treated as provisional unless corroborated by service documentation or behaviour.

## 1. 8253 mode is now resolved

The module ROM contains a dedicated 8253 mode-word routine at `$0825`:

- writes `$36` for counter 0;
- adds `$40` -> `$76` for counter 1;
- adds `$40` -> `$B6` for counter 2.

For an 8253 these control words mean:

- binary counting;
- LSB then MSB load;
- Mode 3 square-wave generator;
- counter select 0/1/2 respectively.

Therefore the authentic DCO timer abstraction should use Mode 3 semantics. For count `N`, output repetition frequency is `f_timer / N`; even counts produce equal high/low halves, odd counts produce `(N+1)/2` clocks high and `(N-1)/2` clocks low.

This directly resolves the largest ambiguity in the original circuit spec.

## 2. Counter reset/reprogram behaviour

The ROM does **not** blindly run six independent free-running phase accumulators.

Normal count update path around `$0464`:

1. interrupts are temporarily disabled;
2. a per-voice hardware address is selected from a table;
3. the divisor low byte is written first;
4. the divisor high byte is written second;
5. interrupts are re-enabled.

When a voice has its reset bit set, code branches to `$04B3`, selects a per-counter reset/control command from the table at `$0B56`, writes the appropriate Mode-3 control word (`36/76/B6`) to the command register, then jumps back to the count-load path.

**Model consequence:** distinguish `update_count()` from `reset_and_load_count()`. A newly/reset voice can have deterministic timer phase initialization, while pitch changes on an already-running voice need not reset the counter phase.

Exact conditions for all same-note/retrigger/steal cases still need allocator-state tracing.

## 3. Timer divisor note table

The module ROM contains a table labelled by the disassembly as `0F30_noteClkDivTbl`.

First values:

```text
F06E E2E9 D61F CA18 BEBA B404 A9E0 A052
974F 8ECC 86C5 7F33 780F 7151 6AEF 64EE ...
```

The surrounding routine accesses both the selected note and the next note, computes the difference, multiplies that difference by an 8-bit fractional pitch value, and subtracts the interpolated amount from the base divisor.

The pitch representation in the same ROM is explicitly handled as 8.8 fixed point:

- high byte = semitone index;
- low byte = 1/256 semitone fine position.

The pitch-build routine starts from `$1818`, then adds/subtracts tuning, LFO and further modulation terms before the note-table interpolation.

**Model consequence:** authentic pitch is best represented first in the ROM's 8.8 semitone coordinate, then converted through the ROM divisor-table interpolation to an integer 8253 count. This preserves the original quantisation in the correct domain.

Do not substitute `count = round(clock / ideal_frequency)` in strict mode until we compare its output against the ROM path and prove equivalence.

## 4. Envelope tables are real ROM data

### Attack table

The module ROM contains a 16-bit attack-increment table beginning at `$0B60`:

```text
4000 2000 1000 0AAA 0800 0666 0555 0492
0400 038E 0333 02E9 02AB 0276 0249 0222
0200 01E2 01C7 01AF 0199 0186 0174 0164 ...
```

Code around `$056E` indexes this as a two-byte table.

### Decay/release table

The ROM contains a shared decay/release coefficient table beginning at `$0D60`:

```text
1000 3000 5000 7000 9000 A000 A800 B000
B800 C000 C800 D000 D800 E000 E800 F000
F080 F100 F180 F200 F280 F300 F380 F400
F480 F500 F580 F600 F680 F700 F780 F800 ...
```

The envelope code looks this table up for decay and release.

### `CalcDecay` at `$083D`

The disassembly shows a compact fixed-point multiply routine using three 8x8 multiplications and omitting the low-low partial product. With `value = VH:VL` and `coeff = CH:CL`, the returned approximation is equivalent to:

```text
VH*CH + ((VH*CL) >> 8) + ((VL*CH) >> 8)
```

This is not the same as an exact 16x16 multiply shifted by 16. The truncation is part of the firmware behaviour and should be preserved in strict `MODEL_106`.

### Envelope shape consequence

The surrounding ROM flow supports the following architecture:

- attack is an integer additive ramp using a table increment;
- decay operates on the difference between current envelope and sustain, applies `CalcDecay`, then adds sustain back;
- release applies the same multiply-style decay toward zero.

This strongly supports the basic envelope architecture used in modern reverse-engineered models, but we should use the actual ROM table data rather than regenerated curve approximations where licensing/provenance permits documenting numerical constants.

## 5. Control-loop rate

Roland's service timing diagram gives approximately 4.2 ms for a complete multiplexed DAC/S&H service cycle. Independent hardware/firmware-oriented work commonly reports approximately 4.23 ms (~236 Hz).

At this stage:

- **confirmed primary:** ~4.2 ms service frame;
- **strong secondary/measurement target:** 4.2335 ms / ~236.21 Hz;
- **not yet frozen:** one exact universal constant, because loop execution can be workload/state dependent.

The reference engine should initially expose the control-frame period as a calibration constant rather than hardwiring it across multiple subsystems.

## 6. What this changes in FM1JUNO

We can now make several stronger architectural commitments:

1. `Timer106` implements actual 8253 Mode 3 semantics.
2. Strict pitch generation uses an 8.8 semitone control coordinate plus ROM-style divisor-table interpolation and integer timer counts.
3. Timer reset is a distinct operation from ordinary count reload.
4. `Envelope106` should be integer/table driven, not a generic exponential ADSR.
5. The ~4.2 ms control service frame remains an intentional part of the sound/control model.

## 7. Still unresolved

- exact full note-divisor table extent and range-boundary treatment;
- exact ordering/scaling of bender, portamento and all LFO contributions in the pitch-build routine;
- count-change timing relative to the 8253's internal reload rules;
- exact allocator logic that decides when `resetVoiceBits` is set;
- exact firmware-loop timing under different active-voice states;
- whether analogue S/H settling should be represented as RC smoothing rather than linear interpolation.

These are now narrow reverse-engineering questions rather than generic synthesis unknowns.
