#pragma once

#include <string>
#include <atomic>
#include <thread>

class ScreenRecorder {
private:
    int width;
    int height;
    int fps;
    std::string outputFile;
    std::atomic<bool> isRecording;
    std::thread recordingThread;

    void recordPlatform();

public:
    ScreenRecorder(int w, int h, int f, const std::string& output);
    void startRecording();
    void stopRecording();
    ~ScreenRecorder();
};

int getScreenRefreshRate();
void saveBMP(const std::string& filename, int width, int height, const unsigned char* data);
