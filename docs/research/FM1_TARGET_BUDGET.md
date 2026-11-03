# FM1JUNO — FM-1 target contract and DSP budget

Status: **Phase-0 deployment architecture frozen**. Detailed implementation/profiling guide for Phase 1. The authoritative closure record is `FM1_TARGET_PHASE0_CLOSURE.md`; actual CPU ticks, linker map and final per-block Q formats remain Phase-1 profiling results.

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

### Yellow

```text
>50% and <=70%
```

### Red

```text
>70% sustained, or any callback overrun
```

Track both average and worst observed render time.

## 5. Internal render quantum

Outer callback size is not the control rate.

Recommended target implementation processes an outer block in internal chunks of:

```text
16 or 32 audio frames
```

but only for work that benefits from coefficient batching.

Never force JUNO-106 firmware events onto a 16/32-frame grid. The strict scheduler owns an absolute audio timeline and may trigger events inside a chunk.

Chunking is an optimization boundary, not a behavioral boundary.

## 6. Time domains on target

Keep separate:

```text
A. audio sample timeline           44.1 kHz
B. 8253 event timeline             analytical fractional-sample events
C. 106 control-service timeline    ordered ~4.2-ms family + restart semantics
D. outer firmware callback         128 or 256 frames depending adapter
E. UI/MIDI/control task            asynchronous/deferred
```

Do not merge B/C/D merely because FM-1 receives audio in blocks.

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

The next half-cycle duration is known analytically from integer count semantics.

Target requirement:

- no 64-bit division in the audio path;
- fixed-frequency conversion constants generated offline;
- ordinary pending count activates at the correct Mode-3 boundary;
- reset-and-load remains distinct.

## 8. MC5534 target strategy

Between reset events the ramp is approximately linear under held DCO CV.

Target representation:

- held fixed-point ramp slope;
- analytical ramp advance;
- event-aware finite reset state;
- pulse comparator against held PWM threshold;
- timing-domain SUB state.

Anti-aliasing preference:

1. event-aware compact correction at saw reset;
2. event-aware correction at pulse threshold crossings and SUB transitions;
3. selective local oversampling only if the physical model needs it;
4. no global source oversampling by default.

## 9. Six-voice 80017A cost is the critical path

Expected highest sustained DSP cost:

```text
6 x [four nonlinear OTA filter stages
     + resonance/Q compensation
     + voice BA662 VCA]
```

Optimization order:

1. topology-correct scalar reference at 1x;
2. identify materially audible nonlinearities;
3. fit cheap fixed-point nonlinear functions/LUTs;
4. test selective 2x filter oversampling;
5. compare against high-rate REF;
6. only then simplify further.

Do not start with global 4x oversampling.

## 10. Single-core requirement and optional dual-core acceleration

**TARGET_BASE must sound and behave correctly on one core.**

The AC791N has a second core and current firmwares demonstrate useful worker patterns, but real units have reported second-core shared-RAM/job reliability problems. Therefore core 2 is never a correctness dependency.

Required one-core baseline:

```text
six physical voices
full allocator/control/timing semantics
event-aware DCO source
topology-preserving 80017A
HPF/global VCA
stereo chorus
```

Optional `TARGET_FULL` split after one-core profiling:

```text
CPU0: voices 0..2
CPU1: voices 3..5
join
CPU0: voice sum -> HPF -> global VCA -> chorus -> output
```

If core 2 is retired/fails, fallback retains all six voices and strict behavior. Only measured optional quality cost such as selective oversampling may be reduced.

## 11. Chorus memory and cost

A 1024-sample ring per channel provides comfortable delay margin:

```text
2 x 1024 x int16 ~= 4 KiB
2 x 1024 x int32 ~= 8 KiB
```

Expected target cost:

- two fractional-delay reads;
- fitted pre/post IIR sections;
- clock-derived bandwidth;
- cheap BBD bias/headroom nonlinearity;
- deterministic BBD/device noise;
- exact dry/wet analogue-sum law.

Do not literally shift 256 buckets at BBD clock rate unless A/B testing proves audible benefit.

## 12. Dinghy memory envelope

Current Dinghy documents:

```text
Code + constants / XIP: 568 KiB
ordinary .data + .bss:   96 KiB
pool:                   336 KiB
stack:                   24 KiB
```

FM1JUNO engineering targets:

```text
core + UI ordinary RAM:      <= 64 KiB total
DSP persistent voice state:  <= 24 KiB preferred
block scratch in BSS:        <= 12 KiB preferred
large delay/LUT workspaces:  use pool
FM1JUNO pool use:            <= 64 KiB preferred
code + const:                <= 450 KiB preferred
```

No large arrays on the 24-KiB stack.

## 13. Scratch-buffer plan

For 256-frame worst adapter size:

```text
CPU0 mono mix:       256 x int32 = 1 KiB
CPU1 mono mix:       256 x int32 = 1 KiB
common noise block:  256 x int32 = 1 KiB
optional temp:                         1-4 KiB
chorus rings:                         4-8 KiB
```

There is no need to keep six full audio buffers in the release path.

## 14. Numeric policy

### Desktop reference

Use high precision where useful:

```text
double / high-rate reference render
```

### Dinghy target baseline

- no runtime float/double dependency;
- no 64-bit division;
- 64-bit multiplication permitted when profiled;
- constants generated offline;
- no universal Q format chosen in advance.

Likely domains:

```text
pitch/control integers: original 106 integer domains
audio signal:            high-headroom signed fixed point
filter state:            Q-format selected by error tests
coefficients/gains:      Q-format per block
Dinghy output:           Q15
optional SDK adapter:    Q23
```

## 15. Voltage-domain reference vs target

REF retains meaningful voltage-like units through the physical signal chain. Target uses fixed point with an explicit documented scale at each block boundary.

Do not normalize away source/VCF/global/chorus headroom relationships.

## 16. Determinism

- explicit common-NOISE PRNG seed;
- separate chorus-noise seed/state;
- deterministic voice allocation;
- defined control-scheduler initial phase/state;
- no wall-clock randomness;
- same event script renderable by host and target.

## 17. Profiling instrumentation

Expose at least:

```text
whole render ticks
peak render ticks
voice-group ticks
post/chorus ticks
worker wait ticks     // if core 2 active
overrun count
core2 error count     // if core 2 active
```

## 18. Quality ladder

### REF

High-precision desktop truth candidate.

### TARGET_FULL

Highest measured topology-preserving FM-1 candidate. Optional selective oversampling and optional core 2.

### TARGET_BASE

Mandatory one-core six-voice strict architecture.

### DIAGNOSTIC_SIMPLE

Debug only. Never silently becomes `MODEL_106`.

## 19. Phase-1 benchmark scenes

A. six-voice bright chord: SAW + PULSE + SUB, VCF open, low RES, chorus II.

B. six-voice high resonance: SAW, mid/high cutoff, near self-oscillation, ENV/LFO, chorus I.

C. six self-oscillating voices.

D. maximum control activity: Portamento + DCO/VCF/PWM LFO + Bender + six voices.

E. fast Solo-Unison phrase.

F. silence/release/chorus tails and subsequent note-on continuity.

## 20. Performance release gate

Before a physical FM-1 build is production-ready:

- no hardware audio overruns in long stress tests;
- TARGET_BASE preferably <=50% callback interval worst case;
- memory/linker reports retain margin;
- output is deterministic under host-vs-target event vectors;
- all target approximations have error comparisons against REF;
- core-2 builds test failure/fallback and retain six-voice correctness.

## 21. Phase-1 implementation order

1. platform-independent exact digital/control state and test vectors;
2. ordered ControlScheduler106 / physical 12-bit DAC-S&H boundary;
3. Timer106 / event timeline;
4. high-precision MC5534 source reference;
5. common NOISE/source mixer;
6. high-precision 80017A candidate;
7. global HPF/VCA;
8. 106 chorus reference;
9. integrated desktop renders;
10. target fixed-point reductions block by block;
11. Dinghy adapter + hardware profiling;
12. optional core-2 TARGET_FULL split after one-core success.

Phase-0 architecture for this block is closed. Phase-1 profiling chooses the implementation tier; it does not redefine the instrument graph.
