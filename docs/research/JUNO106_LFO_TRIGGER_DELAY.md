# JUNO-106 LFO Delay trigger and performance-modulation path

Status: Phase 0 behavioral freeze, except exact firmware-loop period.

Source basis: public module/voice CPU disassembly (`ic29.txt`), uPD7810 instruction semantics, and Roland controller specifications.

This note complements `JUNO106_LFO_PORTAMENTO.md` by focusing on *when* the LFO Delay is reset and how the front-panel/Bender/MIDI modulation path bypasses that delay.

## 1. Automatic LFO Delay is keyed from `voiceRun`, not every Note On

The module main loop first updates the six-bit `voiceRun` state from gate + sustain behavior.

The LFO-delay code then tests whether any of the six `voiceRun` bits are active.

### No running voices

When `voiceRun & 0x3f == 0`, firmware arms an LFO-delay-reset flag.

It does not need to zero the delay accumulators immediately; the important behavior is that the next transition to an active voice is marked as a fresh onset.

### First running voice after silence

When at least one voice is running and the reset/armed flag is set, firmware:

- clears the 16-bit LFO holdoff accumulator;
- clears the 16-bit LFO delay-ramp accumulator;
- clears the holdoff/ramp-complete phase bits;
- begins a new Delay sequence.

### Continued/legato playing

As long as at least one `voiceRun` bit remains active, subsequent Note On events do not re-arm the automatic LFO Delay.

Consequences:

- ordinary legato playing does not restart LFO Delay;
- overlapping polyphonic notes do not restart it;
- sustain can keep `voiceRun` active after keys are released, preventing a new Delay reset;
- a new Delay cycle requires the running-voice set to become empty first.

This should be reproduced explicitly in strict mode.

## 2. Two-stage Delay algorithm

The Delay parameter controls two digital stages.

### 2.1 Holdoff stage

The current 16-bit holdoff accumulator is incremented by a coefficient selected from the same 128-entry Attack-increment family used by the digital envelope.

The Delay parameter indexes this holdoff coefficient directly.

Conceptually:

```text
holdoff += holdoff_inc[delay_value]
if holdoff < 0x4000:
    lfo_onset = 0
else:
    enter_ramp_stage()
```

Until the holdoff threshold is reached, the normal panel LFO contribution has zero onset depth.

### 2.2 Ramp stage

The Delay parameter also chooses one of eight 16-bit ramp increments.

The ROM derives the table byte offset from the upper Delay bits, equivalent to an 8-bin index:

```text
ramp_index = (delay_value >> 4) & 0x07
```

The ramp accumulator adds the selected coefficient. Its high byte becomes the 0..255 onset-depth multiplier.

Conceptually:

```text
ramp += ramp_inc[ramp_index]
if no overflow:
    lfo_onset = high_byte(ramp)
else:
    lfo_onset = 0xff
    mark_ramp_complete()
```

The ramp-rate table begins with an effectively immediate first setting and becomes progressively slower, reaching a coefficient of `0x0100` for the slowest bins.

Do not replace this with a one-pole fade.

## 3. Normal panel DCO-LFO contribution

The front-panel DCO LFO depth is converted through a depth-scaling law and then multiplied by the current LFO Delay onset amount.

Only *after that* does firmware combine the separate performance-modulation amount.

This ordering is critical:

```text
panel_depth_after_delay = panel_dco_lfo_depth * lfo_onset
combined_depth = saturating_add(panel_depth_after_delay, performance_lfo_depth)
pitch_mod = combined_depth * integer_lfo_magnitude
```

Therefore the normal panel DCO-LFO path is delayed, while the performance path below is not.

## 4. Bender LFO-trigger path bypasses LFO Delay

The bender/controller ADC set contains:

- an LFO-trigger state/value;
- an LFO Trigger Sensitivity value.

When the physical trigger is active, firmware raises an internal `modSwDeclick` value in increments of `0x0a` toward the sensitivity-controlled level. This gives a short stepped attack instead of an instantaneous jump.

When the trigger is inactive, the internal physical-trigger contribution is reset to zero.

The resulting physical performance-LFO amount is stored independently from the automatic Delay envelope.

Because this amount is added **after** the panel depth has been multiplied by the Delay onset envelope, the bender LFO-trigger path bypasses LFO Delay.

That matches the intended performance behavior: pushing the Bender/LFO trigger can introduce immediate vibrato even when a long panel LFO Delay is set.

## 5. MIDI modulation shares the performance path

The module CPU also combines MIDI modulation depth with LFO Trigger Sensitivity.

Observed arithmetic shape:

```text
midi_perf = high_byte(midi_mod_depth * lfo_trigger_sensitivity)
```

Firmware then compares the MIDI-derived performance value with the physical trigger/declick value and retains the larger contribution before storing the final performance-LFO depth.

Thus physical trigger and MIDI modulation converge into one performance path before being added to the delayed panel DCO-LFO amount.

The exact compare instruction should be retained in unit tests, but the architectural result is clear:

- panel DCO-LFO depth -> subject to LFO Delay;
- Bender trigger / MIDI modulation -> performance contribution, not subject to LFO Delay;
- both use the same underlying integer LFO waveform/rate.

## 6. Sustain interaction

Because automatic Delay reset uses `voiceRun`, and `voiceRun` retains voices while sustain is active, sustain affects LFO Delay retriggering even though it is not itself an LFO parameter.

Test case:

1. play a note and allow Delay to complete;
2. hold sustain;
3. release all keys;
4. play another note while sustain still keeps a running voice;
5. automatic LFO Delay should **not** restart from zero;
6. release sustain until `voiceRun` becomes empty;
7. next new note should start a fresh holdoff/ramp sequence.

## 7. Clean-room state model

```text
LfoDelay106
  bool armed_after_silence
  uint16 holdoff_accum
  uint16 holdoff_inc
  uint16 ramp_accum
  uint16 ramp_inc
  enum { HOLD, RAMP, FULL } phase

PerformanceLfo106
  uint8 trigger_declick
  uint8 trigger_sensitivity
  uint8 midi_mod_depth
  uint8 final_performance_depth
```

Recommended control flow:

```text
if no_voiceRun:
    armed_after_silence = true
else if armed_after_silence:
    reset_holdoff_and_ramp()
    armed_after_silence = false

tick_delay_stage()
tick_performance_trigger()
combine_depths_after_delay()
```

## 8. Verification vectors

### Automatic Delay

- first note after complete silence;
- overlapping second note;
- legato note change;
- all notes off then new note;
- sustain keeping `voiceRun` active;
- sustain release followed by new note;
- Delay settings at boundaries between each of the eight ramp bins;
- holdoff threshold crossing exactly on `0x4000` boundary;
- ramp overflow/full-scale transition.

### Performance trigger

- trigger off/on/off;
- verify upward declick steps of `0x0a` until sensitivity limit;
- verify release behavior;
- MIDI modulation only;
- physical trigger only;
- both active at unequal levels and equal levels;
- panel LFO Delay at maximum while performance trigger is activated: performance vibrato must appear without waiting for the automatic Delay.

## 9. Remaining uncertainty

- exact real-time duration of each holdoff/ramp tick until the full main-loop cadence is calibrated;
- precise ADC threshold/hysteresis that turns the physical LFO-trigger state into its active bit;
- whether the physical trigger's switch-off transition contains analogue filtering outside the observed digital declick state;
- exact MIDI preprocessing before `midi_mod_depth` reaches the module CPU.

The central LFO Delay trigger/retrigger architecture no longer requires a generic hypothesis.