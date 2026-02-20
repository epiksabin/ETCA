#include "color_data.h"
#include "image_io.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace spectre {

ColorData::ColorData(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
    pixels_.resize(width_ * height_, Color(0, 0, 0));
}

ColorData::ColorData(const std::string& file_path) {
    auto image = etca::load_image(file_path);
    width_ = image.get_width();
    height_ = image.get_height();
    pixels_ = image.get_pixels();
}

void ColorData::set_pixel(uint32_t x, uint32_t y, const Color& color) {
    if (x < width_ && y < height_) {
        pixels_[xy_to_index(x, y)] = color;
    }
}

Color ColorData::get_pixel(uint32_t x, uint32_t y) const {
    if (x < width_ && y < height_) {
        return pixels_[xy_to_index(x, y)];
    }
    return Color(0, 0, 0);
}

ColorData ColorData::extract_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const {
    ColorData region(width, height);
    
    for (uint32_t row = 0; row < height; ++row) {
        for (uint32_t col = 0; col < width; ++col) {
            uint32_t src_x = x + col;
            uint32_t src_y = y + row;
            
            if (src_x < width_ && src_y < height_) {
                region.set_pixel(col, row, get_pixel(src_x, src_y));
            }
        }
    }
    
    return region;
}

Color ColorData::calculate_average_color() const {
    if (pixels_.empty()) {
        return Color(0, 0, 0);
    }
    
    uint64_t sum_r = 0, sum_g = 0, sum_b = 0;
    
    for (const auto& pixel : pixels_) {
        sum_r += pixel.r;
        sum_g += pixel.g;
        sum_b += pixel.b;
    }
    
    size_t count = pixels_.size();
    return Color(
        static_cast<uint8_t>(sum_r / count),
        static_cast<uint8_t>(sum_g / count),
        static_cast<uint8_t>(sum_b / count)
    );
}

void ColorData::fill(const Color& color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void ColorData::save_to_file(const std::string& file_path) const {
    etca::save_image(*this, file_path);
}

} // namespace spectre
