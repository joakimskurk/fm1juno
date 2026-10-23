# JUNO-106 chorus filters — superseded compatibility pointer

Status: **superseded**.

This historical Phase-0 draft is retained only so older links and measurement-plan references remain valid. Its earlier reconstruction-derived designators and open-node conclusions are no longer implementation authority.

Use instead:

1. [`JUNO106_CHORUS_REFERENCE_MODEL.md`](JUNO106_CHORUS_REFERENCE_MODEL.md) — frozen Phase-0 implementation/closure contract.
2. [`JUNO106_CHORUS_FILTERS_MIX.md`](JUNO106_CHORUS_FILTERS_MIX.md) — current original-schematic audio/filter/mix evidence.
3. [`JUNO106_CHORUS_PRIMARY_SCHEMATIC.md`](JUNO106_CHORUS_PRIMARY_SCHEMATIC.md) — primary Roland jack-board transcription boundary.
4. [`data/juno106_chorus_component_evidence.csv`](data/juno106_chorus_component_evidence.csv) — per-component evidence status.
5. [`data/juno106_chorus_calibration_v0.yaml`](data/juno106_chorus_calibration_v0.yaml) — centralized `MEASURE_PENDING` calibration surface.

Important corrections relative to this old draft include:

- the original Roland node/topology evidence is now readable enough to map the chorus architecture directly;
- original MN3101 timing families are mapped as `1.8k / 8.2k / 150p` on both channels;
- the original IC6 dry/wet summing law is now derived directly from Roland values;
- `R54/R57` in the original output region are **1.5 kOhm**, not the earlier 15-kOhm misread;
- clone-only designators must not be confused with Roland original designators;
- remaining physical values are calibration backlog, not unresolved architecture.

Do not add new technical content to this file.
