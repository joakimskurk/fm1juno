# JUNO-106 A1QH80017A VCF/VCA topology and reference-model plan

Status: **`80017A_PHASE0_ARCHITECTURE_FROZEN`** for topology and implementation contract. This file remains the detailed topology/evidence companion to `JUNO106_80017A_REFERENCE_MODEL.md`. Vintage nonlinear scales, exact CV/current mappings and passive tolerances remain centralized `MEASURE_PENDING` calibration.

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

This confirms that the 106 is not a generic monolithic mystery filter and gives a direct bridge to the discrete JUNO-6/60 implementation.

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

Direct measurements on opened original-80017A units report approximately:

```text
filter/interstage R  ~= 68 kOhm
local/shunt R        ~= 550..560 Ohm
filter capacitors    ~= 250 pF

resonance BA662 region:
  two local shunts   ~= 1.5 kOhm
  input feed-forward ~= 5.1 kOhm from VCF input
  feedback           ~= 100 kOhm from VCF output
  additional input   ~= 3.9 kOhm

voice BA662 region:
  VCA input          ~= 4.7 kOhm
  local shunts       ~= 560 Ohm
  output/load        ~= 47 kOhm || unresolved small capacitor
```

These are now retained as `DIRECT_REPAIR_MEASURED_WORKING_EVIDENCE` in the centralized evidence/calibration files. They constrain the first desktop reference, but do not outrank Roland primary documentation or become universal vintage constants without further physical cross-checking.

## 2. Frozen signal topology

Strict per-voice path:

```text
oscillator/source mixer
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

- `x` is the pre-filter oscillator/source mix;
- `y4` is the four-pole output;
- `Phi_BA662` is the BA662's nonlinear differential transfer;
- `A_in`, `A_fb`, `G_direct` and `G_res` are circuit/calibration terms.

The directly measured 5.1k/100k/1.5k/3.9k passive family now constrains this graph, but final effective coefficients remain calibration parameters because the BA662 input impedance and surrounding summing network matter.

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

Candidate numerical functions remain calibration placeholders, not vintage constants. The first implementation may use monotonic tanh/rational approximations with separately fitted small-signal slope, saturation, offset and asymmetry.

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

### 7.3 Modern control sensitivity is sanity data only

AS3109 documentation gives approximately `-19 mV/oct` typical sensitivity and about 250 Hz for a 68k/240p single-stage reference at VC=0.

These values define a plausibility envelope. They are not promoted to strict vintage IR3109/80017A constants.

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

The reference model keeps `A_in`, `A_fb`, `G_direct`, resonance control-current law and BA662 nonlinear scale separate so measured passive evidence and physical captures can be reconciled without changing topology.

## 9. Voice VCA model

The second BA662 is the final per-voice VCA.

Roland service calibration establishes:

- per-voice VCA bias/offset procedures;
- per-voice VCA gain trim;
- after resonance calibration, Bank-3/C4 is adjusted to 6 Vp-p at the per-voice post-VCA test point.

Strict VCA supports:

- control law fitted to the actual VCA-CV domain;
- gain trim;
- bias/offset/thump behavior;
- BA662 differential/current-control saturation at high signal levels;
- ENV versus GATE control behavior supplied by the digital/S&H layer.

The measured ~4.7k input / ~560-ohm local-shunt / 47k output-load family is retained as working hardware evidence.

Modern AS662's roughly 8.5 mS typical gm at 500 uA is used only as a sanity reference, never as an original BA662 constant.

## 10. 240 pF vs ~250 pF: do not collapse the evidence

The exposed JUNO-6/60 circuit and AS3109 reference use 240 pF filter capacitors with 68 kOhm stages.

Direct measurements on opened 80017A hybrids report filter capacitors around 250 pF and resistors around 68 kOhm / 550–560 ohm.

The difference is small but potentially relevant:

- it changes raw pole scale before calibration;
- dielectric/package behavior can affect losses/nonlinearity/temperature;
- Roland's Frequency/Width trims can compensate much of the static absolute frequency difference.

Therefore strict 106 reference work carries this explicitly:

```text
working 106 C_stage = 250 pF  // direct repair measured default
alternate reference = 240 pF // shared-family/AS3109 comparison
```

The final effective value/distribution remains measurement fitting.

## 11. Thermal and supply behavior

Roland's ten-minute warm-up requirement proves temperature matters to calibration at a practical service level.

For Phase 1:

- do not build a thermal simulator into the first audio path;
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
  set_resonance_control(...)
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

## 15. Phase-0 closure

Current implementation authority:

- `JUNO106_80017A_REFERENCE_MODEL.md`;
- `80017A_PHASE0_CLOSURE.md`;
- `data/juno106_80017a_calibration_v0.yaml`;
- `data/juno106_80017a_evidence.csv`.

Frozen:

- four explicit IR3109-family stages;
- explicit BA662 resonance/Q-compensation stage with both stage-4 feedback and source feed-forward;
- explicit BA662 voice VCA;
- per-voice Frequency, Width, Resonance and VCA calibration surfaces;
- topology-preserving nonlinear/implicit reference solver direction;
- native self-oscillation requirement;
- working direct-repair passive family retained with evidence qualification;
- modern AS3109/AS662 values retained only as sanity references.

Calibration backlog remains:

- vintage IR3109 nonlinear scale and stage-buffer headroom/asymmetry;
- effective 240-vs-250-pF distribution/tolerance;
- exact resonance passive-to-effective coefficient fit and BA662 control law;
- VCF code/S&H -> cutoff mapping;
- BA662 voice-VCA CV/current -> gain/headroom law;
- VCA offset/thump behaviour;
- optional thermal/supply dynamics after static warmed-up validation;
- 1x vs selective-2x target deployment decision after profiling.

These quantities can replace centralized calibration without changing the graph.

**Phase-0 80017A state: `80017A_PHASE0_ARCHITECTURE_FROZEN`.**
