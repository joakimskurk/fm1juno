# FM1JUNO Phase 0 — physical JUNO-106 measurement plan

Status: authoritative measurement/validation plan for unresolved strict `MODEL_106` constants.

Purpose: Phase 0 does **not** require every analogue constant to be guessed before implementation. It requires every unresolved constant to have:

1. a named physical quantity;
2. a known source node / test method;
3. a defined stimulus;
4. a capture format;
5. a model parameter or transfer function it will calibrate;
6. an acceptance test.

This document converts the remaining unknowns into reproducible experiments.

---

## 0. Measurement philosophy

### 0.1 Preserve factory calibration first

Before characterising a unit, confirm that it is healthy and reasonably calibrated according to Roland's sequence.

Do not deliberately adjust a good original instrument simply to make a measurement unless necessary. Record existing trimmer positions where practical before changes.

Minimum warm-up before VCF calibration/measurement:

```text
>= 10 minutes
```

Prefer 20–30 minutes for repeatable thermal data.

### 0.2 Capture raw data, fit later

Store raw waveforms/sweeps with metadata. Do not save only screenshots or hand-written numbers.

Preferred raw formats:

```text
CSV / WAV / binary scope export
```

Repository should contain:

- compact derived CSV/test vectors;
- fit coefficients;
- metadata;
- scripts.

Large raw captures can live outside git with checksums + acquisition notes.

### 0.3 One physical unit is not 'the specification'

A single healthy original 106 gives a calibration target, but tolerances matter.

Record:

- serial number / board revision if known;
- 80017A lot codes;
- BBD manufacturer/date codes if visible;
- replacement parts/modifications;
- supply rails;
- temperature/warm-up;
- calibration status.

Strict factory-reference uses nominal/healthy behavior. `DEEP/AGE` can later model measured spread.

---

## 1. Equipment

### Minimum

- 2-channel oscilloscope, preferably 100 MHz or better;
- frequency counter capability or high-resolution scope timing;
- DMM with 1 mV resolution;
- audio interface capable of 24-bit / 96 kHz or better;
- calibrated audio generator / DAW test files;
- MIDI source / keyboard automation;
- safe high-impedance probes.

### Strongly preferred

- 4-channel oscilloscope;
- >= 2 MS/s long-memory capture for chorus clock/LFO work;
- differential probe for awkward floating nodes;
- audio analyser / REW / ARTA / custom sweep script;
- temperature probe.

Do not load high-impedance analogue control nodes unnecessarily. Use x10 probes unless a procedure specifically requires otherwise.

---

## 2. Baseline Roland calibration record

Record the original service anchors before extended measurements.

### 2.1 DCO CV offset

Roland procedure:

```text
MODULE BOARD TP3
MIDI CH pressed -> DAC requests 0 V
VR33 -> 0 V
```

Record residual offset in mV.

Model target:

```text
DcoCv106.zero_offset
```

### 2.2 VCA bias

```text
MODULE BOARD TP7
VR34 -> +0.25 ... +0.27 V
```

Record actual value.

### 2.3 Per-voice service anchors

Voice output test nodes:

```text
TP8  = CH1
TP9  = CH2
TP10 = CH3
TP11 = CH4
TP12 = CH5
TP13 = CH6
```

VCF resonance service nodes:

```text
TP19 = CH1
TP18 = CH2
TP17 = CH3
TP16 = CH4
TP15 = CH5
TP14 = CH6
```

Record:

- self-oscillation amplitude target around 4.8 Vp-p;
- post-VCA target around 6 Vp-p;
- C4 VCF frequency target 248 Hz;
- C6 VCF width target 992 Hz;
- tuning residual after iterative Frequency/Width calibration.

### 2.4 Noise anchor

```text
BANK 6
TP8
held key
VR32 nominal factory target: 4 Vp-p
```

Do not read this as RMS; Roland specifies peak-to-peak scope amplitude.

### 2.5 PWM anchors

```text
BANK 5
C4
TP8..TP13
```

Targets:

```text
nominal 50%: 48..52%
PWM slider 10: 93..97%
```

### 2.6 Chorus bias anchors

JACK BOARD:

```text
TP1 = chorus CH1
TP2 = chorus CH2
CHORUS I
VCA LEVEL 0
inject 10 Vp-p 1 kHz sine into MODULE BOARD TP2
VR1/VR2 -> symmetrical positive/negative clipping halves
```

Record the resulting bias-control positions and maximum symmetrical output before clipping.

---

## 3. Control-loop timing and DAC/S&H schedule

Goal: close the remaining difference between 'approximately 4.2 ms' and the real ordered control frame.

### 3.1 Questions

Measure:

- average full module main-loop period;
- period variance idle vs six active voices;
- timing of DCO-CV slot per physical voice;
- timing of VCF CV slot per physical voice;
- PWM slot;
- SUB and NOISE control slots;
- effect of serial receive interrupts;
- effect of ADC completion interrupts.

### 3.2 Preferred method

Use a logic/scope probe on an accessible DAC MUX/inhibit/address line plus a second reference event.

If probing a MUX line is impractical, instrument a cycle-accurate uPD7811 harness using the known ROM and independently validate one or more real-hardware periods.

### 3.3 Stimuli

Capture at least:

```text
idle / no voice running
one held voice
six held voices
fast LFO
max portamento movement
continuous MIDI parameter input
Solo-Unison new note
```

### 3.4 Output

Create:

```text
control_frame_mean_us
control_frame_std_us
slot_offsets_us[]
slot_jitter_us[]
```

Model targets:

```text
ControlScheduler106
SampleHold106
```

---

## 4. DCO-CV voltage law and S/H behavior

### 4.1 Goal

Map the digital 14/12-bit-style control representation to the loaded analogue DCO-CV voltage at the voice path.

### 4.2 Primary node

Use the service DCO-CV path around TP3 for zero/reference and, where necessary, probe the per-voice S/H output / MC5534 DCO-CV input with a high-impedance x10 probe.

Exact per-voice probe node must be documented from the board/schematic before acquisition.

### 4.3 Stimulus matrix

For 16'/8'/4' and representative notes:

```text
C2 C3 C4 C5 C6 C7
plus semitone/fractional pitch tests via bend/tune where useful
```

Record:

- pitch 8.8 code;
- ROM-derived DCO-CV code;
- held voltage;
- selected range;
- droop until next refresh;
- settling transient after refresh.

### 4.4 Model targets

```text
DcoCv106.code_to_volts
SampleHold106.settle_tau
SampleHold106.droop_rate
range_switch_source_impedance
```

Acceptance: predicted physical ramp amplitude vs note/range should match the subsequent waveform measurement without arbitrary per-note normalization.

---

## 5. MC5534 saw ramp / reset measurement

### 5.1 Goal

Close:

- finite reset duration;
- reset residual voltage;
- ramp linearity;
- slew/headroom;
- amplitude error across keyboard/range.

### 5.2 Probe

Probe a voice's saw/ramp node as close to MC5534 output or the voice test path as the schematic allows.

Use one voice only and document exact component/pin node.

### 5.3 Capture matrix

For each RANGE:

```text
low representative note
mid note
high note
```

At minimum 9 cases.

For each capture measure:

```text
frequency
digital timer divisor
DCO-CV voltage
pre-reset max
post-reset minimum
reset 10-90 / effective duration
linear-ramp slope
Vp-p
cycle-to-cycle variation
```

### 5.4 High-resolution reset capture

Use >= 5–10 MS/s where possible and trigger on DCO positive edge.

Fit candidate models:

```text
instantaneous reset
single exponential discharge
piecewise transistor saturation + recovery
```

Choose the simplest model that matches residual/spectral behavior.

### 5.5 Model targets

```text
Wave5534.reset_tau
Wave5534.reset_duration
Wave5534.reset_floor
Wave5534.ramp_gain
Wave5534.headroom
```

---

## 6. PWM comparator and pulse shape

Use Roland's 50% and 95% service conditions plus intermediate positions.

Capture:

```text
saw node
PWM CV node if accessible
pulse output
```

Slider points:

```text
0, 2, 4, 5, 6, 8, 10
```

Record:

- PWM CV volts;
- duty cycle;
- transition timing relative to saw;
- comparator saturation levels;
- edge slew;
- behavior with PULSE off.

Model targets:

```text
PwmComparator106.threshold_map
PwmComparator106.output_levels
PwmComparator106.edge_slew
```

---

## 7. SUB and oscillator-source mixer

### 7.1 SUB amplitude transfer

Use a single voice with SAW/PULSE/NOISE off.

Sweep SUB LEVEL across all 128 raw values if MIDI/SysEx automation is convenient, otherwise a dense representative grid.

Measure at the per-voice output test point with VCF wide/open and VCA conditions fixed.

Recover:

```text
SUB control code -> output amplitude
```

Then back-fit the transistor collector-supply/gain law rather than using a generic linear gain.

### 7.2 Source contribution/headroom

Measure these combinations at equal fixed voice/filter/VCA state:

```text
SAW
PULSE 50%
SUB max
NOISE max
SAW+PULSE
SAW+SUB
PULSE+SUB
SAW+PULSE+SUB
all + NOISE
```

At both low and high resonance/cutoff conditions.

Goal: identify mixer ratios and whether combined source drive causes measurable compression/nonlinearity before or in 80017A.

### 7.3 Common noise verification

Record simultaneous output from two physical voices with only NOISE enabled.

Cross-correlation should reveal the expected shared underlying noise source after independent voice filter/VCA paths.

This validates the strict `one common noise generator` architecture.

---

## 8. Noise generator spectrum and control law

With only NOISE enabled and VCF wide enough to avoid dominating the spectrum:

Capture 30–60 seconds at >= 96 kHz.

Measure:

- PSD;
- approximate post-noise-VCA cutoff/shape;
- RMS and crest factor;
- DC offset;
- NOISE LEVEL control curve;
- unit-to-unit / time stability if multiple units available.

Fit a deterministic DSP noise generator + analogue shaping network.

Model targets:

```text
Noise106.generator_spectrum
Noise106.post_vca_filter
Noise106.level_law
Noise106.nominal_gain
```

---

## 9. 80017A VCF control-voltage mapping

### 9.1 Frequency calibration surface

For one healthy voice after standard calibration, sweep Cutoff code while self-oscillating.

Repeat at:

```text
C2 / C4 / C6 key-follow contexts
key follow 0 / mid / max
```

Capture actual oscillation frequency for each 14-bit VCF control code sample.

A 64–128 point code grid is enough for a first fit.

Fit:

```text
VCF DAC/S&H code -> log2(cutoff Hz)
```

Do not assume the modern AS3109 nominal mV/oct directly equals the aged original hybrid system.

### 9.2 Width/frequency interaction

Record both calibrated trimmer states and perturbations around nominal to understand per-voice spread parameters.

Model targets:

```text
Filter80017.freq_offset[voice]
Filter80017.freq_scale[voice]
```

---

## 10. 80017A resonance and nonlinearity

### 10.1 Resonance law

With a stable low-level source, sweep RES 0..127 and measure:

- passband gain;
- resonant peak frequency/amplitude;
- onset of self-oscillation;
- self-osc amplitude vs RES beyond onset.

### 10.2 Drive sweep

At several cutoff/resonance points, sweep input amplitude over at least 30 dB.

Record:

- fundamental gain;
- THD;
- 2nd/3rd harmonic ratio;
- clipping symmetry;
- resonance compression.

### 10.3 Stage evidence if accessible

If safe test access exists inside/opened 80017A substitute hardware, stage taps can be measured. Otherwise fit the final topology-preserving model against external transfer behavior plus known circuit topology.

Model targets:

```text
IR3109 stage nonlinearity
buffer headroom/asymmetry
BA662 resonance gain law
Q compensation/feed-forward gain
```

---

## 11. Voice BA662 VCA mapping

With VCF held in a non-limiting condition and a stable oscillator input:

Sweep the digital envelope/VCA control domain while measuring per-voice output.

Do both ENV and GATE modes.

Record:

```text
VCA control code -> voltage gain / dB
bias offset
thump around gate transitions
large-signal gain compression
```

Model targets:

```text
Vca80017.control_to_gain
Vca80017.offset
Vca80017.nonlinearity
```

---

## 12. Global HPF 0/1/2/3

Measure after voice summing and before chorus if an internal node is safely accessible; otherwise derive from output with chorus off and VCA held fixed.

Use a swept sine / log chirp from approximately:

```text
5 Hz .. 30 kHz
```

For each HPF position record magnitude and phase.

Important:

```text
0 = bass boost network
1 = straight/flat route
2 = HPF
3 = stronger HPF
```

Calculate ratios relative to position 1 to cancel unrelated output-stage response.

Model target:

```text
Hpf106.transfer[4]
```

This relative-measurement method is especially robust if exact passive values remain difficult to read.

---

## 13. Global uPC1252 VCA / VCA LEVEL

With chorus off and HPF=1:

- feed/use a stable source;
- automate VCA LEVEL raw 0..127;
- measure output RMS/amplitude;
- repeat at several source amplitudes approaching clipping.

Recover:

```text
raw level -> dB gain
headroom vs level
THD vs level
```

The front-panel warning that high VCA LEVEL can distort makes this a sonic block, not a bookkeeping gain.

Model targets:

```text
GlobalVca106.control_to_gain_db
GlobalVca106.headroom
GlobalVca106.nonlinearity
```

---

## 14. Chorus clock capture — highest-priority missing chorus dataset

### 14.1 Probe

Probe clearly identified MN3101 CP1/CP2 clock outputs on both channels, or another node whose clock convention is explicitly documented.

Do **not** label an OX oscillator measurement as CP clock.

### 14.2 Modes

Capture:

```text
I
II
```

for >= 4 full LFO periods each.

Sampling:

```text
>= 2 MS/s preferred
```

### 14.3 Extract

For each channel/mode:

```text
f_cp(t)
min / max / mean / median f_cp
LFO period
rising vs falling curve
A/B phase relation
cycle-to-cycle repeat error
clock duty
```

Then derive exact physical BBD delay:

```text
t_delay(t) = 128 / f_cp(t)
```

Model targets:

```text
ChorusClock106.mode_I_curve
ChorusClock106.mode_II_curve
channel mismatch
```

---

## 15. Chorus absolute delay and dry/wet gain

### 15.1 Simultaneous audio capture

Inject an impulse, MLS or logarithmic chirp while recording:

- input/dry reference;
- left output;
- right output;
- optionally BBD clock simultaneously on scope.

Recover:

```text
center delay
min/max delay
delay trajectory
A/B delay mismatch
stereo phase relationship
```

### 15.2 Dry/wet

Fit the full outputs as:

```text
yL = gdry*x + gwetL*wetL
yR = gdry*x + gwetR*wetR
```

Do not normalize the coefficients first.

The current ~55/45 claim is a working hardware-reconstruction calibration to verify, not an imposed solution.

---

## 16. Chorus wet-path transfer / filtering

At several clock states (slow/center/fast):

- use sweep/MLS;
- isolate wet path internally if practical;
- otherwise fit it jointly from stereo outputs and known dry path.

Measure:

```text
magnitude
phase/group delay
BBD insertion loss
HF bandwidth vs f_cp
low-frequency coupling
A/B asymmetry
clock leakage
```

This closes the component-network ambiguity in `JUNO106_CHORUS_FILTERS.md` without relying solely on schematic transcription.

---

## 17. Chorus bias/headroom/nonlinearity

Follow Roland's TP1/TP2 bias procedure first.

Then, at nominal bias:

- increase 1 kHz input from small signal through clipping;
- repeat at low/mid/high BBD clock point if feasible.

Record:

```text
positive clip level
negative clip level
THD vs level
softness/knee
clock dependence
channel mismatch
```

Then deliberately offset bias slightly on a service/test unit only and quantify asymmetry.

This supports realistic `AGE` later without corrupting strict nominal calibration.

---

## 18. Chorus noise / clock feedthrough

No musical input.

Record L/R at 96 or 192 kHz for:

```text
chorus OFF
chorus I
chorus II
```

Measure:

- broadband RMS / A-weighted noise;
- PSD;
- BBD clock/heterodyne peaks;
- L/R noise correlation;
- modulation-synchronous noise variation.

Model targets:

```text
BbdNoise106
ClockFeedthrough106
```

Do not merge this with the synth's pre-VCF NOISE generator.

---

## 19. Solo-Unison timing validation

A high-speed logic/scope capture should validate the firmware-derived behavior:

- 31.25 kbit/s assigner->module traffic;
- six sequential Voice On messages;
- reset bit consumption at physical DCO service slots;
- legato new-note behavior;
- note-off followed by held-note fallback.

Capture serial RX plus at least one 8253 output; repeat for several physical voices if possible.

Goal: quantify real inter-voice unison reset staggering rather than forcing one simultaneous DSP reset.

---

## 20. Dataset naming

Recommended filename format:

```text
J106_<serial>_<board>_<block>_<test>_<date>_<take>.<ext>
```

Example:

```text
J106_461234_module_DCO_RAMP_C4_8FT_2026-11-02_T01.csv
```

Companion YAML/JSON metadata:

```text
instrument
board_rev
warmup_minutes
supply_rails
probe
scope
sample_rate
patch/test_mode
controls
calibration_state
notes
sha256
```

---

## 21. Priority if access to a real 106 is limited

If only one short hardware session is available, prioritize:

1. **chorus CP clock captures I/II** — hardest to infer accurately from public data;
2. **MC5534 ramp/reset captures** across range/note;
3. **HPF 0/1/2/3 sweeps**;
4. **chorus dry/wet + absolute delay + transfer sweep**;
5. **DCO-CV S/H voltage/droop**;
6. **80017A cutoff/resonance/drive curves**;
7. **global VCA curve**;
8. **source mixer/SUB/noise control curves**;
9. **control-loop timing / Solo-Unison logic timing**.

The Roland factory anchors should be recorded regardless because they establish whether the measured unit is a valid reference.

---

## 22. Phase-0 exit rule

An unresolved analogue parameter no longer blocks Phase 1 if this document defines:

```text
parameter
measurement method
temporary hypothesis/default
validation metric
```

Phase-1 code must mark such constants as `MEASURE_PENDING` and centralize them in calibration data rather than scattering magic numbers through DSP code.

Once physical data arrives, replacement of `MEASURE_PENDING` constants must not require an architecture rewrite.
