# JUNO-106 LFO and Portamento ROM control model

Status: Phase 0 reverse-engineering notes.

Source basis: literal instructions/data addresses from the public `ErroneousBosh/j106roms` module-CPU disassembly (`ic29.txt`), cross-checked against Roland service documentation. Human labels in the disassembly remain provisional where not independently supported.

## 1. LFO is firmware-generated integer control

The module ROM contains an LFO update routine beginning around `$074E`.

Confirmed state:

- `$FF4D`: 16-bit LFO magnitude accumulator;
- `$FF4B`: 16-bit rate coefficient selected from a ROM table;
- `$FF4A` bit 0: rising/falling state;
- `$FF4A` bit 1: polarity used when applying LFO to pitch/VCF/PWM-related control.

The magnitude accumulator operates over `0x0000..0x1FFF`.

### Rising half-ramp

- Add the integer rate coefficient to the accumulator.
- If result remains below `0x2000`, store it.
- If it reaches/exceeds `0x2000`, clamp to `0x1FFF` and increment the LFO flag byte.

### Falling half-ramp

- Subtract the integer rate coefficient.
- If subtraction stays non-negative, store it.
- If it underflows, clamp to `0x0000` and increment the LFO flag byte.

Because the flag byte is incremented at every endpoint:

- bit 0 toggles every endpoint and therefore controls ramp direction;
- bit 1 changes every two endpoints and therefore supplies the sign/polarity of the modulation magnitude.

The resulting signed LFO is therefore built from four integer half-ramp segments per complete signed period:

1. 0 -> max, positive
2. max -> 0, positive
3. 0 -> max, negative
4. max -> 0, negative

This produces the familiar triangle-like modulation while preserving integer stepping and endpoint clamping.

## 2. LFO rate table and high-rate quantisation

The rate control is converted through a 128-entry 16-bit ROM coefficient table beginning at `$0C60`.

The first entries are small integer steps (`0x0005`, `0x000F`, `0x0019`, `0x0028`, ...), and the coefficient grows with slider position.

Strict-mode consequence:

For a fixed control-loop period `Tloop`, a half-ramp requires approximately:

```text
half_steps = ceil(8192 / coefficient)
```

and a full signed period requires four half-ramps:

```text
period = 4 * half_steps * Tloop
frequency = 1 / period
```

The `ceil()` is important. At high coefficients, several adjacent rate-control values can collapse to the same number of control-loop steps per half-ramp, producing real frequency quantisation/plateaus. An audio-rate ideal triangle oscillator would erase this behaviour.

Exact frequency calibration remains tied to the real firmware-loop period. Roland's service timing gives roughly 4.2 ms for the multiplexed control cycle; modern direct-measurement work suggests a value around 4.23 ms, but we keep it calibratable until independently frozen.

## 3. LFO application to pitch and filter

The pitch-build routine reads the current LFO magnitude, scales it by the requested modulation depth, then uses the polarity flag to add or subtract the result in the firmware's 8.8 semitone pitch domain before the 8253 divisor conversion.

A parallel path applies the LFO to VCF control.

Model consequence:

- calculate LFO in the control domain;
- calculate signed depth-scaled pitch/VCF contributions there;
- then generate timer count / composite VCF CV;
- do not simply run an audio-rate sinusoid directly into oscillator phase and filter cutoff in strict `MODEL_106`.

## 4. LFO delay is a two-stage digital process

The module ROM does not implement LFO delay as a single generic fade.

State and tables show two stages:

### Stage A — holdoff

- current holdoff accumulator around `$FF56`;
- holdoff increment derived from the same Attack-rate table family used by the digital envelope;
- the accumulator advances until its terminal condition, producing an initial period with no LFO depth.

### Stage B — attack/ramp

- current delay-envelope accumulator around `$FF5A`;
- a separate 8-entry ramp-rate table begins at `$0B30`;
- the accumulator is incremented until full-scale and its high byte is used as the rising LFO-depth envelope.

The 8-entry ramp table begins with values including `0xFFFF`, `0x0419`, `0x020C`, `0x015E`, followed by several `0x0100` entries.

This architecture should be reproduced as two explicit digital stages. We should not ship the complete original ROM tables in FM1JUNO unless their redistribution status is deliberately resolved; the clean implementation can use independently derived constants/tests or an optional extraction/calibration workflow.

## 5. Portamento is firmware-rate linear stepping in pitch space

Portamento operates on each voice's calculated pitch state rather than on audio-rate frequency.

Confirmed flow:

1. iterate over all six voice pitch states;
2. compare current 8.8 pitch with desired note pitch;
3. fetch an 8-bit Portamento step coefficient;
4. add or subtract that coefficient from the current pitch according to direction;
5. if the step passes the target, clamp exactly to target;
6. save the updated pitch for the subsequent DCO divisor conversion.

The Portamento coefficient is selected from a 128-entry ROM table beginning at `$0A00`. The table starts with OFF/fast values and decreases toward small integer steps as the control moves toward slower glide.

Model consequence:

Portamento in strict mode is a **linear constant-step glide in the firmware's 8.8 semitone domain at control-loop cadence**, not an exponential one-pole glide in Hz evaluated every audio sample.

This is a major distinction from many VA implementations.

## 6. Implementation shape

Provisional clean-room API:

```text
Lfo106State
  uint16 magnitude;      // 0..0x1fff
  uint16 rate_coeff;
  uint8  phase_flags;    // direction + polarity bits
  uint16 holdoff;
  uint16 delay_ramp;

lfo106_tick(state)
  -> signed control magnitude

Portamento106Voice
  uint16 pitch_8_8;

portamento106_tick(current_pitch, target_pitch, step)
  -> clamped next_pitch
```

The code should reproduce observable arithmetic/behaviour, not copy original ROM machine code.

## 7. Verification tests

### LFO

- rate-table edge values;
- half-cycle sample/tick count for representative low/mid/high coefficients;
- endpoint clamp behaviour;
- direction-bit and polarity-bit sequence across four half-ramps;
- DCO pitch depth in 8.8 domain;
- VCF LFO depth path;
- holdoff duration at several Delay settings;
- delay-ramp duration and shape.

### Portamento

- upward 1-semitone, octave and multi-octave glide;
- downward equivalents;
- OFF coefficient path;
- fastest and slowest coefficients;
- exact no-overshoot clamp;
- six simultaneous voice trajectories in Poly 2;
- interaction with new note assignment/retrigger.

## 8. Remaining uncertainty

- exact full control-loop cadence and workload dependence;
- exact slider-to-table indexing/scaling in every path;
- LFO trigger/reset rules in normal play and manual-trigger mode;
- whether all LFO-derived CVs are serviced on the same effective cadence;
- detailed interaction between Portamento, voice stealing and Poly 2 release rules.
