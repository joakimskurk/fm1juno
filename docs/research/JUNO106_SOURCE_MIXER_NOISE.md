# JUNO-106 source mixer, SUB, PWM and noise architecture

Status: Phase 0 source-path specification for strict `MODEL_106`.

This document freezes what can already be established about the signal presented to each 80017A VCF and deliberately leaves exact passive mixer gains/headroom open until the module-board schematic is read cleanly or measured.

## 0. Evidence classes

- **PRIMARY-CONFIRMED** — Roland service documentation or literal module ROM behavior.
- **HARDWARE/REPAIR CROSS-CHECK** — direct repair/testing evidence on real JUNO-106 hardware.
- **DERIVED** — follows from confirmed digital/analogue relationships.
- **OPEN** — must not be guessed in strict mode.

## 1. The source path is not four independent normalized DSP oscillators

A common VA abstraction would be:

```text
mix = saw_gain*saw + pulse_gain*pulse + sub_gain*sub + noise_gain*noise
```

with four unrelated ideal sources normalized to similar digital amplitudes.

That is not a faithful representation of the 106.

The physical architecture is better represented as:

```text
8253 timing
   |
   +--> MC5534 ramp/PWM section --> WAVE output ----+
   |                                                |
   +--> MC5534 divide-by-two --> SUB output --------+--> per-voice source/input network --> 80017A VCF
                                                    |
common noise generator -> noise VCA -> LP shaping --+
```

The exact external passive summing/gain network remains to be numerically frozen, but the source identities and control structure are already clear.

## 2. MC5534 WAVE output

**PRIMARY-CONFIRMED / HARDWARE-CROSS-CHECK**

Roland describes MC5534 as generating:

- divided-by-two rectangular SUB;
- sawtooth;
- variable-width rectangular/PWM.

Repair probing of original 106 hardware identifies the two WAVE outputs of a dual MC5534A as carrying the **saw + PWM combination**, while the two SUB outputs are separate pins.

Therefore strict architecture should expose per physical voice:

```text
Wave5534Voice106
  wave_out   // physical WAVE node: saw contribution + pulse contribution
  sub_out    // separate divided-by-two source
```

rather than pretending SAW and PULSE have completely unrelated oscillator state.

They are still separately switchable at the instrument level:

- SAW is controlled through the switch/latch path into the MC5534 saw-enable/gate;
- PULSE/SQUARE is disabled by driving the PWM-control path to the comparator-off condition.

The two components ultimately share the same physical ramp/timing system.

## 3. SAW switching

The module ROM receives a SAW on/off bit in the switch data and folds it into the external latch used by the module board.

Hardware repair evidence identifies the corresponding MC5534 saw-enable/gate node as common to the chips and high when saw is off.

Strict rule:

- SAW OFF should suppress the saw contribution at the physical WAVE generator path;
- it should **not** stop the 8253 timer, ramp timing, PWM comparator operation or SUB timing state.

This matters for a pulse-only patch: the physical saw/ramp can still exist internally because PWM depends on it even while the audible saw contribution is gated off.

## 4. PULSE/PWM control law

### 4.1 Firmware representation

**PRIMARY-CONFIRMED from module ROM**

Panel PWM depth arrives as a 7-bit patch parameter and is doubled for internal use:

```text
pwm_depth = raw7 << 1     // nominal 0..254
```

In MANUAL mode, the firmware sets a full-scale reference `0x3FFF`, multiplies it by `pwm_depth`, and subtracts the result from `0x3FFF`:

```text
scaled = floor(0x3FFF * pwm_depth / 256)
pwm_code = 0x3FFF - scaled
```

When the PULSE/SQUARE source is switched off, firmware forces:

```text
pwm_code = 0
```

The resulting code is output through the shared DAC/S&H PWM channel.

### 4.2 Analogue anchors

**PRIMARY-CONFIRMED from Roland service description**

The MC5534 comparator behavior is specified approximately as:

```text
PWM CV ~= +6.0 V  -> 50% duty
PWM CV ~= +0.6 V  -> 95% duty
PWM OFF ~= -0.8 V -> comparator held High; rectangular output disabled
```

Factory adjustment verifies:

```text
50% target: 48..52%
PWM slider 10: 93..97%
```

### 4.3 Numerical firmware/analogue cross-check

The ROM comment notes that the physical PWM control range is limited to roughly 0..100 by the panel resistor network before the firmware's doubling.

At physical maximum:

```text
raw ~= 100
pwm_depth ~= 200
scaled = floor(16383 * 200 / 256) = 12799
pwm_code = 16383 - 12799 = 3584
```

If the conditioned PWM path is approximately linear between the two strong endpoint anchors:

```text
code 0      -> -0.8 V
code 16383  -> +6.0 V
```

then code 3584 predicts:

```text
PWM CV ~= +0.69 V
```

which is strikingly close to Roland's `~+0.6 V -> ~95%` statement.

This strongly corroborates the firmware interpretation and the use of a linear held-CV transfer for the first reference model.

### 4.4 Strict implementation consequence

Do not map the UI PWM slider directly to a mathematically ideal duty-cycle percentage.

Instead:

```text
panel/raw -> firmware integer law -> held PWM CV -> comparator against physical ramp -> duty
```

Duty emerges from the ramp amplitude/shape and the held comparator threshold.

## 5. SUB level law

### 5.1 Digital control

**PRIMARY-CONFIRMED from ROM**

SUB LEVEL uses the same 7-bit-to-14-bit storage operation as several other controls:

```text
sub_code = raw7 << 7
```

so the internal target covers:

```text
0x0000 .. 0x3F80
```

The code is output through the dedicated SUB LEVEL S/H slot before the six DCO service slots.

### 5.2 Analogue mechanism

**PRIMARY-CONFIRMED from Roland**

Roland describes SUB amplitude as varying with a change of the SUB oscillator transistor's collector supply voltage.

The conditioned service-domain label groups DCO CV / SUB LEVEL in a nominal:

```text
0 .. -10 V
```

range.

Therefore the first reference model should not assume SUB LEVEL is simply a linear post-generator digital gain multiplier.

Architecture:

```text
sub_level_code
  -> held analogue control voltage
  -> MC5534/sub transistor collector-supply/amplitude behavior
  -> sub_out
```

It may become numerically close to linear over useful range, but that must be measured/derived rather than imposed.

### 5.3 Global control, independent phases

There is one SUB LEVEL control target shared by all six voices, while each physical voice has its own divide-by-two timing state.

Therefore:

- all six voices receive the same nominal SUB amplitude control;
- they do not share one SUB oscillator phase;
- unison phase behavior follows each physical DCO/timer state.

## 6. One common physical noise generator

**PRIMARY + HARDWARE/REPAIR CROSS-CHECK**

The module parts list identifies a selected `2SC945P` transistor specifically for the noise generator.

Hardware/repair documentation confirms there is **one** noise generator in the JUNO-106, not one per voice.

This is a critical modeling rule.

Strict implementation must generate one common continuous noise waveform:

```text
noise_common(t)
```

and distribute that same instantaneous waveform into the six voice input paths.

Do **not** instantiate six unrelated PRNG/noise generators.

This means that when a six-note chord is played with NOISE only, all six VCFs are processing the same underlying stochastic waveform at that instant, with differences caused by their own filter/VCA states and calibration.

## 7. Noise source and bandwidth

### 7.1 Physical source

The original noise source uses a selected transistor; selection quality matters because Roland wanted a reasonably flat/useful noise spectrum.

A deterministic digital white-noise source is acceptable as a reproducibility mechanism for tests, but the spectral model should be calibrated to the analogue generator/filter system rather than called 'white noise' and stopped there.

### 7.2 Band limiting

Repair engineering on original units identifies `C41` in the RC network **after the noise VCA** as limiting the noise bandwidth to approximately:

```text
~5 kHz
```

Aged/incorrect C41 can widen the spectrum substantially.

Reference architecture:

```text
selected_transistor_noise
 -> noise VCA
 -> ~5 kHz analogue RC shaping
 -> distribution to voice source mixers
```

Exact C41/R partner values remain open until clean primary schematic extraction; the approximate 5 kHz behavior is strong enough to define a measurement/fitting target.

## 8. NOISE LEVEL control law

**PRIMARY-CONFIRMED from ROM**

Like SUB, NOISE LEVEL is stored as:

```text
noise_code = raw7 << 7
```

covering:

```text
0x0000 .. 0x3F80
```

The current value is output near the end of each module main loop to the dedicated noise-level DAC/S&H channel.

Roland's conditioned control-domain table identifies NOISE LEVEL in the nominal:

```text
0 .. +10 V
```

class.

Again, the digital slider law is simple; any actual amplitude curvature/saturation belongs to the analogue noise-VCA/source network.

## 9. Factory NOISE calibration gives an absolute signal anchor

**PRIMARY-CONFIRMED**

Roland's adjustment procedure requires VCA Gain calibration to be completed first, then:

```text
BANK: 6
POLY 1
hold any key
measure TP8 (voice 1 post-VCA point)
adjust VR32 for 4 Vp-p noise
```

This is a much stronger reference than arbitrarily normalizing noise to unity.

Important nuance:

- the 4 Vp-p point is measured after the selected voice's VCF/VCA signal path under the Bank-6 test setup;
- it is not necessarily the raw noise-generator or pre-filter mixer amplitude;
- the eventual model must reproduce the factory test configuration before using 4 Vp-p as a calibration criterion.

## 10. Noise level is global, voice VCF/VCA is per voice

There is one firmware NOISE LEVEL S/H channel and one physical noise generator/noise VCA.

Therefore the most plausible strict topology is:

```text
common noise generator
 -> globally controlled noise amplitude
 -> global noise bandwidth shaping
 -> fan-out to six voice source/input networks
 -> six independent 80017A VCF/VCA chains
```

This matches real-unit test behavior: a dead common generator causes no noise from any voice; a failed individual voice path can remove noise from only that voice.

## 11. Mixer headroom must not normalize source combinations

Strict model should preserve hardware-voltage-like source amplitudes through the source/input network.

Do not do this:

```text
mix /= number_of_enabled_sources
```

and do not automatically constrain the sum to `[-1,1]` before the VCF.

Why:

- saw/pulse source amplitude is physically around the ~12 Vp-p design region;
- SUB is a separate physical source with its own level control;
- noise has an independently calibrated maximum;
- the input network and 80017A can therefore see different drive depending on which sources are enabled and at what levels;
- filter/VCA nonlinearity should respond to that real drive change.

The exact source resistor ratios and resulting 80017A input voltage are still OPEN and will be frozen from schematic/measurement.

## 12. Provisional clean source API

```text
Noise106
  rng_state
  transistor_shape_state
  noise_vca_control
  rc_bandlimit_state

  render_common_noise()

Wave5534Voice106[6]
  render_wave_node()   // saw gate + PWM comparator contribution
  render_sub_node()    // independent per-voice /2 state, shared level control

SourceMixer106[6]
  process(wave_node, sub_node, common_noise)
  // real resistor/gain/headroom network, not normalized sum
```

The API intentionally combines saw/pulse as a WAVE node at the hardware boundary while retaining separate logical controls before that node.

## 13. Secondary hardware pinout evidence

Recent real-unit repair probing reports for MC5534A:

```text
pins 1/4   DCO clock inputs
pins 2/5   DCO CV inputs
pins 13/15 SUB outputs
pins 14/16 saw+PWM WAVE outputs
pin 12     common PWM CV
pin 17     saw on/off gate; high when saw is off
```

This is highly consistent with the service schematic and firmware control paths, but remains tagged as repair/community evidence rather than primary Roland pinout documentation.

## 14. What is already frozen

- SAW and PULSE share the MC5534 ramp/timing system.
- The useful external abstraction is a per-voice WAVE node plus separate per-voice SUB node.
- SAW gate and PULSE/PWM disable are distinct controls.
- PWM must be generated through the recovered integer code law and held comparator CV, not direct ideal duty mapping.
- SUB and NOISE panel parameters both use `raw7 << 7` firmware storage.
- SUB level is one global control applied to six separate physical sub oscillators.
- Noise is **one common continuous source**, not six independent generators.
- Noise level is globally controlled before distribution to the six voice paths.
- Noise bandwidth target is approximately 5 kHz after its VCA/shaping network.
- Factory maximum-noise calibration target is 4 Vp-p at TP8 under Bank-6 test conditions.
- Source sums must preserve drive/headroom and must not be normalized by enabled-source count.

## 15. Still open

- exact WAVE output electrical mix of saw and pulse inside each MC5534 revision;
- exact per-voice WAVE-to-80017A input resistor/gain;
- exact per-voice SUB-to-80017A input resistor/gain;
- exact common-noise fan-out/input resistor/gain;
- source-node DC offsets and coupling capacitors;
- maximum pre-VCF voltage and clipping mechanism;
- SUB transistor amplitude-control law vs held SUB LEVEL voltage;
- noise-VCA transfer curve vs NOISE LEVEL voltage;
- exact noise RC values and spectrum including low-frequency shape;
- whether the 80017A input network itself provides measurable source-dependent loading before OTA nonlinearity.

## 16. Measurement plan for exact mixer freeze

For one calibrated physical voice, preferably CH1:

1. **SAW only** — 8', C4, resonance 0, filter open; capture MC5534 WAVE pin and 80017A input/test-accessible equivalent.
2. **PULSE only** — PWM 50%, same note; capture WAVE node and downstream level.
3. **SAW + PULSE** — compare node peak/rms to individual cases; look for internal/output headroom interaction.
4. **SUB only** — slider 10; capture MC5534 SUB pin and downstream input.
5. Sweep SUB slider 0..10 and fit amplitude vs nominal held-control voltage.
6. **NOISE only** — after correct VR32 calibration; capture common generator/noise-VCA output if accessible and TP8.
7. Sweep NOISE slider and fit control law/spectrum.
8. **All sources** — observe maximum source-mixer/VCF input excursion and any clipping/compression.
9. Repeat representative combinations with resonance/cutoff positions chosen to expose input-drive-dependent filter behavior.

Until these ratios are frozen, Phase-1 reference code should keep each source in hardware-voltage units with explicit configurable gain constants rather than prematurely normalizing them.
