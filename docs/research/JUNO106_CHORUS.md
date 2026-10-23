# JUNO-106 chorus — Phase 0 authoritative index

Status: Phase 0 reference specification. JUNO-106 is authoritative. JUNO-6/60 data is method/topology support only unless independently confirmed for the 106.

Detailed companions:

- `JUNO106_CHORUS_CLOCK.md` — MN3101/MN3009 clock domains, delay law, component evidence and measurement protocol.
- `JUNO106_CHORUS_FILTERS_MIX.md` — dry/wet routing, filter-component evidence, bias/headroom, noise and output-stage plan.

If an older note conflicts with these three files, this index plus the two companion documents win.

## 1. Strict physical architecture

The original chorus is a **single global stereo effect** after the six voice paths and global output section:

```text
six voice VCA outputs
    -> voice summing
    -> global HPF
    -> global VCA / patch level
    -> chorus JACK BOARD
    -> stereo / mono-normalled output
```

The chorus itself contains:

- 2 x MN3009, 256-stage BBDs;
- 2 x MN3101 clock/driver ICs;
- two independently biased wet paths;
- a common dry source mixed into both outputs;
- analogue filtering before/after the BBDs;
- opposite-polarity modulation of the two BBD clocks;
- analogue output mixing/buffering;
- BBD/device noise and clock residue distinct from the synth NOISE source.

Do not instantiate chorus per voice.

## 2. Strict mode state: OFF / I / II

Original 106 patch/SysEx switch encoding uses:

```text
chorus enable: OFF / ON
chorus preset: I / II
```

so the strict stored/user state is:

```text
OFF
I
II
```

The original owner's documentation also states that modes I and II are not used simultaneously.

Modern Roland DSP products expose an `I+II` model and describe it as both buttons pressed. That is useful information about Roland's later effect library, but it does **not** override the original 106 control encoding.

Policy:

```text
MODEL_106_STRICT : OFF / I / II
DEEP             : may expose Roland-style I+II as explicitly non-strict
MODEL_60         : may later implement the different 6/60 I+II hardware behavior
```

## 3. Stereo modulation

The two BBD delays move in opposite directions:

```text
control_A(t) ~= +m(t)
control_B(t) ~= -m(t)
```

Nominal relative control phase is 180 degrees.

SoundForce directly sampled BBD-driver signals from a physical JUNO-106 and reports a basically triangle-like trajectory whose real imperfections were worth preserving rather than replacing with an ideal triangle.

Therefore strict architecture keeps two clock states, not one delay with stereo panning.

## 4. Clock and delay semantics

Never use an ambiguous `clockHz`.

Define:

```text
f_osc = MN3101 internal oscillator frequency
f_cp  = frequency on each CP1/CP2 output
```

Component documentation gives:

```text
f_cp = f_osc / 2
```

For MN3009 `N=256`:

```text
t_delay = N / (2*f_cp)
        = 128 / f_cp
        = N / f_osc
```

The strict reference must model/fitted-calibrate:

```text
triangle-like control voltage
 -> MN3101/control network
 -> f_cp(t)
 -> reciprocal BBD delay(t)
```

not a generic `center_ms + depth*sin()` oscillator.

See `JUNO106_CHORUS_CLOCK.md`.

## 5. Clock-network evidence

Strong working original-106 values:

```text
R124 = 8.2 kOhm
R137 = 8.2 kOhm
C53  = 150 pF
C57  = 150 pF
```

The 8.2 kOhm resistors are independently supported by historical MN3009->MN3007 conversion practice. The 150 pF values are additionally supported by original-board repair evidence identifying C53/C57 as closely matched timing capacitors.

A 106-focused reconstruction converges on the full per-channel external MN3101 family:

```text
~1.8 kOhm / 8.2 kOhm / 150 pF
```

The remaining original designator/node mapping is kept explicit in the clock companion until a clean original crop/continuity map is archived.

## 6. I / II rates are not guessed from JUNO-60

The original 106 service material does not give us sufficiently clean numeric I/II rates in machine-readable form.

A 106-derived adjustable hardware reconstruction spans roughly 0..1.8 Hz and marks physical positions for original I and II. SoundForce has real 106 driver captures but does not publish the numeric trajectories.

Therefore strict constants remain:

```text
chorus_I_rate_hz  = MEASURE
chorus_II_rate_hz = MEASURE
```

Do not silently substitute Juno-60 values.

## 7. Dry/wet routing

The 106 chorus is dry + delayed, not 100% wet.

A 106-focused hardware reconstruction calibrated against the original behavior gives the useful working target:

```text
~55% dry / ~45% wet
```

Treat this as an output calibration target, not normalized DSP coefficients. The physical summers, BBD insertion loss, clock-dependent bandwidth and output stage remain part of the gain law.

No post-sum automatic unity normalization.

## 8. Filtering

Strict chorus must expose at least:

```text
pre_bbd_filter[channel]
clock_dependent_bbd_bandwidth[channel]
post_bbd_filter[channel]
```

A 106-focused reconstruction shows mirrored multi-element filter-component families around the two BBDs rather than one generic one-pole low-pass. This is documented in `JUNO106_CHORUS_FILTERS_MIX.md`.

Do **not** import the KR106 project's published ~14 kHz one-pole result as a 106 constant: that measurement was made on Juno-6 hardware.

Final digital filter coefficients wait for:

1. original 106 node map / SPICE reference;
2. real-106 complex transfer measurements at multiple clock phases.

## 9. Bias and headroom

Roland provides two independent chorus-bias trims and requires a 10 Vp-p / 1 kHz test signal while adjusting each path for symmetrical positive/negative clipping.

This makes BBD operating point part of the instrument.

Strict model needs per channel:

```text
biasOffset
positiveHeadroom
negativeHeadroom
nonlinearTransfer
```

Factory-new defaults use the symmetrical service calibration.

DEEP/AGE may later expose plausible bias drift, gain mismatch, noise increase and reduced headroom.

## 10. Noise model

Keep separate:

```text
front-panel synth NOISE
BBD device noise L/R
clock-feedthrough / heterodyne residue L/R
possible shared supply/control residual
```

Roland's modern 106 models expose chorus noise as an audible characteristic, which supports its perceptual importance, but their parameter scale is not an original-hardware numeric reference.

## 11. Mono behavior

Reference renderer should expose both:

```text
renderStereo() -> L, R
renderMono106() -> original-style L/R mono-normal sum
```

This matters for validation against mono hardware recordings and for comb/gain behavior.

FM-1 itself does not need physical jack sensing unless the hardware API exposes it.

## 12. Desktop reference structure

```text
Chorus106
  ChorusControl106 control
  ClockDriver106 clockA
  ClockDriver106 clockB
  BbdAudioPath106 wetA
  BbdAudioPath106 wetB
  DryWet106 mix
  OutputStage106 output
```

Processing concept:

```text
if OFF:
    return bypass/output-route

control -> two opposite MN3101 clock trajectories
clock -> exact 128/f_cp delay law
x -> pre-filter -> BBD nonlinearity/delay -> post-filter
x + wetA -> L analogue summer
x + wetB -> R analogue summer
-> output stage
```

Diagnostic research switches may isolate:

- ideal vs fitted clock trajectory;
- filters;
- BBD nonlinearity;
- noise;
- clock residue;
- dry path;
- output stage.

These diagnostics are not intended as final user-facing FM-1 clutter.

## 13. FM-1 deployment direction

Because chorus is global, it is a good place to spend fidelity CPU.

Likely deployment model after measurement:

- two fractional delays driven by physical clock-to-delay law;
- separate low-order pre/post filter state per channel;
- cheap BBD soft nonlinearity;
- deterministic low-cost device/clock noise;
- optional small residual LUT for measured driver imperfections;
- explicit analogue-summer/output gain.

Do not literally step 256 buckets unless A/B profiling proves meaningful audible benefit.

## 14. Remaining numerical gate

Before strict chorus constants are frozen we still need:

- exact original I and II LFO rates;
- min/mean/max `f_cp(t)` for both channels/modes;
- center/min/max delay validated against audio measurement;
- original node-by-node pre/post BBD filter transfer;
- clock-dependent BBD insertion loss/bandwidth;
- dry/wet voltage gains;
- factory-calibrated nonlinear/headroom curves;
- BBD noise and clock-feedthrough spectra/correlation;
- mono-summing/output-stage gain/headroom.

All of these now have an explicit measurement route in the companion documents, so no generic VA assumption needs to cross the Phase-0 gate silently.
