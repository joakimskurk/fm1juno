# JUNO-106 chorus clock and MN3009 delay model

Status: Phase 0 reference specification. The clock-domain architecture and original external timing component family are now primary-mapped. The remaining strict numerical unknown is the **real operating trajectory** of the original MN3101/BBD clocks in modes I and II.

## 1. Clock domains

The word `clock` is ambiguous and must never appear unqualified in strict code or measurements.

Define:

```text
f_osc  = MN3101 internal oscillator frequency
f_cp   = frequency present on each physical CP1 / CP2 output
f_xfer = effective bucket-transfer event rate
```

MN3101 component documentation gives:

```text
f_cp = f_osc / 2
```

CP1 and CP2 are opposite phase and approximately 50% duty.

Reference type:

```text
struct BbdClock106 {
    double fOscHz;
    double fCpHz;
};
```

One value must be derived from the other.

## 2. Exact MN3009 delay identity

MN3009 has 256 BBD stages.

When `f_cp` denotes one physical CP phase:

```text
t_delay = N / (2*f_cp)
        = 256 / (2*f_cp)
        = 128 / f_cp
```

Equivalent:

```text
t_delay = N / f_osc
```

Examples:

```text
f_cp =  10 kHz -> 12.80 ms
f_cp =  25 kHz ->  5.12 ms
f_cp =  50 kHz ->  2.56 ms
f_cp = 100 kHz ->  1.28 ms
f_cp = 200 kHz ->  0.64 ms
```

Unit-test invariants:

```text
assert_near(fOsc, 2*fCp)
assert_near(delaySeconds * fCp, 128)
```

This closes the factor-of-two ambiguity that often appears in simplified BBD emulations.

## 3. Original 106 MN3101 timing networks

A readable original Roland jack-board schematic now establishes the mirrored external timing component families directly.

### Channel A

```text
R123 = 1.8 kOhm
R124 = 8.2 kOhm
C53  = 150 pF
```

### Channel B

```text
R136 = 1.8 kOhm
R137 = 8.2 kOhm
C57  = 150 pF
```

Evidence state:

- `R123/R136 = 1.8 kOhm`: `PRIMARY_VALUE_VISIBLE`;
- `R124/R137 = 8.2 kOhm`: `PRIMARY_CONFIRMED` and independently cross-checked by the documented MN3009->MN3007 modification;
- `C53/C57 = 150 pF`: `PRIMARY_CONFIRMED`, with original-board repair evidence additionally confirming that these are closely matched/tight-tolerance timing capacitors.

This replaces the older wording that treated the 1.8k values and exact family as reconstruction-only.

## 4. Independent clock-network cross-check

A documented MN3009 -> MN3007 conversion changes the 256-stage device to a 1024-stage device and reduces JUNO-106 `R124/R137` from 8.2 kOhm to roughly one quarter, about 2.05 kOhm ideally / 2.2 kOhm as a practical historical substitution.

Sanity identity:

```text
N_new / N_old = 4
f_new / f_old ~= 4
=> delay_new ~= delay_old
```

This independently confirms that `R124/R137` are frequency-setting elements in the two original MN3101 oscillator networks.

## 5. Do not invent a precision RC frequency equation

MN3101 contains an internal inverter oscillator. Device documentation describes the external R/C network and provides empirical frequency curves rather than one universal precision equation suitable for declaring the JUNO clock authentic from component values alone.

Therefore:

```text
f = 1/(K*R*C)
```

with a guessed `K` is diagnostic only.

Use two layers:

```text
fCpApprox = mn3101EmpiricalApprox(componentSet, controlVoltage)
```

for sanity/research, and:

```text
fCpStrict = fittedOriginal106Clock(controlVoltage, measuredCapture)
```

for strict rendering.

The original component values now define the **correct physical network**, but they do not by themselves remove process/temperature/control-transfer uncertainty inside the MN3101.

## 6. Physical causal chain

The chorus LFO does not directly equal delay modulation.

```text
triangle-like LFO/control voltage
 -> transistor/control interface
 -> MN3101 oscillator operating point
 -> f_osc(t)
 -> f_cp(t)
 -> reciprocal MN3009 delay(t)
```

Even an ideal triangle control voltage will not produce a perfectly triangular delay trajectory because:

- MN3101 control transfer is not guaranteed linear;
- delay is reciprocal in clock frequency;
- control transistors can add curvature/asymmetry;
- channel tolerances differ slightly.

SoundForce reports sampling the actual BBD-driver signals from a physical JUNO-106 and replaying those trajectories digitally specifically to retain the imperfections of the real driver signal. This is strong support for keeping a measured-trajectory layer rather than replacing the clock with a generic sine/triangle delay LFO.

## 7. Modulation amplitude anchor

A 106-focused hardware reconstruction calibrated against the Roland design uses approximately:

```text
20 Vp-p
```

for the normal/inverted control-LFO pair, with equal amplitude and nominal 180-degree inversion.

Treat this as a **control-domain validation anchor**, not as a delay-depth value. Final strict delay excursion still comes from measured `f_cp(t)`.

## 8. Mode I / II rates remain physical-measurement constants

The original JUNO-106 service documentation does not specify numeric chorus LFO rates. Florian Anwander independently notes the same absence; published JUNO-60 rates are therefore not strict 106 evidence.

Evidence currently establishes:

- original 106 has two slow presets I and II;
- both reuse the same physical dual-MN3101 / dual-MN3009 architecture;
- a 106-derived adjustable reconstruction spans roughly 0..1.8 Hz and marks physical positions corresponding to I and II;
- SoundForce has direct original-106 driver captures but does not publish the numeric trajectories.

Therefore strict calibration remains:

```text
modeI.rateHz  = MEASURE_PENDING
modeII.rateHz = MEASURE_PENDING
```

Development/reference code may use an explicitly tagged provisional trajectory, but **must not call JUNO-60 0.5/0.8-Hz values authentic 106 constants**.

## 9. Reference API

```text
enum class ClockDomain106 {
    Mn3101Oscillator,
    Mn3101CpPhase
};

struct Mn3101TimingNetwork106 {
    double rControlOhm;  // 1.8k nominal original network member
    double rTimingOhm;   // 8.2k nominal original network member
    double cTimingF;     // 150p nominal
};

struct Mn3101State106 {
    double fOscHz;
    double fCpHz;
};

struct ClockDriver106 {
    Mn3101TimingNetwork106 components;
    Mn3101State106 processControlVoltage(double lfoVolts);
};

double mn3009DelaySeconds(double fCpHz) {
    return 128.0 / fCpHz;
}
```

Runtime FM-1 precision may differ; these names define the reference semantics.

## 10. Required original-106 capture

For both channels and both I/II modes:

1. capture >=4 complete LFO periods;
2. identify probe domain explicitly (`f_osc` or `f_cp`);
3. convert every edge interval to instantaneous frequency;
4. store min/max/mean/median;
5. fit rising/falling trajectories separately;
6. compare A/B after accounting for opposite modulation;
7. calculate `t_delay(t)=128/f_cp(t)`;
8. validate with simultaneous audio delay measurement.

Metadata:

```text
instrument serial / board revision
MN3009 marking/date if visible
supply rails
probe point
sample rate
mode
warmup / temperature
bias calibration state
```

Raw captures should remain test assets; source code should store compact fitted coefficients and regression vectors.

## 11. FM-1 simplification path

The deployed FM-1 does not need to run an MN3101 transistor/oscillator simulation at audio rate.

After physical capture:

1. fit normalized control -> `f_cp` with a low-order monotonic function;
2. optionally retain a tiny residual LUT for repeatable hardware imperfections;
3. use exact `128/f_cp` for delay;
4. drive a fractional-delay/BBD-equivalent audio path;
5. model clock-dependent bandwidth, insertion loss, noise and headroom separately.

This preserves the physical causal chain at modest CPU cost.

## 12. Phase-0 freeze state

### Frozen

- explicit `f_osc` and `f_cp` domains;
- `f_cp = f_osc/2`;
- MN3009 stage count `N=256`;
- exact delay identity `t=128/f_cp`;
- original timing families `1.8k / 8.2k / 150p` on both channels;
- R124/R137 role independently cross-checked by historical 4x MN3007 clock modification;
- tight channel matching of C53/C57 is intentional;
- clock-domain modulation precedes reciprocal delay conversion;
- nominal opposite-phase control relationship;
- measured/non-ideal trajectory capability is required;
- no JUNO-60 numeric-rate substitution in strict 106.

### `MEASURE_PENDING`, not architecture blockers

- exact original Mode-I LFO/clock trajectory;
- exact original Mode-II LFO/clock trajectory;
- min/max/mean `f_cp(t)`;
- A/B mismatch and phase error;
- simultaneous clock/audio confirmation of absolute delay;
- clock-dependent BBD insertion loss and bandwidth.

These have explicit acquisition and acceptance procedures in `PHASE0_MEASUREMENT_PLAN.md` and therefore no longer require guessed constants to cross the Phase-0 architecture gate.
