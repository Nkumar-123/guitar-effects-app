#ifndef FFT_H
#define FFT_H

#include "kiss_fft.h" 
#include "config.h"

void init_fft(void);
void perform_fft(const float* input, float* output, int frameCount);

#endif




