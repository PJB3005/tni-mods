#include "src/image/mipmap_gen.hpp"

#include "stb_image.h"
#include "stb_image_write.h"

#include <span>

#include <cstdio>
#include <filesystem>

int main(int const argc, char const * const argv[]) {
    if (argc != 3) {
        std::printf("Need two arguments!\n");
        return 1;
    }

    auto const src = argv[1];
    auto const dst = argv[2];

    std::printf("%s -> %s\n", src, dst);

    int w, h;
    auto const data = stbi_load(src, &w, &h, nullptr, 4);
    if (!data) {
        std::printf("Failed to load image: %s\n", stbi_failure_reason());
        return 1;
    }

    std::printf("Cur path: %s\n", std::filesystem::current_path().string().c_str());

    experiments::LoadedImageBuffer buffer {
        .width = static_cast<uint32_t>(w),
        .height = static_cast<uint32_t>(h),
        .mipLevels = 1,
        .format = experiments::ImageFormat::RGBA8,
    };

    std::vector<uint8_t> dataLevel0;
    dataLevel0.resize(w * h * 4);
    std::memcpy(dataLevel0.data(), data, w * h * 4);
    buffer.data.emplace_back(std::move(dataLevel0));

    stbi_image_free(data);

    std::filesystem::create_directory(dst);

    buffer = experiments::genMipmaps(std::move(buffer));

    int mipW = w;
    int mipH = h;
    for (int level = 0; level < buffer.mipLevels; level += 1) {
        std::string out_path = dst;
        out_path += "/" + std::to_string(level) + ".png";
        stbi_write_png(out_path.c_str(), mipW, mipH, 4, buffer.data.at(level).data(), mipW * 4);

        mipW /= 2;
        mipH /= 2;
    }

    return 0;
}
