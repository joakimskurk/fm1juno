# JUNO-106 A1QH80017A VCF/VCA — Phase 0 reference model

Status: **`80017A_PHASE0_ARCHITECTURE_FROZEN`**. The physical topology, calibration surfaces and deterministic validation vectors are fixed. Vintage IR3109/BA662 nonlinear scales, exact CV/current mappings and final fitted passive tolerances remain centralized `MEASURE_PENDING` calibration.

This block begins at each voice's physical source-input network and the held VCF/VCA/RES control boundaries, and ends at the per-voice VCA output before the global voice sum.

## 1. Frozen physical graph

```text
SOURCE_IN
  -> IR3109 stage 1
  -> IR3109 stage 2
  -> IR3109 stage 3
  -> IR3109 stage 4
  -> BA662 voice VCA
  -> VOICE_OUT

resonance/Q-compensation loop:
SOURCE_IN feed-forward ----+
                           -> BA662 resonance differential stage
IR3109 stage-4 feedback ---+       |
                                   +-> filter-input/summing node
```

Strict `MODEL_106` must preserve the feed-forward/Q-compensation path. A generic `x - k*y4` ladder plus one post-filter saturator is not the source model.

## 2. Primary Roland calibration anchors

Roland's service procedure is part of the reference model, not just repair documentation.

Per physical voice after warm-up:

```text
Resonance calibration:
  C4 / Bank 3
  VCF test point -> 4.8 Vp-p self-oscillation

VCA gain calibration:
  same reference condition
  post-VCA voice point -> 6.0 Vp-p

VCF Frequency:
  C4 -> 248 Hz

VCF Width:
  C6 -> 992 Hz

Frequency/Width interaction:
  repeat until within +/-10 cents
```

These anchors constrain the combined digital-control, DAC/S&H, exponential-converter, resonance and VCA model.

## 3. Internal passive family

### 3.1 Shared filter-stage family

The exposed JUNO-6/60 implementation and modern 3109-family documentation support the familiar:

```text
Rstage ~= 68 kOhm
Cstage ~= 240 pF family
```

Direct measurements on opened original 80017A hybrids report approximately:

```text
interstage/filter resistors ~= 68 kOhm
shunt/input resistors       ~= 550..560 Ohm
filter capacitors           ~= 250 pF
```

Reference policy:

```text
Rstage default = 68 kOhm
Rshunt default = 560 Ohm
Cstage default = 250 pF
```

with evidence state `DIRECT_REPAIR_MEASURED_WORKING_DEFAULT`, not primary Roland truth.

The 240-vs-250-pF difference remains a calibration dimension because the factory Frequency/Width trims can absorb much of the static scale difference.

### 3.2 Reported resonance network

Opened-original measurements report the resonance BA662 surrounding family as approximately:

```text
1.5 kOhm shunt on each differential-input side
5.1 kOhm from VCF input to one resonance input
100 kOhm from VCF output/stage-4 side to the feedback resonance input
3.9 kOhm additional VCF-input/direct-path resistor
```

These values are strong hardware evidence for the relative feed-forward/feedback topology, but exact node gain cannot be promoted merely by treating each pair as an isolated ideal divider. The BA662 input impedance, surrounding summing node and IR3109 input network matter.

Therefore these values are frozen as **working passive evidence**, while final `A_in`, `A_fb`, `G_direct` and loop-current scale remain fitted calibration terms.

### 3.3 Reported voice-VCA network

Opened-original measurements report approximately:

```text
VCA input resistor ~= 4.7 kOhm
local shunts       ~= 560 Ohm
output/load branch ~= 47 kOhm in parallel with an unresolved small capacitor
```

Again, preserve these as working passive evidence without pretending the final BA662 CV-to-gain law is solved.

## 4. IR3109 reference stage

Each stage is modeled as a nonlinear OTA-controlled one-pole followed by its stage buffer.

Reference continuous form:

```text
C_i * dy_i/dt = gm(vc) * Phi_i(v_drive_i - v_state_i)
```

The desktop reference uses a topology-preserving implicit/TPT discretization so resonance and cutoff modulation remain stable near self-oscillation.

Required per-stage calibration capability:

```text
stage[i].gmScale
stage[i].inputScale
stage[i].positiveHeadroom
stage[i].negativeHeadroom
stage[i].slewLimit
stage[i].offset
```

Do not force all four nonlinear stages to be identical. Modern 3109-family documentation distinguishes stage-buffer loading/current capability, so the architecture must permit 1/3 vs 2/4 differences.

## 5. Cutoff-control boundary

Upstream firmware already produces the exact clamped 14-bit VCF control code.

Analogue chain:

```text
vcf_code14
 -> DAC/S&H
 -> per-voice Frequency/Width calibration network
 -> IR3109 exponential converter
 -> common gm control for four stages
```

Reference coordinate:

```text
u = held_vcf_control_coordinate
log2(fc_voice) = offset_voice + width_voice * u + nonlinear_correction(u)
```

`offset_voice` and `width_voice` are mandatory because Roland physically calibrates both Frequency and Width per voice.

`nonlinear_correction` defaults to zero in the first reference pass and is only introduced when hardware data demonstrates meaningful curvature after two-point calibration.

## 6. Modern 3109 data is a constraint, not vintage truth

AS3109 documentation is useful as a sanity envelope:

```text
~ -19 mV/oct typical control sensitivity
~ 250 Hz single-stage cutoff at VC=0 with 68k / 240p
15-octave control range
```

Strict policy:

- do not hard-code `-19 mV/oct` as vintage 80017A truth;
- use it to reject wildly implausible DAC/control fits;
- fit strict `MODEL_106` to Roland Frequency/Width anchors plus physical captures.

## 7. Resonance/Q-compensation model

Reference resonance block:

```text
v_ff = A_in * source_in
v_fb = A_fb * stage4_out
v_diff = v_ff - v_fb

i_res = G_res(res_control, res_cal)
        * Phi_BA662(v_diff; res_nonlinearity)

filter_input = G_direct * source_in + current_to_filter_drive(i_res)
```

This keeps three separable physical ideas:

1. direct signal drive;
2. resonance-controlled stage-4 negative feedback;
3. resonance-controlled input feed-forward/Q compensation.

The measured 5.1k/100k/1.5k/3.9k family constrains `A_in`, `A_fb` and `G_direct`, but final coefficients remain fitted against service/hardware vectors.

## 8. BA662 nonlinear reference

The BA662 is treated as an OTA/VCA element whose small-signal transconductance rises with control current and whose differential/audio transfer becomes nonlinear at larger internal levels.

Reference function:

```text
Iout = gm(Icontrol) * Phi_BA662(Vdiff)
```

The first implementation may use a monotonic tanh/rational differential transfer with separately fitted:

```text
input_scale
small_signal_gm_scale
positive_limit
negative_limit
offset
asymmetry
```

No modern BA662/AS662 numerical constant is silently promoted to vintage truth.

Modern AS662 provides a useful sanity point only: typical forward transconductance is about 8.5 mS at 500 uA control current under its datasheet conditions.

## 9. Voice VCA model

Reference graph:

```text
stage4 audio
 -> reported ~4.7k input network
 -> BA662 voice VCA
 -> local output/load network
 -> per-voice output
```

Control graph:

```text
vca_code/control state
 -> DAC/S&H
 -> 80017A external/internal control conversion
 -> BA662 control current
 -> gain
```

Required state:

```text
voiceVca.gainCal
voiceVca.offsetCal
voiceVca.controlCurve
voiceVca.inputScale
voiceVca.headroom
voiceVca.nonlinearity
```

The Bank-3 service relationship 4.8 Vp-p pre-VCA -> 6.0 Vp-p post-VCA is an end-to-end calibration vector, not permission to hard-code `gain=1.25` globally.

## 10. Self-oscillation must be native

At high resonance with zero external input, the nonlinear feedback network must naturally support oscillation.

Do not inject a separate sine oscillator when RES crosses a threshold.

Reference engine may include extremely small deterministic/device-noise seed below the normal system noise floor so exact floating-point zero does not become an artificial non-oscillating state.

## 11. Numerical solver policy

Desktop/reference priority:

```text
TPT/implicit four-stage solve
+ explicit BA662 resonance block
+ explicit BA662 voice VCA
```

Validation variants:

```text
1x sample rate
2x selective oversampling
stage-shared nonlinearity
stage-specific headroom/nonlinearity
240p reference
250p working 106 reference
```

FM-1 simplification happens only after measured error and CPU cost are known.

## 12. Centralized calibration

All unresolved strict constants live in:

`data/juno106_80017a_calibration_v0.yaml`

No DSP source should scatter guessed values for:

- vintage IR3109 mV/oct;
- BA662 control-current law;
- resonance coefficient;
- 240/250-pF choice;
- self-oscillation limiter amplitude;
- VCA gain curve.

## 13. Deterministic validation vectors

Required regression/measurement vectors:

1. C4 self-oscillation -> 4.8 Vp-p pre-VCA.
2. Same condition -> 6.0 Vp-p post-VCA.
3. C4 Frequency -> 248 Hz.
4. C6 Width -> 992 Hz.
5. Iterate Frequency/Width until both are within +/-10 cents.
6. Resonance sweep from zero to oscillation threshold.
7. Zero-input self-oscillation startup.
8. Large WAVE/SUB/NOISE drive at low/mid/high resonance.
9. Low/mid/high cutoff THD and amplitude.
10. 240p vs 250p sensitivity after recalibration.
11. Stage-specific vs identical-buffer model comparison.
12. 1x vs 2x oversampling spectral/resonance comparison.
13. VCA control sweep at multiple input amplitudes.
14. VCA offset/thump test during envelope/gate transitions.

## 14. Phase-0 closure criterion

Architecture is considered closed when later measurements can replace all remaining coefficients without changing:

```text
four IR3109 stages
explicit resonance BA662 with feed-forward + feedback
explicit voice BA662
per-voice Frequency/Width/Resonance/VCA calibration surfaces
DAC/S&H boundaries
native self-oscillation path
```

That criterion is now satisfied.

Remaining uncertainty is numerical calibration and optional slow thermal behaviour, not topology.

**Phase-0 80017A state: `80017A_PHASE0_ARCHITECTURE_FROZEN`.**
