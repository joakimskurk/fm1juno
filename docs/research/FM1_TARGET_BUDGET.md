# FM1JUNO — FM-1 target contract and DSP budget

Status: Phase 0 / Phase 1 boundary document.

Purpose: define how a high-fidelity JUNO-106 reference model is reduced onto the M-VAVE FM-1 without letting a particular firmware wrapper dictate the synthesis architecture.

## 0. Primary decision

**The DSP core must not depend on the host callback block size.**

Two current 2026 FM-1 software stacks expose different native callback quanta:

### M-VAVE official V14 open SDK

Authoritative SDK header:

```text
FM1_SAMPLE_RATE   = 44100
FM1_BLOCK_FRAMES  = 128
FM1_CHANNELS      = 2
```

`fm1_engine_render()` is called once per I2S/DMA block and must fill exactly 128 interleaved stereo signed-Q23 frames.

### Dinghy / Felucca custom-firmware platform

Current Dinghy platform:

```text
sample rate       = 44100
app_audio frames  = 256
callback interval = ~5.8 ms
sample format     = interleaved stereo Q15-style +/-32767
```

Dinghy is explicitly designed as a complete custom-firmware base and provides host mode, updater/recovery, panel, screen, MIDI and USB audio.

### Consequence

FM1JUNO core API must accept arbitrary practical frame counts and render identically regardless of outer block size:

```text
fm1juno_render(core, output, frames)
```

Required adapter tests:

```text
16 / 32 / 64 / 128 / 256 frame outer blocks
```

With identical timestamped events and deterministic noise seed, all adapters must produce the same sample stream apart from explicitly documented event-boundary quantization.

## 1. Platform choice

### 1.1 Deployment baseline: Dinghy/Felucca

Current recommendation for the first standalone FM1JUNO firmware is **Dinghy/Felucca** rather than trying to insert a new native synth into M-VAVE V14.

Reason:

- Dinghy is intended for arbitrary custom DSP/firmware;
- its source exposes the whole app audio callback;
- it already supplies host mode and recovery/install plumbing;
- several current FM-1 synth projects validate the path;
- we can own every sample of the JUNO engine.

### 1.2 Official M-VAVE V14 remains an authoritative hardware/API reference

M-VAVE's open V14 repository is extremely useful for current device contracts:

- 44.1 kHz / 128-frame native audio format;
- 240x240 panel API;
- Q23 official instrument output convention;
- current r3/pi32v2 toolchain information;
- safe audio/control task separation;
- timing/overrun diagnostics.

However the native device instrument engine is distributed as:

```text
sdk/lib/libfm1_engine.a
```

and the official build links that frozen binary engine. The web simulator includes engine source, but this does not make the native device engine an open arbitrary-synth plug-in target.

Therefore do not architect FM1JUNO around replacing or patching `libfm1_engine.a`.

### 1.3 Portability seam

Repository structure should eventually provide:

```text
core/                  platform-independent state + strict algorithms
reference/             high-precision physical reference DSP
platform/host/         desktop test/render adapter
platform/fm1_dinghy/   first hardware deployment adapter
platform/fm1_sdk/      optional future official-SDK adapter if a suitable custom-engine hook exists
```

The core must know neither Dinghy globals nor M-VAVE SDK objects.

## 2. Licensing boundary

Dinghy/Felucca is GPL-3.0-only. A firmware distribution based on it must comply with that license and provide corresponding source.

M-VAVE V14 app code is GPLv3 with an SDK linking exception; its device engine includes separately licensed binary components.

FM1JUNO policy:

- do not copy GPL reference DSP from KR106/FM-106 into a differently licensed core accidentally;
- use those projects for platform/performance observations unless FM1JUNO intentionally adopts GPL-compatible source;
- if Dinghy is the released firmware base, treating the firmware project as GPL-3.0-compatible is the cleanest path;
- keep research equations, independently derived algorithms and original FM1JUNO implementation provenance documented.

## 3. Hard real-time timing budget

At 44.1 kHz:

```text
128 frames = 2.902494 ms
256 frames = 5.804989 ms
```

Dinghy's current guidance is to keep the audio ISR under roughly half the full interval to leave room for USB/key-scan interrupt pressure.

Normalize all performance results to equivalent time per 128 samples:

```text
hard physical deadline:        2.902 ms / 128
engineering target <= 50%:     1.451 ms / 128
Dinghy equivalent target:      2.902 ms / 256
```

24 MHz TIMER4 measurement equivalents:

```text
128 hard deadline:  ~69,660 ticks
128 target:         ~34,830 ticks
256 hard deadline: ~139,320 ticks
256 target:         ~69,660 ticks
```

Do not use an assumed CPU clock to estimate cost. Measure wall-time with the FM-1 timer on hardware.

## 4. Performance acceptance bands

For the complete instrument at maximum normal load (six active voices, high resonance, modulation active, chorus on):

### Green

```text
<= 50% of callback interval
```

Enough safety margin for platform interrupts and occasional control work.

### Yellow

```text
>50% and <=70%
```

May be acceptable during development but requires targeted optimization before release.

### Red

```text
>70% sustained, or any callback overrun
```

Do not release. Reduce cost or move independent voice work to the second core before reducing fidelity blindly.

Track both average and worst observed render time. A low average does not excuse periodic overruns.

## 5. Internal render quantum

Outer callback size is not the control rate.

Recommended target implementation processes an outer block in internal chunks of:

```text
16 or 32 audio frames
```

but only for work that benefits from coefficient batching.

Never force JUNO-106 firmware events onto a 16/32-frame grid.

The strict scheduler owns an absolute audio timeline and may trigger events inside a chunk:

- 106 control-service events;
- 8253 output transitions;
- pending Mode-3 count activation;
- DCO reset;
- PWM threshold crossing;
- SUB transition;
- MIDI/note events when timestamp information is available.

Chunking is an optimization boundary, not a behavioral boundary.

## 6. Time domains on target

Keep the same conceptual domains as the desktop reference model:

```text
A. audio sample timeline           44.1 kHz
B. 8253 event timeline             analytical fractional-sample events
C. 106 control-service timeline    ~236-238 Hz, exact fitted later
D. outer firmware callback         128 or 256 frames depending adapter
E. UI/MIDI/control task            asynchronous/deferred
```

Do not merge B/C/D merely because the FM-1 receives audio in blocks.

## 7. 8253 target strategy

Do **not** simulate 1/2/4 MHz timer input clocks one tick at a time.

For each physical voice, maintain:

```text
current_count
pending_count
Mode-3 high/low half-cycle state
next_edge_time_fixed
output_state
```

The next half-cycle duration is known analytically from integer count semantics:

```text
even N: N/2, N/2 timer clocks
odd  N: (N+1)/2, (N-1)/2 timer clocks
```

Convert timer-clock duration to fractional audio-sample time using precomputed fixed-point constants for each RANGE clock.

Target requirement:

- no 64-bit division in the audio path;
- fixed-frequency conversion constants are generated offline;
- ordinary pending count becomes active at the proper Mode-3 boundary;
- reset-and-load remains a distinct operation.

## 8. MC5534 target strategy

Most of an authentic DCO cycle is cheap.

Between reset events the ramp is approximately linear under held DCO CV:

```text
dV/dt = -Vdco / (Rrange*Cint)
```

Target representation:

- held fixed-point ramp slope;
- analytical ramp advance;
- event-aware finite reset state;
- pulse comparator against held PWM threshold;
- timing-domain SUB state.

Anti-aliasing preference:

1. event-aware minBLEP/compact correction at saw reset;
2. event-aware correction at pulse threshold crossings and SUB transitions;
3. selective local oversampling only if the physical reset/comparator model still needs it;
4. no global 4x source oversampling by default.

A generic free-running PolyBLEP oscillator is not the source model.

## 9. Six-voice 80017A cost is the critical path

Likely highest sustained DSP cost:

```text
6 x [four nonlinear OTA filter stages
     + resonance/Q compensation
     + voice BA662 VCA]
```

This block gets the largest profiling effort.

Optimization order:

1. topology-correct scalar reference at 1x;
2. identify which nonlinearities materially affect output;
3. fit cheap fixed-point nonlinear functions/LUTs;
4. test selective 2x oversampling around filter core only;
5. compare 1x event/source + 2x VCF against high-rate desktop reference;
6. only then consider coarser approximations.

Do not start with 4x oversampling on all six voices.

## 10. Dual-core strategy

Use one core first for correctness/profiling. Add dual-core when the expensive voice block justifies it.

Preferred split, already proven structurally practical by current FM-106 work:

```text
CPU0: voices 0..2
CPU1: voices 3..5
join
CPU0: voice sum -> HPF -> global VCA -> chorus -> output
```

### Shared immutable block inputs

Before dispatch:

- apply pending note/control events;
- create one immutable parameter snapshot;
- generate the shared physical NOISE block once;
- update global control scheduler state needed by all voices.

Each worker owns only its three physical voice states and local mix buffer.

### Race rule

No UI/MIDI code edits a live voice structure while the worker owns it.

Control events are queued and applied at a defined audio synchronization point.

### Fallback

The core render path must remain valid single-core. Dual-core scheduling is a platform optimization layer, not embedded inside oscillator/filter semantics.

## 11. Chorus memory and cost

MN3009 device range gives a maximum nominal delay around 12.8 ms at its lowest normal clock.

At 44.1 kHz:

```text
12.8 ms ~= 565 samples
```

A power-of-two ring of 1024 samples per channel provides substantial headroom:

```text
2 x 1024 x int16  ~= 4 KiB
2 x 1024 x int32  ~= 8 KiB
```

Therefore memory is not the chorus constraint.

Expected target chorus cost:

- two fractional-delay reads;
- low-order fitted pre/post IIR sections;
- clock-derived bandwidth term;
- cheap BBD bias/headroom nonlinearity;
- deterministic BBD/device noise;
- dry/wet analogue-sum law.

Do not literally shift 256 buckets at BBD clock rate on FM-1 unless A/B testing proves audible benefit.

## 12. Dinghy memory envelope

Current Dinghy documents:

```text
Code + constants / XIP: 568 KiB
ordinary .data + .bss:   96 KiB
pool:                   336 KiB
stack:                   24 KiB
```

Build tooling fails if these regions overflow.

### FM1JUNO engineering targets

Initial targets, to be revised after first build map:

```text
core + UI ordinary RAM:      <= 64 KiB total
DSP persistent voice state:  <= 24 KiB preferred
block scratch in BSS:        <= 12 KiB preferred
large delay/LUT workspaces:  use pool
FM1JUNO pool use:            <= 64 KiB preferred
code + const:                <= 450 KiB preferred
```

These are margin targets, not hardware limits.

No large arrays on the 24 KiB stack.

## 13. Scratch-buffer plan

For 256-frame worst adapter size:

```text
CPU0 mono mix:       256 x int32 = 1 KiB
CPU1 mono mix:       256 x int32 = 1 KiB
common noise block:  256 x int32 = 1 KiB (can be smaller/fixed-point)
optional temp:                         1-4 KiB
chorus rings:                         4-8 KiB
```

There is no need to keep six full audio buffers. Each voice should accumulate into its worker-local mix after any required diagnostic taps are disabled.

Desktop reference builds may expose per-block taps freely.

## 14. Numeric policy

### Desktop reference

Use high precision where it helps:

```text
double / high-rate reference render
```

The reference exists to establish behavior, not to imitate target restrictions.

### Target

Current Dinghy constraints:

- no float/double runtime;
- no 64-bit division;
- 64-bit multiplication is usable;
- fixed-point/LUT/table constants should be generated offline.

Do **not** select one universal Q format now.

Likely domains:

```text
pitch/control integers: original 106 integer domains where known
audio signal:            high-headroom signed fixed point
filter state:            Q-format selected from error tests
coefficients/gains:      Q-format per block
external Dinghy output:  Q15
optional SDK adapter:    Q23
```

Converters live only at platform/output boundaries.

## 15. Voltage-domain reference vs fixed-point target

Desktop physical reference should retain meaningful voltage-like internal units through:

```text
MC5534 -> source mixer -> 80017A -> voice VCA -> global sum -> HPF/VCA -> chorus
```

The target implementation will use normalized fixed point, but every block receives documented calibration scale:

```text
reference volts <-> target fixed unit
```

This prevents accidental per-block normalization from erasing headroom/drive behavior.

## 16. Determinism requirements

For reproducible A/B tests:

- common synth NOISE uses explicit PRNG seed;
- chorus device noise uses separate seed/state;
- voice allocation is deterministic;
- control scheduler begins from a defined phase/state;
- no wall-clock randomness;
- host and FM-1 target can render the same event script.

## 17. Profiling instrumentation

Every target build used for optimization should expose counters for at least:

```text
whole render ticks
peak render ticks
voice-group ticks CPU0
voice-group ticks CPU1
post/chorus ticks
worker wait ticks
overrun count
```

Optional development-only per-block counters:

```text
DCO/source
VCF/VCA
HPF/global VCA
chorus
```

Disable fine instrumentation in release once costs are known.

## 18. Quality ladder

Do not optimize by random simplification. Maintain named fidelity tiers during profiling:

### REF

Desktop high-precision/high-rate truth candidate.

### TARGET_FULL

Best topology-preserving FM-1 candidate, selective oversampling allowed.

### TARGET_BASE

1x VCF, event-aware source, full 106 control semantics, full chorus transfer model.

### DIAGNOSTIC_SIMPLE

Simplified oscillator/filter/chorus blocks used only to localize CPU or sonic error.

The release must be a measured choice between FULL/BASE. DIAGNOSTIC_SIMPLE must never silently become `MODEL_106`.

## 19. Phase-1 benchmark scenes

Every meaningful target optimization should render the same stress scenes:

### A. Six-voice bright chord

```text
SAW + PULSE + SUB
VCF open
RES low
chorus II
```

Source/aliasing + chorus load.

### B. Six-voice high resonance

```text
SAW
cutoff mid/high
RES near self oscillation
ENV/LFO active
chorus I
```

VCF worst normal cost.

### C. Self-oscillation

Six voices at calibrated resonance/self-osc condition.

Filter stability/nonlinearity stress.

### D. Maximum control activity

```text
portamento
DCO LFO
VCF LFO
PWM LFO
bender movement
six active voices
```

Control scheduler/event stress.

### E. Solo-Unison fast phrase

Six physical voices receive sequential new-note messages with repeated legato/release transitions.

DCO reset/count-boundary stress.

### F. Silence / tails

Long release + chorus, then true idle.

Verify silent-voice fast path and no state discontinuity at next note-on.

## 20. Performance release gate

Before calling the FM-1 build production-ready:

- no hardware audio overruns in a long stress test;
- complete render stays preferably <=50% callback interval at six-voice worst case;
- CPU1 worker wait does not dominate CPU0;
- no quality mode changes dynamically due to load unless explicitly designed and inaudible;
- memory/linker reports retain margin;
- output is deterministic under host-vs-hardware event-vector tests;
- all target approximations have an error comparison against REF.

## 21. Immediate Phase-1 implication

We can now start the reference engine without committing to target compromises.

First implementation order should be:

1. platform-independent exact digital/control state and test vectors;
2. Timer106 / event timeline;
3. high-precision MC5534 source reference;
4. high-precision 80017A candidate;
5. source mixer/common noise;
6. global HPF/VCA;
7. 106 chorus clock + wet-path reference;
8. integrated desktop renders;
9. target fixed-point reductions block by block;
10. Dinghy adapter + hardware profiling;
11. dual-core split only after single-core block profile identifies the need.

This keeps fidelity decisions evidence-driven and makes the FM-1 port a reduction of a known reference rather than the reference itself.
