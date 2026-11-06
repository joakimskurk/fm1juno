# Calibration input boundary

Phase-1 implementation must not hide provisional analogue constants inside DSP source.

The Phase-0 machine-readable sources currently live under `docs/research/data/` and remain authoritative until the generator consolidates them into a Phase-1 nominal calibration package.

Planned generated outputs:

```text
calibration/generated/juno106_calibration.h
calibration/generated/juno106_calibration.hpp
```

The generator must retain evidence status and be able to report every active `MEASURE_PENDING` value.
