#pragma once

#include <cstdint>
#include <vector>

namespace experiments {

enum class ImageFormat {
    RGBA8 = 0,
    RGB8 = 1,
};

struct LoadedImageBuffer {
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    ImageFormat format;

    std::vector<std::vector<unsigned char>> data;
};

LoadedImageBuffer genMipmaps(LoadedImageBuffer buffer);

}
