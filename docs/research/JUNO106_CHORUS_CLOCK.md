# JUNO-106 chorus clock and MN3009 delay model

Status: Phase 0 companion to `JUNO106_CHORUS.md`.

Purpose: define the exact clock-domain semantics and the strongest current original-106 component evidence while keeping unmeasured Mode-I/II trajectories explicitly open.

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

## 3. Original 106 clock-network evidence

### 3.1 R124 / R137

Strong original-106 service/modification evidence identifies:

```text
R124 = 8.2 kOhm
R137 = 8.2 kOhm
```

A documented MN3009 -> MN3007 conversion changes the 256-stage device to a 1024-stage device and reduces these clock-setting resistors to roughly one quarter (~2.2 kOhm practical), raising clock frequency by about 4x to preserve delay.

Sanity identity:

```text
N_new / N_old = 4
f_new / f_old ~= 4
=> delay_new ~= delay_old
```

This strongly confirms that R124/R137 participate directly in the original MN3101 frequency-setting networks.

### 3.2 C53 / C57

Original-board repair evidence now independently identifies the two nearby timing capacitors as:

```text
C53 = 150 pF
C57 = 150 pF
```

The repair discussion notes that the originals are tight-tolerance parts used to match the two chorus channels and recommends similarly stable replacements.

This materially raises confidence beyond the previous reconstruction-only evidence.

### 3.3 Full working external network

A 106-focused hardware reconstruction converges on the symmetric per-channel MN3101 external family:

```text
~1.8 kOhm / 8.2 kOhm / 150 pF
```

Working mapping:

```text
CH A: R123 ~= 1.8k, R124 = 8.2k, C53 = 150p
CH B: R136 ~= 1.8k, R137 = 8.2k, C57 = 150p
```

Confidence boundary:

- `R124`, `R137`, `C53`, `C57`: strong original-106 evidence;
- `R123`, `R136` and exact node mapping: retain `SCHEMATIC_CROSS_CHECK` until a clean original crop/continuity map is archived.

## 4. Do not invent a precision RC frequency equation

MN3101 contains an internal inverter oscillator. Device documentation describes the external R/C network and provides empirical frequency curves rather than one universal precision equation suitable for declaring the JUNO clock authentic from components alone.

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

## 5. Physical causal chain

The chorus LFO does not directly equal delay modulation.

```text
triangle-like LFO voltage
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

This is consistent with SoundForce choosing to reproduce captured physical-106 driver signals rather than synthesize a mathematically perfect triangle.

## 6. Modulation amplitude anchor

A 106-focused hardware reconstruction calibrated from the Roland design uses approximately:

```text
20 Vp-p
```

for the normal/inverted control-LFO pair, with equal amplitude and nominal 180-degree inversion.

Use this as a control-domain validation anchor, not as a delay-depth number.

## 7. Mode I / II rates remain measured constants

Do not use JUNO-60 numeric rates as strict 106 values.

Evidence currently establishes:

- original 106 has two slow chorus presets I and II;
- a 106-derived adjustable reconstruction spans roughly 0..1.8 Hz and marks positions matching I and II;
- SoundForce has direct original-106 clock captures but does not publish the numeric trajectories.

Therefore:

```text
modeI.rateHz  = MEASURE
modeII.rateHz = MEASURE
```

The two modes share the same physical clock/BBD architecture.

## 8. Reference API

```text
enum class ClockDomain106 {
    Mn3101Oscillator,
    Mn3101CpPhase
};

struct Mn3101State106 {
    double fOscHz;
    double fCpHz;
};

struct ClockDriver106 {
    Mn3101State106 processControlVoltage(double lfoVolts);
};

double mn3009DelaySeconds(double fCpHz) {
    return 128.0 / fCpHz;
}
```

Runtime FM-1 precision may differ; these names define the reference semantics.

## 9. Required original-106 capture

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

## 10. FM-1 simplification path

The deployed FM-1 does not need to run an MN3101 transistor/oscillator simulation at audio rate.

After physical capture:

1. fit normalized control -> `f_cp` with a low-order monotonic function;
2. optionally retain a tiny residual LUT for repeatable hardware imperfections;
3. use exact `128/f_cp` for delay;
4. drive a fractional-delay/BBD-equivalent audio path;
5. model clock-dependent bandwidth, insertion loss, noise and headroom separately.

This preserves the physical causal chain at modest CPU cost.

## 11. Freeze state

### Frozen

- explicit `f_osc` and `f_cp` domains;
- `f_cp = f_osc/2`;
- MN3009 stage count `N=256`;
- exact delay identity `t=128/f_cp`;
- R124 = R137 = 8.2 kOhm strongly established;
- C53 = C57 = 150 pF now independently supported by physical original-board repair evidence;
- working full external family ~= 1.8k / 8.2k / 150p per channel;
- clock-domain modulation precedes reciprocal delay conversion;
- nominal opposite-phase control-LFO relationship;
- no JUNO-60 numeric-rate substitution in strict 106.

### Open

- clean original archive of the R123/R136 values and exact node mapping;
- exact original Mode-I and Mode-II LFO frequencies;
- exact `f_cp(t)` min/max/mean and nonlinearity;
- channel mismatch;
- simultaneous clock/audio validation of delay;
- clock-dependent BBD insertion loss and bandwidth.
