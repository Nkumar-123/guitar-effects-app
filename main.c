#include <raylib.h>
#include "config.h"
#include "fft.h"
#include <stdbool.h>
#include <math.h>

#define SAMPLE_RATE 44100

extern float micBuffer[];
extern float outputBuffer[];
bool useOverdrive = false;
bool useTremolo = false;
bool useDelay = false;
bool AppState = false;

int FrequencySlider = 0; 
float DepthSlider = 1;

int delaySlider = 500;
float feedbackgainSlider = 0.5;


int max(float *fftBuffer,int fftSize)
{
    int Amp = 0;
    for (int i = 0;i<fftSize;i++)
    {
        Amp = fftBuffer[i]>Amp ? fftBuffer[i] : Amp;
    }

    Amp = (int)log((double)Amp);
    if(Amp>0)return Amp;
 return 1;
}

void DrawWaveform(float* buffer, int frameCount, int channels, int offsetX, int offsetY, int width, int height, Color color) {
    // Draw grid lines
    for (int i = 0; i <= 10; i++) {
        int x = offsetX + (i * width) / 10;
        DrawLine(x, offsetY, x, offsetY + height, LIGHTGRAY);
    }
    for (int i = 0; i <= 4; i++) {
        int y = offsetY + (i * height) / 4;
        DrawLine(offsetX, y, offsetX + width, y, LIGHTGRAY);
    }

    // Draw waveform
    for (int i = 0; i < frameCount - 1; i++) {
        int x1 = offsetX + (i * width) / frameCount;
        int y1 = offsetY + (int)((1 - buffer[i * channels]) * height / 2);
        int x2 = offsetX + ((i + 1) * width) / frameCount;
        int y2 = offsetY + (int)((1 - buffer[(i + 1) * channels]) * height / 2);
        DrawLine(x1, y1, x2, y2, color);
    }
}

void DrawFFT(float* fftBuffer, int fftSize, int offsetX, int offsetY, int width, int height, Color color) {
    // Draw grid lines and frequency labels
    for (int i = 0; i <= 10; i++) {
        int x = offsetX + (i * width) / 10;
        DrawLine(x, offsetY, x, offsetY + height, LIGHTGRAY);
        // Adjust the text position by subtracting a few pixels to move it to the left
        DrawText(TextFormat("%d kHz", i * (SAMPLE_RATE / 20000)), x - 10, offsetY + height + 5, 10, DARKGRAY);
    }

    for (int i = 0; i <= 4; i++) {
        int y = offsetY + (i * height) / 4;
        DrawLine(offsetX, y, offsetX + width, y, LIGHTGRAY);
    }

    // Draw FFT
    int Upper = max(fftBuffer,fftSize);
    for (int i = 0; i < fftSize / 2 - 1; i++) {
        int x1 = offsetX + (i * width) / (fftSize / 2);
        int y1 = (offsetY+160) + (int)((1-fftBuffer[i]) * (10/Upper));
        int x2 = offsetX + ((i + 1) * width) / (fftSize / 2);
        int y2 = (offsetY+160) + (int)((1-fftBuffer[i + 1]) *(10/Upper));
        DrawLine(x1, y1, x2, y2, color);
    }
}

int DrawSlider(int value, int minValue, int maxValue, int offsetX, int offsetY, int width, int height) {
    // Draw slider background
    DrawRectangle(offsetX, offsetY, width, height, LIGHTGRAY);

    // Draw slider handle
    int handleX = offsetX + (int)((float)(value - minValue) / (maxValue - minValue) * width);
    DrawRectangle(handleX - 5, offsetY - 5, 10, height + 10, DARKGRAY);

    // Draw markings and labels
    for (int i = 0; i <= 10; i++) {
        int x = offsetX + (i * width) / 10;
        DrawLine(x, offsetY + height + 5, x, offsetY + height + 15, BLACK);
        DrawText(TextFormat("%d", minValue + i * (maxValue - minValue) / 10), x - 10, offsetY + height + 20, 10, BLACK);
    }

    // Handle mouse input
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){offsetX, offsetY, width, height})) {
        value = (int)(((float)(GetMouseX() - offsetX) / width) * (maxValue - minValue)) + minValue;
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
    }

    return value;
}

float DrawSliderFloat(float value, float minValue, float maxValue, int offsetX, int offsetY, int width, int height) {
    // Draw slider background
    DrawRectangle(offsetX, offsetY, width, height, LIGHTGRAY);

    // Draw slider handle
    int handleX = offsetX + (int)((value - minValue) / (maxValue - minValue) * width);
    DrawRectangle(handleX - 5, offsetY - 5, 10, height + 10, DARKGRAY);

    // Draw markings and labels
    for (int i = 0; i <= 10; i++) {
        int x = offsetX + (i * width) / 10;
        DrawLine(x, offsetY + height + 5, x, offsetY + height + 15, BLACK);
        DrawText(TextFormat("%.1f", minValue + i * (maxValue - minValue) / 10), x - 10, offsetY + height + 20, 10, BLACK);
    }

    // Handle mouse input
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){offsetX, offsetY, width, height})) {
        value = (((float)(GetMouseX() - offsetX) / width) * (maxValue - minValue)) + minValue;
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
    }

    return value;
}

int GetDisplayWidth(void) {
    return GetScreenWidth();
}

int GetDisplayHeight(void) {
    return GetScreenHeight();
}


void fullscreenwindow(int windowwidth, int windowheight) {
    int monitor = GetCurrentMonitor();
    if(!IsWindowFullscreen()){
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
    }
    else{
        ToggleFullscreen();
        SetWindowSize(windowwidth, windowheight);
    }
}

int main(void) {
    // Initialize Raylib with initial window size of 800x900
    InitWindow(800, 900, "Guitar Audio Processor");
    SetTargetFPS(60);

    // Initialize audio processing
    init_audio_processing();
    start_audio_device();
    init_fft();  

    // FFT buffers
    float micFFT[FFT_SIZE];
    float outputFFT[FFT_SIZE];

    // Main game loop
    while (!WindowShouldClose()) {

        

        if (IsKeyPressed(KEY_SPACE)) {
            fullscreenwindow(800, 900);
        }

        // Get current window dimensions
        int DisplayWidth = GetDisplayWidth();
        int DisplayHeight = GetDisplayHeight();

        perform_fft(micBuffer, micFFT, FRAME_COUNT);     
        perform_fft(outputBuffer, outputFFT, FRAME_COUNT); 
        Vector2 mousePoint = GetMousePosition();

        // Define button rectangles 
        Rectangle overdriveButton = { 10, DisplayHeight - 40, 80, 30 };
        Rectangle tremoloButton = { 100, DisplayHeight - 40, 80, 30 };
        Rectangle waveformButton = { 280, DisplayHeight - 40, 80, 30 }; 
        Rectangle DelayButton = { 190, DisplayHeight - 40, 80, 30 };    

        // Check for button clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mousePoint, overdriveButton)) {
                useOverdrive = true;
                useTremolo = false;
                useDelay = false;
            } else if (CheckCollisionPointRec(mousePoint, tremoloButton)) {
                useOverdrive = false;
                useTremolo = true;
                useDelay = false;
            } else if(CheckCollisionPointRec(mousePoint,DelayButton)){
                useOverdrive = false;
                useTremolo = false;
                useDelay = true;
            } 
            else if (CheckCollisionPointRec(mousePoint, waveformButton)) {
                AppState = true;
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (AppState == false) {
            //Draw buttons
            
            DrawRectangleRec(overdriveButton, useOverdrive ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(tremoloButton, useTremolo ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(DelayButton, useDelay ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(waveformButton, LIGHTGRAY);
            DrawText("Overdrive", overdriveButton.x + 5, overdriveButton.y+7, 15, BLACK);
            DrawText("Tremolo", tremoloButton.x + 10, tremoloButton.y+7, 15, BLACK);
            DrawText("Delay", DelayButton.x + 7, DelayButton.y+7, 15, BLACK); 
            DrawText("Waveform", waveformButton.x + 10, waveformButton.y + 7, 15, BLACK); 
            DrawText("Press Space to go full screen and press Waveform",20,20,20,BLACK);
            DrawText("Make Sure your speaker volume is low to avoid feedback",20,60,20,BLACK);
            DrawText("Press Escape to exit the application",20,100,20,BLACK);
            

        } else if (AppState == true) {
            // Draw waveforms and FFTs
            DrawText("Microphone Input", 10, 10, 20, DARKGRAY);
            DrawWaveform(micBuffer, FRAME_COUNT, 2, 10, 40, DisplayWidth - 20, (DisplayHeight / 4) - 100, BLUE);

            DrawText("Microphone FFT", 10, 225, 20, DARKGRAY);
            DrawFFT(micFFT, FFT_SIZE, 10, DisplayHeight / 4-15, DisplayWidth - 20, (DisplayHeight / 4) - 100, GREEN);

            DrawText("Processed Output", 10, 440, 20, DARKGRAY);
            DrawWaveform(outputBuffer, FRAME_COUNT, 2, 10, DisplayHeight / 4 + 200, DisplayWidth - 20, (DisplayHeight / 4) - 100, RED);

            DrawText("Output FFT", 10, 640, 20, DARKGRAY);
            DrawFFT(outputFFT, FFT_SIZE, 10, DisplayHeight / 4+400, DisplayWidth - 20, (DisplayHeight / 4) - 100, ORANGE); 
            // Draw buttons
            DrawRectangleRec(overdriveButton, useOverdrive ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(tremoloButton, useTremolo ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(DelayButton, useDelay ? DARKGRAY : LIGHTGRAY);
            DrawRectangleRec(waveformButton, LIGHTGRAY);
            DrawText("Overdrive", overdriveButton.x + 5, overdriveButton.y+7, 15, BLACK);
            DrawText("Tremolo", tremoloButton.x + 10, tremoloButton.y+7, 15, BLACK);
            DrawText("Delay", DelayButton.x + 7, DelayButton.y+7, 15, BLACK);  //+10
            DrawText("Waveform", waveformButton.x + 10, waveformButton.y + 7, 15, BLACK);  //+7
        }

        if(useTremolo==true){
        // Draw sliders
        FrequencySlider = DrawSlider(FrequencySlider, 0, 400, 10, DisplayHeight - 90, DisplayWidth - 20, 5);
        DrawText(TextFormat("Frequency: %d", FrequencySlider), 10, DisplayHeight - 120, 20, BLACK);

        DepthSlider = DrawSliderFloat(DepthSlider, 0.0f, 1.0f, 10, DisplayHeight - 170, DisplayWidth - 20, 5);
        DrawText(TextFormat("Tremolo Depth: %.2f", DepthSlider), 10, DisplayHeight - 200, 20, BLACK);
    }
    else if(useDelay==true){
        feedbackgainSlider = DrawSliderFloat(feedbackgainSlider, 0.0f, 1.0f, 10, DisplayHeight - 90, DisplayWidth - 20, 5);
        DrawText(TextFormat("Feedback Gain: %.2f", feedbackgainSlider), 10, DisplayHeight - 120, 20, BLACK);

        delaySlider = DrawSliderFloat(delaySlider, 0, 2000, 10, DisplayHeight - 170, DisplayWidth - 20, 5);
        DrawText(TextFormat("Delay time: %d", delaySlider), 10, DisplayHeight - 200, 20, BLACK);

    }

        EndDrawing();
    }

    stop_audio_device();

    CloseWindow();

    return 0;
}