#include "ScreenRecorder.h"
#include <fstream>
#include <cstdint>

void saveBMP(const std::string& filename, int width, int height, const unsigned char* data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return;

    int rowSize = (width * 3 + 3) & ~3; // padded to 4 bytes
    int imageSize = rowSize * height;

    file.put('B'); file.put('M');
    uint32_t fileSize = 54 + imageSize;
    file.write(reinterpret_cast<char*>(&fileSize), 4);
    file.write("\0\0\0\0", 4);
    uint32_t dataOffset = 54;
    file.write(reinterpret_cast<char*>(&dataOffset), 4);

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
    file.write("\0\0\0\0\0\0\0\0\0\0\0\0", 12);

    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 3;
            file.put(data[idx + 2]);
            file.put(data[idx + 1]);
            file.put(data[idx]);
        }
        for (int p = 0; p < (rowSize - width * 3); ++p) file.put(0);
    }
}
