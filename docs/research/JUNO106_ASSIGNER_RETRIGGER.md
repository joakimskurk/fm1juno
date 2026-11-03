# JUNO-106 normal allocator, retrigger and oscillator-reset model

Status: Phase 0 behavioural freeze for normal JUNO-106 play modes.

Source basis:

- Roland JUNO-106 owner's manual for user-visible Assign Mode semantics;
- public `ErroneousBosh/j106roms` assigner-CPU disassembly (`ic1.txt`);
- public module/voice-CPU disassembly (`ic29.txt`);
- MAME uPD7810 core for instruction/skip semantics where loop boundaries were ambiguous.

Human labels in the public disassembly are treated as provisional. The model below is based on literal control flow and data mutation, cross-checked against the owner's manual.

## 1. Important correction: normal play has no seventh-note stealing

Strict `MODEL_106` has six physical voices.

The owner's manual explicitly states that if six keys are already held simultaneously, additional keys do not sound. The normal Poly 1 and Poly 2 assigner routines agree: when no free slot exists, the routine returns without sending another Voice On command.

Therefore:

- do **not** implement oldest-note stealing for normal Poly 1;
- do **not** implement oldest-note stealing for normal Poly 2;
- do **not** infer normal allocation from service-test rotary/non-rotary tests;
- voice stealing may exist only as a future non-authentic/DEEP option and must never leak into strict 106 mode.

## 2. Assigner data structures

The assigner CPU maintains two separate six-entry structures.

### 2.1 `noteTbl`

One byte per physical voice.

Observed semantics:

- low seven bits retain the note number associated with that physical voice;
- bit 7 set means the voice is free/off;
- bit 7 clear means that voice is currently allocated by the assigner.

Importantly, freeing a voice sets bit 7 but preserves the low seven-bit note number. Poly 1 later uses this history for same-note reuse.

### 2.2 `voiceTbl`

Six entries containing the physical voice/note-table identifiers, initialized in physical order.

Poly 1 dynamically reorders this table. Poly 2 does not use it for normal note-on allocation.

This distinction is central to reproducing the different release behaviour of Poly 1 and Poly 2.

## 3. Poly 2 allocation

Normal Poly 2 note-on scans `noteTbl` in fixed physical-voice order.

Algorithm:

```text
for physical_voice in 0..5:
    if noteTbl[physical_voice].free:
        assign requested note to this physical voice
        send Voice On to module CPU
        return
return  // all six occupied; new note is silent
```

Consequences:

- the lowest-index free physical voice is reused first;
- no release-age queue is maintained;
- a recently released low-index voice can be immediately reused by the next new note;
- its analogue/digital release tail is therefore re-articulated rather than being protected;
- this matches the owner's-manual explanation that only the final note or final simultaneously played group naturally receives its full release in Poly 2.

This behaviour also makes Poly 2 appropriate for the firmware-rate per-voice portamento implementation.

## 4. Poly 2 note-off

Normal Poly 2 note-off:

1. scans `noteTbl` for the matching active note;
2. sets bit 7 of that physical voice's `noteTbl` entry;
3. preserves the low seven-bit previous-note value;
4. sends the matching Voice Off command to the module CPU;
5. does not reorder `voiceTbl`.

There is no normal-play voice stealing in this path.

## 5. Poly 1 allocation

Poly 1 uses the dynamic `voiceTbl` order to protect recently released voices while still supporting deliberate same-note reuse.

### 5.1 Active/free ordering

A Poly 1 note-off finds the corresponding physical voice and bubbles its identifier toward the **end** of `voiceTbl` before marking its `noteTbl` entry free.

Therefore the most recently released voice moves to the back of the free region.

The free region retains note history because the note byte's low seven bits are not erased.

### 5.2 New note when at least one voice is free

Poly 1 first establishes that at least one free voice exists. If all six are allocated, it returns without sounding the new note.

When free voices exist, it searches the free region for a voice whose preserved previous note equals the requested note.

If such a voice is found:

- that same physical voice is reused;
- its identifier is bubbled back toward the active side of `voiceTbl`;
- the requested note is sent to that physical voice.

If no same-note free voice exists:

- Poly 1 chooses the free voice at the active/free boundary;
- because newly released voices are bubbled to the back, this is effectively the oldest suitable free voice rather than the most recently released one.

### 5.3 Why this matters sonically

This queue discipline explains the owner's-manual claim that Poly 1 gives more natural releases:

- a recently released voice is deliberately moved away from immediate reuse;
- older free voices are consumed first;
- recent release tails therefore tend to remain audible longer;
- same-note replay is an explicit exception, preferring the physical voice that previously produced that note.

This is not generic round-robin allocation and not oldest-note stealing.

## 6. Module CPU: Voice On always re-arms attack

For every Voice On command received by the module CPU:

1. the voice's gate bit is set;
2. the voice's Attack-phase request bit is set;
3. the new note is compared with the module CPU's stored note for that physical voice;
4. oscillator timer reset is decided separately.

The envelope and oscillator therefore do **not** share one generic retrigger decision.

A new articulation can re-arm the envelope while allowing the DCO timer phase to continue.

## 7. `voiceRun` semantics

The module main loop derives `voiceRun` from gate/sustain state:

- Sustain off: `voiceRun = noteGate`;
- Sustain on: running voices are retained by OR-ing existing `voiceRun` with current gates.

Therefore `voiceRun` is best modeled as a held/sustain-running state, not as "the analogue voice is making any sound".

A released envelope tail can still exist after `voiceRun` is clear.

This distinction is required for oscillator-reset decisions.

## 8. Oscillator reset decision matrix

The module CPU stores a previous note per physical voice and has a separate `resetVoiceBits` bitmap. That bitmap is consumed later by the 8253 programming routine.

The observed Voice On decision can be represented as:

| Incoming condition | Timer control-word reset? | Envelope attack? |
| --- | --- | --- |
| Same note, voice currently running | No | Yes |
| Same note, voice not running | No | Yes |
| Different note, voice currently running/sustained | No | Yes |
| Different note, voice not running | **Yes** | Yes |

The different-note/not-running case sets `resetVoiceBits`. Later the pitch-update routine writes the counter-specific Mode-3 control word (`36/76/B6`) before loading the new divisor.

All other cases update/reload the count without rewriting the Mode-3 control word.

### 8.1 Consequence: same-note free-run

Re-articulating the same pitch after release does not request an 8253 reset. The oscillator remains phase-continuous at the same count while the envelope re-attacks.

This is a strong reason not to reset digital oscillator phase on every MIDI Note On in the emulator.

### 8.2 Consequence: legato/sustain pitch change

If the physical voice is still `voiceRun` because it is held or sustained, assigning a different note changes the timer count without a control-word reset. The current Mode-3 cycle therefore continues into a pending new count rather than synchronizing to a fresh oscillator phase.

## 9. Envelope retrigger is from the current level

The Attack routine fetches the current 14-bit envelope value and adds the Attack increment. It does not zero the envelope before attack.

Therefore a re-articulation during decay/release performs a **current-level re-attack**.

Clean implementation rule:

```text
note_on:
    request_attack = true
    // do not force env = 0
```

The public disassembly's labels such as `rlsPhase` should not be copied literally into the clean state model. The corresponding bit is used in more than the simple human interpretation implied by its name. Implement explicit behavioural states/transitions instead.

## 10. Timer reload timing

The module has two distinct programming paths.

### Ordinary reload

- write new count LSB;
- write new count MSB;
- do not rewrite the Mode-3 control word.

8253 Mode-3 documentation indicates that a newly written count while counting becomes active at the next output transition / end of the current half-cycle. The current half-cycle therefore completes with the old count.

### Reset-and-reload

- write the physical counter's Mode-3 control word;
- load LSB then MSB;
- start a fresh programmed cycle rather than preserving the ordinary running half-cycle state.

Strict `Timer106` must therefore have separate operations:

```text
queue_count(count)       // apply at Mode-3 boundary
reset_and_load(count)    // control-word reset + fresh load
```

An audio-rate phase accumulator that instantly changes increment on every control update would not reproduce this behavior.

## 11. Clean-room allocator state proposal

```text
Assigner106
  note_slot[6]       // note + free flag per physical voice
  poly1_order[6]     // physical voice IDs; dynamic only in Poly 1
  mode               // POLY1 / POLY2 / SOLO_UNISON

PhysicalVoice106
  assigned_note
  note_gate
  sustain_running
  env_state
  timer_state
  previous_note
```

No `stolen` state is required in strict normal polyphony because a seventh simultaneous note is ignored rather than stealing one of the six voices.

## 12. Required verification tests

### Poly 1

- six-note chord followed by seventh held key: seventh produces no Voice On;
- release voices in known order and verify recently released voices move to the back of reuse order;
- play a new different note and confirm oldest appropriate free voice is selected;
- replay a note whose previous physical voice is free and confirm same-note reuse;
- verify same-note reuse does not request timer reset;
- verify release tails are preserved better than Poly 2 under identical note sequences.

### Poly 2

- verify first-free physical voice scan order;
- verify no seventh-note steal;
- release a low-index voice and immediately play a new note: verify that voice is reused;
- verify per-voice Portamento pitch state remains attached to physical voice;
- verify final note/chord receives natural release when no further allocation follows.

### Module retrigger

For each of same/different pitch x running/not-running:

- assert Attack request;
- assert expected `resetVoiceBits` state;
- assert whether timer path is ordinary reload or reset-and-reload;
- begin from a non-zero envelope and verify current-level re-attack;
- repeat with sustain held.

## 13. Remaining uncertainty

- exact interaction of Solo-Unison user mode with physical timer phase and six simultaneous module Voice On messages;
- precise timing between serial Voice On reception, next control-loop pass, envelope update and timer-count write;
- whether any obscure MIDI/sustain edge case differs from keyboard-driven normal play;
- exact analogue consequence of an 8253 control-word reset at the MC5534 input pin.

These uncertainties do not prevent freezing the normal Poly 1/Poly 2 allocation and oscillator-reset rules above.