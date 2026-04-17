#include "ScreenRecorder.h"
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#pragma comment(lib, "gdiplus.lib")

ScreenRecorder::ScreenRecorder(int w, int h, int f, const std::string& output)
    : width(w), height(h), fps(f), outputFile(output), isRecording(false) {}

void ScreenRecorder::startRecording() {
    if (isRecording) return;
    isRecording = true;
    recordingThread = std::thread([this]() {
        recordPlatform();
    });
}

void ScreenRecorder::stopRecording() {
    if (!isRecording) return;
    isRecording = false;
    if (recordingThread.joinable()) {
        recordingThread.join();
    }
    std::cout << "Recording stopped. Use FFmpeg to combine frames: ffmpeg -i frame_%d.bmp -c:v libx264 " << outputFile << std::endl;
}

ScreenRecorder::~ScreenRecorder() {
    stopRecording();
}

int getScreenRefreshRate() {
    DEVMODE devMode;
    ZeroMemory(&devMode, sizeof(devMode));
    devMode.dmSize = sizeof(devMode);

    if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
        return devMode.dmDisplayFrequency > 0 ? devMode.dmDisplayFrequency : 60;
    }
    return 60;
}

void ScreenRecorder::recordPlatform() {
    std::cout << "Starting screen capture on Windows..." << std::endl;
    std::cout << "Resolution: " << width << "x" << height << std::endl;
    std::cout << "FPS: " << fps << std::endl;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    if (width == 0 || height == 0) {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    }

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemoryDC, hBitmap);

    int frameCount = 0;
    while (isRecording) {
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromHBITMAP(hBitmap, NULL);
        if (bitmap) {
            Gdiplus::Rect rect(0, 0, width, height);
            Gdiplus::BitmapData bitmapData;
            bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &bitmapData);

            std::vector<unsigned char> data(width * height * 3);
            unsigned char* src = static_cast<unsigned char*>(bitmapData.Scan0);
            for (int y = 0; y < height; ++y) {
                memcpy(&data[y * width * 3], src + y * bitmapData.Stride, width * 3);
            }

            bitmap->UnlockBits(&bitmapData);
            std::string filename = "frame_" + std::to_string(frameCount++) + ".bmp";
            saveBMP(filename, width, height, data.data());
            delete bitmap;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }

    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

int main() {
    int detectedFps = getScreenRefreshRate();
    ScreenRecorder recorder(0, 0, detectedFps, "output.mp4");
    recorder.startRecording();

    std::cout << "Recording... press Enter to stop." << std::endl;
    std::cin.get();

    recorder.stopRecording();
    return 0;
}
