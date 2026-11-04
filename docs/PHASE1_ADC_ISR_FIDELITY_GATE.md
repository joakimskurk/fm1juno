# P1.1 — Original IC29 ADC ISR fidelity gate

**State:** OPEN. The bank-selected **main-loop ADC service** has been
executed under the independent MAME UPD7810 CPU and verified in 36
six-voice+ADC scenarios. An *actual ADC conversion interrupt* and its
peripheral timing have **not** been executed or validated.

This is a reproducible handoff for the next bounded implementation,
not a claim of original Roland hardware emulation.

## Source and evidence

Unmodified source at test time:
- Public, independently authored but **incomplete/unverified** IC29
  disassembly: https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt
- Upstream MAME JUNO-106 skeleton (BSD-3-Clause):
  https://github.com/mamedev/mame/blob/master/src/mame/roland/juno106.cpp
- Existing focused differential test:
  `tests/cpu_ic29/run_mame_six_voice_adc_handoff.py`, MAME focused
  ADC handoff CI #1 SUCCESS (`41931105705`, code commit `4ab7af7`).

**Original IC29 CPU code windows**, source-transcription byte pin only,
not provenance of the original silicon firmware:
- `0x0020..0x0021` (2-byte Timer/ADC vector entry):
  SHA-256 `15c841dff8407a197a8be456d43283d973c0ba690724b24da5e5d0d6dbc70423`.
- `0x0070..0x008D` (30-byte ADC ISR body):
  SHA-256 `d4578a044eed77c142a2bb1c9f252643e4a95e15a22833a7879b7ea63551ae61`.
- `0x06B3..0x06BC` (10-byte interrupt-control boundary):
  SHA-256 `e1cc6539c764096f614c004e353c27019bd7a8975cd2edc846b2be09d0fb7c31`.
- `0x0652..0x065E`, `0x06E2..0x06E6`, `0x0800..0x0824`
  main-loop handoff, bank select and ADC conditioner are **already
  executed** under MAME; see `PHASE1_IC29_OPCODE_EXECUTION.md`.
  Do not confuse ISR raw-acquisition with main-loop conditioning.

The public listing header calls the IC29 source **4,096 bytes**
(`voice.bin`, `0x0000..0x0FFF`); the stock MAME JUNO-106 driver
declares an **8 KiB** module ROM region and currently instantiates
a `UPD7810` CPU for `modulecpu` at `12_MHz_XTAL`.
The driver's own comment mentions a possible
`µPD7810G or µPD7811G-102-36`. This ambiguity and disassembly
provenance must be resolved before whole-ROM or uPD7811-specific
fidelity claims.

## Original ADC ISR control flow — bounded published facts

The annotated IC29 listing indicates:

| PC | Operation | Effect under the published transcription |
|---|---|---|
| `0020` | `JRE 0070` | Entry from timer/counter/ADC interrupt vector |
| `0070–0071` | `EXA`, `EXX` | Switch to alternate accumulator/register bank |
| `0072` | `MOV A,ANM` | Read A/D mode register |
| `0074` | `XRI ANM,08` | Toggle scanned channel group 0–3 vs 4–7 |
| `0077–007A` | `LXI HL,FF5C`, `STAX (HL+)` | Store the *pre-toggle* ANM value at FF5C and advance pointer |
| `007B–0086` | `MOV A,CR0..CR3`, `STAX (HL+)/(HL)` | Snapshot conversion registers into FF5D..FF60 |
| `0087` | `MVI MKH,05` | Change interrupt mask register |
| `008A–008B` | `EXA`, `EXX` | Restore prior accumulator/register set |
| `008C–008D` | `EI`, `RETI` | Re-enable interrupts and return |

The note about *pre-toggle* ANM is important: the published code
reads ANM into A *before* XORing ANM bit 3. It then writes A to FF5C.
A future test must independently verify this order under a correctly
configured CPU peripheral, not infer it from the main-loop bank tests.

Separately, the main-loop code uses FF5C bit 3 at `0656`:
bank 0 takes the `06E2` branch and returns from `CALF 0800`
at `06E7`; bank 1 returns at `065F`. This selection and both
four-channel RAM updates have **already passed** on the independent
MAME UPD7810 CPU, but FF5C was injected rather than ISR-produced.

Another interrupt-control boundary exists in the published main
loop at `06B3 DI`, `06B4 SKIT FAD`,
`06B7 MVI MKH,04` and `06BA EI`. Code execution of that
boundary with proper **pending hardware interrupt flags** and
cycle-calibrated original peripheral state is still OPEN.

## MAME skeleton gaps to close before scheduling claims

The MAME `juno106.cpp` skeleton defines an empty
`INPUT_PORTS_START(juno106)` and no analogue channel callbacks or
JUNO-106-specific ADC input wiring in the shown driver. It maps the
module CPU ROM and PIT write/latch regions; it is marked
`MACHINE_NO_SOUND | MACHINE_NOT_WORKING`.

Consequences:

1. The core may implement general uPD7810 ADC peripheral behavior,
   but this skeleton alone does **not** supply authenticated physical
   analogue channels, conversion values or their timing.
2. The original ISR entry, `EXA/EXX`, ANM bit toggle, CR0..CR3
   acquisition, MKH and RETI must be tested with **explicitly declared
   and controlled peripheral fixture inputs** before they can be
   described as independently executed.
3. Hand-injected FF5C / FF5D..FF60 snapshots are not proof of an
   ADC interrupt, even when downstream code and both selected banks
   run exactly as expected.
4. A debugger jump directly to `0070` can validate *instructions
   under controlled initial state* but does **not** validate real
   interrupt acceptance or RETI stack timing; label this separately.
5. Pin the actual CPU variant and prove ROM mapping (and any original
   internal program ROM provenance) before using UPD7811-specific
   silicon/cycle claims. The MAME skeleton UPD7810 configuration must
   remain explicit in every test report.

## Next bounded engineering sequence

1. Inspect the precise upstream MAME uPD7810 ADC register, input-callback,
   IRQ-vector and alternate-register semantics; document the needed
   memory/port configuration without copying Roland ROM into the repo.
2. Build an independently controlled peripheral fixture that can set
   ANM and CR0..CR3 and *actually request an ADC interrupt* to the
   supported MAME CPU device. If the stock driver cannot host this
   injection, mark the blocker and scope a local **test-only**
   standalone driver; do not alter the production engine to mask it.
3. First test the original ISR window from `0070` with pinned source
   bytes: verify saved register bank, FF5C mode, FF5D..FF60 input,
   actual ANM toggle and MKH/RETI. Keep this **direct-entry**
   window result distinct from a genuine hardware-interrupt test.
4. Only when peripheral conversion and interrupt triggering are
   independently controlled, test the original IRQ vector
   `0020 → 0070` followed by `RETI` and resumed main-loop
   bank service. Check accepted/conditioned bytes against production
   C and the opposite bank's sentinels.
5. Require an explicit CPU-cycle/clock source before deriving
   microseconds, ADC scan rate, ISR collision windows or PWM/DAC
   aperture timing. Otherwise leave them `MEASURE_PENDING`.
6. Preserve the stable 736 bounded CPU process scenarios and
   independently run the original 682+18+36 regression suites.

**Exit is not achieved merely by this document or by a green direct
opcode window.** The strict P1.1 digital gate remains OPEN until
the actual scheduler/IRQ, CPU mapping, peripheral and timing evidence
has been gathered or explicitly granted a bounded-fidelity exception.
