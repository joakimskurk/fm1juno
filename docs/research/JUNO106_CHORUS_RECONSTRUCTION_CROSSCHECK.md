# JUNO-106 chorus reconstruction cross-check

Status: Phase-0 supporting evidence. This file intentionally separates original JUNO-106 evidence from modern hardware reconstructions and from JUNO-60-family clones.

Purpose: preserve concrete component/node clues that are useful for SPICE/reference work without promoting clone values to strict original-106 truth.

## 1. Evidence classes

- `PRIMARY_106` — original Roland JUNO-106 service/owner documentation.
- `ORIGINAL_106_HARDWARE` — measurements/repair observations on a physical original JUNO-106.
- `106_RECONSTRUCTION` — hardware reconstruction explicitly based on / calibrated against a JUNO-106.
- `JUNO60_CONTROL` — Juno-60-family clone used only as a topology sanity check and negative control.

Strict code may use `PRIMARY_106` and `ORIGINAL_106_HARDWARE` as direct calibration evidence. `106_RECONSTRUCTION` values are provisional defaults/cross-checks until matched against the original board. `JUNO60_CONTROL` values must never silently enter strict 106 calibration.

## 2. Alpes Machines One-O-Six

Alpes Machines publishes a hardware chorus explicitly designed around the JUNO-106 circuit and calibrated against a real 106.

Useful 106-specific statements from its documentation:

- two MN3009/MN3101 BBD channels;
- normal LFO and inverted LFO are adjusted to equal amplitude with 180-degree phase rotation;
- target LFO control amplitude is approximately 20 Vp-p;
- RATE control positions are marked to match original JUNO-106 chorus I and II;
- D20 is used specifically for the JUNO-106 design;
- R25 = 2.2 MOhm is identified as the JUNO-106 LFO-rate value;
- C59 = 2.2 nF is reported absent in original 106 voice A but present in voice B, apparently in the inverted-LFO/control path;
- R110 is a wire in the original 106, while the reconstruction optionally adds anti-thump filtering borrowed from another Roland/Boss design;
- the optional output network can be populated as the JUNO-106 output stage with approximately unity input/output level;
- direct + wet operation is the normal 106-style insert configuration;
- if the right output is unused, L/R are summed to mono on the left output.

These statements are useful because they also identify what *not* to copy into strict mode: optional anti-thump parts, MN3007-specific clock-leakage capacitors and other quality-of-life modifications belong outside strict 106.

## 3. Mirrored wet-path component family

The One-O-Six schematic exposes mirrored component groups around both MN3009 wet paths.

Channel A candidate family:

```text
R58 = 47 kOhm
R62 = 22 kOhm
R68 = 22 kOhm
R74 = 22 kOhm
R78 = 22 kOhm

C24 = 680 pF
C30 = 270 pF
C19 = 820 pF
C28 = 1.8 nF
```

Channel B candidate family:

```text
R60 = 47 kOhm
R64 = 22 kOhm
R69 = 22 kOhm
R75 = 22 kOhm
R79 = 22 kOhm

C25 = 680 pF
C31 = 270 pF
C20 = 820 pF
C29 = 1.8 nF
```

The mirrored values strongly support the architecture already frozen in `JUNO106_CHORUS_FILTERS_MIX.md`: the wet path is a multi-element matched analogue network, not one generic post-delay low-pass.

Do not yet assign these values to strict 106 designators or digital poles without original-board node confirmation.

## 4. BBD output summing

The reconstruction places two equal 3.3 kOhm resistors on the two audio outputs of each MN3009 before the following analogue stage.

Working interpretation:

```text
MN3009 output tap 1 -- 3.3k --+
                              +--> combined wet node
MN3009 output tap 2 -- 3.3k --+
```

This is consistent with the common practice of summing the two complementary BBD outputs to reduce clock feedthrough / recover the delayed signal.

Treat `2 x 3.3 kOhm` as `106_RECONSTRUCTION` evidence until original 106 continuity/schematic reading confirms it.

## 5. Clock network cross-check

The 106 reconstruction converges on the same working external MN3101 family documented in `JUNO106_CHORUS_CLOCK.md`:

```text
~1.8 kOhm / 8.2 kOhm / 150 pF
```

Original-106 evidence is already stronger for:

```text
R124 = R137 = 8.2 kOhm
C53  = C57  = 150 pF
```

so reconstruction data is supporting evidence here, not the authority.

## 6. Control-path asymmetry warning

Alpes Machines explicitly notes that its C59 = 2.2 nF is omitted in original JUNO-106 chorus voice A while present in voice B.

Current interpretation:

- likely belongs to the normal/inverted LFO/control path;
- may contribute to different transient/clock-control residue between channels;
- should not be mistaken for a deliberate L/R audio EQ mismatch.

Strict policy:

```text
CONTROL_PATH_ASYMMETRY = MEASURE_PENDING
```

If confirmed on the original board, model it in `ClockDriver106` / control-trajectory residual rather than by adding arbitrary tone difference to the two audio channels.

## 7. Juno-60 clone as a negative-control topology source

The public `gligli/juno-chorus-clone` project is explicitly a JUNO-60 chorus clone and must not be used as numeric 106 truth.

It is still useful for recognizing recurring Roland chorus structures:

- 2 x MN3009;
- 2 x MN3101;
- two 3.3 kOhm BBD-output summing resistors per BBD;
- mirrored multi-element audio-filter networks;
- independent BBD bias trims;
- dry/wet output summing.

Its mirrored wet filter family includes the same broad capacitor/resistor scales seen in the 106-focused reconstruction, which supports family resemblance but does not prove identity.

Rule:

- use JUNO-60 clone data to catch topology mistakes;
- never use its mode rates, filter coefficients, LFO switching behavior or exact component values as strict 106 constants unless independently confirmed on 106.

## 8. SPICE candidate model

The next desktop-only experiment should build two models:

```text
A) 106_RECONSTRUCTION candidate netlist
B) intentionally simplified reference fit
```

Model A uses the One-O-Six mirrored passive family and explicit MN3009 input/output summing topology.

Model B uses the minimum cascade/biquad structure required to reproduce A's small-signal transfer at several fixed clock points.

Acceptance rule:

- B may replace A in the eventual FM-1 target only if magnitude, phase/group-delay and insertion-gain errors remain below the measured original-106 acceptance thresholds.

Do not optimize against the reconstruction alone. The final fit target remains original hardware.

## 9. Original-106 closure tests

To promote reconstruction values to strict constants:

1. archive a clean original jack-board schematic crop or physical continuity map;
2. identify MN3009 pins 3, 7, 8 and both CP inputs for both channels;
3. trace each resistor/capacitor node around the wet path;
4. verify BBD-output summing resistor values;
5. measure complex wet transfer on a calibrated original 106 at slow/center/fast clock points;
6. compare the measured transfer with the reconstruction SPICE model;
7. retain only component/value claims that survive both topology and measurement comparison.

## 10. Current freeze state

Frozen at architecture level:

- wet filtering is multi-element and mirrored;
- two MN3009 output taps are combined before downstream wet processing in the candidate 106 reconstruction;
- a 20 Vp-p normal/inverted LFO pair is a useful 106 reconstruction calibration anchor;
- 106-specific reconstruction documentation distinguishes original parts from optional anti-thump/MN3007 modifications;
- control-path channel asymmetry is plausible and testable, but not yet a strict constant.

Still open numerically:

- original 106 wet-path node map;
- original resistor/capacitor designators and tolerances;
- exact pre/post split around MN3009;
- output-tap summing values on the original board;
- complex transfer vs BBD clock;
- whether the reported one-channel 2.2 nF omission materially affects audible clock trajectory;
- exact dry/wet and output-stage voltage gains.
