# JUNO-106 chorus filters, bias and mix — Phase 0 evidence model

Status: Phase-0 companion to `JUNO106_CHORUS.md`, `JUNO106_CHORUS_CLOCK.md` and `JUNO106_CHORUS_REFERENCE_MODEL.md`.

The original Roland jack-board schematic is now readable enough that the audio-domain chorus **topology no longer needs to be inferred from a clone BOM**. Numerical digital filter coefficients still wait for checked transfer calculation/measurement.

## 0. Evidence classes

- **PRIMARY-106** — original Roland JUNO-106 service/owner documentation or literal 106 control encoding.
- **PRIMARY-SCHEMATIC-DERIVED** — mathematical consequence of the original topology/component values.
- **ORIGINAL-HARDWARE-CROSS-CHECK** — repair evidence from physical original 106 boards.
- **106-RECONSTRUCTION** — hardware reconstruction explicitly built around the 106 and calibrated against a real unit.
- **MEASURE_PENDING** — topology is fixed but a physical transfer/constant still requires capture.

Reconstruction evidence may resolve legibility and provide acceptance targets. It does not override the original Roland schematic.

## 1. Global topology

The chorus is global and sits after the six voice paths, HPF and global VCA/level path.

```text
mono pre-chorus signal
    |\
    | +-> dry path ---------------------------> IC6 summer L
    | +-> dry path ---------------------------> IC6 summer R
    |
    +-> BBD channel A -> filter/amplifier ---> TP1 -> IC6 summer L
    +-> BBD channel B -> filter/amplifier ---> TP2 -> IC6 summer R
                                                 |
                                                 -> coupling/mute/output -> jacks
```

The two BBD paths are clock-modulated in opposite polarity.

Strict implications:

- one global chorus;
- two physical wet paths;
- common dry source into both output summers;
- no enabled-source normalization;
- output-stage headroom/loading remains explicit.

## 2. Original mode-state correction

Strict original JUNO-106 user state is:

```text
OFF
I
II
```

Original owner documentation says I and II are not used simultaneously.

Modern Roland DSP products exposing `I+II` are not evidence that it was a normal original 106 stored/control state.

```text
MODEL_106_STRICT: OFF / I / II
DEEP:            optional explicitly non-strict I+II
MODEL_60:        separate future hardware behavior
```

## 3. Primary original BBD audio-path component families

The readable Roland schematic gives the following directly.

### 3.1 Input/coupling region

```text
Channel A:
  C44 = 0.1 uF
  R122 = 10 kOhm

Channel B:
  C47 = 0.1 uF
  R113 = 10 kOhm
```

VR1/VR2 inject the independent BBD bias operating points.

### 3.2 MN3009 output combine

```text
Channel A:
  R118 = 3.3 kOhm
  R119 = 3.3 kOhm

Channel B:
  R111 = 3.3 kOhm
  R112 = 3.3 kOhm
```

The two MN3009 output phases are therefore combined through matched resistive paths before the reconstruction/filter stages.

### 3.3 Post-BBD resistor family

```text
Channel A:
  R98 = 22 kOhm
  R97 = 22 kOhm
  R96 = 22 kOhm
  R95 = 22 kOhm

Channel B:
  R107 = 22 kOhm
  R101 = 22 kOhm
  R106 = 22 kOhm
  R104 = 22 kOhm
```

### 3.4 Directly readable post-BBD capacitors

```text
Channel A:
  C45 = 820 pF
  C46 = 680 pF
  C38 = 1.8 nF

Channel B:
  C48 = 820 pF
  C49 = 680 pF
  C43 = 1.8 nF
```

A 106-focused reconstruction independently contains the characteristic mirrored family:

```text
680 pF
270 pF
820 pF
1.8 nF
```

The remaining original 270-pF designator/node is not promoted from the clone by renaming it. It stays a transcription detail to verify from the original board/schematic, while the **multi-element filter architecture itself is frozen**.

## 4. Why this is not a generic one-pole filter

The original circuit visibly contains multiple R/C elements and active transistor stages after each BBD. Therefore strict modeling must support:

```text
pre_bbd_filter[channel]
BBD source/load interaction
post_bbd_filter[channel]
clock_dependent_bbd_bandwidth[channel]
```

The following simple RC frequencies are sanity numbers only:

```text
22k / 680p -> 10.64 kHz
22k / 270p -> 26.79 kHz
22k / 820p ->  8.82 kHz
22k / 1.8n ->  4.02 kHz
```

They are **not** the poles of the complete active circuit.

Do not import KR106's Juno-6-derived approximately-14-kHz one-pole fit as an original-106 constant.

## 5. Exact original output summer — now numerically closed

The two halves of IC6 (`TA75558S`) are mirrored inverting dry/wet summing amplifiers.

### Channel A

```text
R71 = 47 kOhm   wet input
R70 = 100 kOhm  dry input
R72 = 39 kOhm   feedback
```

### Channel B

```text
R73 = 47 kOhm   wet input
R67 = 100 kOhm  dry input
R74 = 39 kOhm   feedback
```

Ideal small-signal law per channel:

```text
y = -(39/100)*dry -(39/47)*wet
```

Therefore:

```text
Gdry_summer = -0.39
Gwet_summer = -0.8297872340425532
```

This is `PRIMARY-SCHEMATIC-DERIVED` and can be regression-tested immediately.

The summer's larger wet-node coefficient does not contradict an approximately 55/45 finished dry/wet result because the BBD/filter path has significant insertion loss before TP1/TP2.

If the reconstruction's 55/45 completed-output ratio were exact, it would imply roughly:

```text
TP_wet / dry_source ~= 0.3845454545
```

before the IC6 input resistors. This is a useful measurement sanity target, **not** an original constant.

## 6. 55/45 dry/wet evidence boundary

Alpes Machines documents its 106-focused One-O-Six insert configuration at approximately:

```text
dry ~= 55%
wet ~= 45%
```

Use this only as an acceptance region for the complete physical path.

Do not implement:

```text
y = 0.55*x + 0.45*delay
```

because that would erase:

- BBD insertion loss;
- clock-dependent bandwidth/gain;
- the original IC6 resistor law;
- output-stage loading/headroom;
- channel mismatch.

## 7. BBD bias/headroom

Roland requires separate chorus-bias adjustment for the two BBD paths.

Factory procedure:

```text
10 Vp-p / 1 kHz input at MODULE BOARD TP2
CHORUS I
VCA LEVEL 0
monitor JACK BOARD TP1 / TP2
VR1 / VR2 -> symmetrical positive and negative clipping halves
```

Strict model therefore retains:

```text
biasOffset[channel]
positiveHeadroom[channel]
negativeHeadroom[channel]
nonlinearTransfer[channel]
```

Factory-new strict defaults represent the symmetrical service-calibration point.

## 8. Output stage correction and boundary

After IC6 the original schematic retains coupling/muting/output circuitry.

Directly readable mirrored anchors include:

```text
R54 = 1.5 kOhm
R57 = 1.5 kOhm
```

An earlier transcription mistakenly recorded these as 15 kOhm. The primary schematic re-check corrects that error.

Keep:

```text
IC6 dry/wet summer
 -> coupling network
 -> mute/output stage
 -> jack/loading law
```

as a separate block.

Still `MEASURE_PENDING`:

- exact complete output-stage static transfer;
- clipping threshold;
- loading correction;
- mono-normal summing gain;
- absolute output polarity after the full chain.

## 9. Channel matching and intentional asymmetry

Original clock timing networks are deliberately mirrored and use matched timing capacitors:

```text
A: R123 1.8k / R124 8.2k / C53 150p
B: R136 1.8k / R137 8.2k / C57 150p
```

Original-board repair evidence specifically supports close matching of C53/C57.

A 106 reconstruction reports a one-channel 2.2-nF control-path capacitor asymmetry. Treat that as a **control/clock-path hypothesis**, not an arbitrary stereo audio-EQ difference, until direct original tracing confirms it.

## 10. Noise model

Keep independent:

```text
front-panel synth NOISE
BBD device/channel noise L/R
clock-feedthrough / heterodyne residue L/R
possible shared supply/control residual
```

Use deterministic seeded sources for regression tests.

## 11. Stereo / mono output

Reference renderer exposes both:

```text
renderStereo() -> L, R
renderMono106() -> original-style mono-normal result
```

A 106-focused hardware reconstruction documents the same physical-use behavior: the two stereo sides are combined when only the mono output is used.

Exact original resistor/contact loading and mono gain remain `MEASURE_PENDING`; do not assume `(L+R)*0.5` until verified.

## 12. Reference implementation order

Desktop reference:

1. exact IC6 summer law;
2. exact `t_delay = 128/f_cp` variable-delay identity;
3. primary-topology pre/post filter representation;
4. fitted filter coefficients from original sweep/SPICE reference;
5. independent channel bias/nonlinearity;
6. BBD insertion-loss vs clock;
7. device/clock noise;
8. output-stage gain/headroom;
9. mono jack law.

Each layer needs a diagnostic bypass.

## 13. FM-1 deployment

Likely target after calibration:

```text
2 x fractional delay line
2 x low-order pre-filter state
2 x low-order post-filter state
2 x cheap BBD nonlinear transfer
2 x deterministic noise/residue state
2 x physical dry/wet summer gains
1 x compact output/jack stage
```

Do not allocate literal 256-bucket audio state unless A/B profiling proves a benefit.

## 14. Phase-0 freeze state

### Frozen

- dual wet paths plus common dry path;
- original input/output BBD component families listed above;
- matched multi-element analogue filtering around both MN3009 paths;
- exact original IC6 dry/wet summing resistor law;
- per-channel BBD bias/nonlinearity requirement;
- original timing-network family;
- final output stage retained as a physical block;
- chorus noise/clock residue independent of synth NOISE;
- strict OFF/I/II user state;
- no generic one-pole/Juno-60 filter substitution.

### Calibration data still `MEASURE_PENDING`

- exact complex pre/post BBD transfer;
- exact 270-pF original designator/node confirmation;
- BBD insertion loss and bandwidth versus clock;
- real completed dry/wet jack gains;
- factory-calibrated BBD nonlinear curve/headroom;
- noise/clock-feedthrough spectra/correlation;
- output-stage transfer;
- mono-normal summing/loading law.

All are assigned physical tests in `PHASE0_MEASUREMENT_PLAN.md`; they no longer require architectural guessing.
