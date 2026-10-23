# JUNO-106 chorus — Phase 0 topology, clock and calibration specification

Status: Phase 0 reference specification. JUNO-106 is authoritative. JUNO-6/60 measurements are method/reference material only unless a 106-specific source independently agrees.

## 0. Evidence policy

Confidence labels used here:

- **PRIMARY-CONFIRMED** — Roland JUNO-106 service/owner documentation or literal 106 hardware topology.
- **106-HARDWARE-DERIVED** — measurement or engineering work explicitly made from a physical JUNO-106.
- **SCHEMATIC-DERIVED** — follows from the 106 schematic/topology, but exact numeric fitting still requires measurement.
- **SECONDARY/CROSS-CHECK** — technically useful third-party work, not sufficient by itself to freeze strict constants.
- **OPEN** — keep configurable and supply an explicit verification test.

Do not import Juno-6/60 I/II rates, center delay, bandwidth or I+II behavior as strict 106 truth merely because the BBD family is shared.

## 1. Placement in the 106 signal path

**PRIMARY-CONFIRMED**

The global path is:

```text
six voice VCA outputs
    -> voice summing
    -> global HPF 0/1/2/3
    -> global VCA / level path
    -> chorus section on JACK BOARD
    -> stereo output / mono-normalled output path
```

The chorus is therefore a global post-voice effect. It must not be instantiated per voice.

Strict engine implication:

```text
mono_pre_chorus = GlobalOutput106(...)
(left, right) = Chorus106(mono_pre_chorus, mode)
```

## 2. Physical BBD architecture

**PRIMARY-CONFIRMED**

The 106 chorus uses:

- 2 x Panasonic MN3009 BBD delay ICs;
- 2 x MN3101 BBD driver/clock ICs;
- two physical chorus channels;
- separate bias adjustment for the two BBD paths;
- surrounding analogue filters, transistor stages and op-amp/buffer/mix circuitry;
- dedicated chorus LFO circuitry on the jack board;
- chorus modes OFF, I and II in the original user interface.

Roland's service procedure exposes two BBD-channel test points and separate bias trimmers:

```text
TP1 / VR1 -> chorus channel 1
TP2 / VR2 -> chorus channel 2
```

The service test feeds a 10 Vp-p / 1 kHz sine into the module-board signal path and adjusts each BBD channel so positive and negative clipping halves are symmetrical. This is strong evidence that BBD bias/headroom and nonlinear clipping symmetry belong in the physical model.

## 3. Stereo modulation relationship

**106-HARDWARE-DERIVED / SCHEMATIC-DERIVED**

Independent 106-focused engineering sources agree that the two BBD clocks are modulated in opposite phase, producing the classic stereo spread.

Reference behavior:

```text
mod_L(t) =  +m(t)
mod_R(t) =  -m(t)
```

Nominal relative modulation phase:

```text
180 degrees
```

This is not merely output panning. The two physical delay times move oppositely.

Strict model therefore requires two independently addressable delay/clock states even if they share one logical LFO source.

## 4. Dry/wet routing

**106-HARDWARE-DERIVED / SECONDARY-CROSS-CHECK**

The 106 chorus is a dry + delayed stereo chorus, not a 100%-wet dual delay.

Two useful engineering references converge on this:

- Florian Anwander's Roland chorus survey describes the Roland mono-to-stereo topology as the dry signal being mixed to both channels while the two BBD paths receive opposite modulation.
- Alpes Machines, after building/calibrating a hardware One-O-Six chorus against a real JUNO-106, documents the original insert balance as approximately **55% dry / 45% wet**.

Working strict-reference default:

```text
dry_gain_nominal = 0.55
wet_gain_nominal = 0.45
```

but these values remain **measurement-calibration defaults**, not sacred normalized DSP percentages. The actual analogue summers may have different voltage gains while producing approximately this perceptual/amplitude ratio.

Important implementation rule:

- do not renormalize dry+wet to unity after the fact;
- preserve the physical summing gain/headroom and calibrate against real output levels;
- OFF must bypass/remove the BBD contribution according to original routing rather than merely setting modulation depth to zero.

## 5. LFO / clock source

### 5.1 What is known

**PRIMARY-CONFIRMED**

The 106 provides two chorus speed modes, I and II. The jack board contains dedicated chorus-LFO circuitry controlling the two BBD driver paths.

**106-HARDWARE-DERIVED**

SoundForce sampled the BBD driver signals directly from a physical JUNO-106 over several periods for its Chorus 6/uChorus products. Its engineering description states that the original driver modulation follows a basic triangle-LFO shape, with the captured waveform retained specifically to preserve real imperfections rather than replacing it with a mathematically ideal triangle.

This is important evidence for model shape:

```text
logical modulation ~= triangle
actual clock trajectory = triangle-like + hardware asymmetry/curvature/imperfection
```

### 5.2 What is not yet frozen

Roland's 106 service notes do not publish numeric I/II LFO frequencies.

Florian Anwander explicitly notes the absence of 106 numeric rates and only *assumes* similarity to the 60. Therefore:

```text
Juno-60 Mode I ~= 0.5 Hz
Juno-60 Mode II ~= 0.8 Hz
```

are **not strict 106 constants**.

Alpes Machines exposes a continuously adjustable clone LFO from 0 to 1.8 Hz and marks physical knob positions corresponding to 106 I and II, but does not publish those preset frequencies numerically.

Strict implementation should therefore use:

```text
chorus_I_rate_hz  = configurable measurement constant
chorus_II_rate_hz = configurable measurement constant
```

until direct 106 waveform/clock captures close this item.

## 6. Model clock, not only delay time

The MN3009 is a bucket-brigade device. The source mechanism is clocked charge transfer, not a continuously addressable digital delay.

Preferred reference abstraction:

```text
ChorusClock106
  phase
  nominal_clock_hz
  modulation
  asymmetry
  jitter/noise terms if measured

Bbd106
  256 bucket-equivalent stages
  clock_state
  input_filter
  bucket transfer / interpolation model
  output_filter
  bias
  headroom
```

For an efficient FM-1 implementation it is acceptable to render an equivalent fractional delay rather than literally shifting 256 analogue buckets per clock edge, provided the conversion remains tied to the physical clock law and is validated against:

- center delay;
- min/max delay;
- modulation trajectory;
- clock-dependent bandwidth;
- phase/pitch behavior;
- bias/headroom;
- noise.

A generic `delay_ms = center + depth * sin()` block is not enough for the strict reference engine.

## 7. Delay law

For a BBD with `N = 256` stages, delay is inversely related to the effective two-phase transfer clock. The exact convention depends on whether a measured clock frequency denotes one phase, the full two-phase transfer rate, or the MN3101 oscillator reference.

Therefore the code should make the convention explicit instead of burying a factor of two:

```text
BbdClockConvention
  DRIVER_OSC_FREQ
  PHASE_CLOCK_FREQ
  EFFECTIVE_BUCKET_TRANSFER_FREQ
```

and convert through one documented function:

```text
delay_seconds = bbd_delay_from_clock(clock_value, convention, 256)
```

**OPEN:** measure/confirm the exact TP/driver signal definition used for the 106 clock captures before freezing the numeric factor.

## 8. I and II are clock presets, not separate chorus algorithms

The evidence supports one physical dual-BBD chorus whose LFO/clock operating point changes between I and II.

Strict architecture:

```text
Chorus106Mode
  OFF
  I
  II
```

Both I and II reuse:

- the same two BBD paths;
- the same analogue signal filtering;
- the same dry/wet topology;
- the same stereo anti-phase relationship;
- the same bias/headroom model.

Mode-specific data should initially be limited to measured clock/LFO preset state unless evidence proves a mode-dependent gain/filter change.

## 9. Original 106 I+II behavior

The original 106 user interface specifies OFF / I / II. Unlike JUNO-6/60, the 106 documentation does not define the old analogue I+II fast-mode circuit as a normal original mode.

Florian Anwander specifically notes that the 106 no longer provides the Juno-6/60 I+II high-speed behavior or the associated inversion change.

Modern Roland products sometimes expose a modelled `I+II` option under a "JUNO-106 Chorus" effect, but this is not sufficient evidence that an original 1984 JUNO-106 had a documented strict hardware I+II mode.

Policy:

- `MODEL_106_STRICT`: OFF / I / II only;
- `DEEP`: may expose I+II experiments explicitly labelled non-strict;
- 6/60 I+II modes may later appear as explicit 6/60-derived alternatives, never silently as 106 behavior.

## 10. Filtering around the BBDs

**PRIMARY-CONFIRMED / 106-HARDWARE-DERIVED**

The 106 schematic and modern hardware reconstructions both preserve analogue filtering before and after the MN3009 paths to suppress BBD alias/clock artifacts and reconstruction residue.

SoundForce states that its 106-derived chorus uses the same audio-path concept and retains filters both before and after the BBDs.

Strict model must therefore expose at least:

```text
pre_bbd_filter
clock_dependent_bbd_bandwidth
post_bbd_filter
```

Do not reuse the KR106/Juno-6 measured `~14 kHz one-pole` result as a 106 constant. KR106's published chorus measurement document is explicitly based on a Juno-6 hardware recording and is only a measurement-method reference here.

**OPEN:** derive 106 RC transfer functions from the jack-board schematic and confirm them with a sweep through a real 106 chorus.

## 11. Bias and nonlinearity

Roland's bias adjustment demonstrates that the BBD operating point is deliberately centered for symmetrical clipping.

Candidate physical model:

```text
x_pre = pre_filter(input)
x_bias = x_pre + bias_offset
x_clip = bbd_input_nonlinearity(x_bias, headroom)
y_bbd  = clocked_delay(x_clip)
y_post = post_filter(y_bbd - output_bias)
```

Strict factory-new default should place the two channels at their calibrated symmetrical-bias points.

DEEP/AGE may later expose:

- CH1/CH2 bias mismatch;
- reduced headroom;
- increased clock feedthrough;
- increased BBD noise;
- left/right gain mismatch.

These should be plausible circuit parameters, not generic random stereo drift.

## 12. Chorus noise

The MN3009 paths are a known source of audible noise, and aging/failing MN3009s can become substantially noisier. Modern Roland 106-chorus models explicitly expose a separate chorus-noise parameter, which supports the audible importance of this component, but their parameterization is not a numeric original-hardware reference.

Strict model should distinguish:

1. synthesizer source noise (the front-panel NOISE generator before VCF);
2. chorus BBD/device noise after the global synth path;
3. possible clock feedthrough/heterodyne artifacts.

These must not be represented by one shared white-noise source.

Noise should preferably be produced per BBD channel with any measured correlation/common-rail component modeled separately.

## 13. Mono output behavior

Alpes Machines documents the 106-style behavior that when no right output is connected, left and right are summed to the mono output.

FM-1 does not have to reproduce physical jack-normal sensing internally unless the platform exposes an equivalent condition, but the reference renderer should provide both:

```text
render_stereo() -> L, R
render_mono_reference() -> original-style L/R sum law
```

This is useful for A/B validation against mono recordings of real hardware.

## 14. Measurement plan — physical JUNO-106

A single good 106 measurement session can close most remaining chorus uncertainty.

### 14.1 BBD driver clocks

Probe the two MN3009/MN3101 clock paths in modes I and II.

Capture at >= 2 MS/s if possible.

For each channel/mode record:

- instantaneous clock period vs time;
- min clock frequency;
- max clock frequency;
- mean/center clock frequency;
- LFO period/rate;
- rising vs falling slope symmetry;
- channel-to-channel phase relation;
- clock waveform duty/asymmetry;
- repeat-period imperfections.

This is higher priority than estimating rate from an audio recording.

### 14.2 Absolute delay

Inject an impulse or broadband MLS/sweep and record dry, L and R simultaneously.

Recover:

- center delay;
- min/max delay;
- L/R opposition;
- whether center delay differs between channels;
- delay-vs-clock calibration.

### 14.3 Dry/wet gain

With a stable sine and chorus enabled:

- measure direct-path gain;
- measure isolated BBD-path gain if circuit access permits;
- otherwise fit dry/wet from two-channel transfer measurements;
- verify/replace the current 55/45 working default.

### 14.4 Filtering

Sweep at several fixed points in the LFO cycle or temporarily hold/inject clock if practical.

Record:

- pre-BBD response;
- BBD+post response;
- clock-dependent high-frequency loss;
- low-frequency coupling/high-pass behavior;
- any measurable channel difference.

### 14.5 Bias/headroom

Follow Roland's bias test and then deliberately offset around the calibrated point.

Measure:

- positive/negative clipping thresholds;
- soft/hard character;
- THD vs input amplitude;
- dependence on BBD clock rate;
- channel mismatch.

### 14.6 Noise / clock artifacts

No input signal, chorus I and II separately:

- RMS/A-weighted noise;
- spectrum to >= 48 kHz if equipment permits;
- clock feedthrough peaks;
- mode-dependent noise change;
- L/R correlation.

## 15. Desktop reference implementation proposal

First reference version should deliberately favor observability over optimization:

```text
Chorus106
  Mode mode
  ChorusClock106 clock_l
  ChorusClock106 clock_r
  BbdPath106 left
  BbdPath106 right
  DryWet106 mix

process(x):
  if mode == OFF:
      return bypass_route(x)

  clock = lfo_clock_model(mode)
  left.set_clock(clock.plus)
  right.set_clock(clock.minus)

  wet_l = left.process(x)
  wet_r = right.process(x)

  return analog_sum(dry_gain*x, wet_gain*wet_l),
         analog_sum(dry_gain*x, wet_gain*wet_r)
```

Keep diagnostic switches for:

- ideal triangle vs captured/fitted clock trajectory;
- ideal fractional delay vs clock-derived BBD model;
- filters on/off;
- noise on/off;
- nonlinearity on/off;
- exact 180° vs measured channel relation.

These are A/B research controls and should not become user-facing clutter in the final FM-1 instrument.

## 16. FM-1 implementation strategy

The chorus is global, so unlike the VCF it is not multiplied by six voices. This makes a deeper model affordable if memory is controlled.

Preferred optimization ladder:

1. two fractional delay lines driven by physical clock-to-delay law;
2. low-order IIR pre/post filters fitted to 106 hardware;
3. cheap per-channel BBD soft nonlinearity;
4. deterministic low-cost BBD/clock noise model;
5. optional clock-trajectory LUT for I and II captured/fitted from real 106;
6. only emulate individual bucket stepping if profiling proves audible benefit worth the CPU cost.

At 44.1 kHz, a maximum chorus delay below ~10 ms requires well under 1 ksample per channel, so delay RAM itself should be modest. Exact allocation waits for measured max delay plus interpolation guard/history.

## 17. Validation vectors

Required automated/reference renders:

- impulse, I and II, stereo;
- 100 Hz / 1 kHz / 8 kHz sine, I and II;
- full-band noise sweep for transfer function;
- loud 1 kHz sine at nominal bias and +/- bias errors;
- silence for noise/clock spectrum;
- stereo correlation and instantaneous delay difference over one full LFO period;
- OFF-vs-I/II level comparison;
- mono-sum reference output.

Acceptance metrics should include:

- LFO rate error;
- clock min/max error;
- delay center/depth error;
- L/R phase error;
- dry/wet gain error;
- magnitude-response error;
- THD/headroom error;
- noise-spectrum error.

## 18. Freeze state

### Frozen enough for architecture

- one global post-HPF/post-global-VCA chorus;
- two MN3009-equivalent BBD paths;
- two MN3101-equivalent clock/driver paths;
- shared triangle-like modulation with opposite channel polarity;
- nominal 180° stereo modulation;
- dry signal mixed into both outputs plus delayed path;
- working 55/45 dry/wet calibration hypothesis from 106-focused hardware reconstruction;
- OFF / I / II strict modes;
- separate per-channel bias/headroom;
- pre/post BBD filtering required;
- BBD noise/clock artifacts separate from synth NOISE source;
- clock-domain representation preferred over arbitrary delay-ms LFO.

### Still open before strict numeric freeze

- exact 106 Mode I rate;
- exact 106 Mode II rate;
- full driver-clock trajectory for both modes;
- clock convention/factor used for delay conversion;
- center/min/max delay per channel;
- exact 106 dry/wet analogue gains;
- exact pre/post filter transfer functions;
- BBD bandwidth vs clock;
- calibrated headroom/nonlinear curve;
- factory-new BBD noise spectrum and clock feedthrough;
- exact mono-output summing gain;
- whether any undocumented physical two-button state is stable/repeatable enough to preserve as a DEEP curiosity.
