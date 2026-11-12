#include "ecg_filter.h"
#include <string.h>

void ecg_filter_init(ECGFilterChain *chain)
{
    memset(chain, 0, sizeof(ECGFilterChain));
    chain->gain_factor = 2.42f / (6.0f * 8388608.0f) * 1e6f;
    chain->lms.mu = 0.01f;
    chain->lms.norm = 1e-6f;
    chain->notch50.b0 = 1.0f;
    chain->notch50.b1 = -1.9754f;
    chain->notch50.b2 = 1.0f;
    chain->notch50.a1 = -1.9754f;
    chain->notch50.a2 = 0.9584f;
    chain->hp05.alpha = 0.999f;
    chain->lp40.b0 = 0.0675f;
    chain->lp40.b1 = 0.1349f;
    chain->lp40.b2 = 0.0675f;
    chain->lp40.a1 = -1.1430f;
    chain->lp40.a2 = 0.4128f;
}

float biquad_process(BiquadFilter *f, float input)
{
    float output = f->b0 * input + f->b1 * f->x1 + f->b2 * f->x2
                   - f->a1 * f->y1 - f->a2 * f->y2;
    f->x2 = f->x1;
    f->x1 = input;
    f->y2 = f->y1;
    f->y1 = output;
    return output;
}

float dcblock_process(DCBlockFilter *f, float input)
{
    float output = input - f->x_prev + f->alpha * f->y_prev;
    f->x_prev = input;
    f->y_prev = output;
    return output;
}

void ecg_differential(ECGFilterChain *chain, int32_t ch1_raw, int32_t ch2_raw)
{
    chain->ch1_raw = ch1_raw * chain->gain_factor;
    chain->ch2_raw = ch2_raw * chain->gain_factor;
    chain->diff_raw = chain->ch1_raw - chain->ch2_raw;
    chain->cm_ref = (chain->ch1_raw + chain->ch2_raw) * 0.5f;
}

float ecg_filter_process(ECGFilterChain *chain, int32_t ch1_raw, int32_t ch2_raw)
{
    ecg_differential(chain, ch1_raw, ch2_raw);
    chain->filtered_output = chain->diff_raw;
    chain->sample_count++;
    return chain->filtered_output;
}
