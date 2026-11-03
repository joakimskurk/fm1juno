# FM1JUNO FM-1 target / deployment — Phase 0 closure

State: **`FM1_TARGET_PHASE0_ARCHITECTURE_FROZEN`**.

This record closes the final project-wide Phase-0 architecture gate. Actual CPU ticks and final Q-format coefficients remain Phase-1 profiling results because the reference engine does not yet exist.

Current platform facts were re-verified on 2026-11-03 against the live M-VAVE FM1 Open and Dinghy sources.

## 1. Platform boundary

### First custom-firmware deployment baseline

Use a **Dinghy/Felucca-family custom-firmware adapter** for the first fully custom FM1JUNO hardware build.

Reasons:

- the application owns `app_audio()` directly;
- host mode exists for build/test without physical FM-1;
- the update/recovery/panel/MIDI/screen plumbing already exists;
- the target restrictions are documented and build-checked;
- the synthesis core can own every sample rather than trying to replace M-VAVE's binary engine.

Dinghy itself is a platform/template reference, not a fidelity/DSP source.

### Official FM1 Open remains authoritative hardware/API evidence

Current official V14 source confirms:

```text
FM1_SAMPLE_RATE  = 44100
FM1_BLOCK_FRAMES = 128
FM1_CHANNELS     = 2
```

The official application links the separately supplied binary SDK engine `sdk/lib/libfm1_engine.a`. Therefore FM1JUNO must not depend on patching that engine as its primary deployment route.

## 2. Core callback contract

The synthesis core is callback-size independent:

```text
fm1juno_render(core, output, frames)
```

Required adapter-equivalence tests:

```text
16 / 32 / 64 / 128 / 256 outer frames
```

For the same timestamped event stream and deterministic seeds, rendered audio must be sample-equivalent except for a specifically documented platform-input timestamp limitation.

Outer callback size is never the JUNO control rate.

## 3. Audio deadlines

At 44.1 kHz:

```text
128 frames = 2.902494 ms
256 frames = 5.804989 ms
```

Dinghy currently calls `app_audio()` with 256 stereo frames and documents 5.8 ms as the hard deadline, with **under half** the interval as the practical target.

Engineering bands for equivalent normalized workload:

```text
GREEN   <= 50% callback interval
YELLOW  > 50% and <= 70%
RED     > 70% sustained OR any overrun
```

At 24-MHz `fm1_ticks()`:

```text
128 hard deadline  ~= 69,660 ticks
128 green target   ~= 34,830 ticks
256 hard deadline ~= 139,320 ticks
256 green target   ~= 69,660 ticks
```

Actual render ticks are Phase-1 measurements, not Phase-0 guesses.

## 4. One-core correctness is mandatory

The AC791N has two cores and current community firmware demonstrates useful second-core workers, but real-unit reports show that some units can misread shared RAM or otherwise fail second-core jobs.

Therefore strict FM1JUNO policy is:

```text
TARGET_BASE must render:
  six physical voices
  full 106 control/timing semantics
  event-aware DCO source
  topology-preserving 80017A model
  HPF/global VCA
  stereo chorus
on one core without audio overruns.
```

Core 2 is an **optional quality/headroom accelerator**, never a correctness dependency.

Allowed dual-core upgrade:

```text
TARGET_FULL:
  CPU0 voices 0..2
  CPU1 voices 3..5
  join
  CPU0 global sum -> HPF -> global VCA -> chorus -> output
```

or another measured split with equivalent semantics.

If core 2 becomes unavailable or unreliable, fallback must retain all six voices and strict behavioral semantics by dropping only measured optional cost, for example selective oversampling or expensive refinement terms.

Do not drop voices or change the voice allocator as a core-2 fallback.

## 5. Target numeric rules

For the Dinghy deployment baseline:

- no runtime `float` / `double` dependency;
- no 64-bit division/modulo in the audio path;
- 64-bit multiplication is allowed where measured affordable;
- constants/LUTs are generated offline;
- original JUNO integer domains are retained directly where known;
- audio/filter/gain Q formats are chosen per block from REF-vs-target error tests, not from one universal Q format.

Platform output conversion:

```text
Dinghy adapter: Q15 full scale +/-32767
optional official-SDK adapter: platform-specific Q23 convention
```

Internal voltage/headroom relationships must not be normalized away merely to fit an output format.

## 6. Fixed architecture time domains

Keep separate:

```text
A. audio sample timeline             44.1 kHz
B. 8253 event timeline               analytical fractional-sample events
C. JUNO control scheduler            ordered ~4.2-ms family + restart semantics
D. outer platform callback           128 or 256 frames
E. UI/MIDI/platform main loop        asynchronous/deferred
```

No adapter may merge B/C/D for convenience.

An internal 16- or 32-frame chunk is permitted only as a batching optimization. The renderer must split a chunk at exact scheduled timer/control/note events where behavior requires it.

## 7. DCO target policy

Do not simulate MHz timer clocks tick by tick.

Per physical voice retain:

```text
current_count
pending_count
Mode3_half_state
next_edge_time_fixed
output_state
```

Use analytical Mode-3 half-cycle lengths and precomputed range-clock conversion constants.

MC5534 target:

- held fixed-point ramp slope;
- analytical ramp between events;
- finite reset event state;
- physical held-PWM comparator boundary;
- timing-domain SUB;
- event-aware antialias correction at actual discontinuities/crossings.

Do not replace the block with a generic free-running PolyBLEP oscillator.

## 8. 80017A target policy

The six-voice VCF/VCA block is the expected sustained cost center.

Optimization ladder:

1. topology-correct scalar 1x target candidate;
2. fit low-cost nonlinear functions/LUTs against desktop REF;
3. measure stage/resonance/VCA error separately;
4. test selective 2x only where it provides audible/measurable benefit;
5. use core 2 for `TARGET_FULL` only after a valid one-core `TARGET_BASE` exists;
6. never start from global 4x oversampling.

## 9. Chorus target memory

A 1024-sample power-of-two delay ring per channel is enough for the known MN3009 delay family with margin:

```text
2 x 1024 x int16 ~= 4 KiB
2 x 1024 x int32 ~= 8 KiB
```

Target chorus uses delay-line equivalence, fitted filtering, bias/headroom, deterministic device noise and exact dry/wet/output laws.

Do not clock a literal 256-bucket software BBD unless A/B validation proves a necessary audible effect.

## 10. Frozen memory engineering targets

Current Dinghy envelope:

```text
code + constants / XIP   568 KiB
.data + .bss               96 KiB
pool                      336 KiB
stack                      24 KiB
```

FM1JUNO Phase-1 targets:

```text
core + UI ordinary RAM       <= 64 KiB
persistent DSP voice state   <= 24 KiB preferred
block scratch in BSS         <= 12 KiB preferred
FM1JUNO pool use             <= 64 KiB preferred
code + constants             <= 450 KiB preferred
```

These are engineering margins, not claims about exact final consumption.

Rules:

- no large arrays on stack;
- use pool for large delay/LUT workspaces;
- no six full-rate per-voice block buffers in release path unless profiling proves necessary;
- worker-local accumulation is preferred.

## 11. Determinism

Host REF, target host mode and physical target must share deterministic test scripts.

Freeze:

- explicit seed for common synth NOISE;
- separate explicit seed/state for chorus device noise;
- deterministic voice allocation;
- deterministic control-scheduler initial phase/state;
- no wall-clock randomness in regression mode;
- timestamped event-vector replay.

## 12. Fidelity ladder

```text
REF
  high-precision desktop physical reference

TARGET_FULL
  highest measured topology-preserving FM-1 quality
  optional selective oversampling / optional core 2

TARGET_BASE
  mandatory one-core six-voice strict architecture
  1x where needed for cost, event-aware source, full timing/control and chorus graph

DIAGNOSTIC_SIMPLE
  debugging only; never a release MODEL_106 mode
```

Every approximation from REF to target needs a named error test.

## 13. Profiling contract

Target builds used for optimization expose at least:

```text
whole_render_ticks
peak_render_ticks
voice_group_ticks
post_chorus_ticks
worker_wait_ticks     // when core 2 is active
overrun_count
core2_error_count     // when core 2 is active
```

Stress vectors are the fixed scenes already defined in `FM1_TARGET_BUDGET.md`:

- six-voice bright chord;
- six-voice high resonance;
- six self-oscillating voices;
- maximum modulation/control activity;
- fast Solo-Unison phrase;
- silence/release/chorus tails.

## 14. Release gate

A physical FM-1 release requires:

- no audio overruns during long worst-case soak tests;
- preferably <=50% normalized callback interval for `TARGET_BASE`;
- memory/linker margins inside the frozen engineering targets or an explicitly reviewed exception;
- deterministic host-vs-target event-vector agreement;
- every target simplification compared against REF;
- core-2 failure/fallback test if `TARGET_FULL` uses core 2;
- no correctness dependency on core 2.

## 15. Phase-0 closure

Architecture gate:

- [x] callback-size-independent core;
- [x] official 44.1-kHz / 128-frame API reference retained;
- [x] Dinghy/Felucca custom-firmware adapter chosen as first custom-DSP deployment family;
- [x] 256-frame Dinghy hardware callback and timing deadline accounted for;
- [x] fixed-point / no-64-bit-division target restrictions frozen;
- [x] separate audio/timer/control/callback/UI time domains frozen;
- [x] event-aware 8253/MC5534 strategy frozen;
- [x] six-voice 80017A cost/quality ladder frozen;
- [x] one-core six-voice `TARGET_BASE` requirement frozen;
- [x] dual-core as optional `TARGET_FULL` accelerator with correctness-preserving fallback;
- [x] RAM/pool/stack/code engineering margins frozen;
- [x] deterministic benchmark/profiling/release policy frozen;
- [x] **FM-1 TARGET PHASE-0 ARCHITECTURE GATE CLOSED.**

Phase-1 measurements still required:

- actual single-core per-block ticks;
- actual memory/linker map;
- selected per-block Q formats;
- 1x-vs-2x error/cost for 80017A;
- whether `TARGET_FULL` needs/benefits from core 2;
- physical long-run overrun/core-2 reliability results.

Those measurements choose the target implementation tier. They do not reopen the synthesis architecture unless the hardware contradicts the verified platform contract.
