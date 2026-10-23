# JUNO-106 chorus reference model — Phase-0 closure contract

Status: **Phase-0 architecture freeze candidate** for strict `MODEL_106` chorus.

This document is the implementation contract that sits above the detailed schematic, clock, filter/mix and measurement notes. It defines what is fixed now, what remains physical calibration data, and what the first desktop/FM-1 implementation must preserve.

The goal is not to claim that unmeasured hardware numbers have magically become known. The goal is to make the remaining unknowns **data**, not architecture decisions.

## 1. Strict signal graph

```text
post-global-VCA mono signal x
        |
        +-------------------------------> dry input of L output summer
        +-------------------------------> dry input of R output summer
        |
        +-> BBD A input/bias/filter
        |      -> MN3009 A
        |      -> post-BBD A filter/amplifier
        |      -> TP1 wet A
        |      -> wet input of L output summer
        |
        +-> BBD B input/bias/filter
               -> MN3009 B
               -> post-BBD B filter/amplifier
               -> TP2 wet B
               -> wet input of R output summer

shared chorus-control oscillator
        -> TP3 / channel-A control
        -> MN3101 A -> MN3009 A clocks
        -> complementary TP4 / channel-B control
        -> MN3101 B -> MN3009 B clocks

L/R IC6 summers
        -> coupling/output/mute network
        -> stereo jacks / mono-normal behavior
```

Strict consequences:

- chorus is one global effect, never per voice;
- two independently clocked/bias-adjusted wet paths are mandatory;
- dry audio remains present in each enabled chorus output;
- channel-A/B modulation is nominally complementary, not stereo panning of one delay;
- no automatic dry/wet normalization;
- BBD/device/clock noise is independent of the front-panel synth NOISE generator.

## 2. User state

Original JUNO-106 strict state:

```text
OFF
I
II
```

Original documentation says I and II are not used simultaneously. Later Roland DSP products expose `I+II`; this is not promoted into `MODEL_106_STRICT`.

```text
MODEL_106_STRICT: OFF / I / II
DEEP:             may expose I+II explicitly as non-strict
MODEL_60:         separate future hardware behavior
```

## 3. Clock and delay contract

Each wet channel owns an MN3101/MN3009 clock state.

Original external timing family:

```text
A: R123 = 1.8k, R124 = 8.2k, C53 = 150p
B: R136 = 1.8k, R137 = 8.2k, C57 = 150p
```

Clock semantics:

```text
f_cp = f_osc / 2
N_MN3009 = 256

t_delay = 128 / f_cp
        = 256 / f_osc
```

The implementation must never store an ambiguous `clockHz`.

Runtime reference state:

```text
ClockState106 {
    fOscHz;
    fCpHz;
    delaySeconds;
}
```

Exact Mode-I/II trajectories remain `MEASURE_PENDING`; see section 12.

## 4. BBD audio path contract

The original schematic and reconstruction cross-check establish a matched multi-element audio path around each MN3009.

Primary-readable original families include:

```text
input coupling:
  A C44 = 0.1u
  B C47 = 0.1u

input/bias:
  A R122 = 10k
  B R113 = 10k

BBD output combine:
  A R118/R119 = 3.3k / 3.3k
  B R111/R112 = 3.3k / 3.3k

post-BBD resistor family:
  A R98/R97/R96/R95 = 22k
  B R107/R101/R106/R104 = 22k

post-BBD capacitor family, directly readable subset:
  A C45 = 820p, C46 = 680p, C38 = 1.8n
  B C48 = 820p, C49 = 680p, C43 = 1.8n
```

A 106-focused reconstruction independently contains the same characteristic filter-value family plus a 270p element in each mirrored path. The exact remaining original designator/node attachment of that element must stay transcription/measurement data rather than being silently renamed from the clone.

Therefore strict DSP exposes:

```text
BbdAudioPath106 {
    PreBbdFilter106 pre;
    BbdTransfer106 bbd;
    PostBbdFilter106 post;
}
```

not:

```text
onePoleLowpass(14kHz)
```

The numerical digital filter fit is calibration data derived from original topology + sweep measurement.

## 5. Output dry/wet summer — numerically derived from the original schematic

The original jack-board schematic now gives a particularly useful block exactly.

### Channel A

```text
wet input resistor  R71 = 47k
dry input resistor  R70 = 100k
feedback resistor   R72 = 39k
```

### Channel B

```text
wet input resistor  R73 = 47k
dry input resistor  R67 = 100k
feedback resistor   R74 = 39k
```

The two halves of IC6 therefore form mirrored inverting summing amplifiers.

Ignoring finite op-amp/loading corrections, each channel follows:

```text
y = -(39k/100k) * dry
    -(39k/47k)  * wet
```

So the **pre-output-stage electrical gains** are:

```text
Gdry_summer = -0.3900000000
Gwet_summer = -0.8297872340
```

and the summer itself weights its wet-input node by:

```text
|Gwet/Gdry| = 2.1276595745
```

This does **not** mean the audible result is 32% dry / 68% wet. `wet` here is the already attenuated/filtered MN3009-path signal at TP1/TP2. BBD insertion loss and the surrounding analogue filter/amplifier network occur before R71/R73.

This resolves an important ambiguity: the published approximately `55% dry / 45% wet` reconstruction result is compatible with a much larger resistor-domain wet gain because the wet path has substantial insertion loss before the summer.

If final output contribution were exactly 55/45 and all other output-stage terms were unity/equal, the implied TP1/TP2 wet-path amplitude relative to the dry-source node would be approximately:

```text
Gwet_path_needed ~= 0.3845454545
```

This number is a **diagnostic implication**, not a frozen original-hardware insertion gain.

## 6. Output stage

After IC6 the original schematic retains coupling/output/muting circuitry. Directly readable mirrored anchors include:

```text
R54 = 1.5k
R57 = 1.5k
```

Earlier research accidentally transcribed these as 15k; the primary schematic re-check corrects that error.

Strict reference keeps:

```text
IC6 summer
 -> coupling
 -> output/mute stage
 -> jack/loading law
```

as a separate block.

```text
Output106 {
    staticGain;
    highpassOrCouplingState;
    positiveHeadroom;
    negativeHeadroom;
    muteState;
    monoNormalLaw;
}
```

Do not fold output headroom into the BBD waveshaper.

## 7. BBD bias and nonlinearity

Roland's factory procedure is architectural evidence, not optional service trivia:

```text
CHORUS I
VCA LEVEL 0
10 Vp-p / 1 kHz injection at MODULE BOARD TP2
monitor JACK BOARD TP1 and TP2
adjust VR1 / VR2 for symmetrical positive/negative clipping
```

Strict channel state therefore requires:

```text
BbdTransfer106 {
    biasOffset;
    positiveHeadroom;
    negativeHeadroom;
    nonlinearCurve;
}
```

Factory-new nominal state is the symmetrical-clipping calibration point.

`DEEP/AGE` may later offset these values; strict mode does not randomize them arbitrarily.

## 8. Noise / residue taxonomy

Keep distinct mechanisms:

```text
synthNoise              // front-panel source before VCF
bbdDeviceNoiseA/B       // BBD/channel electronics
clockFeedthroughA/B     // CP/heterodyne residue
sharedControlResidual   // optional correlated modulation/supply residue
```

Regression/reference rendering uses deterministic seeds.

Do not generate one generic white-noise sample and call it chorus noise.

## 9. Stereo and mono contract

Reference renderer exposes:

```text
renderStereo() -> L, R
renderMono106() -> original-jack-law mono result
```

The One-O-Six reconstruction documents the same JUNO-106-style behavior: when the second stereo output is not used, the two outputs are combined to mono. Exact original loading/gain remains a physical measurement item.

This matters because a simple digital `(L+R)*0.5` may not reproduce the original jack/loading amplitude.

## 10. OFF behavior

`OFF` is not permission to skip every downstream analogue node in a high-precision reference render.

Reference architecture distinguishes:

```text
chorus disabled
wet paths muted/not contributing
original dry/output routing retained
```

A target-optimized build may collapse electrically irrelevant OFF-state blocks only after null/level tests prove equivalence.

## 11. Desktop reference API

```text
enum class ChorusMode106 {
    Off,
    I,
    II
};

struct ChorusCalibration106 {
    ClockTrajectory modeI_A;
    ClockTrajectory modeI_B;
    ClockTrajectory modeII_A;
    ClockTrajectory modeII_B;

    FilterFit preA;
    FilterFit preB;
    FilterFit postA;
    FilterFit postB;

    BbdCurve bbdA;
    BbdCurve bbdB;

    double dryGainA;   // physical output calibration, not normalized mix
    double dryGainB;
    double wetGainA;
    double wetGainB;

    NoiseFit noiseA;
    NoiseFit noiseB;
    ClockResidueFit clockA;
    ClockResidueFit clockB;

    OutputFit outputA;
    OutputFit outputB;
    MonoJackFit mono;
};

struct Chorus106 {
    ChorusMode106 mode;
    ChorusControl106 control;
    ClockDriver106 clockA, clockB;
    BbdAudioPath106 pathA, pathB;
    OutputSummer106 summerA, summerB;
    OutputStage106 outputA, outputB;
};
```

Important: calibration data is centralized. No `0.8`, `5.3ms`, `14kHz`, `0.55` or similar folklore constant may be scattered through production code.

## 12. `MEASURE_PENDING` calibration surface

The architecture is frozen while these numerical calibration values remain to be acquired from a healthy original 106:

### Clock / delay

```text
Mode-I LFO period
Mode-II LFO period
f_cp min/mean/max per channel/mode
rising/falling clock trajectory
A/B phase error from ideal 180 deg
center/min/max absolute audio delay
```

### Audio path

```text
complex wet-path transfer vs f_cp
BBD insertion loss vs clock
clock-dependent HF bandwidth
actual TP1/TP2 gain relative to dry-source node
```

### Output mix/stage

```text
full dry contribution at jack
full wet contribution at jack
IC6/op-amp finite correction if relevant
post-IC6 output gain/headroom
mono-normal loading/gain
```

### Nonlinearity / noise

```text
nominal calibrated positive/negative BBD clip knees
THD vs level and clock
BBD/device noise PSD
clock-feedthrough peaks
L/R noise correlation
```

Every item has a concrete procedure in `PHASE0_MEASUREMENT_PLAN.md`.

## 13. Development defaults before hardware capture

A reference implementation may need to run before the first physical capture. These defaults are allowed only when tagged `PROVISIONAL_MEASURE_PENDING`.

Rules:

1. use the **original topology and exact clock-to-delay identity**;
2. use original component-derived summer gains;
3. use the 106 reconstruction only for bounded initial calibration targets such as approximately 55/45 completed-output dry/wet;
4. do not label JUNO-60 I/II numeric rates as 106 measurements;
5. keep all provisional numbers in one calibration file;
6. regression tests must distinguish `architecture invariants` from `provisional calibration vectors`.

This lets Phase 1 begin without turning missing hardware access into fake certainty.

## 14. FM-1 deployment contract

The global chorus is a relatively cheap place to preserve fidelity.

Expected target representation:

```text
shared low-rate chorus-control trajectory
2 x fitted clock states
2 x fractional delay lines
2 x small pre-filter states
2 x small post-filter states
2 x cheap calibrated BBD nonlinearities
2 x deterministic noise/residue states
2 x explicit physical summer/output paths
```

Do not simulate all 256 buckets literally unless profiling/A-B tests demonstrate an audible benefit.

At 44.1 kHz, a conservative 20 ms maximum stereo delay reserve costs only:

```text
882 samples/channel
1764 samples stereo
```

which is small enough that memory pressure should not force a generic chorus architecture.

## 15. Deterministic tests required before target optimization

### Structural

```text
strict modes are exactly OFF/I/II
N_MN3009 == 256
f_cp == f_osc/2
delay * f_cp == 128
clock A/B use independent state
```

### Summer

```text
Gdry = -39/100
Gwet = -39/47
left/right resistor-law equality at nominal values
no automatic gain normalization
```

### Calibration separation

```text
changing measured clock calibration must not change filter topology
changing BBD noise must not affect synth NOISE source
changing output-stage gain must not alter BBD bias curve
```

### Rendering

```text
OFF has zero wet contribution
I/II preserve common dry path
stereo A/B delays move in opposite control direction
mono renderer uses explicit jack law
```

## 16. Phase-0 closure rule for chorus

The chorus block is ready to leave Phase 0 when all of the following are true:

```text
[x] original global signal topology mapped
[x] original dual MN3009 / dual MN3101 architecture mapped
[x] original timing component family mapped
[x] exact MN3009 clock/delay convention frozen
[x] original dry/wet summer resistor law derived
[x] BBD bias/headroom architecture frozen
[x] multi-element filter architecture frozen
[x] output stage retained as a physical block
[x] strict OFF/I/II semantics frozen
[x] noise/residue mechanisms separated
[x] stereo/mono render contracts defined
[x] unresolved numerical calibration values centralized
[x] every unresolved value has a physical measurement test
```

Physical captures will improve the **calibration dataset**. They no longer require changing the architecture.

That is the definition of `CHORUS_PHASE0_FROZEN`.
