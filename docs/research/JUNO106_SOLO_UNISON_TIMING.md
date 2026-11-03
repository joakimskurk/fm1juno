# JUNO-106 Solo-Unison scan, retrigger and timing model

Status: Phase 0 behavioural specification for strict `MODEL_106`.

Primary evidence:

- Roland JUNO-106 owner's/service documentation for Solo-Unison and master/slave architecture;
- `ErroneousBosh/j106roms` assigner ROM disassembly (`ic1.txt`);
- `ErroneousBosh/j106roms` module ROM disassembly (`ic29.txt`);
- MAME uPD7810 instruction semantics used to verify the skip behaviour of `EQIW`, `NEIW` and `ONAW` rather than trusting provisional disassembly comments.

## 1. Solo-Unison is six real physical voices

When both POLY 1 and POLY 2 are selected in normal play, the assigner takes a dedicated Solo-Unison path.

A normal Solo-Unison Note On:

1. adds keyboard transpose to the note;
2. selects the module board;
3. sends Voice On command `0x88`, then the note;
4. repeats for `0x89..0x8D`;
5. does **not** modify `noteTbl`;
6. does **not** reorder or use the Poly-1 `voiceTbl` queue.

A Solo-Unison Note Off sends all six physical Voice Off commands `0x80..0x85`.

Implementation consequence:

Do not implement strict Solo-Unison as one DSP voice rendered six times from a shared phase/envelope object. It is six independent physical-voice state machines receiving the same target note.

## 2. `FFD0` is a per-scan arbitration latch, not persistent note state

The assigner clears `$FFD0` to zero before each complete combined MIDI+keyboard note-map scan.

The normal Solo-Unison Note On path performs an equality test against zero:

- if `$FFD0 == 0`, the first new Note On in that scan is accepted and sent to all six voices;
- the routine then sets `$FFD0 = 0x80`;
- later Note On changes encountered during the same scan are ignored.

The Note Off path also tests `$FFD0`:

- if `$FFD0 == 0`, it sends all six Voice Off commands and sets `$FFD0 = 0xFF`;
- if another unison event has already claimed that scan, the later event is ignored.

At the end of the complete note-map scan, `NEIW $FFD0,$FF` conditionally calls `clrNoteBits` only when `$FFD0 == 0xFF`.

Therefore a handled Solo-Unison Note Off deliberately clears the assigner's combined note-history map. Any keys that remain physically/MIDI-held will appear as new Note On transitions on the next scan.

This explains the otherwise mysterious `$FFD0` behavior without treating the original human comments as authoritative.

## 3. Priority behavior

### 3.1 Sequential overlapping presses

Because `$FFD0` is reset at the start of every complete scan, a newly pressed note on a later scan is accepted even while another key remains held.

Therefore ordinary sequential playing behaves as **new-note / last-event priority**:

```text
hold C4 -> C4 sounds
press E4 while holding C4 -> E4 is sent to all six voices
press G4 while holding both -> G4 is sent to all six voices
```

Every accepted Note On also re-arms all six envelope attacks through the module CPU.

### 3.2 Multiple Note Ons in the same scan

The combined note map is traversed from its high-note end toward its low-note end, and changed bits within a byte are processed from high bit toward low bit.

Only the first Solo-Unison Note On in the scan is accepted.

Therefore truly simultaneous changes that land in one scan have **scan-order priority**, which corresponds to the highest encountered note in the current firmware traversal.

This is subtly different from a generic timestamp-based last-note stack.

### 3.3 Releasing a key while other keys remain held

The first accepted Solo-Unison Note Off in a scan:

1. sends Voice Off to all six physical voices;
2. sets `$FFD0 = 0xFF`;
3. causes the combined note-history map to be cleared at the end of that scan.

On the next scan, every still-held key is rediscovered as a fresh Note On. The first note encountered in scan order becomes the new unison note.

Strict behavioural model:

- release causes an all-six off event rather than popping a remembered last-note stack;
- remaining held notes are reconstructed from the physical/MIDI map on the following scan;
- fallback priority is therefore scan-order based, nominally highest-held-note first.

This odd edge behavior should be retained in the desktop reference model and verified against real hardware before deciding whether the FM-1 performance build may simplify it.

## 4. Mixed Note On / Note Off changes within one scan

`$FFD0` is a single arbitration latch, so event ordering matters.

- If a Note On is handled first, `$FFD0` becomes `0x80`; later unison changes in that scan are ignored.
- If a Note Off is handled first, `$FFD0` becomes `0xFF`; later changes in that scan are ignored and the combined note map is cleared afterward.

This is another reason not to replace the original assigner with a generic monophonic priority helper in strict mode.

## 5. Module-side envelope and timer behavior

Each of the six Voice On messages enters the same module-side state machine as Poly 1/2.

For every physical voice:

- `noteGate` is set;
- Attack is requested;
- Attack resumes from the current 14-bit envelope level;
- the new note is compared with that physical voice's previous note;
- oscillator reset is independent from the envelope retrigger.

The existing strict reset matrix therefore applies to Solo-Unison too:

| Per-voice condition at Voice On | 8253 control-word reset | ENV attack |
| --- | --- | --- |
| same note + running | no | yes |
| same note + not running | no | yes |
| different note + running/sustained | no | yes |
| different note + not running | yes | yes |

### First unison note from silence

When the instrument is not running and the incoming note differs from the stored per-voice note, each physical voice requests its own timer reset.

### Legato/new unison note while six voices are already running

All six voices normally preserve their 8253 phase state and receive ordinary new counts rather than control-word resets. All six envelopes still re-attack from their current levels.

This is authentic 106 behavior and must not be replaced with phase-reset-on-every-note semantics.

## 6. Serial burst timing

Both CPUs configure their internal serial link at 31,250 bit/s, 8 data bits, one stop bit.

One serial byte therefore occupies approximately:

```text
10 bits / 31250 bit/s = 320 us
```

### Solo-Unison Note On burst

The assigner queues:

- one module-board select byte;
- six Voice On command bytes;
- six note bytes.

Nominal wire time:

```text
13 bytes * 320 us = 4.16 ms
```

This is almost exactly one nominal module main/control-loop period (~4.2 ms).

### Solo-Unison Note Off burst

The assigner queues:

- one module-board select byte;
- six Voice Off command bytes.

Nominal wire time:

```text
7 bytes * 320 us = 2.24 ms
```

## 7. The six oscillator resets are not guaranteed simultaneous

`resetVoiceBits` is set by the serial ISR when a Voice On requires a reset, but the actual 8253 control-word/reset is consumed later when the main loop reaches that physical voice in the DCO programming section.

Consequences:

- six Solo-Unison reset requests arrive serially, not atomically;
- the DCO loop also programs voices sequentially;
- a burst can straddle the DCO section of two adjacent main-loop passes;
- therefore the six DCOs should **not** be assumed to restart at one identical phase/time;
- serial interrupts themselves also perturb exact loop timing slightly.

This may be sonically useful: a strict unison does not collapse six nominally identical DCOs into a perfectly phase-locked single oscillator multiplied by six.

Reference-engine rule:

Preserve separate per-voice pending reset states and apply them at each voice's scheduled DCO update slot. Do not broadcast one shared phase reset to all six voices.

## 8. Sustain interaction

Sustain is handled on the module board through `voiceRun`.

With sustain active, Voice Off clears gate state but retained `voiceRun` can keep voices in the sustained running state. A later different Solo-Unison Note On can therefore preserve timer phase while re-arming attack, exactly as described by the normal reset matrix.

Sustain behavior should be tested independently from the assigner's `$FFD0` scan arbitration.

## 9. Clean-room state proposal

```text
SoloUnisonScan106
  arbitration        // 0x00 idle-this-scan / ON_HANDLED / OFF_HANDLED

begin_note_scan():
  arbitration = NONE

on_changed_note(note, is_on):
  if arbitration != NONE:
      ignore for unison assignment
      return

  if is_on:
      send_same_note_to_six_voices(note)
      arbitration = ON_HANDLED
  else:
      send_off_to_six_voices()
      arbitration = OFF_HANDLED

end_note_scan():
  if arbitration == OFF_HANDLED:
      clear_combined_note_history()
```

This reproduces behavior without copying Roland ROM machine code.

## 10. Required tests

- first note from silence: six Voice On messages and six independent Attack requests;
- first different note from silence: all six pending timer resets set independently;
- overlapping second note: all six attacks re-arm, no timer reset if all voices remain running;
- same-note re-articulation: no timer reset;
- two new notes in one scan: verify only scan-first/highest note is accepted;
- release current note while another remains held: six Voice Offs, note history clear, held-note reconstruction next scan;
- release a non-current held note: verify actual firmware all-off/reconstruction behavior rather than generic note-stack behavior;
- mixed on/off events in the same scan in both scan-order permutations;
- repeat all relevant cases with sustain active;
- burst aligned just before vs just after the DCO-update section to test one-frame reset-slot differences.

## 11. Remaining verification

The ROM control flow is strong enough to freeze the algorithm above. Still worth measuring on real hardware or a cycle-accurate harness:

- exact real-keyboard scan order mapped to musical note numbers at the very ends of the 61-key range;
- audible timing of release/fallback when several keys are held in Solo-Unison;
- distribution of the six DCO restart times for first-note attacks;
- whether serial-ISR workload measurably changes the nominal ~4.2 ms main-loop period;
- the analogue MC5534 response to closely spaced but non-simultaneous timer reset events.
