# FM1JUNO deterministic event script v1

Version: `1`.

The event script is an absolute-timestamp input format for host/reference and target replay tests. Outer callback/block size is deliberately absent from event semantics.

Canonical CSV columns:

```text
version,sample,frac_q32,type,voice,value,aux
```

Time is:

```text
time_q32 = (sample << 32) | frac_q32
```

Rows must be sorted by `time_q32`. Rows sharing the same timestamp are applied in file order.

Event types:

```text
VOICE_ON      voice=0..5, value=note 0..127
VOICE_OFF     voice=0..5, value=note 0..127
SUSTAIN       value=0/1
DCO_COMMAND   voice=0..5, value=8253 count, aux=0/1/2 for 16'/8'/4'
CONTROL_STEP  advance one ROM-macro scheduler event
```

`CONTROL_STEP` is a deterministic Phase-1 harness event. It does not claim that real hardware spaces scheduler slots one audio sample apart. Physical microsecond offsets replace these explicit steps later through the control-timing calibration layer without changing external event timestamps.

Required invariance rule:

The same v1 script replayed through 16/32/64/128/256-frame outer adapters must produce identical control trace and final control state.
