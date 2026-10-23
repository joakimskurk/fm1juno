# JUNO-106 chorus filters, bias and mix — Phase 0 evidence model

Status: Phase 0 companion to `JUNO106_CHORUS.md` and `JUNO106_CHORUS_CLOCK.md`.

Purpose: freeze what is known about the **audio-domain** chorus path around the two MN3009 BBDs without importing Juno-6/60 transfer functions or inventing a clean digital wet/dry chorus.

## 0. Evidence classes

- **PRIMARY-106** — original Roland JUNO-106 service/owner documentation or literal 106 control encoding.
- **ORIGINAL-HARDWARE-CROSS-CHECK** — repair evidence from physical original 106 boards.
- **106-RECONSTRUCTION** — hardware reconstruction explicitly built around the 106 chorus and calibrated against a real 106.
- **DERIVED** — mathematical consequence of established values/topology.
- **OPEN** — node mapping, transfer curve or level still needs a clean original schematic read or hardware measurement.

A reconstruction is useful for disambiguating component families and measurement strategy, but it does not override the original board.

## 1. Global signal topology

**PRIMARY-106 / 106-RECONSTRUCTION**

The chorus is global and sits after the six voice paths and global level/HPF section.

Reference structure:

```text
mono pre-chorus signal
    |\
    | +-> dry path ---------------------------> output summer L
    | +-> dry path ---------------------------> output summer R
    |
    +-> BBD channel A -> post filter ---------> output summer L
    +-> BBD channel B -> post filter ---------> output summer R
```

The two BBD paths are clock-modulated in opposite phase.

Strict implications:

- one global chorus, not six per-voice choruses;
- two physical wet paths;
- common dry source mixed into both outputs;
- no enabled-source normalization;
- output-summer headroom must remain measurable/configurable.

## 2. Dry/wet balance

**106-RECONSTRUCTION**

Alpes Machines documents its One-O-Six insert configuration as matching the JUNO-106 at approximately:

```text
dry ~= 55%
wet ~= 45%
```

This is a useful calibration starting point, not a license to write:

```text
y = 0.55*x + 0.45*delay
```

and call it finished.

The analogue path can have:

- frequency-dependent wet insertion loss;
- BBD gain variation versus clock;
- output-stage inversion/gain;
- channel mismatch;
- headroom-dependent nonlinearity.

Working reference API should therefore store physical gains separately:

```text
DryWet106 {
    dryGainL;
    dryGainR;
    wetGainL;
    wetGainR;
    outputGainL;
    outputGainR;
}
```

The 55/45 figure is an acceptance-region target after the complete analogue path, not an internal normalized coefficient requirement.

## 3. Original mode-state correction: strict 106 is OFF / I / II

**PRIMARY-106**

The original JUNO-106 parameter encoding stores chorus using two switch bits with the semantics:

```text
chorus enable: OFF / ON
chorus preset: I / II
```

This produces three meaningful user states:

```text
OFF
I
II
```

The original owner's documentation also states that I and II are not used simultaneously.

Modern Roland DSP products expose a later `I+II` model and describe it as both buttons pressed simultaneously. That is useful evidence about Roland's modern effect library, but it does not establish an additional normal stored state in the original 1984 JUNO-106.

Policy:

```text
MODEL_106_STRICT: OFF / I / II
DEEP:            may expose I+II as an explicitly non-strict extension
MODEL_60:        may later implement the physically different 6/60 I+II behavior
```

Do not silently map a modern Roland `I+II` algorithm into strict 106.

## 4. BBD input/output filtering is multi-element, not one generic low-pass

**PRIMARY-106 topology / 106-RECONSTRUCTION component cross-check**

The original 106 and 106-derived hardware reconstructions place analogue filtering before and after each MN3009 path.

A 106-focused One-O-Six reconstruction exposes mirrored component families around the two BBD channels. The following values are useful as a **component-set cross-check**, not yet a frozen original-node netlist.

### Channel A reconstruction family

```text
R58  = 47 kOhm
R62  = 22 kOhm
R68  = 22 kOhm
R74  = 22 kOhm
R78  = 22 kOhm

C24  = 680 pF
C30  = 270 pF
C19  = 820 pF
C28  = 1.8 nF
```

### Channel B reconstruction family

```text
R60  = 47 kOhm
R64  = 22 kOhm
R69  = 22 kOhm
R75  = 22 kOhm
R79  = 22 kOhm

C25  = 680 pF
C31  = 270 pF
C20  = 820 pF
C29  = 1.8 nF
```

The mirrored values strongly support a deliberately matched multi-pole stereo filter family.

### 4.1 Sanity frequencies only

If a 22 kOhm resistor were paired as a simple first-order RC with each capacitor, the individual `1/(2*pi*R*C)` values would be approximately:

```text
22k / 680p  -> 10.64 kHz
22k / 270p  -> 26.79 kHz
22k / 820p  ->  8.82 kHz
22k / 1.8n  ->  4.02 kHz
```

These numbers are **not** declared cutoff frequencies of the actual circuit. The real topology contains active stages and interacting impedances. They merely show that the component family is entirely plausible for BBD anti-alias / reconstruction / clock-residue shaping.

This is a stronger architecture clue than importing the KR106 project's Juno-6-derived `14 kHz one-pole` fit.

## 5. Exact node topology is intentionally still open

The available machine-readable original service material does not preserve enough spatial wiring information to assign every resistor/capacitor above to a specific pole with high confidence.

Therefore Phase 0 does **not** yet freeze a guessed cascade such as:

```text
LP1 -> LP2 -> MN3009 -> LP3 -> LP4
```

or arbitrary biquad coefficients.

Required closure path:

1. obtain/inspect a clean original 106 jack-board schematic crop or physical board continuity map;
2. transcribe nodes around both MN3009 channels;
3. create a SPICE/netlist reference;
4. calculate small-signal transfer functions at fixed BBD clock points;
5. compare against a sweep through a calibrated original 106;
6. fit the minimum digital structure that meets the measured response.

Until then the desktop reference API should expose abstract stages:

```text
BbdAudioPath106 {
    PreFilter106 pre;
    BbdNonlinearity106 bbd;
    PostFilter106 post;
}
```

with coefficient sets explicitly tagged `PROVISIONAL`.

## 6. Channel matching and deliberate asymmetry

### 6.1 Clock timing capacitors

**ORIGINAL-HARDWARE-CROSS-CHECK**

A physical JUNO-106 jack-board repair identifies original `C53` and `C57` as the two closely matched 150 pF capacitors around the chorus clock networks. The repair discussion specifically notes the tight-tolerance capacitor choice and suggests polystyrene/silver-mica-class replacement for channel matching.

This materially strengthens the clock document's working value:

```text
C53 = 150 pF
C57 = 150 pF
```

and supports treating channel clock mismatch as a tolerance/calibration effect rather than arbitrary stereo randomization.

### 6.2 One control-path asymmetry reported in the 106 reconstruction

Alpes Machines documents that a 2.2 nF capacitor corresponding to its `C59` is omitted in the original JUNO-106 chorus voice A while present in voice B, and associates this with the inverted-LFO/control path rather than the main audio filter.

Do not misclassify this as a left/right audio EQ difference.

For strict modeling this is currently:

```text
CONTROL_PATH_ASYMMETRY_HYPOTHESIS
```

not a frozen audio-filter asymmetry.

If a direct original schematic/board read confirms it, the effect belongs in the clock/control trajectory residual, not in a generic stereo tone offset.

## 7. Bias and BBD headroom

**PRIMARY-106**

Roland requires separate chorus bias adjustment for the two BBD channels.

Factory procedure:

- feed a 10 Vp-p, 1 kHz sine into the specified module-board point;
- monitor the two chorus test points;
- adjust each channel's bias trim until positive and negative clipping halves are symmetrical.

This proves that the strict model must retain:

```text
biasOffset[channel]
positiveHeadroom[channel]
negativeHeadroom[channel]
nonlinearTransfer[channel]
```

A hard digital clip applied after the delay is not an adequate physical model.

Recommended diagnostic model:

```text
v_in_biased = preFilter(x) + biasOffset
v_limited   = bbdInputTransfer(v_in_biased)
v_delayed   = clockedBbd(v_limited)
y_wet       = postFilter(v_delayed - dcRemoval)
```

Factory-new strict defaults should be centered at the symmetrical clipping calibration point.

## 8. BBD noise and clock residue

The chorus path needs at least three distinct unwanted-signal mechanisms:

```text
bbdDeviceNoiseL/R
clockFeedthroughL/R
sharedSupplyOrControlResidual
```

Do not merge these with the synth's front-panel noise generator.

A deterministic seeded noise source is preferable for regression testing. Real-time builds may use a cheaper PRNG once spectrum/correlation are calibrated.

## 9. Output stage

A 106-derived hardware reconstruction explicitly includes an optional output network described as the JUNO-106 output stage and targets approximately unity input/output level.

This is useful evidence that strict reference should keep:

```text
wet/dry summer -> output analogue stage -> jack law
```

rather than treating the digital sum as the final sample.

Open items:

- original resistor/capacitor node map;
- inversion count/absolute polarity;
- nominal voltage gain;
- clipping threshold;
- mono-normal summing gain and loading.

Absolute polarity is lower priority acoustically than gain/headroom, but it matters for null tests and dry/wet comb response.

## 10. Reference-renderer implementation order

Do not jump directly to a complicated bucket simulation.

Implement in this order on desktop:

1. ideal dry path with explicit gain;
2. exact clock-derived variable delay from `JUNO106_CHORUS_CLOCK.md`;
3. provisional pre/post filter blocks with coefficients fitted to the eventual original-node/SPICE reference;
4. separate channel bias/nonlinearity;
5. dry/wet analogue summer gains;
6. BBD/device noise;
7. clock-feedthrough residual;
8. output-stage gain/headroom.

Every layer must have a diagnostic bypass so its audible and numerical contribution can be isolated.

## 11. FM-1 deployment strategy

The chorus remains one of the cheaper places to spend fidelity CPU because it is global.

Likely FM-1 representation after measurement:

```text
2 x fractional delay line
2 x small pre-filter state
2 x small post-filter state
2 x cheap nonlinear BBD transfer
2 x deterministic noise/filter state
1 x shared chorus-control/LFO state
2 x output-summer states
```

Keep coefficients for L/R separate in the reference build. Collapse them only if measured differences are below the acceptance threshold.

Do not allocate a literal 256-bucket audio state per channel unless A/B profiling proves an audible advantage over the clock-derived fractional-delay model.

## 12. Required measurements to freeze the audio filters

On a healthy, calibrated original 106:

### Fixed/known clock point

At several phases of Chorus I and II:

- inject swept sine / logarithmic chirp;
- capture dry path and both wet test/output paths;
- estimate complex transfer function, not magnitude only.

Recover:

- pre/post combined magnitude;
- phase/group delay beyond BBD transport delay;
- channel mismatch;
- clock-dependent HF loss;
- insertion gain.

### Headroom

At 100 Hz, 1 kHz, 5 kHz:

- increase level through clean -> onset -> obvious BBD clipping;
- fit positive and negative knees separately;
- repeat at slow/fast points of the chorus clock cycle.

### Noise

With synth audio muted and chorus enabled:

- FFT/RMS per channel;
- L/R correlation;
- identify clock-related peaks;
- repeat I vs II.

## 13. Freeze state

### Frozen enough for architecture

- dual wet paths plus common dry path;
- approximate 55/45 original-style dry/wet working target;
- multi-element analogue filtering before/after BBDs;
- mirrored filter-component families in a 106-focused reconstruction;
- separate per-channel BBD bias/nonlinearity;
- channel clock matching is intentional;
- C53/C57 = 150 pF is now supported by original-board repair evidence;
- strict original mode state remains OFF / I / II;
- chorus noise/clock residue is independent of synth NOISE;
- final output stage remains part of reference signal path.

### Open before numerical strict freeze

- original 106 node-by-node pre/post BBD filter netlist;
- exact original filter component values/designators and tolerances;
- measured complex transfer functions versus clock;
- exact BBD insertion loss versus clock;
- factory-calibrated nonlinear curve/headroom;
- exact dry/wet voltage gains;
- output-stage transfer and mono-normal summing law;
- direct confirmation/meaning of the reported one-channel 2.2 nF control-path asymmetry.
