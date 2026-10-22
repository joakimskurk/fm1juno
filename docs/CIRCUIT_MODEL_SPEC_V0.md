# FM1JUNO Circuit Model Specification v0

Status: Phase 0 / JUNO-106-first baseline. This file defines the *shape* of the reference engine before implementation. Numerical constants that are not yet supported by evidence remain explicitly open.

## 0. Scope

Primary target: Roland JUNO-106 architecture and behaviour.

Secondary reference: Roland JUNO-60 only where it:

- reveals shared circuit ancestry hidden by 106 hybrid packages;
- offers a better measurement method;
- later supplies a deliberate DEEP-mode alternative.

Authentic `MODEL_106` must not silently inherit 60-specific clock, envelope, HPF or chorus behaviour.

## 1. Runtime domains

The reference engine should distinguish four time domains.

### 1.1 Audio domain

44.1 kHz target-compatible sample stream for the eventual FM-1 port. Desktop reference builds may run higher internally for validation.

### 1.2 DCO event domain

Timer terminal-count / edge events derived from a shared master-clock concept and integer programmable counts. These events reset/trigger analogue-inspired waveform generation.

### 1.3 106 control-frame domain

Multiplexed DAC/S&H control service. Primary-source timing target: approximately 4.2 ms per full service cycle (~238 Hz). Parameters represented through this domain must support quantised updates at this cadence in authentic mode.

### 1.4 UI/MIDI domain

Asynchronous parameter/key events that update target state. Audio/control domains consume bounded state snapshots; no allocation or locks in audio rendering.

## 2. Voice structure

Authentic polyphony: 6 voices.

Per voice state:

- assignment state: idle / held / released / stolen;
- key number and velocity placeholder;
- note age / allocation ordinal;
- current DCO range;
- programmable timer count and phase/edge state;
- DCO ramp capacitor state;
- pulse comparator state;
- sub divide state;
- digital-envelope state;
- sampled DCO CV;
- sampled PWM CV;
- sampled VCF CV;
- sampled VCA CV;
- per-voice VCF calibration;
- per-voice resonance calibration;
- per-voice VCA gain/bias/offset calibration;
- optional DEEP tolerance terms, disabled in strict 106 mode unless they represent plausible factory variation.

## 3. DCO clock/timer model

### 3.1 Master clock

Base clock: 8 MHz.

Range divider:

- 4': /2 -> nominal 4 MHz timer clock
- 8': /4 -> nominal 2 MHz timer clock
- 16': /8 -> nominal 1 MHz timer clock

This range division occurs before the two 8253 timer ICs.

### 3.2 Timer counts

Each voice is one counter in a 16-bit programmable interval timer. Divide data is computed from:

- key pitch;
- LFO pitch modulation;
- bender;
- portamento;
- tune.

Open item: identify exact 8253 mode and whether the audible edge period corresponds directly to N/fclk, 2N/fclk or a mode-dependent duty pattern. Until resolved, reference code must isolate this relation behind a `Timer106` abstraction rather than embedding a guessed formula throughout the oscillator.

### 3.3 Count quantisation

Authentic mode must store and update an integer count. Frequency therefore follows timer quantisation rather than an unconstrained floating phase increment.

Required validation vectors once timer mode is resolved:

- MIDI/key C2, C3, C4, C5, C6 at all three ranges;
- +/-50 cent tune extremes;
- max LFO pitch modulation;
- bender extremes;
- slow and fast portamento trajectories;
- unison/shared-clock phase behaviour.

### 3.4 DCO update timing

Do not assume timer count changes continuously every audio sample. Count writes are CPU/control events. The reference engine should permit count updates at control/event boundaries and test the audible consequence of updating immediately vs next waveform cycle.

Exact write cadence is open pending ROM analysis/direct measurement.

## 4. MC5534-inspired wave generator

The wave generator is modelled as analogue waveform formation triggered by DCO timing edges.

### 4.1 Saw

State variable represents the ramp capacitor.

On each DCO reset edge:

1. discharge/reset the ramp state with a finite reset characteristic if evidence supports it;
2. begin charge toward the next edge using a slope determined by sampled DCO CV and range-dependent scaling;
3. target approximately constant ~12 Vpp hardware-domain ramp amplitude across note/range before model normalisation.

Reference implementation should retain a hardware-voltage-like internal scale through wave generation and mixer calibration before converting to final normalised DSP levels.

### 4.2 DCO amplitude compensation

DCO CV contains key + LFO + bend + portamento + tune contribution and is transformed by range-selected resistor networks before the ramp generator.

Purpose: compensate the natural frequency dependence of a fixed-RC charge ramp so saw/pulse amplitude stays nearly constant.

Open: derive transfer function from schematic component values and validate against measured captures.

### 4.3 Pulse/PWM

Pulse derives from comparing saw voltage against sampled PWM CV.

Calibration anchors from primary service data:

- PWM CV around +6 V -> ~50% duty;
- PWM CV around +0.6 V -> ~95% duty;
- factory 50% tolerance 48–52%;
- factory high-PWM tolerance 93–97%;
- PWM OFF drives comparator into disabled pulse state.

The authentic model should therefore share the saw reset/shape and not synthesize an unrelated ideal pulse.

### 4.4 Sub

Sub is a divide-by-two rectangular waveform derived from the DCO/wave-generator chain, one octave below the selected range. Level is controlled by a sampled analogue control value.

### 4.5 Anti-aliasing

Anti-aliasing is an implementation layer, not a source-model replacement.

Preferred order for reference experiments:

1. event-aware BLEP/minBLEP corrections at saw reset and pulse transitions;
2. selective 2x oversampling if filter/wave nonlinearity still aliases materially;
3. avoid global 4x oversampling until FM-1 profiling proves it affordable.

## 5. Noise and oscillator mixer

Inputs:

- saw switch;
- pulse switch;
- sub level;
- noise level.

Strict model requirements:

- preserve switch behaviour for saw/pulse;
- preserve continuous sampled controls for sub/noise;
- model the real pre-VCF source gain/headroom once schematic gains are extracted;
- retain enough headroom to reproduce filter drive changes when multiple sources are combined.

Open: exact source gains and noise spectrum/amplitude law.

## 6. Digital envelope / CV generator

The 106 envelope is software-generated by the slave CPU.

Nominal segment ranges:

- A: 1.5 ms–3 s
- D: 1.5 ms–12 s
- S: 0–100%
- R: 1.5 ms–12 s

Architecture:

1. key/voice events update envelope state;
2. envelope algorithm computes a control value in the control domain;
3. ENV contribution is folded into the composite VCF CV and/or VCA CV;
4. values reach analogue circuitry through the multiplexed DAC/S&H system.

Strict 106 mode should expose the ~4.2 ms service-frame character. Optional interpolation may exist as an A/B diagnostic but must not become the default until hardware comparison supports it.

Open:

- segment law/curve;
- parameter-to-time mapping;
- numerical resolution;
- retrigger level/state;
- exact relation between service frame and ENV arithmetic step.

These are high priority for hardware capture or ROM analysis.

## 7. VCF — 80017A topology

### 7.1 Structure

Per voice:

- four OTA low-pass stages based on IR3109 family topology;
- controlled resonance feedback using BA662;
- final per-voice BA662 VCA.

### 7.2 DSP candidate

Reference candidate: four cascaded one-pole OTA-integrator stages represented with a topology-preserving transform, plus resonance feedback and stage/VCA nonlinearities.

Requirements:

- 24 dB/oct low-pass behaviour;
- stable self oscillation;
- calibrated cutoff tracking;
- resonance amplitude matching service targets;
- per-voice calibration terms;
- preserve drive-dependent behaviour around OTA/VCA nodes rather than adding one generic waveshaper after the filter.

### 7.3 Control voltage

Composite VCF CV includes:

- base cutoff;
- ENV with polarity;
- LFO;
- keyboard follow;
- bender.

Primary nominal ranges:

- cutoff 5 Hz–50 kHz stated service range;
- ENV +/-14 octaves;
- LFO +/-3.5 octaves;
- bender +/-3.5 octaves.

Open: exact exponential conversion and control scaling. Initial reference model should work in an explicit control-voltage/log-frequency coordinate so later calibration does not require restructuring.

### 7.4 Factory calibration as evidence

Per-voice trimmers exist for:

- resonance;
- VCA gain;
- VCF frequency;
- VCF width.

Reference model therefore stores voice-specific calibration parameters even if strict factory-new defaults are matched closely.

## 8. Voice VCA

Per-voice VCA is the second BA662 section in the 80017A.

Composite VCA control includes:

- ENV mode; or
- GATE mode;
- patch VCA level in the broader signal path/control system.

Model requirements:

- gain control curve must be calibrated, not assumed linear in UI value;
- include bias/offset capability because service procedures explicitly trim both bias and thump/offset-related behaviour;
- nonlinear VCA behaviour should be tested at high internal signal levels.

## 9. Voice summing and global HPF

Six voice outputs sum before the global static HPF/output chain.

Authentic HPF states:

- 0: low-frequency boost;
- 1: nominal bypass/unprocessed response;
- 2: high-pass state;
- 3: higher high-pass state.

Strict model uses four discrete transfer functions.

The 106 includes buffering before the global output VCA; do not import the JUNO-60 load-dependent rolloff into `MODEL_106`.

Open: derive exact transfer functions from jack-board schematic and validate with hardware sweeps.

## 10. Global VCA / output gain

The post-HPF global level path includes a uPC1252-family VCA and associated buffering. Keep this separate from six per-voice VCAs because:

- it changes global gain/headroom before chorus/output;
- it participates in the 60-vs-106 HPF/output difference;
- it may add its own nonlinear/level-dependent character.

Open: control law and saturation/headroom.

## 11. Chorus 106

Signal architecture from schematic:

- global mono/post-HPF signal split into two chorus channels;
- each channel uses MN3009 BBD;
- each BBD is clocked by MN3101-family driver circuitry;
- separate bias trim per BBD channel;
- surrounding transistor/op-amp filtering and gain stages;
- stereo outputs.

Authentic front-panel modes: OFF, I, II.

Do not reuse JUNO-60 I/II/I+II constants as 106 truth.

Reference chorus block should be capable of representing:

- variable BBD delay driven by clock/LFO;
- discrete BBD-sampling character if audible;
- pre/post bandwidth limitation;
- BBD bias/headroom nonlinearity;
- clock/noise floor;
- stereo modulation relationship;
- dry/wet topology from actual schematic.

Service-bias procedure confirms nonlinear clipping symmetry is relevant enough to calibrate in hardware.

High-priority measurement requirement: capture I and II rate, delay min/max, stereo phase, spectral rolloff and noise on real 106 hardware or a trustworthy direct measurement set.

## 12. LFO, delay and performance control

Primary nominal LFO range: 0.1–30 Hz. Delay time: 0–3 s.

LFO affects at least DCO and VCF, and modulation/bender controls are read by the slave CPU.

Open:

- exact waveform shape and symmetry;
- rate law;
- delay envelope law;
- trigger-sensitivity semantics;
- internal update resolution.

Until measured, use no generic sine-LFO assumption in the locked model. A temporary diagnostic LFO may be used only behind a clearly named hypothesis flag.

## 13. Voice allocation / retrigger

Allocator must be a separate module from DSP voices so normal-mode behaviour can be corrected without touching audio blocks.

Known service-test behaviours provide evidence for:

- ordered/non-rotary assignment;
- rotary/cyclic assignment;
- seventh-note stealing;
- six-voice unison test mode.

Normal `POLY 1` and `POLY 2` semantics remain to be frozen from owner documentation + tests.

## 14. MODEL_60 borrowing policy

Allowed in strict 106 implementation only when the topology is demonstrably shared.

Examples:

- IR3109/BA662 stage behaviour can inform 80017A modelling because teardown confirms the same IC family inside the hybrid.
- 60 DCO ramp-reset measurements can suggest test methods, but 60 master-clock modulation must not replace the 106 fixed-clock/timer arithmetic.
- 60 IR3R01 ENV is not valid for strict 106.
- 60 HPF loading is not valid for strict 106.
- 60 chorus constants are not valid for strict 106 without 106 corroboration.

DEEP mode may later expose 60-derived alternatives as explicit options.

## 15. Reference-engine APIs (provisional)

These names are architectural placeholders, not frozen implementation API.

```text
Control106
  process_ui_event(...)
  tick_control_frame(...)
  build_voice_targets(...)

Timer106[6]
  set_range(...)
  set_count(...)
  advance_audio_time(...)
  emit_edges(...)

Wave5534[6]
  on_dco_edge(...)
  set_dco_cv(...)
  set_pwm_cv(...)
  render(...)

Envelope106[6]
  note_on(...)
  note_off(...)
  tick(...)

Filter80017[6]
  set_vcf_cv(...)
  set_resonance(...)
  process(...)

Vca80017[6]
  set_vca_cv(...)
  process(...)

Hpf106
  set_mode(0..3)
  process(...)

Chorus106
  set_mode(OFF/I/II)
  process_stereo(...)
```

## 16. Phase-0 exit tests

Before implementation is declared ready to begin, Phase 0 should resolve or consciously defer every item below:

- [ ] 8253 timer mode/count relationship
- [ ] DCO note/count table or derivation
- [ ] DCO CV/ramp slope law
- [ ] source mixer gains/headroom
- [ ] exact ENV curve/time mapping or documented measurement plan
- [ ] LFO waveform/rate/delay behaviour or documented measurement plan
- [ ] 80017A circuit constants sufficient for first reference filter
- [ ] 106 HPF transfer functions
- [ ] 106 chorus measured constants or measurement plan
- [ ] normal Poly 1/Poly 2 allocation/retrigger semantics
- [ ] FM-1 CPU/SRAM budget table prepared for Phase 4 profiling

Only then should the first reference DSP implementation be treated as a fidelity build rather than an exploratory prototype.
