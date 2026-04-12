#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
//will create different versions later
#ifdef _WIN32
    #include <windows.h>
    #include <gdiplus.h>
    #pragma comment(lib, "gdiplus.lib")
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <gtk/gtk.h>
#endif

// Simple BMP writer
void saveBMP(const std::string& filename, int width, int height, const unsigned char* data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return;

    int rowSize = (width * 3 + 3) & ~3; // padded to 4 bytes
    int imageSize = rowSize * height;

    // BMP header
    file.put('B'); file.put('M');
    uint32_t fileSize = 54 + imageSize;
    file.write(reinterpret_cast<char*>(&fileSize), 4);
    file.write("\0\0\0\0", 4); // reserved
    uint32_t dataOffset = 54;
    file.write(reinterpret_cast<char*>(&dataOffset), 4);

    // DIB header
    uint32_t headerSize = 40;
    file.write(reinterpret_cast<char*>(&headerSize), 4);
    file.write(reinterpret_cast<char*>(&width), 4);
    file.write(reinterpret_cast<char*>(&height), 4);
    uint16_t planes = 1;
    file.write(reinterpret_cast<char*>(&planes), 2);
    uint16_t bpp = 24;
    file.write(reinterpret_cast<char*>(&bpp), 2);
    uint32_t compression = 0;
    file.write(reinterpret_cast<char*>(&compression), 4);
    file.write(reinterpret_cast<char*>(&imageSize), 4);
    file.write("\0\0\0\0\0\0\0\0\0\0\0\0", 12); // rest zero

    // Pixel data, bottom to top, BGR
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 3;
            file.put(data[idx + 2]); // B
            file.put(data[idx + 1]); // G
            file.put(data[idx]);     // R
        }
        // padding
        for (int p = 0; p < (rowSize - width * 3); ++p) file.put(0);
    }
}

class ScreenRecorder {
private:
    int width, height, fps;
    std::string outputFile;
    std::atomic<bool> isRecording;
    std::thread recordingThread;

public:
    ScreenRecorder(int w, int h, int f, const std::string& output)
        : width(w), height(h), fps(f), outputFile(output), isRecording(false) {}

    void startRecording() {
        if (isRecording) return;
        isRecording = true;
        recordingThread = std::thread([this]() {
#ifdef _WIN32
            recordWindows();
#else
            recordLinux();
#endif
        });
    }

    void stopRecording() {
        if (!isRecording) return;
        isRecording = false;
        if (recordingThread.joinable()) {
            recordingThread.join();
        }
        std::cout << "Recording stopped. Use FFmpeg to combine frames: ffmpeg -i frame_%d.bmp -c:v libx264 " << outputFile << std::endl;
    }

    ~ScreenRecorder() {
        stopRecording();
    }

private:
#ifdef _WIN32
    void recordWindows() {
        std::cout << "Starting screen capture on Windows..." << std::endl;
        std::cout << "Resolution: " << width << "x" << height << std::endl;
        std::cout << "FPS: " << fps << std::endl;

        // Initialize GDI+
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
                unsigned char* src = (unsigned char*)bitmapData.Scan0;
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
#else
    void recordLinux() {
        std::cout << "Starting screen capture on Linux..." << std::endl;
        std::cout << "Resolution: " << width << "x" << height << std::endl;
        std::cout << "FPS: " << fps << std::endl;

        Display* display = XOpenDisplay(NULL);
        if (!display) {
            std::cerr << "Cannot open display" << std::endl;
            return;
        }

        Window root = DefaultRootWindow(display);
        int screen = DefaultScreen(display);

        if (width == 0 || height == 0) {
            width = DisplayWidth(display, screen);
            height = DisplayHeight(display, screen);
        }

        int frameCount = 0;

        while (isRecording) {
            XImage* img = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
            if (!img) {
                std::cerr << "Cannot get image" << std::endl;
                break;
            }

            std::vector<unsigned char> data(width * height * 3);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    unsigned long pixel = XGetPixel(img, x, y);
                    data[(y * width + x) * 3 + 0] = (pixel >> 16) & 0xFF; // R
                    data[(y * width + x) * 3 + 1] = (pixel >> 8) & 0xFF;  // G
                    data[(y * width + x) * 3 + 2] = pixel & 0xFF;         // B
                }
            }

            std::string filename = "frame_" + std::to_string(frameCount++) + ".bmp";
            saveBMP(filename, width, height, data.data());

            XDestroyImage(img);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
        }

        XCloseDisplay(display);
    }
#endif
};

#ifndef _WIN32
extern "C" {
void on_start_clicked(GtkWidget *widget, gpointer data) {
    ScreenRecorder* rec = static_cast<ScreenRecorder*>(data);
    rec->startRecording();
}

void on_stop_clicked(GtkWidget *widget, gpointer data) {
    ScreenRecorder* rec = static_cast<ScreenRecorder*>(data);
    rec->stopRecording();
}
}
#endif
//linux version and windows version seperation
int main(int argc, char *argv[]) {
#ifndef _WIN32
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Screen Recorder");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *start_button = gtk_button_new_with_label("Start Recording");
    GtkWidget *stop_button = gtk_button_new_with_label("Stop Recording");
    GtkWidget *status_label = gtk_label_new("Ready");

    gtk_box_pack_start(GTK_BOX(vbox), start_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), stop_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, TRUE, TRUE, 0);

    ScreenRecorder recorder(0, 0, 30, "output.mp4");

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), &recorder);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), &recorder);

    gtk_widget_show_all(window);
    gtk_main();
#else
    ScreenRecorder recorder(0, 0, 30, "output.mp4");
    recorder.startRecording();
    // For Windows, perhaps add a console menu or something, but for now, just start
#endif
    return 0;
}