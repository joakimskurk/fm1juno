# P1.1 — Original IC29 next-frame scheduler at 0x02EC

**State:** Source-bounded implementation and focused CI **in progress**.
The **previous original 0x0738..0x07B7 tail and 0x07B5 JMP to
0x02EC are verified**. The new **0x02EC..0x03A0** CPU execution
must not be counted in the green total until its own 36-case test passes.
P1.1 strict gate is OPEN; no original-chip cycle or audio claims.

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

1. Focused source-pinned MAME 36-case job PASS (all six
   original ENV/VCF and four ADC comparisons preserved per case).
2. Three distinct LFO delay/holdoff/active branch outcomes
   traced at original breakpoints, resonance original DAC
   call/return stack and values correct, no hidden
   original opcode substitution.
3. Independent genuine IRQ events **after the 0x02EC frame
   entry** separately checked if present. Do not count a
   debugger marker or guessed cycle duration as evidence.
4. Then append the isolated source slice to the complete
   independent CPU matrix, update documented totals and
   only subsequently extend to `0x03A1..0x04D4`.
5. Original 8253 phase, calibrated analogue voltage/real
   controller clock and authenticated full original ROM
   remain OPEN, even when this synthetic-input CPU slice
   becomes green.
