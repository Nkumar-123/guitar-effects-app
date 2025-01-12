#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "config.h"
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.1415926
#define SAMPLE_RATE 44100

ma_device device; 
ma_hpf hpf1;
ma_lpf lpf,lpf2;

extern bool useOverdrive;
extern bool useTremolo;
extern bool useDelay;
extern int FrequencySlider;
extern float DepthSlider;
extern int delaySlider;
extern float feedbackgainSlider;

float micBuffer[FRAME_COUNT * 2];
float outputBuffer[FRAME_COUNT * 2];
float overdriveBuffer[FRAME_COUNT * 2];
float* delayBuffer = NULL;
float lpf_buffer[FRAME_COUNT*2];
float pPoints[SAMPLE_RATE];
float micBuffer_delay[SAMPLE_RATE];
float delayLineInput[SAMPLE_RATE];

FILE *wavFile;
long int wavDataSize = 0;


// WAV file header structure
typedef struct {
    char chunkId[4];
    int chunkSize;
    char format[4];
    char subchunk1Id[4];
    int subchunk1Size;
    short audioFormat;
    short numChannels;
    int sampleRate;
    int byteRate;
    short blockAlign;
    short bitsPerSample;
    char subchunk2Id[4];
    int subchunk2Size;
} WavHeader;

// Function to write WAV header
void writeWavHeader() {
    WavHeader header;

    // RIFF chunk descriptor
    strncpy(header.chunkId, "RIFF", 4);
    header.chunkSize = 36 + wavDataSize;
    strncpy(header.format, "WAVE", 4);

    // fmt sub-chunk
    strncpy(header.subchunk1Id, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 3; // IEEE float
    header.numChannels = 2;
    header.sampleRate = SAMPLE_RATE;
    header.bitsPerSample = 32;
    header.byteRate = SAMPLE_RATE * header.numChannels * (header.bitsPerSample / 8);
    header.blockAlign = header.numChannels * (header.bitsPerSample / 8);

    // data sub-chunk
    strncpy(header.subchunk2Id, "data", 4);
    header.subchunk2Size = wavDataSize;

    fseek(wavFile, 0, SEEK_SET);
    fwrite(&header, sizeof(WavHeader), 1, wavFile);
}

float overdrive(float x) {
    if (x >= 0 && x <= 1.0f / 3.0f) {
        return 2.0f * x;
    } else if (x > 1.0f / 3.0f && x <= 2.0f / 3.0f) {
        return (3.0f - powf((2.0f - 3.0f * x), 2.0f)) / 3.0f;
    } else if (x > 2.0f / 3.0f) {
        return 1.0f;
    }
}

void generate_triangle_wave(double *t, double *wave, int n, double a, double amplitude) {
    for (int i = 0; i < n; ++i) {
        double phase=fmod(2*PI*a*t[i],2*PI);
        if (phase < PI) {
            wave[i]=amplitude*(2*(phase/PI)-1);
        } else {
            wave[i]=amplitude*(1-2*((phase-PI)/PI));
        }
    }
}

int Delay_line(float time_ms){
   int delay_samples = (int)((time_ms/1000) * SAMPLE_RATE);
   return delay_samples;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    MA_ASSERT(pDevice->capture.format == pDevice->playback.format);
    MA_ASSERT(pDevice->capture.channels == pDevice->playback.channels);

    // MIC VALUES FOR PLOTTING
    for (ma_uint32 i = 0; i < frameCount * pDevice->capture.channels; ++i) {
        micBuffer[i] = ((float*)pInput)[i];
    }

    if(useTremolo==TRUE){
        double t[SAMPLE_RATE];
        double wave[SAMPLE_RATE];
        double a = FrequencySlider;
        double amplitude = 1;

        double dt = 0.01 / SAMPLE_RATE;
        for (int i = 0; i < SAMPLE_RATE; ++i) {
            t[i] = i * dt;
        }
        generate_triangle_wave(t, wave, SAMPLE_RATE, a, amplitude);

        // Window parameters
        static int windowSize = 960;
        static int windowPosition = 0;
        float depth = DepthSlider;

        // Process audio
        for (ma_uint32 i = 0; i < frameCount * pDevice->capture.channels; ++i) {
            micBuffer[i] = ((float*)pInput)[i];

            //WINDOWED TREMOLO EFFECT
            double windowedWaveValue = micBuffer[i] * ((1-depth) + depth * wave[windowPosition % SAMPLE_RATE]);

            outputBuffer[i] = 10 * windowedWaveValue;

            //MOVE WINDOW
            windowPosition++;
        }

        for (ma_uint32 i = 0; i < frameCount * pDevice->capture.channels; ++i) {
            ((float*)pOutput)[i] = outputBuffer[i];

        }
    }
else if(useDelay==TRUE){
    int delaytime = delaySlider; 
    int delayLength = Delay_line(delaytime*2);  
    static int writeIndex = 0;
    float feedback = feedbackgainSlider;

    if(delayBuffer == NULL){
        printf("Allocating memory for delay buffer...\n");
        delayBuffer = (float*)calloc(delayLength * device.playback.channels, sizeof(float));
        if(delayBuffer == NULL){
            printf("Failed to allocate delayBuffer\n");
            exit(1);
        }
        printf("Memory allocated for delay buffer\n");
    }

    for (ma_uint32 i = 0; i < frameCount * device.playback.channels; ++i) {
        int readIndex = writeIndex - delayLength;
        if (readIndex < 0) readIndex += delayLength * device.playback.channels;

        float input = ((float*)pInput)[i];
        float delayedSample = delayBuffer[readIndex];

        // Mix the input with the delayed signal
        float output = input + feedback * delayedSample;

        // Write to the output buffer
        ((float*)pOutput)[i] = output;

        // Update the delay buffer
        delayBuffer[writeIndex] = input;

        // Increment and wrap the write index
        writeIndex = (writeIndex + 1) % (delayLength * device.playback.channels);

        micBuffer[i] = input;
        outputBuffer[i] = output;
    }
}
    else if(useOverdrive==TRUE){
        ma_hpf_process_pcm_frames(&hpf1, overdriveBuffer, pInput, frameCount);

        ma_lpf_process_pcm_frames(&lpf, overdriveBuffer, overdriveBuffer, frameCount);

        for (ma_uint32 i = 0; i < frameCount * pDevice->capture.channels; ++i) {
            lpf_buffer[i] = ((float*)overdriveBuffer)[i];
        }

        // Process overdrive effect
        for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {

            // Amplified samples to bring the value in range for the overdrive effect
            float AmplifiedSample = 100*((float*)overdriveBuffer)[i];

             // Apply overdrive  
            float overdrivenSample = overdrive(fabsf(AmplifiedSample));

            // Apply the overdriven value with the original sign
            overdriveBuffer[i] = copysign(overdrivenSample, AmplifiedSample); 
        }

        for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
            ((float*)pOutput)[i] = overdriveBuffer[i];
            outputBuffer[i] = overdriveBuffer[i]; // Store overdrive effect for plotting
        }

        ma_lpf_process_pcm_frames(&lpf2, overdriveBuffer, overdriveBuffer, frameCount);
    }

    // Write the output buffer to the WAV file
    fwrite(pOutput, sizeof(float), frameCount * pDevice->playback.channels, wavFile);
    wavDataSize += frameCount * pDevice->playback.channels * sizeof(float);
}

void init_audio_processing() {
    
    wavFile = fopen("output.wav", "wb");
    if (wavFile == NULL) {
        printf("Failed to open output.wav file\n");
        return;
    }

    
    WavHeader placeholder = {0};
    fwrite(&placeholder, sizeof(WavHeader), 1, wavFile);
}

void start_audio_device() {
    ma_device_config deviceConfig;
    ma_result result;

    //HIGH PASS FILTER TO GET RID OF BACKGROUND NOISE
    ma_hpf_config hpfConfig = ma_hpf_config_init(ma_format_f32, 2, 44100, 1000, 8); 
    result = ma_hpf_init(&hpfConfig, NULL, &hpf1);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize LPF\n");
        return;
    }

    //LOW PASS FILTER FOR AA EFFECT
    ma_lpf_config lpfConfig = ma_lpf_config_init(ma_format_f32, 2, 44100, 11025, 8); 
    result = ma_lpf_init(&lpfConfig, NULL, &lpf);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize LPF\n");
        return;
    }

    //LOW PASS FILTER
    ma_lpf_config lpfConfig2 = ma_lpf_config_init(ma_format_f32, 2, 44100, 7500, 8); 
    result = ma_lpf_init(&lpfConfig2, NULL, &lpf2);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize LPF2\n");
        return;
    }

    // DUPLEX CONFIG 
    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.capture.pDeviceID  = NULL;
    deviceConfig.capture.format     = ma_format_f32;
    deviceConfig.capture.channels   = 2;
    deviceConfig.capture.shareMode  = ma_share_mode_shared;
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format    = ma_format_f32;
    deviceConfig.playback.channels  = 2;
    deviceConfig.dataCallback       = data_callback;

    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize device\n");
        return;
    }

    ma_device_start(&device);
}

void stop_audio_device() {
    ma_device_uninit(&device);
    ma_hpf_uninit(&hpf1,NULL);
    ma_lpf_uninit(&lpf, NULL); 
    ma_lpf_uninit(&lpf2, NULL); 

    // Update the WAV header with the final file size
    writeWavHeader();

    fclose(wavFile);
    if (delayBuffer != NULL) {
        free(delayBuffer);
        delayBuffer = NULL;
    }
}