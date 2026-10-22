# FM1JUNO — architecture proposal

Status: research/design baseline. Implementation intentionally deferred.

## 1. Design principle

Build one shared Juno-family engine from circuit blocks, with model-specific behaviour selected at the block level. Do **not** build a generic VA synth and approximate JUNO-60/JUNO-106 character with presets or EQ.

Profiles:

- `MODEL_60`
- `MODEL_106`
- `MODEL_DEEP`

The shared blocks are oscillator/wave generation, mixer, four-pole low-pass topology, resonance path, VCA and BBD chorus. Model-specific behaviour lives primarily in control generation, envelope behaviour, HPF/output loading, calibration/spread and parameter laws.

## 2. Voice signal path

### 2.1 Note/control domain

Per voice state should include:

- MIDI note / key number
- voice assignment and key age
- DCO timer phase/count state
- LFO contribution
- pitch bend
- portamento state
- envelope state
- filter/VCA calibration offsets
- optional DEEP voice-card tolerances

Authentic profiles use six voices.

### 2.2 DCO timing core

The JUNO DCO is not merely a digital saw oscillator. The intended model separates **period timing** from **waveform generation**.

For the 106 profile:

1. derive period/count from key + LFO + bend + portamento + tune;
2. use range-dependent master-clock behaviour corresponding to the original 4'/8'/16' division scheme;
3. update period at cycle boundaries rather than continuously inside the cycle;
4. retain a shared master-clock concept across voices;
5. generate the audio waveform from an analogue-inspired charge/reset model;
6. derive pulse by comparison against the saw/ramp and sub by divide-by-two behaviour.

This is important for glide/LFO stepping, phase relationships and unison behaviour.

For the 60 profile, the exact DCO control/modulation path needs an additional schematic pass and measurement validation before it is frozen. It should not simply inherit the 106 control law.

Aliasing control should be separate from the model: preserve the intended reset/ramp shape, then use band-limited discontinuities and/or limited oversampling where necessary.

### 2.3 Wave mixer

Sources:

- saw
- pulse/PWM
- sub oscillator
- noise

The model should preserve original source-level laws and headroom before the VCF. DEEP mode may expose continuous source levels even where the original front panel used switches, but authentic modes must retain original behaviour.

### 2.4 VCF / resonance

Target topology: four-pole OTA low-pass corresponding to the IR3109 family signal path, with a separate controlled resonance feedback path. The 106 encapsulates the closely related filter/VCA circuitry in the 80017A module.

Preferred DSP direction:

- topology-preserving/TPT or state-space four-pole structure;
- zero-delay or iterated feedback only if needed for stable resonance behaviour;
- lightweight nonlinearity at the OTA/resonance/VCA stages rather than a generic output saturator;
- self-oscillation and cutoff tracking tested explicitly;
- optional selective 2× oversampling around the nonlinear filter if the hardware budget supports it.

Do not commit to 4× oversampling before FM-1 profiling.

### 2.5 Envelope and VCA

`MODEL_60` should reproduce the analogue envelope character associated with the IR3R01-based voice architecture, including component/voice variation where it is audible and measurable.

`MODEL_106` should reproduce digitally generated envelope/control behaviour, including update cadence and parameter quantisation where relevant.

Both feed the voice VCA model. The goal is to model envelope *shape and timing*, not only match nominal A/D/S/R endpoints.

### 2.6 Voice sum, global HPF and output VCA

The HPF is a global post-voice-summing stage and is one of the important 60/106 differentiators.

The architecture therefore keeps separate post-sum models:

- `HPF_60`: passive stepped network plus its downstream loading/output interaction.
- `HPF_106`: stepped network with the different position-0/buffer behaviour documented in the 106 circuit.

This stage must be measured/tested as a transfer function at multiple output-VCA levels; a fixed EQ lookup is not sufficient if the loading interaction proves significant.

### 2.7 Chorus

Model the chorus as an actual dual variable-delay/BBD-inspired subsystem:

- mono input to two delayed paths;
- two 256-stage MN3009-like BBD paths;
- opposite-phase modulation between left/right;
- pre/post bandwidth limitation;
- clock/delay quantisation where audible;
- mild signal-path nonlinearity;
- controllable authentic noise floor in DEEP mode;
- mode-dependent LFO/rate/depth behaviour.

Measured JUNO-60 reference targets for initial calibration:

- Chorus I: ~0.513 Hz, ~1.66–5.35 ms delay range.
- Chorus II: ~0.863 Hz, ~1.66–5.35 ms delay range.
- I+II: ~9.75 Hz, ~3.3–3.7 ms in the independent measurement set.

Treat these as calibration references, not immutable truth; validate against service documentation and additional captures.

## 3. Modulation/control

Authentic surface:

- DCO range 16'/8'/4'
- saw / pulse / sub / noise
- PWM with appropriate MAN/LFO/ENV routing per model
- resonant VCF
- positive/negative envelope modulation where present
- key follow
- VCA ENV/GATE behaviour
- ADSR
- LFO rate/delay and trigger behaviour
- pitch bend
- portamento where applicable
- hold / key transpose / arpeggiator behaviour for the 60 profile
- original 106 performance/poly behaviour where applicable

DEEP surface may add a second modulation envelope, velocity/aftertouch routing, expanded LFO shapes/ranges and clock sync, but these must live outside the authentic profile state.

## 4. DEEP mode

DEEP is an exposed circuit lab, not a generic mod-matrix synth pasted onto a Juno skin.

Candidate controls:

- 60 ↔ 106 block-level morph/switching
- per-voice calibration spread
- DCO clock/phase spread
- filter cutoff/resonance card spread
- envelope timing spread
- output/HPF loading amount
- BBD clock, bias, noise, bandwidth, stereo phase, wet/dry and headroom
- unison detune/phase policy
- optional 8/12 voice mode if benchmarks allow
- expanded modulation and performance routing

A single AGE macro may be provided, but internally it should drive physically meaningful tolerances rather than random pitch/filter noise.

## 5. FM-1 implementation constraints

Baseline platform assumptions from the current open SDK/repository:

- 44.1 kHz sample rate
- 128-frame audio blocks
- stereo signed Q23 engine output
- audio render called from the device audio path
- 240×240 ST7789 display
- tight internal-SRAM placement requirements for audio callbacks
- build-time runtime heap floor of 49,152 bytes in the current SDK build checks

Therefore:

- no allocation, locking or filesystem access in render;
- fixed-size voice and delay state;
- precomputed parameter tables where useful;
- fixed-point or mixed fixed/floating implementation selected only after profiling the actual PI32v2 target;
- parameter smoothing must be deterministic and bounded;
- UI work must not steal audio deadlines.

One 128-frame block at 44.1 kHz spans about 2.90 ms. The target should keep worst-case synth render comfortably below that deadline and preserve headroom for the rest of the application.

## 6. UI direction

Do not miniaturise the entire JUNO front panel onto the 240×240 display.

Suggested page model:

- HOME — patch/model/performance and four most useful live controls
- DCO — range, saw, pulse/PWM, sub, noise
- FILTER — cutoff, resonance, ENV, LFO, key follow, HPF
- ENV/VCA — ADSR, gate/env, level
- MOD — LFO, delay, pitch mod, portamento
- CHORUS — authentic mode selector plus DEEP service controls
- PERF — poly/unison/hold/arp/performance settings
- DEEP — card spread, circuit model and advanced controls

The hardware encoders should always map to the four visible primary parameters. Button mappings should preserve fast playability before menu completeness.

## 7. State and presets

Patch state should be versioned and model-aware. Separate:

- authentic patch parameters
- global performance state
- DEEP extension state
- calibration/model version

Factory-patch values should not be copied into the repository unless licensing/source status is clear. The engine should support importing/documenting parameter sets independently of shipping copyrighted data.
