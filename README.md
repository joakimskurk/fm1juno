# FM1JUNO

Circuit-informed Juno-family virtual-analog instrument for the M-VAVE FM-1.

## Status

**Planning and research only. No synth implementation has started yet.**

The project goal is not a generic "Juno-ish" subtractive synth. The target is a compact engine whose signal flow and control behaviour are derived from the Roland JUNO-60 and JUNO-106 circuits, with three operating profiles:

- **60** — JUNO-60-oriented DCO/control behaviour, analogue-style per-voice envelope behaviour, 60 HPF/output voicing and chorus.
- **106** — JUNO-106 timer/DCO behaviour, digitally generated control/envelope behaviour, 106 HPF/output voicing and chorus.
- **DEEP** — the same circuit blocks exposed and extended rather than replaced: voice-card spread, calibration/age, expanded modulation, detailed BBD controls, unison/voice options and 60↔106 block morphing where technically meaningful.

Authentic mode starts at **6 voices**. Higher polyphony is a DEEP target only after benchmarking on the FM-1.

## Proposed signal path

`MIDI / keyboard → note allocator → DCO timing → saw / pulse / sub + noise → voice mixer → 4-pole VCF + resonance path → voice VCA → six-voice sum → global HPF / output VCA → dual-BBD chorus → stereo out`

The implementation will preserve the important architectural distinction between the two machines instead of treating them as different EQ curves on the same generic synth.

## FM-1 target

The current FM-1 open platform renders stereo audio at **44.1 kHz in 128-frame blocks**. Device work must therefore be deterministic, allocation-free in the audio path and explicitly budgeted for CPU/SRAM before adding oversampling or extra voices.

The 240×240 display and physical controls will use a page-oriented JUNO panel rather than attempting to squeeze the original front panel onto one screen.

## Documents

- `docs/ARCHITECTURE.md` — proposed audio/control architecture.
- `docs/PLAN.md` — staged research, prototype, validation and FM-1 integration plan.
- `docs/REFERENCES.md` — primary and secondary technical sources.

## Ground rule

Fidelity comes before feature count. DEEP features may extend the instrument, but authentic 60/106 profiles must remain reproducible and testable.
