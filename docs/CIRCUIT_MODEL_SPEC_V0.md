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

8253 Mode-3 count/edge events derived from a shared master-clock concept and integer programmable counts. These timing edges reset/trigger the analogue-inspired waveform generator.

### 1.3 106 control-frame domain

Multiplexed DAC/S&H control service. Primary-source timing target: approximately 4.2 ms per full service cycle (~238 Hz nominal from the service drawing). Reverse-engineering/hardware work points to ~4.23 ms / ~236 Hz as a stronger calibration target, but exact loop timing is not yet frozen because firmware workload may perturb it.

Parameters represented through this domain must support quantised updates at this cadence in authentic mode.

### 1.4 UI/MIDI domain

Asynchronous parameter/key events that update target state. Audio/control domains consume bounded state snapshots; no allocation or locks in audio rendering.

## 2. Voice structure

Authentic polyphony: 6 voices.

Per voice state:

- assignment state: idle / held / released / stolen;
- key number and note age / allocation ordinal;
- current 8.8 fixed-point pitch coordinate;
- current DCO range;
- integer 8253 timer count;
- Mode-3 timer state and output phase;
- pending normal count reload vs reset-and-reload state;
- DCO ramp capacitor state;
- pulse comparator state;
- sub divide state;
- 14-bit digital-envelope state;
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

### 3.2 Confirmed 8253 programming mode

The public module-ROM disassembly shows a dedicated setup routine that writes:

- `0x36` to counter 0;
- `0x76` to counter 1;
- `0xB6` to counter 2.

These are 8253 **Mode 3 square-wave-generator** control words with binary counting and LSB-then-MSB count loading.

Therefore strict `Timer106` semantics are now defined:

- output repetition frequency is `f_timer / N` for integer count `N`;
- for even `N`, high and low durations are each `N/2` timer clocks;
- for odd `N`, high duration is `(N+1)/2` clocks and low duration `(N-1)/2` clocks;
- writing a control word/reset path and loading a new count is semantically distinct from merely loading a new count while a voice is already running.

### 3.3 Pitch coordinate and count generation

Module firmware handles pitch in an 8.8 fixed-point semitone coordinate:

- upper byte: semitone;
- lower byte: 1/256 semitone fraction.

The pitch-build routine begins from a fixed base value and adds/subtracts Tune, LFO and other modulation terms before conversion to a timer count.

The ROM contains a 16-bit note-divisor table beginning at `$0F30`. The count-conversion routine:

1. selects the integer-note table entry;
2. fetches the next-note divisor;
3. computes their difference;
4. scales that difference by the 8-bit fractional pitch amount;
5. subtracts the interpolation amount from the selected divisor;
6. writes the resulting integer count LSB then MSB to the selected 8253 channel.

Strict mode should implement this ROM-domain path rather than computing `round(clock / ideal_frequency)` directly. An ideal-frequency path may remain as an A/B diagnostic.

### 3.4 Count reload vs oscillator reset

The ROM has two paths:

**Running voice / ordinary modulation:** load the new divisor LSB then MSB without rewriting the Mode-3 control word.

**Voice reset:** select a per-counter `36/76/B6` control word, write it to the appropriate 8253 command register, then return to the normal count-load path.

This implies that phase/retrigger behaviour belongs in the control/allocator model. Do not reset oscillator phase for every pitch update or every generic MIDI event.

Open: freeze the exact allocator/retrigger conditions that set the ROM's voice-reset bits.

### 3.5 Required DCO validation vectors

- C notes across keyboard and all three ranges using ROM table/count path;
- all twelve semitone table entries and fractional interpolation midpoints;
- +/-50-cent Tune extremes;
- max LFO pitch modulation;
- Bender extremes;
- slow and fast Portamento trajectories;
- ordinary count reload while a Mode-3 cycle is active;
- reset-and-reload phase behaviour;
- Solo-Unison/shared-clock phase behaviour.

## 4. MC5534-inspired wave generator

The wave generator is modelled as analogue waveform formation triggered by DCO timing edges.

### 4.1 Saw

State variable represents the ramp capacitor.

On each relevant DCO timing transition:

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

The 106 envelope is software-generated by the slave CPU and is now sufficiently understood to reject a generic ADSR implementation.

Nominal front-panel ranges:

- A: 1.5 ms–3 s
- D: 1.5 ms–12 s
- S: 0–100%
- R: 1.5 ms–12 s

### 6.1 Confirmed ROM representation

Envelope level is a 14-bit integer domain with peak around `0x3FFF`.

The module ROM contains:

- a 16-bit Attack increment table at `$0B60`;
- a shared 16-bit Decay/Release coefficient table at `$0D60`;
- a fixed-point decay helper at `$083D`.

### 6.2 Attack

Attack is an **additive integer ramp**. Each envelope/control tick adds the table-derived Attack increment until the peak condition is reached.

The ROM attack table begins:

```text
4000 2000 1000 0AAA 0800 0666 0555 0492 ...
```

This is fundamentally different from the JUNO-60 IR3R01 RC attack.

### 6.3 Decay and release

The ROM's `CalcDecay` routine uses three 8x8 multiply partial products. With `value = VH:VL` and `coeff = CH:CL`, the resulting approximation is equivalent to:

```text
VH*CH + ((VH*CL) >> 8) + ((VL*CH) >> 8)
```

The low-low product is omitted. This truncation is part of strict firmware behaviour.

Decay applies the operation to the difference between current envelope and Sustain, then restores the Sustain offset. Release applies the same style of multiply toward zero.

The shared coefficient table begins:

```text
1000 3000 5000 7000 9000 A000 A800 B000
B800 C000 C800 D000 D800 E000 E800 F000
F080 F100 F180 F200 F280 F300 F380 F400 ...
```

### 6.4 Service cadence and analogue reconstruction

The digital envelope result reaches VCF/VCA control through the multiplexed DAC/S&H control system. Strict mode should therefore preserve the firmware/control-frame cadence rather than silently evaluating ADSR equations per audio sample.

Open questions are now narrower:

- exact slider/index transformations before table lookup;
- precise retrigger state/level rules for all allocator cases;
- exact loop period under different workload/voice states;
- S/H and downstream RC settling shape;
- whether audio-rate reconstruction should be zero-order hold plus analogue RC, rather than linear interpolation.

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

LFO affects at least DCO and VCF, and modulation/bender controls are read by the slave CPU. ROM tracing already confirms LFO pitch amount is added/subtracted in the 8.8 pitch-build domain before timer-divisor conversion.

Open:

- exact LFO accumulator/waveform shape and symmetry;
- rate law/table;
- delay envelope law;
- trigger-sensitivity semantics;
- exact bender scaling;
- portamento update law and interaction with Poly 2.

Until recovered, use no generic sine-LFO assumption in the locked model. A temporary diagnostic LFO may be used only behind a clearly named hypothesis flag.

## 13. Voice allocation / retrigger

Allocator must be a separate module from DSP voices so normal-mode behaviour can be corrected without touching audio blocks.

Roland owner documentation confirms:

- `POLY 1`: normal six-voice polyphony, intended for natural independent releases;
- `POLY 2`: also one voice per key, but only the last note/note group retains natural release; designed for portamento use; if six keys are simultaneously held, additional keys do not sound;
- `POLY 1 + POLY 2`: Solo-Unison, monophonic with all six voices assigned to the played key.

Service-test modes separately demonstrate rotary, non-rotary and six-voice assignment primitives, but those test-button labels must not be confused directly with normal user-mode behaviour.

Open: recover the exact normal-mode assigner state machine, release stealing, same-note retrigger and the conditions that cause module `resetVoiceBits`.

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

Pitch106
  build_pitch_8_8(...)
  pitch_to_timer_count(...)

Timer106[6]
  set_range(...)
  load_count(...)
  reset_and_load_count(...)
  advance_audio_time(...)
  emit_edges(...)

Wave5534[6]
  on_dco_edge(...)
  set_dco_cv(...)
  set_pwm_cv(...)
  render(...)

Envelope106[6]
  set_rom_indices(...)
  note_on(...)
  note_off(...)
  tick_control_frame(...)

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

- [x] 8253 timer mode/count relationship
- [x] Locate the ROM note-divisor table and interpolation mechanism
- [ ] Complete the pitch-build/modulation law and range-boundary handling
- [ ] DCO CV/ramp slope law
- [ ] source mixer gains/headroom
- [x] Core 106 ENV algorithm/table architecture
- [ ] Complete ENV slider/retrigger/control-loop timing validation
- [ ] LFO waveform/rate/delay behaviour or documented measurement plan
- [ ] 80017A circuit constants sufficient for first reference filter
- [ ] 106 HPF transfer functions
- [ ] 106 chorus measured constants or measurement plan
- [ ] exact normal Poly 1/Poly 2 allocator/retrigger state machine
- [ ] FM-1 CPU/SRAM budget table prepared for Phase 4 profiling

Only then should the first reference DSP implementation be treated as a fidelity build rather than an exploratory prototype.
