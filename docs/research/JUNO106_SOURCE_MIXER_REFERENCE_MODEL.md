# JUNO-106 source mixer / SUB / common NOISE — Phase 0 reference model

Status: **`SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`**. Exact source gains, analogue control curves and pre-VCF headroom remain centralized `MEASURE_PENDING` calibration data.

This block begins at the external audio/control outputs of each MC5534 voice generator plus the one common noise generator and ends at each A1QH80017A audio input.

## 1. Frozen physical graph

```text
per physical voice i = 0..5:

8253 timing_i
  -> MC5534 voice section_i
       -> shared ramp/timing
       -> SAW gate ---------+
       -> PWM comparator ---+-> WAVE_i
       -> divide-by-two --------> SUB_i

one common physical source:
selected 2SC945P noise transistor (Tr21)
  -> IC14 BA662 noise VCA
  -> post-noise-VCA RC shaping (C41 family; ~5 kHz target)
  -> NOISE_COMMON

for each voice:
WAVE_i + SUB_i + NOISE_COMMON
  -> original per-voice passive/input network
  -> A1QH80017A_i audio input
```

The source network is a voltage/current/headroom network, not a normalized four-source DSP mixer.

## 2. Physical voice mapping

The six voices use three dual MC5534-family wave-generator packages and six A1QH80017A VCF/VCA hybrids:

```text
voices 1/2 -> MC5534 IC12 -> VCF/VCA IC13 / IC11
voices 3/4 -> MC5534 IC8  -> VCF/VCA IC9  / IC7
voices 5/6 -> MC5534 IC4  -> VCF/VCA IC5  / IC3
```

Original/repaired-board wave-generator probe points are the cathodes of:

```text
voice 1 D6
voice 2 D5
voice 3 D4
voice 4 D3
voice 5 D2
voice 6 D1
```

These expose the voice WAVE-generator output path and are useful future measurement boundaries.

## 3. WAVE is one physical boundary, not two free oscillators

SAW and PULSE/PWM share the same MC5534 ramp/timing system. Strict reference therefore keeps:

```text
Wave5534Voice106 {
    rampState;
    sawGate;
    pwmComparator;
    waveOut;
}
```

The logical SAW and PULSE switches remain distinct before `waveOut`, but the audio presented to the following source network is one physical WAVE node.

Consequences:

- SAW OFF does not stop the timer/ramp or SUB state;
- PULSE OFF is a comparator-control condition, not oscillator deletion;
- saw+pulse simultaneous drive must retain real WAVE-node amplitude/headroom interaction;
- do not render SAW and PULSE as unrelated free-running phases.

## 4. PWM boundary

Recovered MANUAL-mode integer law:

```text
pwm_depth = raw7 << 1
scaled    = floor(0x3fff * pwm_depth / 256)
pwm_code  = 0x3fff - scaled
```

PULSE OFF:

```text
pwm_code = 0
```

Roland analogue anchors:

```text
~+6.0 V -> ~50% duty
~+0.6 V -> ~95% duty
~-0.8 V -> rectangular output disabled/high
```

Strict duty therefore emerges from:

```text
firmware code -> held PWM CV -> comparator against physical ramp
```

not `duty = slider`.

## 5. SUB architecture

Digital target:

```text
sub_code = raw7 << 7
range    = 0x0000 .. 0x3f80
```

Roland describes SUB amplitude control through the SUB oscillator/transistor collector-supply domain. The Module Board also shows an individual discrete SUB/output-control stage per voice between the MC5534 SUB output and the following voice-input region.

Strict graph:

```text
one shared SUB LEVEL code/control voltage
 -> six physical per-voice amplitude stages
 -> six independent divide-by-two SUB phases
```

So:

- amplitude control is global;
- SUB oscillator phase/state remains per physical voice;
- no normalized post-generator `sub *= raw/127` assumption is frozen.

The exact held-voltage-to-amplitude curve remains `MEASURE_PENDING`.

## 6. Common NOISE architecture is now primary-mapped

Original Module Board evidence identifies:

```text
Tr21 = selected 2SC945P noise transistor
IC14 = BA662 noise VCA
VR32 = factory NOISE LEVEL calibration trim
C41 = post-noise-VCA RC-shaping family member
```

Independent repair evidence reports that the C41 network limits useful noise bandwidth to approximately 5 kHz and that aging C41 can make the noise spectrum substantially wider.

Freeze:

```text
Noise106
  SelectedTransistorNoise source
  BA662NoiseVca vca
  PostNoiseRcShape shape
  CommonFanout fanout
```

There is one continuous stochastic waveform shared by all six voices before their individual filters.

Do not instantiate six PRNGs.

## 7. NOISE LEVEL boundary

Firmware target:

```text
noise_code = raw7 << 7
range      = 0x0000 .. 0x3f80
```

Roland places NOISE LEVEL in the nominal `0..+10 V` conditioned-control class before the physical noise-VCA network.

Strict split:

```text
NoiseLevelControl106
  code14
  heldVolts

BA662NoiseVca106
  held/control mapping
  gain/nonlinearity
```

The BA662 control law in this particular external circuit is not replaced by a generic linear gain curve.

## 8. Factory absolute noise anchor

Roland adjustment:

```text
VCA GAIN calibrated first
BANK 6
POLY 1
hold any key
measure TP8 (voice 1 post-VCA)
VR32 -> 4 Vp-p noise
```

This is a system-level acceptance vector, not a statement that `NOISE_COMMON` itself equals 4 Vp-p.

Reference test must reproduce the complete Bank-6 path before comparing to this target.

## 9. Source-input network and drive

The original schematic clearly shows independent WAVE, SUB and common-NOISE contributions converging on each voice's 80017A input region. The repeated designator map proves the structure is mirrored across six physical voices.

The exact passive gains and coupling details are not yet clean enough to promote from the public scan into strict constants.

Therefore reference state is:

```text
SourceInput106[voice] {
    waveGain;
    subGain;
    noiseGain;
    couplingState;
    dcOffsets;
    sourceImpedances;
    headroom;
}
```

All numeric fields above live in centralized calibration data until primary transcription or measurement closes them.

## 10. No source normalization

Forbidden:

```text
mix /= enabledSourceCount
mix = clamp(mix, -1, +1) before VCF
normalize_each_source_to_unity
```

Why:

- WAVE is physically near the ~12 Vp-p DCO design region;
- SUB has an independent physical amplitude mechanism;
- NOISE has an absolute factory calibration;
- source combinations therefore change actual drive into the 80017A;
- filter input/OTA/resonance nonlinear behavior must be allowed to respond to that drive.

## 11. Source-dependent loading boundary

Until measured, preserve the possibility that source impedance and the 80017A input network interact.

Reference graph must permit:

```text
source_i_voltage + source_i_impedance
 -> passive summing/loading
 -> Filter80017Input
```

rather than assuming an ideal zero-impedance voltage adder.

If measurement later proves loading negligible, FM-1 can collapse the linear network without architecture changes.

## 12. Deterministic test noise

For regression tests:

- use one deterministic seeded source;
- fan the exact same sample to all six voices;
- apply analogue spectrum/gain shaping after the raw PRNG source;
- keep the musical NOISE source separate from BBD/chorus noise.

Runtime PRNG choice is an optimization detail as long as calibrated spectrum/statistics are preserved.

## 13. Reference API

```text
struct Wave5534Voice106 {
    double renderWave();
    double renderSub();
};

struct Noise106 {
    double renderCommon();
};

struct SourceInputCalibration106 {
    double waveGain;
    double subGain;
    double noiseGain;
    // physical impedance/coupling/headroom parameters
};

struct SourceMixer106 {
    double process(double wave,
                   double sub,
                   double commonNoise,
                   const SourceInputCalibration106& cal);
};
```

`SourceMixer106::process()` is explicitly **not** an enabled-source-normalized sum.

## 14. Measurement boundaries

Useful original nodes/test points:

- WAVE: D6..D1 cathodes for voices 1..6;
- per-voice VCF outputs: TP19..TP14;
- per-voice VCA outputs: TP8..TP13;
- common noise circuit around Tr21 / IC14 / C41 / VR32;
- MC5534 SUB outputs / per-voice discrete SUB stages where safely accessible.

Required captures are specified in `PHASE0_MEASUREMENT_PLAN.md`.

## 15. Phase-0 closure

Architecture gate closed:

```text
[x] per-voice WAVE physical boundary
[x] SAW/PULSE shared-ramp relationship
[x] independent SAW-gate and PULSE-disable semantics
[x] firmware PWM law and physical comparator boundary
[x] six per-voice SUB timing states with one global SUB LEVEL control
[x] physical SUB amplitude-control stage retained
[x] one selected-transistor common NOISE source
[x] IC14 BA662 common noise VCA retained
[x] post-noise-VCA RC shaping retained (~5 kHz measurement target)
[x] exact NOISE LEVEL firmware law
[x] Roland 4 Vp-p factory noise acceptance vector retained
[x] common-noise fanout before six VCFs
[x] source-network impedance/headroom retained
[x] enabled-source normalization forbidden
[x] unresolved gains/curves centralized
[x] all unresolved numerical quantities have measurement routes
```

Calibration backlog:

```text
[ ] exact WAVE -> 80017A passive gain/impedance
[ ] exact SUB -> 80017A passive gain/impedance
[ ] exact NOISE -> 80017A fanout gain/impedance
[ ] coupling/DC offsets and maximum pre-VCF excursion
[ ] SUB held-control-voltage -> amplitude curve
[ ] BA662 noise-VCA control -> gain curve
[ ] exact noise RC transfer and calibrated spectrum
[ ] source-combination headroom/compression/loading
```

Those values can be replaced without changing the graph.

**Phase-0 source-mixer state: `SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`.**
