#include "ScreenRecorder.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>
#include <gtk/gtk.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

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
    Display* display = XOpenDisplay(NULL);
    if (!display) return 60;

    int screen = DefaultScreen(display);
    int refreshRate = 60;

    XRRScreenConfiguration* config = XRRGetScreenInfo(display, RootWindow(display, screen));
    if (config) {
        Rotation rotation;
        SizeID sizeID = XRRConfigCurrentConfiguration(config, &rotation);
        short* rates = XRRConfigRates(config, sizeID, &refreshRate);
        if (rates && refreshRate > 0) {
            refreshRate = rates[0];
        }
        XRRFreeScreenConfigInfo(config);
    }

    XCloseDisplay(display);
    return refreshRate > 0 ? refreshRate : 60;
}

void ScreenRecorder::recordPlatform() {
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
                data[(y * width + x) * 3 + 0] = (pixel >> 16) & 0xFF;
                data[(y * width + x) * 3 + 1] = (pixel >> 8) & 0xFF;
                data[(y * width + x) * 3 + 2] = pixel & 0xFF;
            }
        }

        std::string filename = "frame_" + std::to_string(frameCount++) + ".bmp";
        saveBMP(filename, width, height, data.data());
        XDestroyImage(img);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps));
    }

    XCloseDisplay(display);
}

static ScreenRecorder* globalRecorder = nullptr;

static void on_start_clicked(GtkWidget* widget, gpointer data) {
    if (globalRecorder) {
        globalRecorder->startRecording();
    }
}

static void on_stop_clicked(GtkWidget* widget, gpointer data) {
    if (globalRecorder) {
        globalRecorder->stopRecording();
    }
}

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Screen Recorder");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget* start_button = gtk_button_new_with_label("Start Recording");
    GtkWidget* stop_button = gtk_button_new_with_label("Stop Recording");
    GtkWidget* status_label = gtk_label_new("Ready");

    gtk_box_pack_start(GTK_BOX(vbox), start_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), stop_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, TRUE, TRUE, 0);

    int detectedFps = getScreenRefreshRate();
    ScreenRecorder recorder(0, 0, detectedFps, "output.mp4");
    globalRecorder = &recorder;

    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), nullptr);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), nullptr);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
