# JUNO-106 A1QH80017A VCF/VCA topology and reference-model plan

Status: Phase 0 topology freeze / numerical-model design. This document separates facts that are strong enough to freeze from exact component/model constants that still require direct 106 validation.

Primary target is the JUNO-106 A1QH80017A behavior. JUNO-6/60 circuitry is used only as visibility into a demonstrably shared IR3109 + BA662 architecture hidden inside the 106 hybrid.

## 1. Evidence hierarchy

### Tier A — Roland JUNO-106 primary documentation

The 1984 service documentation establishes that:

- each of the six voices has one A1QH80017A VCF/VCA hybrid;
- VCF and VCA are separately controlled by composite control voltages;
- VCF CV contains Cutoff, ENV, LFO, Key Follow and Bender;
- VCA CV contains ENV/GATE behavior;
- the filter is individually calibrated per voice for Resonance, Frequency and Width;
- the VCA is individually calibrated per voice for gain/offset-related behavior;
- the service procedure uses self-oscillation and precise frequency targets as calibration references.

Important factory anchors:

- VCF Resonance: C4, Bank 3, adjust each voice for a 4.8 Vp-p sine at the VCF test point;
- VCA Gain: same test condition, adjust post-VCA voice output for 6 Vp-p;
- VCF Frequency: C4, adjust each voice to 248 Hz (B3);
- VCF Width: C6, adjust each voice to 992 Hz (B5);
- Frequency and Width adjustments interact and must be repeated until within +/-10 cents;
- VCF calibration requires at least ten minutes warm-up.

These are stronger fidelity targets than arbitrary software-normalized cutoff/resonance values.

### Tier B — physical 80017A teardown

A physically opened original 80017A shows:

- one IR3109-family four-stage VCF IC;
- two BA662 VCAs/OTAs;
- SMD capacitors;
- printed resistors/passives on the hybrid substrate.

The teardown identifies one BA662 as resonance-control element and the second as the final per-voice VCA.

This confirms that the 106 is not a generic monolithic "mystery filter" and gives a direct bridge to the discrete JUNO-6/60 implementation.

### Tier B — original JUNO-6/60 topology as circuit visibility

The corresponding JUNO-6/60 filter/VCA circuit exposes the same functional building blocks externally:

- IR3109 wired as four cascaded low-pass stages;
- BA662 in the resonance/Q-compensation network;
- BA662 as final voice VCA.

Independent circuit analysis of the original schematic identifies the standard filter stages as 68 kOhm / 240 pF networks.

This does **not** prove every SMD passive inside every 80017A has the identical numeric value. It does establish the circuit topology and a high-confidence starting family of values.

### Tier C — modern AS3109 electrical reference

The Alfa AS3109 is not assumed bit-for-bit/electrically identical to vintage IR3109 silicon. It is useful as a modern published reference for the intended architecture:

- four variable-transconductance amplifier stages under one exponential control converter;
- approximately 560-ohm input resistance in each amplifier;
- typical 68 kOhm + 240 pF low-pass wiring;
- typical single-stage cutoff around 250 Hz at VC=0 for R=68 kOhm, C=240 pF;
- about -19 mV/oct typical control sensitivity, 15-octave control range;
- stage buffers are not identical: stages 1/3 use passive loads while 2/4 use active current-source loads, with different output-current capability.

Use these values as architectural/modeling constraints, not as proof of exact vintage-device parameters.

### Tier C — direct repair/clone engineering observations

Repair/clone engineers report additional measured original-80017A values and behavior, including roughly:

- 68 kOhm inter-stage/input resistors;
- approximately 550–560 ohm shunts;
- approximately 250 pF filter capacitors in measured 80017A samples;
- separate resonance input/feed-forward and output-feedback resistor networks;
- a final BA662 VCA input network;
- measurable importance of internal nonlinear behavior, extreme-frequency resonance behavior, temperature and supply interaction.

These observations are useful hypotheses/measurement targets. They are not elevated above Roland documentation or physical teardown without independent cross-checking.

## 2. Frozen signal topology

Strict per-voice path:

```text
oscillator mixer
    -> IR3109 stage 1
    -> IR3109 stage 2
    -> IR3109 stage 3
    -> IR3109 stage 4
    -> BA662 voice VCA
    -> voice output / summing bus
```

Resonance is a separate BA662-controlled loop around the four filter stages.

The resonance BA662 receives both:

1. filtered stage-4 output as feedback into one differential input;
2. a feed-forward/input signal into the other input.

Its output is returned to the filter input/summing node.

This means increasing Resonance changes **both feedback and effective input drive/passband compensation**. Strict mode must preserve that topology.

## 3. Q / passband compensation

A conventional four-pole feedback low-pass loses passband level as resonance increases. Roland compensates this in the JUNO circuit by routing an input-derived component through the same resonance BA662 differential stage.

Conceptually, the resonance OTA sees a differential quantity of the form:

```text
v_res_diff = A_in * x - A_fb * y4
```

and injects a resonance-controlled current/signal back into the filter input:

```text
r = G_res(control) * Phi_BA662(v_res_diff)
filter_drive = G_direct * x + r
```

where:

- `x` is the pre-filter oscillator mix;
- `y4` is the four-pole output;
- `Phi_BA662` is the BA662's nonlinear differential transfer;
- `A_in`, `A_fb`, `G_direct` and `G_res` are circuit/calibration terms.

This is a topology statement, not yet a frozen numeric equation.

A model that simply computes `u = x - k*y4` and places one generic saturator after the four poles misses both the Q-compensation input path and the physical location of limiting.

## 4. Per-stage IR3109 model

Each IR3109 section is an OTA-controlled one-pole integrator followed by a high-impedance buffer.

A useful continuous-time reference form is:

```text
C * dy/dt = I_bias(VC) * Phi_IR3109(v_in - y)
```

or an equivalent transconductance form:

```text
dy/dt = gm(VC)/C * Phi_norm(v_in - y)
```

where `gm(VC)` is controlled exponentially by the common VCF control input.

### 4.1 Do not assume four perfectly identical nonlinear stages

Modern AS3109 documentation explicitly distinguishes buffer loading/current capability among stages:

- stages 1 and 3: passive-load buffers;
- stages 2 and 4: active-current-source-load buffers;
- stage 4 has the highest specified positive output-current capability.

Even if the first reference implementation shares the same pole coefficient, it should permit stage-specific:

- output headroom;
- buffer saturation/asymmetry;
- DC offset;
- slew/current limit;
- tolerance.

Do not bake the assumption "four identical tanh one-poles" so deeply into the architecture that this cannot be corrected later.

## 5. Nonlinearity placement

Evidence supports nonlinear behavior in several physically distinct places:

1. each IR3109 OTA differential stage;
2. IR3109 output buffers/current limits;
3. BA662 resonance differential stage;
4. final BA662 voice VCA;
5. possibly passive/source-loading interactions at high internal levels.

The first fidelity reference should therefore avoid one global post-filter waveshaper.

### Candidate first nonlinear approximation

For IR3109 OTA differential behavior:

```text
Phi_IR3109(v) ~= Vscale * tanh(v / Vscale)
```

or a cheap monotonic rational approximation with equivalent small-signal slope and saturation.

For BA662:

```text
Phi_BA662(v) ~= Bscale * tanh((v + offset) / Bscale)
```

with explicit gain/bias/offset calibration capability.

`Vscale`, `Bscale`, offsets and asymmetry are **not frozen constants** yet. They should be fitted to service/hardware vectors rather than copied from an unrelated OTA model.

## 6. Discretization candidate

The desktop reference model should use an implicit/topology-preserving one-pole form per stage so cutoff modulation can change at firmware/S&H cadence without instability.

Preferred first candidate:

- trapezoidal/TPT integrators;
- nonlinear OTA equation solved implicitly or with one/two Newton iterations;
- optional 2x oversampling around the nonlinear four-pole block during reference validation;
- downselect to a cheaper fixed-point/rational formulation only after error and FM-1 cost are measured.

Reasoning:

- the physical OTA nonlinearity sits inside the integrator loop;
- explicit one-pole saturation can shift cutoff/resonance incorrectly at high signal levels;
- self-oscillation is a mandatory calibration state, so numerical stability at high Q matters more than convenience.

## 7. Cutoff-control model

The digital VCF arithmetic is frozen separately in `JUNO106_VCF_CONTROL.md` and produces a clamped 14-bit per-voice control value.

The analogue boundary is therefore:

```text
vcf_code_14
  -> DAC/S&H voltage
  -> per-voice frequency offset/width calibration
  -> IR3109 exponential converter
  -> gm / pole frequency
```

### 7.1 Per-voice calibration parameters are mandatory

Roland provides separate Frequency and Width trims per voice. The model must therefore support at least:

```text
log2_fc = offset_voice + width_voice * control_coordinate
```

or an equivalent affine calibration before the exponential frequency conversion.

One global cutoff curve plus random voice offsets is insufficient because the hardware explicitly calibrates both intercept and slope.

### 7.2 Factory anchor pair

The Bank-3 service procedure supplies two especially useful points after warm-up:

```text
C4 test -> self-oscillation target 248 Hz
C6 test -> self-oscillation target 992 Hz
```

The second target is exactly four times the first over a two-octave keyboard change. The Frequency and Width trims interact until both points are within +/-10 cents.

This is a strong validation pair for the combined digital-control + S/H + exponential converter + per-voice calibration model.

Do not infer that every arbitrary panel/control code maps to a perfect one-octave-per-unit law solely from these two service points; use them as calibration constraints.

## 8. Resonance model and calibration

Roland calibrates each voice's resonance separately and specifies a self-oscillating C4 sine of approximately 4.8 Vp-p at the pre-VCA filter test point.

Model requirements:

- continuous progression from low Q to self-oscillation;
- BA662-controlled feedback amount;
- input/passband compensation coupled to resonance;
- soft amplitude limiting in/around the resonance OTA rather than hard digital clipping;
- per-voice resonance calibration;
- stable sinusoidal self-oscillation across the useful cutoff range;
- no startup-only fake sine generator.

### Required resonance vectors

- silence/input zero, resonance sweep through oscillation threshold;
- C4 self-oscillation -> 4.8 Vp-p hardware-domain target;
- same resonance setting at low, mid and high cutoff;
- large oscillator input plus high resonance;
- filter sweep across self-oscillation range;
- compare attack/transient behavior after large cutoff jumps.

## 9. Voice VCA model

The second BA662 is the final per-voice VCA.

Roland service calibration establishes:

- per-voice VCA bias/offset procedures;
- per-voice VCA gain trim;
- after resonance calibration, Bank-3/C4 is adjusted to 6 Vp-p at the per-voice post-VCA test point.

Strict VCA should support:

- control law fitted to the actual VCA-CV domain;
- gain trim;
- bias/offset/thump behavior;
- BA662 differential/current-control saturation at high signal levels;
- ENV versus GATE control behavior supplied by the digital/S&H layer.

Secondary measured passive values around original 80017A VCA/resonance networks should remain configurable hypotheses until independently cross-checked.

## 10. 240 pF vs ~250 pF: do not collapse the evidence

The exposed JUNO-6/60 circuit and AS3109 reference use 240 pF filter capacitors with 68 kOhm stages.

A repair engineer's direct measurements on opened 80017A hybrids report filter capacitors around 250 pF and resistors around 68 kOhm / 550–560 ohm.

The difference is small but potentially relevant:

- it changes raw pole scale before calibration;
- dielectric/package behavior can affect losses/nonlinearity/temperature;
- Roland's Frequency/Width trims can compensate much of the static absolute frequency difference.

Therefore strict 106 reference work should carry this as an explicit parameter/hypothesis:

```text
C_stage_106 = measured/fitted value near 240–250 pF
```

not silently assume either `240 pF because JUNO-60` or `250 pF because one repair measurement`.

## 11. Thermal and supply behavior

Roland's ten-minute warm-up requirement proves temperature matters to calibration at a practical service level.

Modern clone engineering also reports audible/measurable transient tuning differences under extreme self-oscillation/cutoff changes due to thermal distribution, and sensitivity to supply behavior.

For Phase 1:

- do **not** build a thermal simulator into the first audio path;
- make cutoff calibration/state architecture capable of adding a very slow thermal drift term later;
- first validate static warmed-up behavior;
- only add thermal/supply cross-modulation if A/B measurements show material audible value relative to FM-1 cost.

## 12. Noise and self-oscillation seeding

A real analogue filter has device noise and will eventually self-oscillate without a deterministic digital kick.

The digital model needs a physically plausible mechanism to escape exact digital zero at high resonance, but must not use a large always-on arbitrary noise injection that changes normal SNR.

Reference strategy:

- model measured/pre-filter oscillator/noise floor normally;
- permit extremely small per-voice device noise in the filter/VCA path;
- measure whether that naturally seeds oscillation quickly enough;
- only add explicit startup seeding if hardware timing demands it, and make it below the normal noise floor.

## 13. Proposed reference API

```text
Filter80017Voice
  set_vcf_code(uint16 code14)
  set_resonance_control(uint16/uint8 code)
  set_frequency_cal(offset)
  set_width_cal(scale)
  set_resonance_cal(scale)
  process(sample)

IR3109Stage[4]
  set_gm(...)
  process_implicit(...)

BA662Resonance
  set_control(...)
  process(input_feedforward, stage4_feedback)

BA662VoiceVca
  set_control(...)
  set_gain_cal(...)
  set_bias_offset(...)
  process(...)
```

The API keeps resonance topology explicit instead of hiding it inside a generic `res` coefficient.

## 14. Phase-1 desktop reference experiments

Before FM-1 optimization, build a host reference capable of:

1. linear four-stage sweep with resonance off;
2. nonlinear per-stage model A/B against linear stages;
3. resonance loop with and without Roland input/Q compensation;
4. warmed-up self-oscillation calibration to service targets;
5. per-voice Frequency/Width calibration using the 248/992 Hz anchor pair;
6. BA662 VCA calibration from 4.8 Vp-p pre-VCA to 6 Vp-p post-VCA service condition;
7. 240 pF vs 250 pF sensitivity run;
8. 1x vs 2x oversampling alias/resonance test;
9. stage-identical vs stage-specific buffer/headroom model;
10. large cutoff jumps while self-oscillating to expose transient/numerical errors.

Record CPU cost and error for every variant so the final FM-1 simplification is evidence-based.

## 15. Frozen now

- 80017A contains IR3109 + two BA662-class devices and associated passives;
- four-pole 24 dB/oct low-pass topology;
- one BA662 in the resonance/Q-compensation loop;
- second BA662 as final voice VCA;
- resonance topology includes both stage-4 feedback and input feed-forward/Q compensation;
- filter nonlinearity belongs inside OTA/buffer/resonance/VCA nodes, not one post-filter shaper;
- per-voice Frequency, Width, Resonance and VCA calibration terms are required;
- digital 14-bit VCF control is a separate upstream block;
- first reference implementation should preserve four explicit stages and explicit BA662 resonance/VCA blocks.

## 16. Still open / do not guess

- exact vintage IR3109 transconductance/nonlinear scale versus modern AS3109;
- exact capacitor value/distribution inside representative 80017A units (240 vs ~250 pF question);
- exact original 80017A printed-resistor values/tolerances beyond corroborated measurements;
- BA662 differential transfer scale/asymmetry and control-current law in this exact circuit;
- precise VCF DAC/S&H voltage-to-IR3109-CV gain/offset;
- exact self-oscillation threshold curve versus cutoff;
- warmed-up voice-to-voice variation after proper calibration;
- thermal/supply dynamics worth preserving on FM-1;
- aliasing cost/benefit of 2x processing on the target.

Every open numerical parameter must enter Phase 1 either as a fitted/calibrated parameter or with an explicit measurement plan; no generic 'Roland filter' constant may silently become strict 106 truth.

## 17. Source index

Primary / strong sources:

- Roland `JUNO-106 Service Notes`, First Edition, 31 July 1984.
- Original JUNO-6/60 service schematic for the exposed shared circuit topology.
- Physical original-80017A teardown by Obsolete Technology.

Electrical/circuit references:

- Alfa RPAR AS3109 datasheet, used as a modern architectural/electrical reference, not vintage-proof.
- Electric Druid analysis of Roland IR3109 filter implementations, especially JUNO-6 Q compensation.

Secondary engineering observations:

- Sound Doctor opened-80017A passive measurements.
- Analogue Renaissance D80017/AR80017A clone engineering and calibration observations.
- AMSynths IR3109/80017A analysis.

URLs and provenance are maintained in `docs/research/SOURCE_INDEX.md`.