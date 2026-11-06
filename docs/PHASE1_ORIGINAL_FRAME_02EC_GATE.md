# P1.1 — Original IC29 next-frame scheduler at 0x02EC

**State:** The bounded original **0x02EC..0x03A0** source-byte
scheduler slice is **VERIFIED IN STOCK AND COMPILED MAME**:
[stock original-frame gate](https://github.com/joakimskurk/fm1juno/actions/runs/42133442733)
and [compiled eight-channel nonzero-ADC gate](https://github.com/joakimskurk/fm1juno/actions/runs/42133442637)
**SUCCESS, 36/36 CPU processes in each distinct input profile**.
Combining 940 earlier selected process scenarios, 36 stock
original-frame entries, 36 separately verified compiled nonzero
original-0738 tails and 36 compiled nonzero original-frame entries
gives **1,048 historically verified selected MAME CPU process
scenarios** across separate completed CI jobs.
The overall P1.1 strict gate remains OPEN; this is
**not** an authenticated Roland ROM, original UPD7811 silicon,
physical CPU clock/ADC voltage, complete frame scheduler or audio.

## Confirmed prerequisite

- The separately compiled eight-channel test MAME
  and genuine ADC IRQ → ISR/RETI → six original ENV/VCF →
  bank-selected conditioner → original post-ADC DI/SKIT FAD/EI,
  VCA, LFO/PWM, noise and 0x07B5 JMP have separate verified evidence.
  **940** is the pre-02EC source-tail historical baseline,
  not the current total. The independently completed
  original-frame and nonzero-tail stages bring the selected
  process-scenario count to **1,048**.
- The stock MAME original tail gate now has an additional
  **saved-interrupted-PC proof**. Focused [#10](https://github.com/joakimskurk/fm1juno/actions/runs/42129860723)
  passed **36/36** using the actual return PC stored by MAME's
  internal ADC IRQ: SP `FFEB` corresponds to an interrupt
  *inside* the published 0x082F..0x083C DAC helper, while SP
  `FFED` corresponds to original main-loop instructions including
  the original `0x0749` CALF call boundary or the `0x02EC`
  destination of source-pinned `0x07B5 JMP 0x02EC`.
  An earlier assertion incorrectly assumed the space between
  debugger CALL and RETURN markers was always inside the helper;
  this is false for an IRQ just before/after the CALF opcode.
  The test no longer guesses CPU stack depth from logger ordering.
- The prior compiled nonzero callback workflow's extended
  0x0738..0x07B7 tail stage has also been updated to use the
  same saved-PC distinction; confirm a completed green
  **compiled** run separately before increasing the cumulative
  count by those additional 36 cases.

## Next source-pinned original-byte slice

Published unofficial IC29 transcription:
`https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt`

- Bytes `0x02EC..0x03A0` (181 original published bytes),
  SHA-256
  `3cd0206778e6fa8650fd8d803ff4d7665cd15f9963803d1ff98a9118be70c7df`.
- `0x02EC..0x0300`: original frame/IC40 latch, sustain and
  running-voice mask update.
- `0x0301..0x030C`: original resonance 16-bit DAC at
  `0x082F` and restored CPU stack. Note the helper returns
  with A=EAL (low byte), not the pre-CALF channel-select A=06.
- `0x030D..0x03A0`: original LFO holdoff, attack and
  modulation accumulation, reaching `0x03A1`.
- A synthetic RAM fixture deliberately exercises original
  LFO status `FF1E` bit1/bit2 combinations in three
  distinct branch modes; nonzero resonance `0x1234`;
  LFO holdoff and delay-attack test rates; and both
  previously verified ADC bank modes.
- At the new `0x03A1` stop, the original LFO arithmetic
  is allowed to overwrite **D** in DE; only the unmodified
  **E** low-byte pointer and balanced CPU stack can be
  asserted as invariants. On the holdoff path original
  `0x0370 LXI DE,FF56` changes E=56; on the other
  routes E stays 31 from source `0x0737 POP DE`.

**New test:** `tests/cpu_ic29/run_mame_original_frame_scheduler_entry.py`.
**Focused workflow:** `.github/workflows/mame-original-frame-scheduler.yml`.

## Verified source-byte original frame and interrupt audit

Focused original-frame #4 first passed all 36
[original 02EC..03A0 CPU fixtures](https://github.com/joakimskurk/fm1juno/actions/runs/42132354390),
then focused #5 repeated 36/36 after **tightening
source provenance and IRQ checks**:

- **18 original six-voice phase patterns × both ADC banks**,
  actual internal ADC interrupt and original ISR/RETI,
  all previously verified six original ENV/VCF/DAC
  calculations, original post-ADC FAD boundary, VCA/LFO/PWM/noise
  and real `0x07B5 JMP 0x02EC`.
- **216 per-voice ENV/VCF and 144 ADC selected-bank**
  comparisons continue to match separately compiled
  production C. No original published CPU opcode or
  production algorithm is changed.
- The **181-byte** original frame `0x02EC..0x03A0`
  implements voice-run from gate/sustain,
  a full original `0x082F` resonance DAC call and
  return with `EA=0x1234`,
  then holdoff, attack and full-LFO modulation source branches.
  Both ADC banks successfully execute all three branches.
- Synthetic relocation area `0x0863..0x09E9`
  is now itself source-pinned: **391 published NOP bytes**,
  SHA-256
  `db6f6097124dc8eaba5c36f64fdc1e3e44201b6c0f3fd6a11fbab4b38657ad6b`.
  The test fails if this source region changes or is not all zeros.
- A **separate real MAME CPU IRQ audit** reads the saved return-PC
  from the original ADC interrupt stack. In these 36 chosen
  cases, **four further genuine ADC interrupts** occurred
  after the original frame restart and before the stop
  `0x03A1`, at saved PCs
  `0x02F2`, `0x02EE`, `0x02F4`, `0x02F4`.
  They all had MAME CPU SP=`FFED`, consistent original
  bank/mask state, alternate-bank ADC ISR snapshot
  and original RETI. The test also validates the
  `SP=FFEB` nested DAC-helper case if it occurs, but
  does **not** claim it was observed in this 36-case set.
  These are emulator fixture observations, not physical
  interrupt timing statistics.
- Both the **focused CI** and a new appended stage in
  `.github/workflows/mame-independent-cpu-probe.yml`
  now execute this test. The combined full CI result
  must be checked separately before claiming it passed
  on the latest commit.
- `.github/workflows/mame-compiled-nonzero-adc.yml`
  runs this scheduler test using the independently compiled
  `mame0285` Juno-106 binary with eight distinct nonzero
  analogue callbacks, `--input-profile fixed-eight`.
  [Compiled workflow 42133442637](https://github.com/joakimskurk/fm1juno/actions/runs/42133442637)
  passed **36/36** additional original-frame CPU cases, with
  **216 voice and 144 selected-bank ADC parity comparisons**.
  The compiled CPU log also recorded **six further genuine
  original-frame ADC interrupts** with saved return PCs
  `02EE`, `02F2`, `02FD`, `02F2`, `02F4`, `02EE`.
  These are separate from the **36/36 compiled original 0738
  tail cases** verified in [compiled MAME #33](https://github.com/joakimskurk/fm1juno/actions/runs/42131473570).
  Both groups are included in the **1,048** historical total.

No standalone full original scheduler LFO/PWM or resonance
native host parity oracle exists for this whole 181-byte
segment; the branch and source-state invariants are
currently checked against independent MAME CPU execution
and source-derived expectations only.

## New source-pinned 0x03A1..0x03D6 extension (6 November 2026)

**Focused stock-MAME PASS: 36/36 independent CPU processes.**
[GitHub Actions original pitch-offset gate 42277911705](https://github.com/joakimskurk/fm1juno/actions/runs/42277911705)
ran the original ADC interrupt, ISR/RETI, six-voice and ADC
chains, the proven 02EC scheduler, and then original
**03A1..03D6 master VCA and signed Tune/LFO/Bend offset**
instructions. The prior 03A1 debugger gate continues before
the new 03D7 stop; all 36 old frame checks and 36 new
pitch-offset checks passed in this focused job.

- Source-pinned published IC29 bytes: full **02EC..03D6**
  (235 bytes), SHA-256
  `a33160eb553a6f16fc83199e739c104fc31971075facd97f8ed2e355052351e8`;
  newly added **03A1..03D6** (54 bytes), SHA-256
  `ee512b953239a68c34663d1f8846eb98251cf0bd29e81108e9267969c2d4a3c2`.
- Real CPU `CALF 082F` master VCA call used the test-only
  RAM value `0x5678`; source helper retained EA and returned
  A=EAL `0x78`. The test checks call and return and balanced
  original CPU stack.
- Source-level signed Tune + signed LFO pitch + signed Bend
  accumulation starts at `0x1818` and stores its 16-bit
  wrapped output at `FF6F`. Every case compares actual
  source CPU EA and RAM with an independent arithmetic oracle
  constructed from live RAM snapshots at `03A1`.
- The original ADC service may replace test-only Tune/Bend
  startup seeds before the `03A1` snapshot. Actual focused
  examples include Tune `0x80` / Bend `0x0123` on one
  bank, and Tune `0x27` / Bend `0x0000` on the other.
  The differential check uses observed RAM state rather than
  assuming initial synthetic values survive.
- Both earlier LFO-delay branch tests and the six-voice
  control/ADC parity comparisons remain mandatory.
- `--extended-pitch` is an **additive stock-MAME fixture**;
  the default runner still stops at `03A1`. This focused
  extension has also passed as a required prerequisite
  in the [compiled nonzero Portamento gate](https://github.com/joakimskurk/fm1juno/actions/runs/42312573688).
  Neither result is physical PIT/audio proof.
- These 36 extended cases **are not automatically added** to
  the already reported 1,048 historical case count, which
  includes earlier distinct input profiles.

**COMPLETE:** original `03D7..0406` six-voice Portamento,
with [stock 36/36](https://github.com/joakimskurk/fm1juno/actions/runs/42312573517)
and [compiled nonzero 36/36](https://github.com/joakimskurk/fm1juno/actions/runs/42312573688).
See [dedicated handoff](PHASE1_ORIGINAL_PORTAMENTO_GATE.md).
**NEXT:** `0407..04D4` SUB DAC, PIT/8253 timer and DCO-CV
DAC paths. Keep those unverified until their own gates pass.

## Synthetic ROM relocation and limitations

The earlier CPU fixture initializes synthetic RAM at `0x0200`
and waits for original ADC ISR FF5C publication using synthetic
polling code at `0x0400`. Those locations would overlap
the newly executed authentic frame source; therefore the
**test-only synthetic** initialization is relocated to a
public-disassembly NOP window starting at `0x0863`, and the
test-only polling loop moves to `0x09E0`. The CPU's reset
stub jumps to the relocated boot, whose original test
state setup and wait-on-FF5C behavior are unchanged.

The **base** 02EC source gate stops at 03A1; the optional
pitch-offset gate stops at 03D7, and the Portamento extension
stops at 0407. Only the original `0407..04D4` SUB DAC,
DCO-8253 timer updates and six DCO-CV outputs remain
unverified by the extended frame tests. The initial ISR-RETI→VCF
step still uses a declared synthetic polling/JMP bridge,
so **this new test does not remove the strict full-scheduler
or UPD7811 provenance blocker**.

## Handoff / exit conditions for the new slice

1. **COMPLETE:** source-pinned stock-MAME 36-case
   original frame entry job PASS, including all prior
   original ENV/VCF/ADC parity checkpoints.
2. **COMPLETE:** three LFO delay/holdoff/active branches,
   resonance DAC `EA=1234`, source NOP-hole pinning
   and source-authentic original frame `03A1` stop.
3. **COMPLETE:** four genuine further frame IRQs observed;
   checked CPU-stacked return PC, ANM bank, masked FAD
   status and original alternate-bank ISR/RETI.
4. **COMPLETE:** the full independent CPU matrix passed
   [42133442641](https://github.com/joakimskurk/fm1juno/actions/runs/42133442641)
   and the compiled eight-channel `fixed-eight` gate passed
   [42133442637](https://github.com/joakimskurk/fm1juno/actions/runs/42133442637).
   The distinct 36 compiled original-frame CPU cases are now
   counted, bringing the cumulative selected scenario total
   to **1,048**.
5. **COMPLETE through `0x0406`:** original master VCA,
   Tune/LFO/Bend and six-voice Portamento pass both stock
   and compiled nonzero-ADC MAME CPU gates, 36/36 per profile.
   **NEXT** original `0x0407..0x04D4` SUB DAC, PIT/DCO
   timer and DCO-CV paths with peripheral I/O assertions.
   The synthetic IRQ→`04D5` bridge remains until
   original scheduler-to-voice entry is verified.
6. Original 8253 phase, calibrated analogue voltage/real
   controller clock, real uPD7811 silicon and
   authenticated full original ROM remain OPEN.
