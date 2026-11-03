# JUNO-106 A1QH80017A VCF/VCA — Phase 0 closure record

Status: **`80017A_PHASE0_ARCHITECTURE_FROZEN`**.

Purpose: close the VCF/VCA architecture gate without turning modern replacement-IC data or one teardown measurement into fake vintage certainty.

## 1. What is closed

The strict per-voice graph is fixed as:

```text
source input
 -> IR3109 stage 1
 -> IR3109 stage 2
 -> IR3109 stage 3
 -> IR3109 stage 4
 -> BA662 voice VCA
 -> voice output

source input feed-forward ----+
                              -> BA662 resonance/Q-compensation stage
stage-4 output feedback ------+       |
                                      +-> filter input/summing node
```

This topology is invariant under later fitting of vintage nonlinear scale, passive tolerances and CV/current mappings.

## 2. Working original-hybrid passive evidence

Direct measurements on opened original 80017A hybrids provide a useful numerical family:

```text
filter/interstage R  ~= 68 kOhm
local/shunt R        ~= 550..560 Ohm
filter C             ~= 250 pF

resonance network:
  shunts             ~= 1.5 kOhm each
  feed-forward input ~= 5.1 kOhm
  feedback from VCF  ~= 100 kOhm
  direct/input path  ~= 3.9 kOhm

voice VCA:
  input              ~= 4.7 kOhm
  local shunt        ~= 560 Ohm
  output/load        ~= 47 kOhm || unresolved small C
```

These are frozen as `DIRECT_REPAIR_MEASURED_WORKING_EVIDENCE`, not primary Roland constants.

## 3. Modern replacement data policy

AS3109 and AS662 published data are retained only as plausibility/sanity envelopes.

Examples:

```text
AS3109 ~ -19 mV/oct typical
AS3109 ~250 Hz at VC=0 with 68k/240p
AS662  ~8.5 mS gm at 500 uA typical
```

They are **not** strict vintage coefficients.

## 4. Calibration architecture is fixed

Every physical voice owns explicit calibration state for:

```text
VCF Frequency/intercept
VCF Width/slope
Resonance amount
VCA gain
audio/control offsets
```

The analogue cutoff boundary is represented as an affine log-frequency calibration plus optional measured correction, rather than one global arbitrary cutoff curve.

The resonance block keeps independent:

```text
input feed-forward coefficient
stage-4 feedback coefficient
direct signal coefficient
BA662 control/current law
BA662 differential nonlinearity
```

The VCA keeps independent:

```text
control/current law
gain trim
offset trim
input scaling
headroom/nonlinearity
```

## 5. Roland anchors are mandatory regression tests

Per voice after warm-up:

```text
4.8 Vp-p self-oscillation pre-VCA
6.0 Vp-p post-VCA
248 Hz at C4 Frequency test
992 Hz at C6 Width test
+/-10 cents after iterative Frequency/Width adjustment
```

A candidate numerical model that cannot be calibrated to these anchors is rejected even if it sounds subjectively Juno-like.

## 6. Native self-oscillation requirement

Self-oscillation must arise from the actual four-stage/resonance loop.

Forbidden:

```text
if resonance > threshold:
    add_sine_oscillator()
```

A tiny deterministic/device-noise seed may be used only to avoid exact digital-zero lock, below the measured normal noise floor.

## 7. Remaining calibration backlog

```text
[ ] fit vintage IR3109 differential nonlinear scale
[ ] fit stage-buffer headroom/asymmetry/slew
[ ] measure/freeze final 240-vs-250-pF effective stage capacitance distribution
[ ] fit resonance BA662 differential/input-current law
[ ] fit exact passive-to-effective A_in/A_fb/G_direct coefficients
[ ] measure VCF code/S&H -> cutoff curve for all voices
[ ] fit voice BA662 VCA control -> gain/headroom curve
[ ] measure VCA offset/thump behavior
[ ] decide whether slow thermal/supply dynamics justify target CPU/state
[ ] choose 1x vs selective 2x deployment after reference A/B + profiling
```

All items above can replace centralized calibration without changing the graph.

## 8. Central artifacts

- `JUNO106_80017A_TOPOLOGY.md` — detailed evidence/topology research.
- `JUNO106_80017A_REFERENCE_MODEL.md` — current implementation contract.
- `data/juno106_80017a_calibration_v0.yaml` — centralized strict calibration.
- `data/juno106_80017a_evidence.csv` — evidence state per fact/value.
- `PHASE0_MEASUREMENT_PLAN.md` — acquisition/acceptance routes.

## 9. Closure

**Result: 80017A Phase-0 architecture gate is closed. Remaining work is numerical calibration, physical fitting and FM-1 cost reduction, not an unresolved filter/VCA topology decision.**
