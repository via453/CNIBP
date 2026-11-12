#ifndef __ECG_FILTER_H
#define __ECG_FILTER_H

#include "stm32f10x.h"
#include <stdint.h>

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float x1, x2;
    float y1, y2;
} BiquadFilter;

typedef struct {
    float alpha;
    float x_prev, y_prev;
} DCBlockFilter;

typedef struct {
    float w[16];
    float x[16];
    float mu;
    float norm;
    int converged;
    int sample_count;
} LMSFilter;

typedef struct {
    float ch1_raw, ch2_raw;
    float diff_raw;
    float cm_ref;
    LMSFilter lms;
    float lms_output;
    BiquadFilter notch50;
    DCBlockFilter hp05;
    BiquadFilter lp40;
    float gain_factor;
    float filtered_output;
    int sample_count;
} ECGFilterChain;

void ecg_filter_init(ECGFilterChain *chain);
float ecg_filter_process(ECGFilterChain *chain, int32_t ch1_raw, int32_t ch2_raw);
void ecg_differential(ECGFilterChain *chain, int32_t ch1_raw, int32_t ch2_raw);
float biquad_process(BiquadFilter *f, float input);
float dcblock_process(DCBlockFilter *f, float input);

#endif
