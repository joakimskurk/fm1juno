# FM1JUNO Phase-1 implementation status

Updated: 2026-11-03
Active branch: `phase1/reference-engine`
Authority: `docs/PHASE1_START_CONTRACT.md` -> `docs/PHASE1_REFERENCE_ENGINE_PLAN.md` -> relevant Phase-0 block-specific models.

**State: `P1.0_TRACE_SCAFFOLD_RUNNING / P1.1_DIGITAL_IN_PROGRESS`.**
**Do not mark P1.1 as complete, and do not begin claiming analogue/audio 106 fidelity.**

## 1. Current checked-in code

Portable C core:

- `src/common/fixed_time.h`: absolute Q32.32 sample/subsample time;
- `src/common/event_queue.*`: fixed-capacity deterministic ordered event queue;
- `src/control106/assigner106.*`: Poly 1 / Poly 2 / Solo-Unison voice logic;
- `src/control106/envelope106.*`: 14-bit ADSR state and clean-room Attack/Decay/Release/Sustain coefficient laws;
- `src/control106/lfo106.*`: integer LFO and compact rate/depth laws;
- `src/control106/lfo_delay106.*`: two-stage LFO Delay and physical/MIDI performance paths;
- `src/control106/portamento106.*`: constant-step pitch glide and compact step law;
- `src/control106/control_math106.*`: Tune and DCO/VCF Bender arithmetic helpers;
- `src/control106/pitch106.*`: 8.8 pitch arithmetic, exact index/clamp/interpolation, derived DCO-CV curve and replaceable 8253 base-count candidate;
- `src/control106/vcf_control106.*`: firmware-domain VCF global base, per-voice ENV/Key Follow, final clamp and DAC quantization;
- `src/control106/dac106.*`: 14-bit-to-physical-12-bit boundary;
- `src/control106/control_scheduler106.*`: non-atomic ROM macro schedule plus voice-event restart;
- `src/control106/module_control106.*`: six-voice integration harness, event trace, timer/ENV, now ordered per-voice VCF holds;
- `src/timer106/timer8253_106.*`: 8253 Mode-3 event-time model.

Host/test entry points:

- CMake host build and GitHub Actions;
- `fm1juno_ref` reference scaffold;
- `fm1juno_render` CSV event-script/trace CLI;
- deterministic unit, integration, invariance, pitch and coefficient tests.

**Important limitation:** `fm1juno_render` currently prints trace/state results and explicitly says `audio_wav=not_available_until_reference_audio_blocks`. There is not yet a WAV-producing reference instrument, 80017A DSP or MC5534 audio renderer. A "render" executable name is not evidence of audio-fidelity completion.

## 2. New VCF control work

Files:

- `src/control106/vcf_control106.h`
- `src/control106/vcf_control106.c`
- `tests/unit/test_vcf_control.c`
- `tests/unit/test_module_control.c` (new scheduler-integration vectors)

Behavioral boundary:

1. `global_raw = cutoff14 +/- floor(lfoMag*depthAfterDelay/256)/2 +/- bend` in signed widened arithmetic;
2. global value is **not** prematurely clamped;
3. per physical voice, update ENV then add/subtract scaled ENV;
4. Key Follow uses `(pitch88>>2)+(pitch88>>3)` relative to `0x1680`, multiplied by `rawKeyFollow*2`;
5. clamp final per-voice value to `0..0x3fff`, then convert to 12-bit DAC code;
6. commit voices in ROM scheduler order, preserving a partially completed pass if a note event causes restart (remaining slots are abandoned).

This is a well-grounded macro-arithmetic model, **not** a claim that every obscure uPD7811 carry/borrow-flag corner has been compared against a cycle-accurate original-CPU execution.

Current `module_control106_set_voice_pitch()` is an explicit P1 harness boundary for the **already computed** current Portamento pitch; the complete note->Portamento->DCO/VCF producer is not yet wired into the module. Likewise, an arbitrary VCF parameter setter currently stands in for the eventual timestamped parameter-receive path.

## 3. Timer divisor candidate: evidence and gate

**Do not silently promote `pitch106_timer_build_derived_candidate()` to ROM-exact strict mode.**

The original 104-value 8253 divisor series is not exactly described by a single equal-tempered inverse exponential. Instead of copying its ROM words, the candidate independently generates 104 integer counts **once at initialization**, then exact firmware-style fractional interpolation is applied during control updates.

For index `i` with `x=2^(-i/12)`, the compact current candidate is:

- `i=0..71: round(61355*x + 210*x*x)`
- `i=72..83: round(61355*x)`
- `i=84..103: round(61325*x)`

The code uses integer Q24 for the reciprocal semitone recurrence. There is **no original 104-entry note table in production code**.

An offline comparison against the 104 public disassembly values found:

- maximum approximate base-count pitch deviation: **0.4782 cents** across the examined sequence;
- highest indices `72..103` matched the checked base counts exactly in that comparison;
- the low-register count deviation can be larger than one LSB without producing a large cent deviation;
- no claim of bit-identical timer counts is made;
- runtime lookup/interpolation is cheap; generating the profile uses 64-bit multiplication **only during initialization**, not inside the 44.1-kHz render callback.

Regression tests in `test_pitch_conversion.c` use sparse independent observation anchors, an integer relative-error bound, clamp and fractional-interpolation vectors. They deliberately assert that the experimental profile differs from at least one original anchor.

**Open strict gate:** obtain an independently derived exactly matching compact base law, or explicitly approve and document a bounded pitch-error fidelity tier distinct from the bit-exact control tier. Do not mix these two labels.

The DCO-CV curve is also a clean-room exponential generator: its available sparse hardware/ROM-derived anchor checks are within one LSB. It is **not** certified bit-identical across every possible index or fraction.

## 4. Current quality gates

Confirmed:

- [x] Host CMake project and GitHub Actions workflow.
- [x] Integer ENV, LFO, Portamento and DCO/VCF Bend primitives.
- [x] Clean-room compact parameter coefficient laws.
- [x] Ordered module control scheduler with Voice On/Off restart.
- [x] Physical 12-bit DAC quantization boundary.
- [x] 8253 Mode-3 baseline event model.
- [x] Deterministic 16/32/64/128/256-frame **control trace** invariance tests.
- [x] VCF global/per-voice arithmetic tests, signed unclamped base, Key Follow and ENV corner vectors.
- [x] VCF S/H integration test for interleaved per-voice updates and interrupt/restart.
- [x] Derived divisor-candidate profile and sparse error-bound regression vectors.
- [x] Latest host-tests CI on `1123c3fd7c73e5eb64537507d0d8f0b4195d49b5` completed successfully.

Still open before **P1.1 exact digital/control exit**:

- [ ] Full timestamped panel/MIDI parameter ingestion and control-parameter state feeding the scheduler.
- [x] Shared DCO service: an injected Portamento pitch and latched global pitch offset now feed both 8253 count and DCO-CV/S&H from **one** final pitch coordinate (candidate profile explicitly identified).
- [ ] Replace the current injected pitch/offset harness with complete timestamped Tune/LFO/Bend/Portamento producer state and raw-parameter ingestion.
- [ ] Certify a ROM-equivalent divisor base or explicitly label/configure the accepted non-exact tier.
- [ ] Complete VCF global/voice dataflow tests against executable CPU arithmetic for rare carry/borrow cases.
- [ ] Cover all documented 8253 pending/initial/reset/range/odd-count transition edge cases with independent timestamp vectors.
- [ ] Make all necessary LFO and modulation service events produce persistent held-control updates rather than trace-only macro calls.
- [ ] Resolve/explicitly defer exact microsecond slot offsets to calibrated scheduler data. Do not fabricate them.

Before the broader **Phase 1A/1D audio exit**:

- [ ] WAV writer and real audio reference engine; current render CLI is trace-only.
- [ ] Generate/read central calibration manifest and print all active `MEASURE_PENDING` values.
- [ ] Event queue integration into final sample-accurate audio dispatcher and end-to-end callback-invariance renders.
- [ ] High-precision MC5534 reference, then shared noise/mixer, 80017A, global output and chorus according to plan.

## 5. Shared DCO pitch/CV integration now implemented

Files: `module_control106.h/.c` and `tests/unit/test_module_control.c`.

- The host harness injects a complete Portamento 8.8 voice pitch and a global Tune/LFO/Bender 8.8 offset.
- The global offset becomes *active* on the ROM `PITCH_AND_PORTAMENTO` scheduler slot.
- Each subsequent physical `DCO_VOICE` service computes one final pitch coordinate; **that same value** drives profile-derived timer count and DCO-CV.
- Timer count reset/queue trace precedes that voice's physical DCO-CV S/H trace.
- An ordinary pitch change queues a new count without discarding the current Mode-3 half-cycle, while its DCO-CV hold is updated at this voice's scheduled service point.
- The injected base-count profile is explicitly replaceable, and the current generated candidate is not strict count-bit-identical.
- Default/manual `DCO_COMMAND` behavior remains available for deterministic legacy tests.

The new integration regression verifies one-frame global-offset causality, initial reset, later ordinary count queueing, shared pitch and the timer-write-before-CV ordering.

**This does not mean the full original 106 pitch pipeline is done.** Real raw parameter reception, LFO/ENV servicing, per-voice Portamento state updates, and full 8253 clock-edge coordination still need implementation and independent vectors.

## 6. Next implementation slice

Complete a **real scheduled control-parameter producer** rather than growing more direct setters:

1. raw 7-bit parameter changes enter a timestamped command queue;
2. derive Tune, LFO depth/onset and signed Bender in their original integer domains;
3. advance Portamento per physical voice in the earlier ROM control region;
4. provide the new global offset and per-voice pitch to the already integrated DCO timer/CV and VCF paths;
5. prove control traces remain identical for callback sizes 16/32/64/128/256 and for interrupted frame/restart vectors.

Keep a distinct profile-quality flag for `ROM_EXACT` versus `DERIVED_APPROXIMATE` divisors. Do not promote the latter silently.

Do not transition to MC5534 audio fidelity until the remaining exact P1.1 digital gates have an explicit passing test or reviewed exception.
