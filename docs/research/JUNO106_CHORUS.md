# JUNO-106 chorus — Phase 0 authoritative index

Status: **`CHORUS_PHASE0_FROZEN`** for architecture and implementation contract. Physical-unit captures remain required to replace explicitly centralized `MEASURE_PENDING` calibration values; they do not reopen the architecture unless measurements contradict the primary schematic.

JUNO-106 is authoritative. JUNO-6/60 data is method/topology support only unless independently confirmed for the 106.

## 0. Authority map

Use these documents together, in this order for chorus work:

1. `JUNO106_CHORUS_REFERENCE_MODEL.md` — current implementation/closure contract.
2. `JUNO106_CHORUS_CLOCK.md` — MN3101/MN3009 clock domains, original timing network and exact delay law.
3. `JUNO106_CHORUS_FILTERS_MIX.md` — BBD audio path, original dry/wet summer, bias/headroom, noise and output stage.
4. `JUNO106_CHORUS_PRIMARY_SCHEMATIC.md` — direct Roland jack-board schematic transcription/evidence boundary.
5. `data/juno106_chorus_component_evidence.csv` — per-component evidence state.
6. `data/juno106_chorus_calibration_v0.yaml` — the only allowed home for provisional/measured strict calibration values.
7. `netlists/juno106_chorus_output_mix_reference.cir` — verified original IC6 summer subcircuit.
8. `PHASE0_MEASUREMENT_PLAN.md` — acquisition tests for all remaining measured constants.

If an older note conflicts with this index or the reference model, the newer specialized chorus documents win.

## 1. Strict physical architecture

The original chorus is one **global stereo effect** after the six voice paths and global output section:

```text
six voice VCA outputs
 -> voice summing
 -> global HPF
 -> global uPC1252 VCA / patch level
 -> chorus JACK BOARD
      -> common dry path -> L/R summers
      -> wet A -> MN3009 A <- MN3101 A
      -> wet B -> MN3009 B <- MN3101 B
      -> analogue filtering/bias/headroom
      -> IC6 dry+wet stereo summers
      -> coupling/mute/output network
 -> stereo / mono-normalled output
```

Frozen consequences:

- chorus is not per voice;
- two wet paths remain physically independent;
- both outputs contain a common dry contribution when chorus is enabled;
- wet A/B clocks are nominally complementary/opposite polarity;
- no enabled-source or dry/wet unity normalization;
- BBD noise/clock residue is separate from front-panel synth NOISE;
- the post-summer analogue output/jack path remains part of the reference model.

## 2. Strict mode state

Original user/patch state:

```text
OFF
I
II
```

The original owner's documentation states I and II are not used simultaneously.

Policy:

```text
MODEL_106_STRICT : OFF / I / II
DEEP             : may expose later Roland-style I+II explicitly as non-strict
MODEL_60         : separate future 6/60 behavior
```

## 3. Original timing network and exact delay law

The original Roland jack-board schematic now gives the mirrored MN3101 timing families directly:

```text
Channel A: R123 = 1.8 kOhm, R124 = 8.2 kOhm, C53 = 150 pF
Channel B: R136 = 1.8 kOhm, R137 = 8.2 kOhm, C57 = 150 pF
```

R124/R137 are independently cross-checked by the historical MN3009 -> MN3007 conversion, and C53/C57 matching is supported by original-board repair evidence.

Never use an ambiguous `clockHz`.

```text
f_osc = MN3101 internal oscillator frequency
f_cp  = frequency on one physical CP1/CP2 output
f_cp  = f_osc / 2
```

For MN3009 `N=256`:

```text
t_delay = N/(2*f_cp)
        = 128/f_cp
        = N/f_osc
```

This identity is frozen and testable.

## 4. Mode I / II trajectories are calibration, not guessed architecture

The original 106 documentation does not publish trustworthy numerical Mode-I/II LFO/clock trajectories. SoundForce has captured real JUNO-106 BBD-driver signals and reports preserving their non-ideal shape rather than substituting an ideal triangle, but does not publish the numerical curves.

Therefore:

```text
modeI.clockTrajectoryA/B  = MEASURE_PENDING
modeII.clockTrajectoryA/B = MEASURE_PENDING
```

Do not substitute JUNO-60 numeric rates as strict 106 constants.

The physical causal chain remains fixed:

```text
triangle-like control
 -> control/interface network
 -> MN3101 oscillator
 -> f_cp(t)
 -> reciprocal MN3009 delay(t)
```

## 5. Original BBD audio-path architecture

Primary-readable mirrored component families include:

```text
input coupling:
 A C44 = 0.1 uF
 B C47 = 0.1 uF

input/bias region:
 A R122 = 10 kOhm
 B R113 = 10 kOhm

BBD output combine:
 A R118/R119 = 3.3 kOhm / 3.3 kOhm
 B R111/R112 = 3.3 kOhm / 3.3 kOhm

post-BBD resistor family:
 A R98/R97/R96/R95 = 22 kOhm
 B R107/R101/R106/R104 = 22 kOhm

primary-readable post-BBD capacitors:
 A C45 = 820 pF, C46 = 680 pF, C38 = 1.8 nF
 B C48 = 820 pF, C49 = 680 pF, C43 = 1.8 nF
```

A 106-focused reconstruction independently shows the characteristic mirrored `680p / 270p / 820p / 1.8n` filter family. The remaining original 270-pF designator/node is retained as a transcription/measurement detail rather than being invented from the clone.

Strict architecture is therefore a multi-element BBD anti-alias/reconstruction/filter path, **not** one arbitrary 10–14 kHz one-pole LPF.

## 6. Original dry/wet summer — exact schematic-derived law

The two halves of IC6 (`TA75558S`) are mirrored inverting summing amplifiers.

```text
Channel A:
 wet R71 = 47 kOhm
 dry R70 = 100 kOhm
 feedback R72 = 39 kOhm

Channel B:
 wet R73 = 47 kOhm
 dry R67 = 100 kOhm
 feedback R74 = 39 kOhm
```

Ideal small-signal law per channel:

```text
y = -(39/100)*dry -(39/47)*wet
```

Freeze:

```text
Gdry_summer = -0.3900000000
Gwet_summer = -0.8297872340425532
|Gwet/Gdry| = 2.127659574468085
```

These are electrical gains at the IC6 summer inputs, not normalized audible mix percentages.

The approximately 55% dry / 45% wet figure from a 106-focused reconstruction remains a **completed-output calibration target**. It is compatible with the original resistor law because the MN3009/filter wet path has insertion loss before TP1/TP2.

If 55/45 were exact, the implied TP wet-path amplitude relative to the dry-source node would be about `0.3845454545`. This is diagnostic only, not a frozen original constant.

## 7. Bias, headroom and nonlinearity

Roland's service procedure requires independent `VR1/VR2` adjustment with a 10 Vp-p / 1 kHz stimulus so TP1/TP2 clip symmetrically.

Strict channel state therefore includes:

```text
biasOffset
positiveHeadroom
negativeHeadroom
nonlinearTransfer
```

Factory-new strict state corresponds to the symmetrical-clipping calibration point. `DEEP/AGE` may later expose plausible drift separately.

## 8. Output stage and corrected value

The reference retains:

```text
IC6 summer
 -> coupling
 -> mute/output stage
 -> jack/loading law
```

Direct original schematic re-check gives:

```text
R54 = 1.5 kOhm
R57 = 1.5 kOhm
```

An earlier research transcription had misread these as 15 kOhm; that value is superseded.

Exact completed output gain/headroom and mono-normal loading remain `MEASURE_PENDING` calibration data.

## 9. Noise and residue

Keep separate mechanisms:

```text
front-panel synth NOISE
BBD device/channel noise A/B
clock feedthrough / heterodyne residue A/B
possible shared control/supply residual
```

Reference regression renders use deterministic seeds.

## 10. Stereo / mono contract

Reference renderer exposes:

```text
renderStereo() -> L, R
renderMono106() -> original-jack-law mono result
```

Do not silently replace original mono jack/loading behavior with `(L+R)*0.5`; its exact gain remains a measurement item.

## 11. Reference implementation contract

```text
Chorus106
  ChorusControl106 control
  ClockDriver106 clockA
  ClockDriver106 clockB
  BbdAudioPath106 pathA
  BbdAudioPath106 pathB
  OutputSummer106 summerA
  OutputSummer106 summerB
  OutputStage106 outputA
  OutputStage106 outputB
```

Processing concept:

```text
mode/control
 -> two physical clock trajectories
 -> exact 128/f_cp delay law
x -> pre-filter -> calibrated BBD transfer/delay -> post-filter
x + wetA -> original IC6 summer A
x + wetB -> original IC6 summer B
-> output/jack stage
```

All provisional/measured numerical calibration lives in:

```text
data/juno106_chorus_calibration_v0.yaml
```

Do not scatter folklore constants such as `0.8 Hz`, `5 ms`, `14 kHz`, or `0.55` through DSP source.

## 12. FM-1 deployment direction

The chorus is global, so preserving fidelity is relatively inexpensive.

Expected representation:

- shared low-rate control trajectory;
- two fitted MN3101 clock states;
- two fractional delay lines;
- separate small pre/post filter states;
- two calibrated cheap BBD nonlinearities;
- deterministic BBD/clock-noise states;
- exact original summer gains;
- compact output/jack model.

At 44.1 kHz, even a conservative 20 ms stereo delay reserve is only 882 samples/channel / 1764 samples total.

Do not literally model all 256 buckets unless profiling/A-B tests demonstrate audible benefit.

## 13. Phase-0 closure

The chorus architecture gate is now closed:

```text
[x] original global topology mapped
[x] dual MN3009 / dual MN3101 architecture mapped
[x] original timing component family mapped
[x] exact clock-domain semantics and delay identity frozen
[x] original multi-element BBD filter architecture mapped
[x] exact IC6 dry/wet summer resistor law derived
[x] independent BBD bias/headroom architecture frozen
[x] output/jack stage retained as a physical block
[x] strict OFF/I/II semantics frozen
[x] noise/residue mechanisms separated
[x] stereo/mono render contracts defined
[x] unresolved numerical calibration centralized
[x] every unresolved hardware quantity has an explicit measurement test
```

Remaining physical captures are **calibration backlog**, not unresolved chorus architecture:

- Mode-I/II clock trajectories and exact rates;
- min/mean/max `f_cp` and resulting delay excursions;
- clock-dependent BBD insertion loss/bandwidth;
- measured complex wet-path transfer;
- completed-output dry/wet gain;
- calibrated BBD nonlinear/headroom curves;
- BBD/clock noise spectra/correlation;
- output-stage and mono-jack gain/loading.

These remain `MEASURE_PENDING` until a healthy original JUNO-106 is captured. They must be replaced with measured data before claiming final hardware-calibrated strict fidelity, but they no longer require an architectural decision.

**Phase-0 chorus state: `CHORUS_PHASE0_FROZEN`.**
