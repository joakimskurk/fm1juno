# P1.1 — Original IC29 next-frame scheduler at 0x02EC

**State:** The bounded original **0x02EC..0x03A0** source-byte
scheduler slice is now **FOCUSED STOCK-MAME VERIFIED**:
[GitHub Actions original frame #5](https://github.com/joakimskurk/fm1juno/actions/runs/42132712384)
**SUCCESS, 36/36 CPU processes**. This extends the prior
940 selected independent MAME CPU process fixtures to **976
historically verified cases** (stock-MAME 02EC extension adds
36; compiled nonzero follow-ons remain separately pending).
The overall P1.1 strict gate remains OPEN; this is
**not** an authenticated Roland ROM, original UPD7811 silicon,
physical CPU clock/ADC voltage, complete frame scheduler or audio.

## Confirmed prerequisite

- The separately compiled eight-channel test MAME
  and genuine ADC IRQ → ISR/RETI → six original ENV/VCF →
  bank-selected conditioner → original post-ADC DI/SKIT FAD/EI,
  VCA, LFO/PWM, noise and 0x07B5 JMP have separate verified evidence.
  The selected historic repertoire of **940** original-byte
  MAME CPU process scenarios remains the last verified count.
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
  additionally runs this scheduler test using the
  independently compiled `mame0285` Juno-106 binary
  with eight distinct nonzero analogue callbacks,
  `--input-profile fixed-eight`. This is an
  **additional 36-case test pending its own green result**
  and must not be added to historical selected counts yet.

No standalone full original scheduler LFO/PWM or resonance
native host parity oracle exists for this whole 181-byte
segment; the branch and source-state invariants are
currently checked against independent MAME CPU execution
and source-derived expectations only.

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

The execution boundary intentionally stays at source `0x03A1`:
the rest of original frame logic (`0x03A1..0x04D4`)
including tune/LFO/bend construction, portamento,
DCO-8253 timer updates and six pitch/CV outputs remains
unverified by this new test. The initial ISR-RETI→VCF
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
4. **NEXT:** confirm *complete combined matrix*
   containing the new stock gate is green, and separately
   confirm the compiled eight-channel `fixed-eight`
   version before adding its distinct 36 scenarios.
5. **NEXT original-byte slice:** extend `0x03A1..0x04D4`
   (tune/LFO/bender, portamento and six PIT/DCO
   timer/CV updates) with original source bytes
   and deterministic analogue/PIT fixture state.
   The synthetic IRQ→`04D5` bridge remains until
   original scheduler-to-voice entry is verified.
6. Original 8253 phase, calibrated analogue voltage/real
   controller clock, real uPD7811 silicon and
   authenticated full original ROM remain OPEN.
