# JUNO-106 global HPF / output — Phase-0 closure record

Status: **`GLOBAL_OUTPUT_PHASE0_ARCHITECTURE_FROZEN`**.

This file supersedes the older three-line Section F wording in `PHASE0_CHECKLIST.md` where that wording conflicts with newer specialized research. The repository authority rule is that the newest specialized block specification wins.

## Architecture gate

- [x] Confirm one global HPF after the six-voice sum.
- [x] Confirm original semantic states 0 bass boost / 1 flat / 2 HPF / 3 higher HPF.
- [x] Confirm TC4052 discrete analogue route selection.
- [x] Confirm 106 IC4A isolation/summing before global VCA and distinguish it from 60 loading.
- [x] Freeze HPF1 as nominal straight route through the physical downstream chain.
- [x] Derive HPF2 from original 47 kOhm / 15 nF components: first-order, ideal `225.7516923289 Hz`.
- [x] Derive HPF3 from original 47 kOhm / 4.7 nF components: first-order, ideal `720.4841244540 Hz`.
- [x] Correct the old `cutoff/Q` task: Q is not applicable to these first-order branches.
- [x] Retain TC4052 Ron as a small calibration term rather than deleting it or hard-coding a datasheet typical.
- [x] Freeze HPF0 graph as direct contribution plus a two-stage/two-pole-two-zero bass-emphasis contribution into IC4A.
- [x] Retain original HPF0 component/node families rather than replacing them with a generic digital shelf.
- [x] Recover exact VCA LEVEL digital storage law `raw7 << 7`.
- [x] Separate the Roland external VCA-control network from the uPC1252 core transfer.
- [x] Freeze uPC1252 as a current-mode dbx-family global VCA, not a linear gain multiplier.
- [x] Retain the post-uPC1252 current-to-voltage/output stage before chorus.
- [x] Treat owner-manual high-VCA-level distortion as authentic headroom behavior.
- [x] Forbid active-voice normalization and hidden pre-model limiting.
- [x] Centralize all unresolved strict numerical data in `data/juno106_global_output_calibration_v0.yaml`.
- [x] Define deterministic component-ideal HPF2/3 SPICE references.
- [x] Define explicit original-unit measurement routes for every remaining calibration value.

**Result: GLOBAL OUTPUT PHASE-0 ARCHITECTURE GATE CLOSED.**

## Calibration backlog — `MEASURE_PENDING`

These do not reopen the architecture unless measurements contradict primary evidence:

- [ ] complete original HPF0 passive/node transcription or equivalent measured complex transfer;
- [ ] installed TC4052 on-resistance at relevant operating points if its contribution proves measurable;
- [ ] IC4A exact gain/headroom/slew;
- [ ] digital VCA LEVEL code -> held control voltage;
- [ ] held control voltage -> uPC1252 control-pin voltage;
- [ ] complete VCA LEVEL -> output gain/dB curve;
- [ ] post-VCA current-to-voltage static gain;
- [ ] global overload/clipping/THD surface versus source drive and VCA LEVEL;
- [ ] final loading into the chorus input.

## Implementation authority

Use:

- `JUNO106_HPF_GLOBAL_OUTPUT.md`
- `JUNO106_GLOBAL_OUTPUT_REFERENCE_MODEL.md`
- `data/juno106_global_output_calibration_v0.yaml`
- `data/juno106_global_output_evidence.csv`
- `netlists/juno106_hpf23_component_reference.cir`

No implementation should use a guessed HPF0 shelf height, a linear `raw/127` VCA law, JUNO-60 output loading, or an arbitrary output waveshaper as strict-106 truth.
