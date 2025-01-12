#include "fft.h"
#include "config.h"
#include "kiss_fft.h"
#include <math.h>
#include <stdlib.h>

static kiss_fft_cfg cfg;


void init_fft() {
    cfg = kiss_fft_alloc(FFT_SIZE, 0, NULL, NULL);
    if (cfg == NULL) {
        printf("Failed to allocate memory for FFT\n");
        exit(1);
    }
}

void perform_fft(const float* input, float* output, int frameCount) {
    kiss_fft_cpx in[FFT_SIZE];
    kiss_fft_cpx out[FFT_SIZE];

    // Prepare the input for FFT
    for (int i = 0; i < FFT_SIZE; ++i) {
        in[i].r = (i < frameCount) ? input[i * 2] : 0.0f;
        in[i].i = 0.0f;
    }

    // FFT CALCULATION
    kiss_fft(cfg, in, out);

    // MAGNITUDE OF FFT
    for (int i = 0; i < FFT_SIZE; ++i) {
        output[i] = sqrtf(out[i].r * out[i].r + out[i].i * out[i].i);
    }
}