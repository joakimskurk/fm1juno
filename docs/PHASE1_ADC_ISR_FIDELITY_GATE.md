# P1.1 — Original IC29 ADC ISR fidelity gate

**State:** P1.1 strict gate OPEN, but the **bounded, test-only
nonzero analogue-source ADC gate is VERIFIED**. A separately
compiled, source-pinned MAME 0.285 UPD7810 JUNO-106 machine with eight
distinct AN0..AN7 callbacks passed **36/36 full ADC-peripheral IRQ →
original ISR/RETI → six original ENV/VCF voices → original ADC
conditioner** process tests. Older original-byte MAME CPU tests
remain green. This does **not** authenticate the actual Roland analogue
input voltages, CPU UPD7811/ROM identity, full original scheduler or
cycle-accurate original hardware timing.

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
The separate direct-source test and the later real internal-ADC
interrupt tests have now confirmed this **pre-toggle storage order**
on MAME's UPD7810 CPU core, including nonzero callback-sourced data.
This remains bounded emulator evidence, not hardware calibration.

Separately, the main-loop code uses FF5C bit 3 at `0656`:
bank 0 takes the `06E2` branch and returns from `CALF 0800`
at `06E7`; bank 1 returns at `065F`. This selection and both
four-channel RAM updates have **already passed** on the independent
MAME UPD7810 CPU. The newer end-to-end
IRQ→six-voice→ADC-service test also uses FF5C **written by the
original interrupt handler**, including eight distinct nonzero
callback-sourced channel readings. Its intervening scheduler
remains synthetic and explicitly documented.

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

## New verified evidence: direct original-ISR opcodes and real internal ADC IRQ

**Focused GitHub Actions `MAME original ADC ISR direct-opcode gate`
#4 SUCCESS** (test code `5f27ee4`, run `41933092493`) after
independently executed original opcode byte SHA checks:
`0x0020..0x0021` and `0x0070..0x008D`.

- `tests/cpu_ic29/run_mame_adc_isr_direct_probe.py`:
  **20/20 direct-vector CPU scenarios PASS**, covering ANM
  00/08/10/18 and five CR0..CR3 combinations each. Synthetic boot
  sets primary A, BC, DE and HL sentinels and JMPs to original vector
  0020; debugger explicitly sets ANM and CR0..CR3 to controlled
  nonzero values. MAME executes original EXA/EXX, copies the
  **pre-XOR ANM** to FF5C, changes ANM bit3, stores CR registers
  in FF5D..FF60 and restores primary registers. The breakpoint
  at 008D is deliberately *before RETI*: a direct vector JMP is
  not a hardware interrupt. The initial failure was only a
  debugger-fixture radix error (unprefixed decimal numbers were
  interpreted as hexadecimal); fixed with explicit `0xNN`
  literals in commit `e02e6fe`.
- `tests/cpu_ic29/run_mame_adc_isr_peripheral_probe.py`:
  **4/4 real MAME internal-ADC-triggered CPU interrupt round trips
  PASS.** The synthetic boot has **no jump to vector**. It sets
  ANM, MKH=04, enables interrupts and idles until the MAME
  UPD7810's own ADC conversion engine raises INTFAD. MAME's
  real IRQ logic pushes PSW/PC (SP FFF0→FFED), vectors at
  **0020**, executes original 0070..008D, and executes **RETI**
  back into the interrupted idle loop (SP returns to FFF0).
  Original A/D ISR toggles ANM 00→08, 08→00, 10→18, 18→10,
  records the prior bank mode in FF5C, writes four CR values
  to FF5D..FF60, changes MKH to 05, and restores registers.
  No CPU register/stack initialization was performed by the
  debugger after boot; the IRQ event is from the MAME CPU
  peripheral, **not** a debugger-controlled vector jump.

MAME `upd7810.cpp` implements ADC timer-driven scan conversion,
raises `INTFAD` at the four-channel cycle boundary and accepts
that IRQ at vector 0020 if MKH bit 0 is unmasked and IFF is set.
The Juno-specific driver does **not** bind AN0..AN7 callbacks;
MAME's default read8 callback returns **zero**. This means the
4/4 hardware-triggered tests certify the emulator's internal
IRQ/RETI control flow with all conversion samples zero, NOT
the JUNO-106's actual analogue source voltages or any
**original/physical** clock timing. Original `0x06B3`
DI/SKIT FAD/MKH/EI control section remains outside these tests.

The previously verified **736** bounded MAME CPU process scenarios
plus 20 direct ISR + 4 peripheral IRQ scenarios yield **760
selected process scenarios** with independently verified green
focused runs. The full combined workflow adds both new gates as
separate final stages; this does not by itself claim the newest
full-matrix run completed until CI confirms it.

## Linked original IRQ → six-voice → ADC service — MAME focused #2 VERIFIED

The first end-to-end **single-process** control-flow test passed:
`tests/cpu_ic29/run_mame_adc_irq_to_six_voice_service.py` executes
18 six-voice fixture sets for both ADC bank modes. **Focused CI #2
SUCCESS** at commit `592807a`, run `41976855747`:
**36/36 MAME processes, 216/216 voice calculations and
144/144 ADC channel comparisons** match compiled native production C.
These 36 bring the selected verified MAME repertoire to **796**
processes, alongside the prior 760. The full combined matrix has
been extended to include this path; validate its run status
independently.

The internal MAME ADC scan engine **actually raises INTFAD**,
causes CPU hardware-style stack entry at original vector
`0020`, executes original `0070..008D` ISR and `RETI`,
then resumes a **synthetic, explicitly documented poll-and-jump**
at `0400` that dispatches to original `04D5` after FF5C
has been published. The unchanged original six-voice loop and
published `0652/06E2` bank select and `0800` four-channel ADC
conditioning code then execute. The test inspects real ISR entry,
FF5C/FF5D..FF60 snapshot and returned CPU stack, all six
original VCF DAC helper calls and their per-voice results,
selected-bank ADC outputs and unselected-bank sentinel integrity.

The first focused attempt timed out due to overlapping synthetic
bootstrap and original IRQ vector addresses. The test bootstrap
was moved to `0200` and original `0020` and
`0070` bytes left intact; this fixed the **harness** only.
The test does NOT add original firmware scheduler code between
IRQ and six-voice main loop, nor does it modify the production
control model. New `adc_full_bank_reference_cli.c` calls production
ADC functions for all four arbitrary raw/held-byte pairs.

**Critical physical input limitation remains unchanged:** No
AN0..AN7 channels are connected by upstream MAME's Juno-106
driver. The hardware-sourced `CR0..CR3` ADC readings are
all **zero**. This new test DOES NOT debugger-inject those
conversion registers after interrupt, and does not verify
nonzero physical conversion or original analogue timing.

### Test-only nonzero analogue callback requirement — VERIFIED in compiled MAME

MAME's `upd7810_device` exposes eight ADC read callbacks
`an0_func()` through `an7_func()`. In the pinned
MAME `mame0285` implementation, the CPU's internal ADC
converter reads those callbacks into CR0..CR3 and generates
INTFAD after a full four-channel scan. The ordinary upstream
JUNO-106 driver leaves these analogue inputs unbound and reads zero.
The test variant binds them to the following deterministic **nonzero**
8-bit values (hex), **not** physical Roland slider/knob voltages:

| MAME ADC input | AN0 | AN1 | AN2 | AN3 | AN4 | AN5 | AN6 | AN7 |
|---|---|---|---|---|---|---|---|---|
| Test-only fixed value | 11 | 2D | 49 | 65 | 81 | 9D | B9 | D5 |

**Actual compiled-driver GitHub Actions
`MAME 0.285 compiled nonzero ADC callback gate` #3 SUCCESS**,
code commit `78e1d510`,
[workflow run #41981525577](https://github.com/joakimskurk/fm1juno/actions/runs/41981525577).

- **Exact MAME upstream git commit:** `f379cd060ac3b2bd567ebbd0957aed059d6895ff`.
  GitHub Actions checks out this commit independently; it does not
  install or silently reuse a generic system MAME.
- `tests/cpu_ic29/patch_mame_juno106_analog_fixture.py`
  inserts eight callbacks in the **test-only** copy of upstream
  `src/mame/roland/juno106.cpp`. Modified source SHA-256:
  `b78fe5a9323281e1fef0531b2f4b6f916d2566860608d4adec02f12211bc6c30`.
- The focused workflow `.github/workflows/mame-compiled-nonzero-adc.yml`
  builds a real `juno106` executable with a **limited MAME
  driver-only build** and MAME 0.285's required **Qt5 debugger**.
  Actual CI-built binary SHA-256:
  `d025443aa7e3fb5275fc6f07c94b0438d51d4e4d43158d54c1f9564f08830b67`
  (hash of the particular CI build, not claimed portable).
- The compiled binary is invoked with
  `--input-profile fixed-eight`, and **36/36 full CPU
  processes PASS**: 18 six-voice ATTACK/DECAY/RELEASE patterns
  × 2 ADC banks. That is **216 per-voice ENV/VCF and 144 ADC
  channel comparisons**, using separately compiled C production
  oracles. All six original DAC subroutine entry/return checks,
  the original `0x0020` interrupt vector, `0x0070` handler,
  CPU stack push, original RETI and bank-selected
  `0x0652/0x06E2/0x0800` service checkpoints pass.
- **Bank 0 confirmed** at the original ISR/CR checkpoint:
  `11 2D 49 65`. **Bank 1 confirmed:** `81 9D B9 D5`.
  The original handler publishes the pre-toggle bank mode
  to FF5C and the four real MAME-converter output bytes to
  FF5D..FF60. The original selected-bank conditioner consumes
  them, while the unselected bank's independent sentinels persist.
  There are **no debugger writes to CR0..CR3** in this test.
- Earlier source-only patch/negative-control gate
  [#11](https://github.com/joakimskurk/fm1juno/actions/runs/41978017143)
  also passed: unpatched, zero-input stock MAME intentionally
  **fails** when asked to satisfy the `fixed-eight` values.
  This independent fail-closed control remains part of the
  focused stock-MAME regression workflow.
- The preceding **796 selected MAME CPU process fixtures**
  remain green in complete independent CPU matrix #75 and
  other focused workflows on commit `78e1d510`;
  the 36 newly verified compiled-driver nonzero-input cases
  extend the cumulative **selected test repertoire to 832/832
  process scenarios**. Do not conflate count with original ROM
  instructions or original hardware cycles.

#### Harness issues found and resolved

The first full source-build attempt (#1) failed at build-configuration
time because MAME 0.285 requires **Qt5** debugger dependencies;
the workflow initially installed Qt6. Replacing it with Qt5
resolved compilation. Build attempt #2 **compiled the patched
binary** and passed bank 0 but failed bank 1:
its original ISR returned the bank 1 FF5C mode while CR0..CR3
still contained bank 0 samples from a pending pre-bank-change
ADC conversion. Commit `78e1d510` changed **synthetic fixture
startup only**: initialize ANM bank before the long RAM setup;
clear stale pending FAD requests with the original CPU
`SKIT FAD` instruction before enabling interrupts.
The third compiled run passed all 36 cases. The published
original IC29 source opcode segments and production ADC/ENV/VCF
arithmetic were **not modified** to make it pass.

#### Reproduction and strict remaining limits

The complete reproducible build-and-execute command sequence
lives in `.github/workflows/mame-compiled-nonzero-adc.yml`.
The source patch and test runner are respectively
`tests/cpu_ic29/patch_mame_juno106_analog_fixture.py` and
`tests/cpu_ic29/run_mame_adc_irq_to_six_voice_service.py`.

The compiled test's actual peripheral interrupt and nonzero
conversion have been verified **within MAME UPD7810**, but the
handoff *after* real original RETI back to the original six-voice
loop is still **synthetic**: a fixture-only `0x0400`
poll tests FF5C publication and jumps to source-pinned
`0x04D5`. Hence this verifies component ordering and control
values in a bounded emulator process, **not** full source
firmware scheduling or exact original interrupt interleaving.
In addition, 0x06B3..0x06BC main-loop interrupt boundary,
authentic hardware UPD7811 identity/ROM mapping, 8253/GATE
phase and real ADC voltage/clock calibration remain OPEN.

**Bounded nonzero ADC callback milestone: CLOSED as verified.
Strict P1.1 original digital fidelity gate: STILL OPEN.**

## Updated next engineering sequence

1. **COMPLETE:** deterministic nonzero test-only AN0..AN7
   callbacks on a source-pinned MAME executable, verified with
   36 genuine ADC peripheral IRQ → original ISR/RETI →
   original six-voice and ADC bank service tests.
2. **NEXT:** replace the fixture-only `0x0400` post-RETI
   polling/JMP with the applicable **published original
   main-loop scheduling/IRQ handoff**, separately hash-pinned
   and protected by explicit interrupt-entry checkpoints.
   Trace asynchronous collision cases under the emulated
   CPU; do not infer original time in microseconds.
3. Execute and inspect the original `0x06B3..0x06BC`
   DI/SKIT FAD/MKH/EI pending-IRQ masking section
   and 8253/CPU timer interactions, then evaluate the
   required timing and external PIT evidence.
4. Validate actual hardware CPU variant and authenticated
   original Roland ROM provenance, source clock and physical
   input voltage/ADC curve before making original-uPD7811
   or physical timing claims.

## Previously planned engineering sequence (partially completed)



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

**Exit is not achieved merely by this document, 20 direct opcode cases
or four zero-input MAME peripheral IRQ round trips.** The strict P1.1 digital gate remains OPEN until
the actual scheduler/IRQ, CPU mapping, peripheral and timing evidence
has been gathered or explicitly granted a bounded-fidelity exception.
