#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define FFT_SIZE 1024
#define FRAME_COUNT 1024 


extern float micBuffer[FRAME_COUNT * 2];
extern float outputBuffer[FRAME_COUNT * 2];

extern bool useOverdrive;
extern bool useTremolo;
extern bool useDelay;
extern int FrequencySlider;
extern float DepthSlider;

extern int delaytimeSlider;
extern float feedbackgainSlider;

void init_audio_processing(void);
void start_audio_device(void);
void stop_audio_device(void);

#endif 