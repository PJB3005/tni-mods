#include "mipmap_gen.hpp"
#include "stb_image_resize2.h"
#include "srgb.hpp"

#include <algorithm>
#include <cstdint>

namespace experiments {

namespace {

constexpr size_t PixelSize(ImageFormat const format) {
    switch (format) {
    case ImageFormat::RGB8:
        return 3;
    case ImageFormat::RGBA8:
        return 4;
    default:
        throw std::runtime_error("Unknown image format!");
    }
}

/*
constexpr float magic_kernel_sharp_2013(float x) noexcept
{
    if      (x < 0.0f)  x = -x;
    if      (x <= 0.5f) return 17.0f / 16.0f - (7.0f / 4.0f) * x * x;
    else if (x <= 1.5f) return 0.25f * (4.0f * x * x - 11.0f * x + 7.0f);
    else if (x <= 2.5f) return -0.125f * (x - 2.5f) * (x - 2.5f);
    else return 0.0f;
}

template<size_t pixelWidth=4>
uint8_t mipmap_sample(uint8_t const* source, uint32_t sourceWidth, uint32_t sourceHeight, uint32_t mipX,
                               uint32_t mipY, uint32_t offset) {
    const auto x0 = std::min(mipX * 2, sourceWidth - 1);
    const auto x1 = std::min(x0 + 1, sourceWidth - 1);
    const auto y0 = std::min(mipY * 2, sourceHeight - 1);
    const auto y1 = std::min(y0 + 1, sourceHeight - 1);

    auto const s1 = source[(static_cast<size_t>(y0) * sourceWidth + x0) * pixelWidth + offset];
    auto const s2 = source[(static_cast<size_t>(y1) * sourceWidth + x0) * pixelWidth + offset];
    auto const s3 = source[(static_cast<size_t>(y0) * sourceWidth + x1) * pixelWidth + offset];
    auto const s4 = source[(static_cast<size_t>(y1) * sourceWidth + x1) * pixelWidth + offset];

    return (s1 + s2 + s3 + s4) / 4;
}

template<size_t pixelWidth=4>
uint8_t mipmap_sample_srgb_aware(uint8_t const* source, uint32_t sourceWidth, uint32_t sourceHeight, uint32_t mipX,
                               uint32_t mipY, uint32_t offset) {
    const auto x0 = std::min(mipX * 2, sourceWidth - 1);
    const auto x1 = std::min(x0 + 1, sourceWidth - 1);
    const auto y0 = std::min(mipY * 2, sourceHeight - 1);
    const auto y1 = std::min(y0 + 1, sourceHeight - 1);

    auto const s1 = srgb_to_linear(source[(static_cast<size_t>(y0) * sourceWidth + x0) * pixelWidth + offset]);
    auto const s2 = srgb_to_linear(source[(static_cast<size_t>(y1) * sourceWidth + x0) * pixelWidth + offset]);
    auto const s3 = srgb_to_linear(source[(static_cast<size_t>(y0) * sourceWidth + x1) * pixelWidth + offset]);
    auto const s4 = srgb_to_linear(source[(static_cast<size_t>(y1) * sourceWidth + x1) * pixelWidth + offset]);

    return linear_to_srgb((s1 + s2 + s3 + s4) / 4.0f);
}

template<ImageFormat format>
void makeLevelKernelRgb8Rgba8(uint8_t* dst, uint32_t dst_w, uint32_t dst_h, uint8_t const* src, uint32_t src_w, uint32_t src_h) {
    constexpr size_t pixelSize = PixelSize(format);

	if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
	    throw std::runtime_error("Invalid size!");
	}

	// continuous ratios
	const float fx_ratio = static_cast<float>(src_w) / static_cast<float>(dst_w);
	const float fy_ratio = static_cast<float>(src_h) / static_cast<float>(dst_h);

	// scale used to compute integer footprint radius (>= 1)
	const float scale_x = (fx_ratio > 1.0f) ? fx_ratio : 1.0f;
	const float scale_y = (fy_ratio > 1.0f) ? fy_ratio : 1.0f;

	int radius_x = 3;
	int radius_y = 3;

	if (dst_w < src_w) radius_x = static_cast<int>(std::ceilf(2.5f * scale_x));
	if (dst_h < src_h) radius_y = static_cast<int>(std::ceilf(2.5f * scale_y));

	for (int dy = 0; dy < dst_h; ++dy)
	{
		// map dst pixel center to source continuous coordinate:
		float src_y_f = ((static_cast<float>(dy) + 0.5f) * static_cast<float>(src_h) / static_cast<float>(dst_h)) - 0.5f;
		int iy = static_cast<int>(floor(src_y_f));
		float frac_y = src_y_f - static_cast<float>(iy);   // in [0,1)

		for (int dx = 0; dx < dst_w; ++dx)
		{
			float src_x_f = ((static_cast<float>(dx) + 0.5f) * static_cast<float>(src_w) / static_cast<float>(dst_w)) - 0.5f;
			int ix = static_cast<int>(floor(src_x_f));
			float frac_x = src_x_f - static_cast<float>(ix);

			float sum_r = 0.0;
			float sum_g = 0.0;
			float sum_b = 0.0;
			float sum_a = 0.0;
			float wsum = 0.0;

			//int isedge = 0;

			// iterate over contributing source pixels
			for (int ky = -radius_y; ky <= radius_y; ++ky)
			{
				int sy = iy + ky;
				if (static_cast<unsigned>(sy) >= (unsigned)src_h) { /*isedge = 1; #1#continue; }

				// distance in Y between sample pos and that row
				float dy_dist = std::fabs(frac_y - static_cast<float>(ky)) / scale_y;
				if (dy_dist >= 2.5f) continue;
				float wy = magic_kernel_sharp_2013(dy_dist);

				for (int kx = -radius_x; kx <= radius_x; ++kx)
				{
					int sx = ix + kx;
					if (static_cast<unsigned>(sx) >= (unsigned)src_w) {/* isedge = 1;#1# continue; }

					float dx_dist = std::fabs(frac_x - static_cast<float>(kx)) / scale_x;
					if (dx_dist >= 2.5f) continue;
					float wx = magic_kernel_sharp_2013(dx_dist);

					float w = wx * wy;

					int pixel_index = (sy * src_w + sx) * pixelSize;

					sum_r += srgb_to_linear(src[pixel_index++]) * w;
					sum_g += srgb_to_linear(src[pixel_index++]) * w;
					sum_b += srgb_to_linear(src[pixel_index++]) * w;
					if constexpr (format == ImageFormat::RGBA8) {
					    sum_a += static_cast<float>(src[pixel_index]) / 255.0f * w;
					}
					wsum += w;
				}
			}

			int idx = (dy * dst_w + dx) * pixelSize;

			//if (!isedge) wsum = 1.0;			// for upscaling no normalization is needed

			if (wsum > 0.0)
			{
				// normalize
				wsum = 1.0f / wsum;
				dst[idx++] = linear_to_srgb(sum_r * wsum);
				dst[idx++] = linear_to_srgb(sum_g * wsum);
				dst[idx++] = linear_to_srgb(sum_b * wsum);
				if constexpr (format == ImageFormat::RGBA8)
				    dst[idx] = static_cast<uint8_t>(sum_a * wsum * 255.0f);
			}
			else
			{
				// no contributors
				dst[idx++] = dst[idx++] = dst[idx++] = 0;
				if constexpr (format == ImageFormat::RGBA8)
				    dst[idx] = 0;
			}
		}
	}
}

template<ImageFormat format>
void makeLevelKernelRgb8Rgba8(uint8_t* layer, uint32_t mipWidth, uint32_t mipHeight, uint8_t const* prevLayer, uint32_t prevWidth, uint32_t prevHeight) {
    constexpr size_t pixelSize = PixelSize(format);

    for (uint32_t y = 0; y < mipHeight; ++y) {
        for (uint32_t x = 0; x < mipWidth; ++x) {
            layer[(y * mipWidth + x) * pixelSize + 0] = mipmap_sample_srgb_aware<pixelSize>(prevLayer, prevWidth, prevHeight, x, y, 0);
            layer[(y * mipWidth + x) * pixelSize + 1] = mipmap_sample_srgb_aware<pixelSize>(prevLayer, prevWidth, prevHeight, x, y, 1);
            layer[(y * mipWidth + x) * pixelSize + 2] = mipmap_sample_srgb_aware<pixelSize>(prevLayer, prevWidth, prevHeight, x, y, 2);
            if constexpr (format == ImageFormat::RGBA8) {
                layer[(y * mipWidth + x) * pixelSize + 3] = mipmap_sample<pixelSize>(prevLayer, prevWidth, prevHeight, x, y, 3);
            }
        }
    }
}
*/

}

LoadedImageBuffer genMipmaps(LoadedImageBuffer buffer) {
    if (buffer.mipLevels != 1) {
        throw std::runtime_error("Image must only have 1 mip level!");
    }

    auto prevWidth = buffer.width;
    auto prevHeight = buffer.height;

    auto const* prevLayer = &buffer.data.at(0).at(0);

    while (true) {
        if (prevWidth == 1 && prevHeight == 1) {
            break;
        }

        auto const mipWidth = std::max(prevWidth / 2u, 1u);
        auto const mipHeight = std::max(prevHeight / 2u, 1u);

        std::vector<uint8_t> newLevel;
        newLevel.resize(PixelSize(buffer.format) * mipWidth * mipHeight);

        constexpr auto filter = STBIR_FILTER_BOX;

        switch (buffer.format) {
        case ImageFormat::RGBA8: {
            stbir_resize(
                prevLayer, prevWidth, prevHeight, prevWidth * PixelSize(buffer.format),
                newLevel.data(), mipWidth, mipHeight, mipWidth * PixelSize(buffer.format),
                STBIR_RGBA, STBIR_TYPE_UINT8_SRGB, STBIR_EDGE_CLAMP, filter);
            //makeLevelKernelRgb8Rgba8<ImageFormat::RGBA8>(newLevel.data(), mipWidth, mipHeight, prevLayer, prevWidth, prevHeight);
            break;
        }
        case ImageFormat::RGB8: {
            stbir_resize(
                prevLayer, prevWidth, prevHeight, prevWidth * PixelSize(buffer.format),
                newLevel.data(), mipWidth, mipHeight, mipWidth * PixelSize(buffer.format),
                STBIR_RGB, STBIR_TYPE_UINT8_SRGB, STBIR_EDGE_CLAMP, filter);
            //makeLevelKernelRgb8Rgba8<ImageFormat::RGB8>(newLevel.data(), mipWidth, mipHeight, prevLayer, prevWidth, prevHeight);
            break;
        }
        default:
            throw std::runtime_error("Unknown format!");
        }

        buffer.data.emplace_back(std::move(newLevel));
        buffer.mipLevels += 1;
        prevWidth = mipWidth;
        prevHeight = mipHeight;
        prevLayer = &buffer.data.back().at(0);
    }

    return buffer;
}

}
