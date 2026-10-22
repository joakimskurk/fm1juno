# FM1JUNO — staged plan

This plan deliberately separates **reference research**, **audio-model validation** and **FM-1 optimisation**. The first hardware build should not be the first time the DSP can be inspected or measured.

## Phase 0 — freeze the reference map

Goal: establish what each JUNO-60/JUNO-106 block actually does before coding the instrument.

Deliverables:

1. annotated signal-flow map for both instruments;
2. component/topology table for DCO, VCF, VCA, ENV, HPF and chorus;
3. parameter ranges and control laws from service/owner documentation;
4. list of verified similarities and verified differences;
5. uncertainty register for claims that still rely on secondary analysis;
6. test-vector specification for every audio block.

Special attention:

- JUNO-60 DCO modulation/control path versus 106;
- analogue 60 envelope versus digitally generated 106 envelope;
- global HPF/output loading differences;
- 60/106 chorus differences, including I+II behaviour;
- actual Poly-1/Poly-2/unison allocation rules and retrigger behaviour;
- control scanning/update rates and parameter quantisation.

Exit criterion: no important sonic block is represented only by the phrase "Juno style".

## Phase 1 — reference engine on desktop/web

Goal: build an inspectable high-precision reference model before optimising for FM-1.

Implement in order:

1. voice allocator and note/event model;
2. DCO timer/charge-reset core;
3. saw/pulse/sub/noise mixer;
4. envelope generators;
5. four-pole VCF and resonance path;
6. voice VCA and voice summing;
7. model-specific HPF/output stage;
8. BBD-inspired stereo chorus;
9. LFO, portamento and performance modes;
10. presets/state.

Reference build should expose scope/test taps between blocks and be able to render deterministic WAV vectors offline.

Exit criterion: automated block tests plus convincing A/B behaviour against documented/measured references.

## Phase 2 — JUNO-60 fidelity pass

Goal: calibrate `MODEL_60` as an instrument, not merely as individual blocks.

Tests:

- DCO spectrum over keyboard/ranges;
- PWM duty and modulation law;
- sub/noise balance;
- filter cutoff/resonance/self-oscillation;
- ADSR timing and curvature;
- HPF positions at several VCA levels;
- voice-to-voice spread;
- chorus I/II/I+II delay/rate/spectrum/noise;
- arpeggiator, hold, transpose and retrigger behaviour.

Use real-hardware captures if available later; until then, keep service documentation and independent measurements separate in the evidence log.

## Phase 3 — JUNO-106 fidelity pass

Goal: calibrate `MODEL_106` independently where behaviour differs.

Tests:

- 8 MHz/range-divided timer model;
- period quantisation and per-cycle updates;
- shared-master-clock phase behaviour;
- digitally generated envelope/control cadence;
- 80017A-style VCF/VCA calibration;
- 106 HPF position-0/buffer behaviour;
- portamento and performance/poly modes;
- 106 chorus modes and noise/headroom.

Exit criterion: switching 60↔106 changes specific modeled circuits/behaviours, not just an EQ or macro parameter set.

## Phase 4 — FM-1 minimal engine port

Goal: six-voice authentic engine on the real FM-1 audio ABI with no DEEP extras.

Constraints:

- 44.1 kHz / 128 frames;
- deterministic audio callback;
- no allocation in render/event paths;
- fixed-size chorus buffers/state;
- CPU/SRAM measurement added before optimisation decisions.

First device build should include:

- six voices;
- one model profile initially (whichever reference profile is most mature);
- DCO + VCF + envelope + VCA;
- HPF;
- chorus I/II;
- MIDI note/CC handling;
- minimal diagnostic UI.

Benchmark worst cases: six-note chord, max resonance, active PWM/LFO, portamento, chorus, rapid MIDI, screen redraw and USB/Bluetooth activity.

Exit criterion: stable audio deadline with deliberate safety margin and no UI/MIDI stalls.

## Phase 5 — FM-1 fidelity/optimisation

Only now decide:

- oscillator oversampling policy;
- filter oversampling policy;
- interpolation quality in BBD delays;
- fixed-point versus mixed numeric representation;
- lookup-table sizes;
- which DSP routines should be forced into internal SRAM;
- whether self-oscillation/nonlinearity needs iteration or can use a cheaper stable approximation.

Quality should degrade by changing algorithmic quality modes only if measurements prove necessary. Do not remove characteristic behaviour simply to make an early build pass.

## Phase 6 — full FM-1 instrument UI and functionality

Implement page-oriented 240×240 UI and fast control mapping.

Priority order:

1. performance/live controls;
2. DCO/filter/envelope editing;
3. chorus and HPF;
4. patch storage;
5. arp/hold/portamento/poly modes;
6. MIDI CC mapping and external control;
7. diagnostics/about/model version.

Authentic pages stay simple. Advanced parameters live under DEEP.

## Phase 7 — DEEP engine

Add extensions one by one with CPU-cost measurements and regression tests against authentic profiles.

Candidates:

- physically meaningful AGE/card-spread system;
- block-level 60↔106 morphing;
- per-voice oscillator/filter/envelope calibration;
- detailed BBD service page;
- expanded modulation envelope/LFO;
- velocity/aftertouch routing;
- unison detune/phase options;
- extra polyphony if benchmarks support it.

Authentic 60/106 output must remain bit/revision-testable after every DEEP addition.

## Phase 8 — validation and release discipline

Keep golden vectors for:

- oscillator waveforms and pitch;
- PWM;
- envelopes;
- filter sweeps/resonance;
- HPF responses;
- chorus impulse/delay modulation;
- complete representative patches.

Every model-altering change increments a model revision. Presets store the revision that created them so later calibration changes can be migrated deliberately.

Release gates:

- simulator/reference tests pass;
- device build fits;
- audio deadline passes worst-case stress test;
- no dynamic allocation in real-time paths;
- preset round-trip passes;
- authentic modes still match golden vectors.
