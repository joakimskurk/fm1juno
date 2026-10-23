# JUNO-106 chorus filtering and wet-path signal model

Status: Phase 0 companion to `JUNO106_CHORUS.md` and `JUNO106_CHORUS_CLOCK.md`.

Purpose: define what is already safe to implement about the audio path around the two MN3009 devices, while explicitly refusing the common shortcut of replacing the entire path with one arbitrary low-pass filter.

## 0. Evidence rule

The original JUNO-106 schematic is authoritative.

The Alpes Machines One-O-Six schematic is useful because it is a readable 106-focused hardware reconstruction and its documentation explicitly identifies which options correspond to original JUNO-106 choices. It is used to recover topology/component relationships where the original scan is difficult to read.

It is **not** automatically treated as the original circuit. Added input gain, external-LFO, LED, mute, power-supply and optional anti-thump parts are excluded from strict 106 unless corroborated by Roland.

## 1. No single generic chorus low-pass

The strict 106 wet path must not be represented as:

```text
input -> variable delay -> one LPF -> mix
```

Both the original schematic and the 106-focused reconstruction show several analogue functions around each MN3009:

- AC coupling / level conditioning;
- bias injection at the BBD input;
- BBD output-pair summing;
- multiple transistor/RC filtering stages;
- clock-leakage suppression;
- output coupling/buffering;
- final wet/dry summing/output stage.

Therefore the reference model should expose a **cascade of physical sections**, even if later FM-1 optimization collapses some linear sections into a lower-order equivalent transfer function.

## 2. Symmetric BBD channels

The reconstruction preserves two substantially symmetric wet paths.

Channel A working component family includes:

```text
MN3009 U1
MN3101 U2
BBD output pair resistors: 3.3 kOhm + 3.3 kOhm
main repeated audio resistors: 22 kOhm family
capacitor family around the wet path:
  680 pF
  270 pF
  820 pF
  1.8 nF
additional coupling/filter capacitors:
  2.2 nF family
  1 uF / 10 uF coupling family
```

Channel B mirrors those values around U3/U4.

The component symmetry strongly supports one shared `BbdAudioPath106` topology with per-channel component tolerances rather than two unrelated DSP designs.

## 3. Concrete working value sets from the 106 reconstruction

These values are preserved as **cross-check data**, not yet as a strict original netlist.

### Channel A

```text
R52 = 3.3 kOhm
R53 = 3.3 kOhm
R58 = 47 kOhm
R62 = 22 kOhm
R68 = 22 kOhm
R74 = 22 kOhm
R78 = 22 kOhm
R71 = 22 kOhm
R83 = 22 kOhm

C24 = 680 pF
C30 = 270 pF
C19 = 820 pF
C28 = 1.8 nF
C16 = 2.2 nF
C9  = 2.2 nF
```

### Channel B

```text
R54 = 3.3 kOhm
R55 = 3.3 kOhm
R60 = 47 kOhm
R64 = 22 kOhm
R69 = 22 kOhm
R75 = 22 kOhm
R79 = 22 kOhm
R72 = 22 kOhm
R84 = 22 kOhm

C25 = 680 pF
C31 = 270 pF
C20 = 820 pF
C29 = 1.8 nF
C18 = 2.2 nF
C10 = 2.2 nF
```

The reconstruction documentation also identifies an asymmetry relevant to the original 106:

- a 2.2 nF option equivalent to `C59` is omitted in original JUNO-106 chorus voice A while present in voice B;
- this may relate to filtering/transient control in the inverted-LFO branch.

This should be treated as a real **candidate left/right asymmetry**, not erased by blindly mirroring every capacitor.

## 4. First-order corner-frequency sanity checks

The values above do **not** prove which resistor is paired with which capacitor in the original transfer function, but they provide useful scale checks.

For a simple single RC pair with `R = 22 kOhm`:

```text
C = 270 pF -> fc ~= 26.8 kHz
C = 680 pF -> fc ~= 10.6 kHz
C = 820 pF -> fc ~= 8.82 kHz
C = 1.8 nF -> fc ~= 4.02 kHz
C = 2.2 nF -> fc ~= 3.29 kHz
```

These very different corners reinforce that the network is a multi-pole / multi-zero analogue conditioning system rather than one 14 kHz low-pass.

Do not assign these corner values directly to the final transfer until the exact nodes are mapped.

## 5. BBD output-pair summing

MN3009 provides complementary/interleaved output pins that are combined externally.

The 106-focused reconstruction uses equal 3.3 kOhm resistors on each BBD output before the subsequent wet-path filtering.

Reference model requirement:

```text
BbdCore106
  output_phase_a
  output_phase_b

BbdOutputCombiner106
  combine(output_phase_a, output_phase_b)
```

A deployment implementation may mathematically collapse the pair into one equivalent sampled output, but reference tests should keep the combiner concept visible because imperfect phase/output balance can contribute to clock residue and HF response.

## 6. BBD input bias is part of the audio transfer

Roland's service procedure adjusts each BBD bias for symmetrical clipping. The 106 reconstruction injects an adjustable negative DC bias at MN3009 pin 3 and preserves separate A/B trimmers.

Therefore the pre-BBD path should be represented as:

```text
x_ac = input_coupling_and_prefilter(x)
x_biased = x_ac + bias[channel]
x_bbd = bbd_input_transfer(x_biased)
```

not as a post-delay waveshaper.

Bias affects available positive/negative headroom before bucket transfer and can therefore change distortion and effective noise behavior.

## 7. Optional reconstruction parts that must NOT leak into strict 106

The One-O-Six documentation explicitly identifies several improvements/options that are not original strict-106 behavior:

- `C42 = 10 nF` plus `R110 = 100 kOhm` anti-thump high-pass borrowed from Boss CE-300; original 106 uses a wire at the corresponding R110 position;
- `C60` / `C56 = 100 uF` clock-leakage additions for MN3007 conversion; omitted with MN3009;
- optional `C62 = 220 pF` inversion-transient tuning;
- modern input-gain stage;
- external-LFO path;
- direct-mute feature;
- LED/VU circuitry;
- optional +/-12 V regulators.

Strict `MODEL_106` excludes those additions unless a separate original-106 source proves otherwise.

This is important because a clone can sound "cleaner" than the original precisely because of such anti-thump/clock-leakage changes.

## 8. Original-style output stage

The reconstruction documentation identifies paired networks:

```text
Channel A: R102 / R107 / R104 / C40
Channel B: R103 / R108 / R106 / C41
```

as the JUNO-106-style output stage, designed for approximately 1:1 input/output gain in that reconstruction.

Working optional values shown in the schematic are:

```text
33 kOhm
2.2 kOhm
8.2 kOhm
1 nF
```

per channel family.

This is useful as a topology cross-check for the final wet/dry/output stage, but exact original designator mapping and analogue gain must still be read from the Roland schematic before strict numerical freeze.

## 9. Dry/wet mixing remains separate from wet-path filtering

The final model must keep three concepts independent:

```text
wet_path_transfer(channel)
dry_path_transfer(channel)
final_analog_sum(dry, wet)
```

Why:

- dry path bypasses the BBD delay/filter chain;
- the wet path has clock-dependent bandwidth and BBD distortion;
- summing resistors/output stages set the final balance and headroom;
- measuring a 55/45 overall balance does not identify the internal wet-path gain by itself.

Do not tune BBD filter gain merely to force the final 55/45 ratio.

## 10. Reference topology proposal

The desktop model should start with explicit sections:

```text
BbdAudioPath106[channel]
  InputCoupling106
  PreBbdFilter106
  Bias106
  Mn3009Equivalent106
  OutputPairCombiner106
  PostBbdFilterA106
  PostBbdFilterB106
  WetOutputBuffer106
```

This is intentionally more granular than the eventual FM-1 implementation.

Once the original node map is frozen, all **linear time-invariant** sections around the time-varying BBD can be reduced analytically while preserving:

- magnitude;
- phase;
- gain;
- DC blocking;
- noise bandwidth.

Nonlinear bias/headroom stays separate.

## 11. Exact-netlist closure task

Before strict numeric freeze, create a node table from the original Roland schematic:

```text
node_id
source component/pin
destination component/pin
R/C value
function
channel A/B/common
confidence
```

Minimum nodes to map per channel:

1. signal split from global output;
2. BBD input coupling;
3. bias injection node;
4. MN3009 input pin;
5. MN3009 two output pins;
6. 3.3 kOhm output combiner;
7. each transistor/RC filter stage;
8. wet output node;
9. dry/wet summing node;
10. final L/R output buffer.

No transfer-function coefficient should be labeled `STRICT_FROZEN` until this table is complete.

## 12. Measurement plan for transfer fitting

Even with a complete schematic, fit against hardware.

### A. Chorus OFF

Measure dry path transfer alone:

- 20 Hz–20 kHz magnitude;
- phase/group delay;
- gain;
- headroom.

### B. Wet-only internal test

At a stable/frozen or very slow clock point, measure from BBD input node to wet output node.

Repeat at low/center/high clock frequency.

This separates:

- static analogue RC response;
- clock-dependent BBD aperture/sample response;
- BBD insertion loss.

### C. Full chorus

Fit L/R output with measured dry path + measured wet paths and independently measured clock trajectory.

Only then solve the final dry/wet summer gains.

## 13. FM-1 simplification target

The likely efficient result is **not** a transistor-by-transistor runtime model.

After fitting, each channel can probably become:

```text
static prefilter IIR
clock-derived fractional delay/BBD state
clock-dependent HF loss term
static postfilter IIR cascade
cheap bias/headroom nonlinearity
wet gain
```

The reference model remains explicit so the reduced implementation can be validated section-by-section.

## 14. Freeze state

### Frozen at architecture level

- multiple analogue filtering/conditioning stages exist around each MN3009;
- two channels are substantially symmetric but strict model must allow a real A/B asymmetry;
- BBD output phases are externally combined;
- BBD bias precedes the delay and controls clipping symmetry;
- dry path and wet filtering are separate;
- one generic 10–14 kHz LPF is rejected as the source model;
- 106-clone optional anti-thump/MN3007 improvements must not leak into strict 106;
- reference engine will expose prefilter, BBD, output combiner, postfilter, output buffer and final summer as separate conceptual blocks.

### Open before numeric freeze

- complete original node-to-node Roland netlist for both BBD channels;
- exact original values of each pre/post filter passive;
- exact original A/B asymmetry around the 2.2 nF branch;
- dry-path transfer;
- wet-path transfer at several clock frequencies;
- phase/group-delay behavior;
- BBD insertion loss vs clock;
- final wet/dry summing resistor/gain law;
- exact clock-feedthrough suppression and residual spectrum.
