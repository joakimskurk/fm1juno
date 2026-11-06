# P1.1 — 8253 Mode-3 independent edge tests and strictness boundary

Updated: 2026-11-03
Status: **regular-count event model TESTED; strict original-unit timing NOT fully certified**.

## Sources

- Intel 8253/8253-5 datasheet (November 1986), Mode-3 square-wave definition, LSB/MSB programming and clock/gate semantics:
  https://www.cpcwiki.eu/imgs/e/e3/8253.pdf
- Roland module CPU disassembly:
  https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt
  - at 0x0413..0x04B3, the note divisor is calculated and sent to selected 8253 counter **LSB then MSB**;
  - the reset path explicitly writes a Mode-3 control word **before** the count;
  - the ordinary change path does not rewrite the control word.
- Frozen project evidence: `docs/research/JUNO106_PITCH_DCOCV.md`.

## Implemented digital timer contract

Reference C code: `src/timer106/timer8253_106.{h,c}`.

- Internal register count 0 maps to 65536 clocks.
- For **regular** Mode-3 values, high duration in counter input clocks is `ceil(N/2)`, low is `floor(N/2)`.
- With non-reset timer-count updates the current active half-cycle is not truncated by the software API. A fully written new count is pending and applied at the next output boundary **in the current reference approximation**.
- A reset-and-load discards the pending write and establishes high output with a new phase origin. The reference accepts a Q32.32 timestamp and rounds the interval using the appropriate calibrated clock-period constant.
- Separate ranges in this host model have clock families `1 MHz / 2 MHz / 4 MHz`. The corresponding Q32.32 sample-tick units at 44.1 kHz are 189408058, 94704029 and 47352014. These are **project clock assumptions** and require final original-board confirmation, especially the installed 8253 speed grade.
- A later `commit_edge` returns the absolute scheduled time and toggles output. Outer host callback boundaries are not timer events.

Important limitations:

- The simplified model is event-domain, not transistor/TTL/cycle-accurate. On the actual chip the initial count is loaded on input-clock edges; the exact offset from the CPU's MSB write to initial/phase-reset edge is **hardware verification pending**. A test passing exact Q32 arithmetic does not prove that physical sub-clock phase.
- Special small Mode-3 counts 1 and 3 are **excluded** from the regular half-period contract because original 8253 behavior can be exceptional; using the generic floor law on count 1 would create a zero-duration low output. The normal JUNO-106 documented note-divisor range does not use these values.
- `timer8253_106_regular_mode3_count()` identifies the supported subset (0, 2, and 4..65535). Scripted `DCO_COMMAND` validates this and rejects 1/3. The low-level timer functions are kept C ABI-compatible; direct callers must check this predicate before injecting exceptional values.
- The unconfigured module-control test harness now uses register count 0 (the defined 65536-clock case), not 1. This is a **safe test placeholder**, not an accurate note-on pitch without a requested profile.
- LIVE range/prescaler changes during a running timer require clock-phase/board confirmation. These tests only cover **range changes via explicit reset**; do not describe this as exact phase-preserving PF-port switching.
- GATE inputs, timing between LSB/MSB, physical 8253 input-clock phase and alternate manufacturer behavior remain open.
- An injected derived divisor base curve is still **not ROM-bit-exact**. Do not mistake exact edge scheduling for exact original pitch counts.

## Independent edge vectors now in CI

Test source: `tests/unit/test_timer_edge_vectors.c`. Reference known counts are directly written as expected constants, not reconstructed by the production `high_clocks` helper.

| Vector | Key assertion |
|---|---|
| Regular even/odd N | 2, 4, 5, 8, 9, 10, 11, 65534, 65535 high/low clocks + period total |
| N=0 special | 65536 total, 32768 high, 32768 low |
| Mid-high count update | original high duration is intact, next low takes the pending count |
| Two mid-low writes | latest complete count wins, current low duration unchanged |
| Reset mid-half with fractional sample timestamp | queued count is discarded; new 13-clock cycle starts with high=7 and low=6 |
| Range-resolved reset vectors | absolute edge timestamps scale with each specified input clock |
| Pending zero-count | pending N=0 transitions to long half-period at the next modeled boundary |
| Exceptional value gate | count=1 and 3 rejected from raw DCO test scripts; counts 0, 2, 4 accepted |

This is a **necessary** P1.1 validation slice, **not** the end of P1.1. Future work must compare ambiguous edge/reload/range/gate behavior against a separate executable hardware-level model or original board measurement, and then adjust the reference if it disagrees. The FM-1 target is not the authority for this model.
