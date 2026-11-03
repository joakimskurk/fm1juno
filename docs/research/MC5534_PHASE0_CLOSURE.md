# JUNO-106 MC5534 wave generator — Phase 0 closure record

Status: **`MC5534_PHASE0_ARCHITECTURE_FROZEN`**.

Purpose: close the wave-generator architecture gate without pretending that scan-limited/reset-transient calibration is already measured.

## 1. What is closed

The strict reference graph is fixed as:

```text
8253 physical timing
 -> positive-edge reset event
 -> finite-reset analogue ramp state

held DCO CV
 -> RANGE-selected slope resistor
 -> C54-family integrator
 -> physical ramp

physical ramp
 -> SAW gate
 -> PWM comparator against held PWM CV
 -> WAVE output

DCO timing
 -> /2 SUB state
 -> analogue SUB amplitude stage
 -> SUB output
```

This graph will not change when later hardware measurements replace provisional capacitance, reset or headroom values.

## 2. Evidence-backed invariants

- DCO CV is a held analogue slope-control signal.
- DCO CV contains key/LFO/Bender/Portamento/Tune information but not RANGE.
- RANGE is applied after DCO-CV generation through R85/R86/R87 selection.
- 16' explicitly uses R85.
- positive-going DCO edges initiate C54 discharge/reset.
- the saw/pulse design target is approximately 12 Vp-p, not exact normalized amplitude.
- PWM is derived by comparing the same physical ramp against held PWM CV.
- SUB is a timing-domain divide-by-two source.
- factory PWM voltage/duty anchors and tolerances are retained as validation vectors.

## 3. Working physical component family

```text
16' R85 ~= 399 kOhm
 8' R86 ~= 200 kOhm
 4' R87 ~= 100 kOhm
C54      ~= 1 nF
```

The topology/mapping is primary-supported. The numerical resistor/capacitor reads remain strongly cross-checked rather than falsely labelled as a perfect primary transcription.

## 4. Reset closure decision

The earlier architecture requirement was that reset must not be assumed instantaneous.

That requirement is now satisfied by an explicit replaceable reset state with:

```text
duration
tau/kernel
residual voltage
threshold
saturation
recovery
```

Exact values remain `MEASURE_PENDING`.

The secondary/reconstruction estimate around a few microseconds is retained only as an experiment hypothesis. It is not a strict JUNO-106 constant.

## 5. Why this is enough for Phase 1

Later captures can change:

- effective C54;
- loaded DCO-CV scale and S/H droop;
- reset duration/residual;
- buffer slew/headroom;
- comparator delay/recovery;
- amplitude-error surface.

None of those requires changing the engine graph or control/event boundaries.

Therefore the remaining uncertainty is calibration, not architecture.

## 6. Central artifacts

- `JUNO106_MC5534_RAMP.md` — detailed research basis.
- `JUNO106_MC5534_REFERENCE_MODEL.md` — current implementation contract.
- `data/juno106_mc5534_calibration_v0.yaml` — all replaceable strict calibration values.
- `data/juno106_mc5534_evidence.csv` — evidence state per fact/value.
- `PHASE0_MEASUREMENT_PLAN.md` — direct capture routes.

## 7. Remaining calibration backlog

```text
[ ] archive clean primary R85/R86/R87 printed values
[ ] archive clean primary/direct C54 value
[ ] measure loaded DCO-CV code -> volts law
[ ] measure S/H source impedance and droop
[ ] capture reset duration/residual across note/range
[ ] fit MC5534 ramp-buffer headroom/slew/asymmetry
[ ] measure comparator timing/recovery if audibly relevant
[ ] capture saw/pulse amplitude-error surface across note/range
```

These must be replaced with physical data before claiming final hardware-calibrated strict fidelity.

## 8. Closure

**Result: MC5534 Phase-0 architecture gate is closed without importing JUNO-60 reset constants or normalizing away real 106 amplitude behaviour.**
