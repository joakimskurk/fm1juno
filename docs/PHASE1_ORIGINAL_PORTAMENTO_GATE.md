# P1.1 — Original JUNO-106 Portamento CPU gate (0x03D7..0x0406)

**State: VERIFIED in stock and compiled nonzero-input MAME, 2026-11-06.**

Evidence:
- [Stock MAME — 36/36 PASS](https://github.com/joakimskurk/fm1juno/actions/runs/42312573517).
- [Compiled MAME 0.285, eight distinct nonzero ADC callbacks — 36/36 PASS](https://github.com/joakimskurk/fm1juno/actions/runs/42312573688).

Both runs continue the previous real ADC IRQ → original ISR/RETI → six original ENV/VCF voices → selected-bank ADC → post-ADC FAD and original VCA/LFO/PWM/noise → original 07B5 jump to 02EC → resonance/LFO-delay frame → master VCA/Tune/LFO/Bend → original six-voice Portamento. Both preserve **216 voice and 144 ADC comparison checks** per input profile.

## Authentic source boundary

Source: https://github.com/ErroneousBosh/j106roms/blob/master/ic29.txt

| Original range | Bytes | Pinned SHA-256 |
|---|---:|---|
| New Portamento 03D7..0406 | 48 | dc51f3151027350521d0d9a8f85c6cd9b6c2bd5a28588ab6688c69da9c799e23 |
| Complete frame 02EC..0406 | 283 | 4698319f2537aef41b0047f759abcd473b5a1288bfe75f120b6408ab447fb301 |

Runner: `tests/cpu_ic29/run_mame_original_frame_scheduler_entry.py --extended-portamento`.
Stock gate: `.github/workflows/mame-original-portamento.yml`.
Compiled nonzero gate: `.github/workflows/mame-compiled-nonzero-adc.yml`.

The original 48 bytes are SHA-pinned separately from the full 283-byte frame. The runner never substitutes synthetic control instructions inside the original slice.

## Checks executed

- Original FF0F voice index resets, DE points at FF71 and HL at FF09.
- Six current 8.8 pitches at FF71..FF7C are preserved from earlier ENV/VCF fixture, not re-seeded.
- Synthetic desired-note targets are installed at FF09..FF0E, with distinct above/below/equal relationships; the test-only FF7D seed varies over 00/01/20/80.
- Original ADC may overwrite FF7D. Differential expectations always use the **observed** coefficient from original CPU RAM at 03D7, not an assumed synthetic input.
- For each of the six original 03E0 loop entries, check the voice number, DE/HL cursor and balanced stack. For each 03EC write, compare source CPU EA and destination with an independently calculated constant-step result. Finally verify FF0F=06, DE=FF7D, SP=FFF0 and all six resulting pitch words at the 0407 stop.
- Assert the original direct, nonzero-step, ascending and descending branch paths per voice. Every 36-case profile must collectively exercise direct, step, up, down, equal, target clamp and partial movement. Both did.
- Prior CPU IRQ entry/RETI, saved return PC, ANM bank state and prior six-voice/ADC differential checks remain mandatory. They cannot be bypassed by a later successful Portamento check.

Observed coefficients: stock MAME 00/01/20/80; compiled nonzero MAME 00/01/20/67/80. The ADC-derived 67 is not a chosen fixture expectation.

**Counting:** These are longer continuations of previously chosen profiles, not 72 new independently selected input profiles. The previously reported **1,048 historical selected MAME CPU process scenarios** remains the correct cumulative accounting.

## Open limitations

The bootstrap and initial FF5C polling/JMP bridge remain synthetic and are placed in a SHA-pinned source NOP ROM window. MAME UPD7810 execution is not original UPD7811 hardware, authenticated full Roland cold-start ROM, voltage-calibrated ADC/DAC timing, original 8253 phase timing or audio proof. Strict P1.1 remains **OPEN** and PR #3 remains draft.

## Next (not yet verified)

1. Source-pin original **0407..041B**: SUB oscillator DAC call/return, PA mux, first voice index and bitmask setup. Stop at 041C.
2. Extend through **041C..04D4**: six final pitch transforms, note-index clamp and fraction interpolation, original timer-divisor table behavior, physical 8253 LSB/MSB writes, reset-bit path and six DCO-CV DAC updates. Observe actual mapped PIT/DAC I/O rather than just reaching an instruction boundary.
3. Keep the derived non-bit-exact production 8253 divisor law separate from strict original ROM-table equivalence and from physical clock/edge validation. Do not claim authentic audible JUNO synthesis yet.

No original Roland firmware is vendored in the project.