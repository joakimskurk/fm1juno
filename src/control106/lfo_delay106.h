#ifndef FM1JUNO_LFO_DELAY106_H
#define FM1JUNO_LFO_DELAY106_H

#include <stdbool.h>
#include <stdint.h>

typedef enum LfoDelayPhase106 {
    LFO_DELAY106_HOLD = 0,
    LFO_DELAY106_RAMP,
    LFO_DELAY106_FULL
} LfoDelayPhase106;

typedef struct LfoDelay106 {
    bool armed_after_silence;
    uint16_t holdoff_accum;
    uint16_t ramp_accum;
    LfoDelayPhase106 phase;
} LfoDelay106;

typedef struct PerformanceLfo106 {
    uint8_t trigger_declick;
} PerformanceLfo106;

/* Exact clean-room coefficient laws from the Delay control. */
uint16_t lfo_delay106_holdoff_increment(uint8_t delay_raw);
uint8_t lfo_delay106_ramp_index(uint8_t delay_raw);
uint16_t lfo_delay106_ramp_increment_from_index(uint8_t ramp_index);
uint16_t lfo_delay106_ramp_increment(uint8_t delay_raw);

void lfo_delay106_init(LfoDelay106 *d);
bool lfo_delay106_observe_voice_run(LfoDelay106 *d, uint8_t voice_run_bits);
uint8_t lfo_delay106_tick(LfoDelay106 *d,
                          uint16_t holdoff_increment,
                          uint16_t ramp_increment);
uint8_t lfo_delay106_tick_raw(LfoDelay106 *d, uint8_t delay_raw);
uint8_t lfo_delay106_onset(const LfoDelay106 *d);

void performance_lfo106_init(PerformanceLfo106 *p);
uint8_t performance_lfo106_tick(PerformanceLfo106 *p,
                                bool physical_trigger_active,
                                uint8_t trigger_sensitivity,
                                uint8_t midi_mod_depth);
uint8_t lfo_delay106_combine_depth(uint8_t panel_depth,
                                   uint8_t onset,
                                   uint8_t performance_depth);

#endif
