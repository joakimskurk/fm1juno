# FM1JUNO Circuit Model Specification v0

Status: Phase 0 / JUNO-106-first baseline. This file defines the reference-engine architecture before production DSP implementation. Numerical constants that are not yet evidence-backed remain explicitly open.

## 0. Scope

Primary target: Roland JUNO-106 architecture and behavior.

Secondary reference: Roland JUNO-60 only where it:

- reveals shared circuit ancestry hidden by 106 hybrid packages;
- offers a better measurement method;
- later supplies a deliberate DEEP-mode alternative.

Authentic `MODEL_106` must not silently inherit 60-specific clock, envelope, HPF or chorus behavior.

## 1. Runtime domains

The reference engine distinguishes four time domains.

### 1.1 Audio domain

44.1 kHz target-compatible sample stream for the eventual FM-1 port. Desktop reference builds may run higher internally for validation.

### 1.2 DCO event domain

8253 Mode-3 count/edge events derived from a shared master-clock concept and integer programmable counts. These timing edges drive the analogue-inspired waveform generator.

### 1.3 106 control-frame domain

Multiplexed DAC/S&H control service. Primary service documentation gives approximately 4.2 ms per full control cycle. Reverse-engineering/direct-measurement work points to roughly 4.23 ms / 236 Hz as a strong calibration target, but exact workload-dependent cadence remains open.

Strict mode must preserve quantized firmware-rate updates for controls represented in this domain.

### 1.4 UI/MIDI/assigner domain

Asynchronous key, panel and MIDI events feed an assigner/control state machine. Audio/control rendering consumes bounded snapshots; no allocation or locks occur in the audio callback.

## 2. Voice structure

Authentic normal polyphony: six physical voices. A seventh simultaneously held key is ignored rather than stealing a sounding voice.

Per physical voice state:

- assigner state: free / allocated;
- current/preserved note identity;
- note-gate state;
- sustain-running state;
- current 8.8 fixed-point Portamento pitch;
- current DCO range;
- integer 8253 timer count;
- Mode-3 active/pending count and output phase;
- pending ordinary count reload vs reset-and-reload request;
- DCO ramp capacitor state;
- pulse comparator state;
- sub divide state;
- 14-bit digital-envelope value/state;
- sampled DCO CV;
- sampled PWM CV;
- sampled VCF CV;
- sampled VCA CV;
- per-voice VCF calibration;
- per-voice resonance calibration;
- per-voice VCA gain/bias/offset calibration;
- optional DEEP tolerance terms, disabled in strict 106 mode unless they model plausible factory variation.

Poly 1 additionally owns a six-entry dynamic voice-order queue. Poly 2 does not use that queue for normal allocation.

## 3. DCO clock/timer and pitch model

### 3.1 Master clock and range

Base DCO master clock: 8 MHz.

Range division before the two 8253 timer ICs:

- 4': /2 -> nominal 4 MHz timer clock;
- 8': /4 -> nominal 2 MHz timer clock;
- 16': /8 -> nominal 1 MHz timer clock.

### 3.2 Confirmed 8253 mode

The module ROM programs each counter with:

- `0x36` for counter 0;
- `0x76` for counter 1;
- `0xB6` for counter 2.

These are 8253 Mode-3 square-wave-generator control words, binary counting, LSB then MSB.

Strict `Timer106` semantics:

- repetition frequency = `f_timer / N`;
- even `N`: high and low each `N/2` timer clocks;
- odd `N`: high `(N+1)/2`, low `(N-1)/2` clocks;
- ordinary count update and control-word reset are separate operations.

### 3.3 8.8 pitch coordinate

Pitch is represented as unsigned 8.8 fixed-point semitones:

- high byte = integer semitone;
- low byte = 1/256 semitone.

The final pitch path is now frozen as:

```text
base/global = 0x1818
            + signed Tune
            + signed DCO-LFO pitch amount
            + signed Bender pitch amount

per_voice_porta = step_toward(current_pitch, note << 8, porta_step)
final_pitch      = per_voice_porta + base/global
```

Portamento therefore moves the physical voice's note-domain pitch first. Tune, LFO and Bender are then applied as a global offset to every voice before timer/DCO-CV conversion.

### 3.4 Tune law

Tune magnitude is applied directly in the 8.8 pitch domain with separate sign.

Maximum magnitude is approximately `127/256` semitone = 49.6 cents, independently matching Roland's approximately +/-50-cent specification.

### 3.5 LFO pitch law

The integer firmware LFO magnitude is multiplied by a DCO-LFO depth quantity that already includes the LFO-delay/onset envelope and modulation-wheel contribution. The result is reduced to an 8.8-domain pitch amount; the LFO polarity bit determines addition/subtraction in the global pitch build.

Strict mode must not replace this with an audio-rate sinusoid applied directly to oscillator phase.

### 3.6 Bender pitch law

Bender processing produces separate DCO- and VCF-bend values. The DCO path is normalized into semitone/fraction form and then added/subtracted in the same global 8.8 pitch coordinate.

Roland's approximately +/-1200-cent DCO Bender range is a validation anchor.

Open: compact clean-room equation for every intermediate Bender scaling/truncation step.

### 3.7 Portamento law

Portamento is per physical voice and firmware-rate.

Observed control conversion:

- zero selects the OFF/no-step path;
- non-zero 8-bit control is shifted right once;
- the resulting 0..127 index selects an integer pitch-step coefficient;
- each control pass adds/subtracts that constant step in 8.8 semitone space;
- overshoot is clamped exactly to the target note.

This is not an audio-rate exponential glide in Hz.

### 3.8 Timer-divisor conversion

The ROM contains a 16-bit divisor table. Given integer semitone `n` and fractional byte `f`:

1. fetch divisor `N0` for `n`;
2. fetch `N1` for `n+1`;
3. compute `N0-N1`;
4. multiply that difference by `f` with ROM-style integer truncation;
5. subtract the scaled difference from `N0`;
6. write result LSB then MSB to the selected counter.

Strict mode should reproduce this count-space interpolation rather than substitute `round(clock / ideal_frequency)`.

### 3.9 Ordinary count reload vs oscillator reset

**Ordinary modulation/pitch update:** write count LSB/MSB only.

8253 Mode-3 documentation indicates a new count written while counting becomes active at the next output transition/end of the current half-cycle. The current half-cycle is not replaced immediately.

**Reset-and-load:** write the counter-specific Mode-3 control word, then LSB/MSB count. This is a fresh timer programming/synchronization event.

Required API distinction:

```text
Timer106::queue_count(N)
Timer106::reset_and_load_count(N)
```

### 3.10 Confirmed oscillator-reset decision

Every module Voice On re-arms envelope Attack, but does not necessarily reset the DCO timer.

Observed decision:

| Incoming state | 8253 reset? | ENV Attack? |
| --- | --- | --- |
| Same note + running | No | Yes |
| Same note + not running | No | Yes |
| Different note + running/sustained | No | Yes |
| Different note + not running | Yes | Yes |

Therefore timer phase and envelope articulation are independent state decisions.

### 3.11 DCO validation vectors

- integer and fractional notes across all three ranges;
- Tune center/endpoints;
- LFO positive/negative peak;
- Bender extremes and sensitivity settings;
- Portamento up/down trajectories;
- ordinary count update during both Mode-3 half-cycles;
- reset-and-load versus ordinary reload phase behavior;
- same-note re-articulation;
- different-note legato/sustain re-assignment;
- Solo-Unison once its normal-mode physical timing is frozen.

## 4. MC5534-inspired wave generator

The wave generator is modeled as analogue waveform formation driven by DCO timing edges.

### 4.1 Saw

State variable represents the ramp capacitor.

On each relevant DCO timing edge:

1. discharge/reset the ramp state with a finite characteristic if supported by evidence;
2. charge toward the next timing edge with slope determined by DCO CV and range-selected analogue scaling;
3. target approximately constant ~12 Vpp hardware-domain ramp amplitude over note/range before final DSP normalization.

Maintain a hardware-voltage-like internal scale through wave generation and mixer calibration where practical.

### 4.2 Digital DCO-CV law

The ROM's DCO-CV note data are well described, before DAC saturation, by the independently derivable law:

```text
cv_code(n) ~= round(32 * 2^(n/12))
```

The code approximately doubles every octave and is fractionally interpolated using the same final 8-bit pitch fraction as the timer-divisor path.

This fits the service-manual explanation: to keep capacitor-ramp amplitude nearly constant while oscillator frequency rises, charging current must rise approximately in proportion to frequency.

Strict mode should implement the clean derived law plus ROM-equivalent quantization/interpolation rather than copy the complete original table.

### 4.3 Analogue DCO-CV/range network

The derived digital code is transformed by range-selected resistor/current circuitry before the MC5534 ramp capacitor.

Still open and must not be guessed:

- R85/R86/R87 exact values/effective ratios;
- C54/effective ramp capacitance;
- current-source/transistor nonlinearity;
- DAC/S&H source impedance and settling;
- finite discharge transient;
- real ramp amplitude versus pitch/range.

### 4.4 Pulse/PWM

Pulse derives from comparison of the actual saw/ramp voltage with sampled PWM CV.

Service calibration anchors:

- PWM CV about +6 V -> ~50% duty;
- PWM CV about +0.6 V -> ~95% duty;
- factory 50% tolerance 48–52%;
- factory high-PWM tolerance 93–97%;
- PWM OFF drives the comparator into disabled pulse state.

Saw and pulse must share the same ramp state; do not synthesize an unrelated ideal pulse.

### 4.5 Sub

Sub is a divide-by-two rectangular waveform derived from the DCO/wave-generator chain, one octave below the selected range. Level is controlled by sampled analogue control.

### 4.6 Anti-aliasing

Anti-aliasing is an implementation layer, not a source-model replacement.

Preferred order:

1. event-aware BLEP/minBLEP corrections at saw reset and pulse/sub transitions;
2. selective 2x oversampling where nonlinear blocks materially alias;
3. avoid global 4x oversampling until FM-1 profiling proves it affordable.

## 5. Noise and oscillator mixer

Inputs:

- saw switch;
- pulse switch;
- sub level;
- noise level.

Strict requirements:

- preserve discrete saw/pulse switch behavior;
- preserve continuous sampled sub/noise controls;
- model actual pre-VCF source gain/headroom once schematic values are extracted;
- preserve enough internal headroom that simultaneous sources change filter drive correctly.

Open:

- exact saw/pulse/sub/noise gains;
- sub control law;
- noise control law;
- noise spectrum/bandwidth;
- pre-VCF clipping/headroom.

## 6. Digital envelope / CV generator

The 106 envelope is software-generated by the slave CPU; a generic ADSR is not acceptable for strict mode.

Nominal front-panel ranges:

- A: 1.5 ms–3 s;
- D: 1.5 ms–12 s;
- S: 0–100%;
- R: 1.5 ms–12 s.

### 6.1 ROM representation

Envelope level is a 14-bit integer domain with peak around `0x3FFF`.

Firmware contains:

- 16-bit Attack increment table;
- shared 16-bit Decay/Release coefficient table;
- fixed-point decay helper using three 8x8 partial products.

### 6.2 Attack

Attack is an additive integer ramp. Each firmware envelope update adds the selected Attack increment until peak/clamp.

Crucially, Voice On does **not** force the current envelope value to zero. Attack reads the current envelope and adds from there.

Strict retrigger rule: current-level re-attack.

### 6.3 Decay/release

With `value = VH:VL` and `coeff = CH:CL`, the ROM helper behaves as:

```text
VH*CH + ((VH*CL) >> 8) + ((VL*CH) >> 8)
```

The low-low partial product is omitted.

Decay applies this to `env - sustain`, then restores the Sustain offset. Release applies it toward zero.

### 6.4 Phase labels

Do not expose the public disassembly's provisional bit names such as `rlsPhase` as clean implementation truth. The same bit participates in transitions that do not map cleanly to a simple one-label state.

Implement behavioral envelope states/transitions derived from control flow instead.

### 6.5 Service cadence and reconstruction

Envelope/CV values reach analogue circuitry through the multiplexed DAC/S&H network. Strict mode should preserve firmware-rate stepping.

Open:

- complete slider/index validation;
- exact loop cadence under different workloads;
- S/H and downstream RC settling/reconstruction;
- whether audio-rate reconstruction should be zero-order hold + analogue RC rather than interpolation.

## 7. VCF — 80017A topology

### 7.1 Structure

Per voice:

- four OTA low-pass stages based on IR3109 family topology;
- controlled resonance feedback using BA662;
- final per-voice BA662 VCA.

### 7.2 DSP candidate

Reference candidate: four cascaded one-pole OTA-integrator stages represented with a topology-preserving transform, plus resonance feedback and stage/VCA nonlinearities.

Requirements:

- 24 dB/oct low-pass behavior;
- stable self oscillation;
- calibrated cutoff tracking;
- resonance amplitude matching service targets;
- per-voice calibration terms;
- preserve drive-dependent behavior around OTA/VCA nodes rather than add one generic post-filter waveshaper.

### 7.3 Control voltage

Composite VCF CV includes:

- base cutoff;
- ENV with polarity;
- LFO;
- keyboard follow;
- Bender.

Primary nominal ranges:

- cutoff 5 Hz–50 kHz stated service range;
- ENV +/-14 octaves;
- LFO +/-3.5 octaves;
- Bender +/-3.5 octaves.

Open: exact exponential control conversion/scaling. Work in an explicit control-voltage/log-frequency coordinate so later calibration does not require architectural changes.

### 7.4 Factory calibration evidence

Per-voice trimmers exist for:

- resonance;
- VCA gain;
- VCF frequency;
- VCF width.

Reference model stores voice-specific calibration terms even if strict factory-new defaults are closely matched.

## 8. Voice VCA

Per-voice VCA is the second BA662 section in the 80017A.

Composite control includes ENV or GATE behavior plus broader level control.

Requirements:

- calibrated gain-control law, not UI-linear assumption;
- bias/offset capability because service procedures explicitly trim related behavior;
- test nonlinear VCA behavior at high internal levels.

Gate mode must follow module `voiceRun` semantics, not simply raw MIDI gate, because sustain affects `voiceRun`.

## 9. Voice summing and global HPF

Six voice outputs sum before the global HPF/output path.

Authentic HPF states:

- 0: low-frequency boost;
- 1: nominal flat/bypass response;
- 2: high-pass state;
- 3: higher high-pass state.

Strict mode uses four discrete transfer functions.

The 106 includes buffering before the global output VCA; do not import JUNO-60 load-dependent rolloff into strict 106 mode.

Open: exact transfer functions from jack-board component values and hardware validation.

## 10. Global VCA / output gain

Post-HPF global level path includes a uPC1252-family VCA and buffering. Keep it separate from the six per-voice VCAs.

Open: exact gain-control law and saturation/headroom.

## 11. Chorus 106

Signal architecture:

- global mono/post-HPF signal split into two chorus channels;
- each channel uses MN3009 BBD;
- each BBD clocked by MN3101-family driver circuitry;
- separate BBD bias trim per channel;
- surrounding filtering/gain stages;
- stereo outputs.

Authentic front-panel modes: OFF, I, II.

Do not reuse JUNO-60 chorus constants as 106 truth without 106 corroboration.

Reference chorus must be capable of:

- variable BBD delay from actual clock/LFO behavior;
- BBD bandwidth/sample character where audible;
- pre/post filtering;
- bias/headroom nonlinearity;
- clock/noise floor;
- correct stereo modulation relationship;
- actual dry/wet topology.

Open/high priority: I/II rates, min/max delay, stereo phase, spectral rolloff, noise/bias behavior.

## 12. LFO, delay and performance control

The 106 LFO is a firmware-generated integer control process, not a generic audio-rate sine/triangle oscillator.

### 12.1 LFO accumulator

Confirmed state:

- magnitude accumulator `0x0000..0x1FFF`;
- integer rate coefficient selected by panel setting;
- direction changes at endpoints;
- hard endpoint clamp;
- one flag bit controls rising/falling;
- another supplies signed modulation polarity.

A full signed period consists of four half-ramp segments:

1. 0 -> max positive;
2. max -> 0 positive;
3. 0 -> max negative;
4. max -> 0 negative.

Integer stepping creates real high-rate frequency quantization/plateaus. Preserve it in strict mode.

### 12.2 LFO Delay

Delay is two-stage digital behavior:

1. holdoff period;
2. linear depth-ramp/onset stage.

Do not replace it with a single generic fade-in.

Open: exact normal-play/manual-trigger reset semantics and final calibrated loop cadence.

### 12.3 Portamento

Portamento law is frozen in section 3.7. Interaction with Poly 2 follows physical-voice first-free allocation, so the glide state remains attached to the reused physical voice.

## 13. Voice allocation / retrigger

Allocator is separate from DSP voices and from module timer/envelope decisions.

### 13.1 User-mode contract

Roland owner documentation confirms:

- `POLY 1`: six-voice polyphony optimized for natural releases;
- `POLY 2`: one voice per key, intended for Portamento; only the final note/note group naturally receives its full release if no new allocation follows;
- if six keys are already held, additional keys do not sound;
- `POLY 1 + POLY 2`: Solo-Unison, monophonic, all six voices assigned to the played key.

Normal user-mode behavior must not be inferred from separate service-test rotary/non-rotary labels.

### 13.2 Assigner structures

`noteTbl` stores one byte per physical voice:

- bit 7 = free flag;
- low seven bits retain current/previous note.

`voiceTbl` is a reorderable list of physical voice IDs used by Poly 1.

### 13.3 Poly 2

Poly 2:

- scans physical `noteTbl` from first to last;
- allocates the first free physical voice;
- if none are free, ignores the new note;
- note-off marks the matching physical voice free;
- does not maintain release-age ordering.

This naturally permits a recently released low-index voice to be immediately reused, shortening its release tail under continued playing.

### 13.4 Poly 1

Poly 1:

- requires at least one free voice; otherwise new note is ignored;
- maintains active/free order via `voiceTbl`;
- on note-off, bubbles the released physical voice to the back of the queue;
- for a new note, first searches free voices for one whose preserved previous note matches the requested note;
- if found, reuses that same physical voice;
- otherwise takes the free voice at the active/free boundary, effectively favoring an older free voice over the most recently released one.

This explains Poly 1's more natural release behavior without invoking generic round-robin or voice stealing.

### 13.5 Sustain / `voiceRun`

Module `voiceRun` is derived from gate and sustain:

- sustain off: running set follows current gates;
- sustain on: previously running voices are retained while new gates are OR-ed in.

`voiceRun` is therefore not equivalent to "audible release tail exists".

### 13.6 Retrigger separation

Module Voice On always requests envelope Attack. Timer reset is decided independently using previous note + `voiceRun`, as frozen in section 3.10.

Same-note replay can re-attack the envelope while leaving the 8253 phase free-running.

## 14. MODEL_60 borrowing policy

Allowed in strict 106 only when topology is demonstrably shared.

Examples:

- IR3109/BA662 stage behavior may inform 80017A modeling because teardown confirms the same IC family inside the hybrid;
- 60 DCO ramp/reset measurements can suggest test methods, but 60 master-clock modulation must not replace 106 fixed-clock/8253 arithmetic;
- 60 IR3R01 ENV is not valid for strict 106;
- 60 HPF loading is not valid for strict 106;
- 60 chorus constants are not valid for strict 106 without 106 corroboration.

DEEP mode may later expose 60-derived alternatives explicitly.

## 15. Reference-engine APIs (provisional)

```text
Assigner106
  note_on(note)
  note_off(note)
  set_mode(POLY1/POLY2/SOLO_UNISON)
  set_sustain(...)

Control106
  process_ui_event(...)
  tick_control_frame(...)
  build_voice_targets(...)

Pitch106
  global_offset_8_8(tune, lfo, bend)
  portamento_tick(...)
  timer_count_from_pitch(...)
  dco_cv_code_from_pitch(...)

Timer106[6]
  set_range(...)
  queue_count(...)
  reset_and_load_count(...)
  advance_audio_time(...)
  emit_edges(...)

Wave5534[6]
  on_dco_edge(...)
  set_dco_cv(...)
  set_pwm_cv(...)
  render(...)

Envelope106[6]
  note_on_from_current_level(...)
  note_off(...)
  tick_control_frame(...)

Lfo106
  set_rate(...)
  set_delay(...)
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

Before a build is called fidelity/reference implementation, Phase 0 should resolve or explicitly defer each item below.

- [x] 8253 Mode-3 timer/count relationship.
- [x] ROM-style note-divisor interpolation mechanism.
- [x] Global/per-voice pitch-build ordering.
- [x] Tune law in 8.8 domain.
- [x] Portamento architecture/indexing.
- [x] Normal count reload vs reset-and-load semantics.
- [x] Normal Poly 1 / Poly 2 allocator behavior and no-seventh-note rule.
- [x] Module oscillator-reset decision matrix.
- [x] Current-level envelope re-attack behavior.
- [x] Core 106 ENV arithmetic/table architecture.
- [x] LFO accumulator/rate architecture and two-stage Delay structure.
- [x] Digital DCO-CV octave/exponential law.
- [ ] Exact Bender normalization/truncation.
- [ ] Exact control-loop timing / workload variation.
- [ ] Solo-Unison physical timer/retrigger behavior.
- [ ] Analogue DCO-CV/range-network/ramp constants.
- [ ] Source mixer gains/headroom and noise path.
- [ ] 80017A circuit constants sufficient for first reference filter.
- [ ] 106 HPF transfer functions.
- [ ] 106 chorus clock/delay/filter constants or explicit hardware-measurement plan.
- [ ] FM-1 CPU/SRAM budget table prepared for implementation profiling.

No generic VA assumption may silently cross this gate.