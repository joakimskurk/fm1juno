# FM1JUNO — technical reference register

This file tracks evidence used to design the engine. Primary manufacturer documentation wins over secondary analysis. Secondary measurements are useful for calibration, but uncertainties must stay visible until independently verified.

## Roland JUNO-106

### Primary/service material

- Roland, **JUNO-106 Service Notes**, first edition, 31 July 1984.
  - CPU/module-board block diagram.
  - 8 MHz oscillator and range divider.
  - 8253 programmable interval timers.
  - MC5534/MC5534A wave generator.
  - D/A, demultiplexer and sample/hold control voltages.
  - A1QH80017A VCF/VCA modules.
  - calibration/service procedures and parameter ranges.
- Roland JUNO-106 owner/service specifications.

### Secondary circuit analysis

- Dave Cornutt / Sequence 15, **Analyzing the Juno-106 DCO circuit**.
  - useful interpretation of timer period behaviour, shared clock, per-cycle updates and range-dependent timer clock.
- Dave Cornutt / Sequence 15, **Why does the Juno-60 sound different from the Juno-106?**
  - comparison of VCF/VCA topology and post-sum HPF/output loading.

Secondary claims must be checked against schematics and measurements before being frozen into model constants.

## Roland JUNO-60

### Primary/service material

- Roland, **JUNO-60 Service Notes**, first edition, 10 April 1983.
  - μPD8253 programmable interval timer DCO architecture.
  - IR3109 VCF.
  - BA662 VCA/resonance control.
  - IR3R01 envelope generator.
  - MN3009 BBD / MN3101 driver chorus.
  - arpeggiator, envelope/LFO ranges and calibration material.
- Roland JUNO-60 technical specifications / owner documentation.

### Independent measurement archive

- Andy H / `pendragon-andyh/Juno60` (MIT), circuit/audio analysis archive.
  - DCO/filter/envelope/chorus research and captured WAV material.
  - Chorus measurement set currently used as the initial calibration reference:
    - I ≈ 0.513 Hz, delay ≈ 1.66–5.35 ms.
    - II ≈ 0.863 Hz, delay ≈ 1.66–5.35 ms.
    - I+II ≈ 9.75 Hz, delay ≈ 3.3–3.7 ms.
  - The project notes that service documentation appears to contain a likely decimal/label issue for the fast I+II rate; retain this as an uncertainty until independently validated.

## Shared/Juno-family circuit observations to verify

- The 60 IR3109 + BA662 filter/VCA architecture and the 106 80017A module are closely related and should support one shared topology with model-specific calibration/behaviour.
- Both machines are six-voice DCO polysynths with saw/pulse/sub/noise feeding a resonant four-pole low-pass architecture.
- The global HPF/output path is not identical and should not be reduced to a single shared EQ curve.
- The 60 envelope architecture is analogue/IR3R01-based; the 106 control/envelope generation is CPU/DAC/sample-and-hold based.
- DCO timing/waveshaping should be modeled as a timing-controlled analogue-inspired ramp/waveshaper rather than a generic wavetable oscillator.

## M-VAVE FM-1 platform

- M-VAVE `fm1_open` repository and SDK.
- Current open engine ABI/reference source indicates:
  - `FM1_SAMPLE_RATE = 44100`.
  - `FM1_BLOCK_FRAMES = 128`.
  - stereo engine render path.
  - signed Q23 output from `fm1_engine_render()`.
  - 240×240 ST7789 display in the open application code.
  - audio callbacks must satisfy strict internal-memory/runtime constraints in the current SDK build checks.
  - current build checks require at least 49,152 bytes runtime heap and package sizing is explicitly checked.

The FM-1 repository is a moving upstream dependency. Pin an upstream commit when implementation begins; do not design against an unversioned `main` branch.

## Validation policy

For each major block, record:

1. source schematic/service reference;
2. inferred mathematical model;
3. uncertain assumptions;
4. measurement/reference audio used;
5. test vectors;
6. final implementation constants and model revision.

If a secondary source conflicts with Roland documentation or real measurements, keep both in the log and resolve experimentally rather than averaging them.
