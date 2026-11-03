# Source mixer / SUB / common NOISE — Phase 0 closure

State: **`SOURCE_MIXER_PHASE0_ARCHITECTURE_FROZEN`**.

This closure record supersedes older checklist wording where it implies that the source-path architecture itself is still unresolved. Remaining analogue ratios, control curves, spectra and large-signal limits are calibration backlog.

## Architecture gate

- [x] Three dual MC5534-family packages mapped to six physical voices.
- [x] Six A1QH80017A voice hybrids mapped to the same physical voices.
- [x] SAW and PULSE/PWM share one MC5534 ramp/timing system per voice.
- [x] WAVE retained as the physical post-generator source boundary.
- [x] SAW gate and PULSE-disable semantics kept distinct without deleting oscillator timing state.
- [x] PWM integer control law and physical held-CV comparator boundary frozen.
- [x] SUB retained as a separate divide-by-two physical source per voice.
- [x] One shared SUB LEVEL target with six independent physical SUB phases frozen.
- [x] One selected-transistor common NOISE source frozen.
- [x] IC14 BA662 common noise VCA frozen as its own analogue block.
- [x] Post-noise-VCA RC shaping retained; approximately 5 kHz healthy-unit bandwidth kept as a fitting target rather than a generic final coefficient.
- [x] NOISE LEVEL integer control boundary frozen.
- [x] VR32 / Bank-6 / TP8 / 4-Vp-p Roland factory acceptance vector retained.
- [x] Same instantaneous NOISE source fans to all six voice-input regions.
- [x] Physical source-input impedance/headroom is retained conceptually.
- [x] Enabled-source normalization is forbidden.
- [x] All unresolved numerical gains/curves live in one calibration surface.
- [x] Every unresolved hardware quantity has an explicit measurement route.

## Machine-readable artifacts

- `JUNO106_SOURCE_MIXER_REFERENCE_MODEL.md`
- `JUNO106_SOURCE_MIXER_NOISE.md`
- `data/juno106_source_mixer_calibration_v0.yaml`
- `data/juno106_source_mixer_evidence.csv`
- `netlists/juno106_source_mixer_topology.csv`

## Calibration backlog — `MEASURE_PENDING`

The following must be replaced with direct original-unit data before claiming final hardware-calibrated strict fidelity:

- exact WAVE -> 80017A passive gain/source impedance;
- exact SUB -> 80017A injection gain/source impedance;
- exact common-NOISE fanout/injection ratio per voice;
- source coupling/DC offsets;
- SUB held-control-voltage -> amplitude curve;
- BA662 noise-VCA control -> gain curve;
- exact healthy noise spectral transfer;
- combined-source maximum pre-VCF excursion;
- first clipping/compression node under high source drive;
- source-dependent loading at the 80017A input.

These quantities are explicitly replaceable calibration data and do not require changing the frozen DSP graph.

## Implementation consequence

Phase-1 source code may now implement:

```text
Wave5534Voice106[6]
Noise106 commonNoise
SourceMixer106[6]
```

with all unresolved analogue constants loaded from calibration data.

No generic four-source normalized mixer is allowed to cross the strict-106 boundary.

**Closure result: source mixer architecture is Phase-0 complete; hardware calibration remains pending.**