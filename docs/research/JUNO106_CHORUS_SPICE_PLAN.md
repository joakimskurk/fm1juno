# JUNO-106 chorus — SPICE/reference reduction plan

Status: Phase-0 planning document. No production DSP is authorized by this file.

Purpose: convert the remaining chorus uncertainty into a reproducible reference-model workflow before FM-1 optimization.

## 1. Two-model rule

Build two separate desktop models:

```text
MODEL_A = physical candidate / SPICE reference
MODEL_B = reduced DSP-equivalent candidate
```

`MODEL_A` is allowed to be slow and explicit. `MODEL_B` is allowed to be cheap, but only after it is numerically fitted and validated against A and, ultimately, a real original 106.

Do not tune MODEL_B by ear first.

## 2. MODEL_A blocks

Reference chain:

```text
mono input
 -> direct/dry path
 -> channel split

wet A:
  pre-conditioning/filter network
  BBD bias injection
  MN3009-equivalent nonlinear/clocked delay
  complementary-output summing
  post-filter/buffer

wet B:
  mirrored pre-conditioning/filter network
  BBD bias injection
  MN3009-equivalent nonlinear/clocked delay
  complementary-output summing
  post-filter/buffer

control:
  triangle-like LFO
  normal + inverted control paths
  MN3101-equivalent clock law
  f_cp -> t_delay = 128/f_cp

output:
  dry + wet A -> left analogue summer/output
  dry + wet B -> right analogue summer/output
  original-style mono-normal sum path
```

## 3. Candidate wet passive family

Until original 106 continuity/schematic reading closes the netlist, use the 106-focused reconstruction as `CANDIDATE_A` only.

Per channel candidate family:

```text
47 kOhm
4 x 22 kOhm-class audio-filter resistors
680 pF
270 pF
820 pF
1.8 nF
2 x 3.3 kOhm MN3009 output-summing resistors
```

Exact node placement remains `MEASURE_PENDING` / `ORIGINAL_NODEMAP_PENDING`.

## 4. BBD abstraction levels

MODEL_A should support three switchable BBD representations:

### A0 — ideal transport

```text
variable fractional delay only
```

Used to isolate filter/mix effects.

### A1 — clock-derived sampled transport

```text
delay = 128/f_cp
clock-dependent sampling/interpolation loss
```

Used as the main reference candidate.

### A2 — nonlinear/headroom BBD

Adds:

```text
input bias
asymmetric/symmetric calibrated headroom
soft limiting / transfer knee
clock-dependent insertion loss
noise
clock feedthrough residual
```

A2 is the physical-fidelity target; A0/A1 remain diagnostic baselines.

## 5. Clock trajectory

Strict clock model:

```text
control_voltage_A = +m(t)
control_voltage_B = -m(t)

f_cp_A = F_A(control_voltage_A)
f_cp_B = F_B(control_voltage_B)

t_delay_A = 128/f_cp_A
t_delay_B = 128/f_cp_B
```

`F_A` and `F_B` are not assumed linear.

Initial fitting sources, in priority order:

1. original 106 CP captures;
2. original 106 hardware-repair measurements;
3. 106-focused reconstruction only as a provisional shape/range prior.

## 6. Bias model

Factory calibrated state is defined by Roland's symmetrical clipping adjustment.

Candidate transfer:

```text
x_biased = x + bias
x_limited = softclip_asym(x_biased, neg_headroom, pos_headroom, knee)
```

At factory nominal:

```text
abs(neg_headroom) ~= abs(pos_headroom)
```

but do not hard-code perfect symmetry; store channel calibration values separately.

## 7. MODEL_B reduction targets

MODEL_B may use:

```text
2 x fractional delay line
2 x low-order IIR pre/post equivalents
2 x cheap nonlinear transfer functions
2 x deterministic noise/filter states
1 x shared control trajectory + per-channel residual
2 x analogue-summer gain stages
```

MODEL_B must preserve:

- exact clock-to-delay convention;
- opposite modulation polarity;
- measured center/min/max delay;
- wet transfer magnitude and phase;
- clock-dependent HF loss;
- dry/wet gain;
- clipping/headroom behavior;
- relevant noise/clock-residue spectrum.

## 8. Reduction acceptance metrics

At minimum test three BBD clock points:

```text
slow-clock / max delay
center
fast-clock / min delay
```

and both chorus modes.

### Small-signal transfer

Across 20 Hz–20 kHz:

```text
magnitude error target: <= 0.5 dB in main audible passband
phase/group-delay error: chosen after original measurement uncertainty is known
```

Do not force a tighter target than the physical reference data supports.

### Delay

```text
center/min/max delay error <= 1% or <= 20 us, whichever is looser
```

until hardware data justifies a tighter bound.

### Stereo

Track:

```text
instantaneous delay difference
L/R correlation
relative modulation phase
```

### Large signal

Match:

```text
clipping onset within 0.5 dB
THD trend / knee shape within measurement repeatability
```

### Noise

Match:

```text
broadband RMS
spectral tilt
major clock-related peaks
L/R correlation
```

within measured unit/take variation.

## 9. Regression stimuli

Generate deterministic reference renders for:

```text
impulse
100 Hz sine
1 kHz sine
8 kHz sine
log sweep
white/pink noise
silence
large 1 kHz amplitude sweep
```

For every stimulus capture:

```text
OFF
I
II
stereo
mono-reference sum
```

At internal debug build also capture:

```text
pre-BBD
BBD output before post filter
post-wet
pre-output-summer
final L/R
```

## 10. Parameter provenance

Every constant in MODEL_A gets one tag:

```text
PRIMARY_106
ORIGINAL_106_HARDWARE
106_RECONSTRUCTION
DERIVED
MEASURE_PENDING
```

MODEL_B coefficients additionally store the fit dataset/checksum that produced them.

No anonymous magic constants.

## 11. FM-1 decision gate

Only after MODEL_A/B comparison exists do we decide:

- interpolation order;
- whether clock residual LUT is audible enough to keep;
- whether pre/post filters can collapse to one or two biquads;
- whether BBD nonlinearity needs oversampling;
- whether clock feedthrough is worth explicit synthesis;
- memory length and guard samples;
- fixed-point vs float/mixed precision.

Until then, FM-1 chorus CPU estimates are provisional.

## 12. Immediate next technical task

Before implementing MODEL_A numerically, close enough of the original-106 jack-board node map to answer:

1. exact dry branch into each final summer;
2. exact signal path into MN3009 pin 3;
3. exact relation of MN3009 pins 7/8 to the two 3.3 kOhm candidate summing resistors;
4. which of the 820 pF / 680 pF / 270 pF / 1.8 nF elements are pre-BBD vs post-BBD;
5. output-stage inversion/gain before the jacks.

If original schematic reading remains ambiguous, a physical continuity map plus swept transfer is sufficient to proceed without guessing.
