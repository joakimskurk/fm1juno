# JUNO-106 MC5534 wave generator — Phase 0 reference model

Status: **`MC5534_PHASE0_ARCHITECTURE_FROZEN`**. Exact reset transient, loaded DCO-CV behaviour, comparator/buffer nonlinearity and final amplitude-error curves remain centralized `MEASURE_PENDING` calibration data.

This block begins at each physical 8253 DCO timing output plus held DCO CV / PWM CV / SUB LEVEL controls and ends at the external MC5534 WAVE and SUB source boundaries.

## 1. Frozen physical graph

```text
per physical voice:

8253 Mode-3 timing
  -> positive-going DCO edge
      -> finite MC5534 ramp-reset event

held DCO CV
  -> RANGE-selected slope resistor
      16' -> highest R
       8' -> middle R
       4' -> lowest R
  -> integration capacitor C54-family node
  -> linear analogue ramp between reset events

same physical ramp
  -> SAW gate contribution
  -> analogue comparator against held PWM CV
  -> PULSE/PWM contribution
  -> external WAVE node

DCO timing
  -> divide-by-two state
  -> SUB source
  -> analogue SUB-level amplitude stage
  -> external SUB node
```

The reference engine must preserve this graph. A free-running normalized digital saw/square pair is not an acceptable strict-106 source architecture.

## 2. Primary Roland facts

Roland service documentation directly establishes:

- MC5534-family devices at Module Board IC4/IC8/IC12;
- a rectangular DCO timing input drives waveform generation;
- DCO CV is used for saw/PWM amplitude compensation and includes key, LFO, Bender, Portamento and Tune, but not RANGE;
- RANGE is applied after DCO-CV generation through a selector choosing R85/R86/R87;
- for 16', DCO CV charges C54 through R85;
- C54 is discharged on the **positive-going DCO edge** through the reset/discharge transistor path;
- saw and pulse are designed to remain approximately 12 Vp-p, not mathematically identical cycle-to-cycle;
- PWM compares the physical ramp against PWM CV;
- approximately +6.0 V PWM CV corresponds to ~50% duty;
- approximately +0.6 V corresponds to ~95% duty;
- approximately -0.8 V disables the rectangular output;
- SUB is a timing-derived divide-by-two source whose amplitude is controlled through the collector-supply domain.

## 3. RANGE resistor family

The working original-106 resistor set remains:

```text
16' : R85 ~= 399 kOhm
 8' : R86 ~= 200 kOhm
 4' : R87 ~= 100 kOhm
```

The 4:2:1 resistance relationship follows the physical amplitude-compensation requirement and is independently corroborated by published circuit readings of the 106.

Evidence state:

```text
mapping/topology  = PRIMARY_CONFIRMED
numeric values    = SCHEMATIC_CROSS_CHECK_STRONG
```

Do not silently replace 399 kOhm with an exact mathematical 400 kOhm in strict mode once the physical set is used.

## 4. Ramp law

Between reset events, the first reference model uses the physical integrator law:

```text
dV/dt = -V_dco_cv_held / (R_range * Cint)
```

and therefore over one uninterrupted interval `dt`:

```text
V(t+dt) = V(t) - V_dco_cv_held * dt / (R_range * Cint)
```

This law is an architectural invariant. Later calibration may alter effective capacitance, source resistance, reset dead-time and buffer transfer without changing the graph.

## 5. Integrating capacitance

`C54` is the named original ramp-capacitor family member in Roland's service description.

A working value of approximately:

```text
Cint ~= 1.0 nF
```

is strongly consistent with the 106 schematic family, the 399k/200k/100k range network, recovered DCO-CV law and the ~12 Vp-p amplitude target.

It remains `SCHEMATIC_CROSS_CHECK_STRONG`, not promoted to primary-readable truth until a clean component read or direct board measurement is archived.

## 6. Finite reset is mandatory

Strict architecture must not encode reset as a permanently instantaneous assignment.

Reference state:

```text
struct RampReset106 {
    bool active;
    double elapsed;
    double tauOrKernel;
    double residualVolts;
    double threshold;
    double saturation;
};
```

A positive-going DCO edge starts the reset event. The implementation must allow a finite discharge interval and non-zero residual/recovery behaviour.

The previously discussed `10k/270p`, `2.2k/1nF`, roughly-5-us reconstruction values remain **hypothesis-only** and must not become strict constants unless original-106 evidence or direct capture confirms them.

## 7. Physical source state machine

```text
struct Wave5534Ramp106 {
    double vCap;
    double heldDcoCv;
    double rangeROhm;
    double cIntF;
    RampReset106 reset;

    void onPositiveDcoEdge();
    void advance(double dt);
};
```

Conceptually:

```text
onPositiveDcoEdge:
    reset.active = true
    reset.elapsed = 0

advance(dt):
    if reset.active:
        vCap = calibratedResetTransfer(vCap, dt)
        if resetFinished:
            reset.active = false
    else:
        vCap += -heldDcoCv * dt / (rangeROhm * cIntF)
```

The reset function is replaceable calibration, not an architecture choice.

## 8. SAW/PWM shared-ramp contract

Strict pulse generation:

```text
pulse = analogueComparator(rampVoltage, heldPwmCv, comparatorCalibration)
```

Therefore pulse inherits:

- the same DCO edge timing;
- the same ramp slope quantization;
- reset dead-time/residual;
- any real ramp curvature/slew near the reset boundary;
- held-CV stepping and S/H behaviour.

This is why `SawOscillator` and `PulseOscillator` must not be implemented as unrelated oscillator cores.

## 9. Approximate 12 Vp-p is a validation region

Strict model must **not** normalize every cycle to 12 Vp-p.

Allow amplitude error to arise naturally from:

- discrete DCO-CV code;
- RANGE resistor ratios;
- real Cint;
- finite reset time;
- source/switch resistance;
- S/H droop;
- MC5534 buffer/comparator limits.

The desired behaviour is structured hardware error, not post-hoc random drift.

## 10. SUB timing contract

SUB is a separate timing-domain divide-by-two state tied to DCO edges.

```text
DcoTiming106 -> SubDivider106 -> SubAmplitude106 -> SUB_OUT
```

Do not derive SUB by pitch-shifting or downsampling rendered saw audio.

The amplitude-control transfer remains part of the source-mixer calibration block.

## 11. Reference implementation modes

Desktop reference should support:

```text
RESET_INSTANT_DIAGNOSTIC
RESET_PARAMETRIC_PHYSICAL
RESET_CAPTURE_FITTED
```

Only the latter two are candidates for strict rendering. The instant mode exists to isolate the audible/measurement contribution of the finite reset.

Anti-aliasing is a separate layer. Preferred experiments:

1. high-rate oversampled physical reference;
2. event-aware correction at finite reset/crossing events;
3. hybrid analytical ramp + localized oversampled/reset kernel.

## 12. Centralized calibration surface

All unresolved numerical values live in:

`data/juno106_mc5534_calibration_v0.yaml`

No source file should hard-code folklore values such as `reset_us = 5` or exact `C54=1nF` as strict truth before evidence promotion.

## 13. Deterministic validation vectors

Required tests:

- amplitude vs note across all three RANGE states;
- equivalent-pitch comparison across adjacent RANGE choices;
- reset duration/residual at low/mid/high frequency;
- 50% PWM target and 93-97% high-PWM target;
- PWM OFF suppression;
- held DCO-CV step response and S/H update behaviour;
- Solo-Unison six-voice reset staggering using the real serial/service schedule;
- saw/pulse spectral comparison against a healthy original unit.

## 14. Phase-0 closure

Architecture gate closed:

```text
[x] 8253 timing and analogue wave generation remain separate blocks
[x] held DCO CV drives ramp slope
[x] RANGE is applied through a discrete resistor-selection stage after DCO-CV generation
[x] positive-going DCO edge initiates C54 reset/discharge
[x] reset is finite/configurable in the reference architecture
[x] ramp is linear between reset events in the first physical model
[x] SAW and PWM share the same physical ramp
[x] PWM is comparator-derived from held PWM CV
[x] SUB is timing-domain divide-by-two
[x] ~12 Vp-p is a validation target, not per-cycle normalization
[x] unresolved reset/CV/headroom constants are centralized
[x] all unresolved numerical quantities have direct measurement tests
```

Calibration backlog:

```text
[ ] clean primary confirmation of R85/R86/R87 printed values
[ ] clean primary confirmation / direct measurement of C54
[ ] loaded DCO-CV code -> volts law per voice
[ ] S/H source impedance, switch Ron and droop
[ ] real 106 reset duration and residual vs frequency/level
[ ] MC5534 ramp-buffer slew/headroom/asymmetry
[ ] comparator propagation/saturation/recovery if audibly relevant
[ ] measured amplitude-error surface across note/range
```

Those values can replace calibration data without changing the engine graph.

**Phase-0 MC5534 state: `MC5534_PHASE0_ARCHITECTURE_FROZEN`.**
