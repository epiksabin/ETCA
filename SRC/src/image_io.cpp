#include "image_io.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <png.h>

namespace etca {

// ============================================================================
// PPM Loader Implementation
// ============================================================================

spectre::ColorData PPMLoader::load(const std::string& file_path) const {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }
    
    // Read magic number (P6 for binary PPM)
    std::string magic;
    file >> magic;
    if (magic != "P6") {
        throw std::runtime_error("Not a valid binary PPM file (P6 format expected): " + file_path);
    }
    
    // Skip comments
    int c;
    while ((c = file.peek()) == '#') {
        std::string line;
        std::getline(file, line);  // Skip comment line
    }
    
    // Read width and height
    uint32_t width, height;
    file >> width >> height;
    
    if (width == 0 || height == 0) {
        throw std::runtime_error("Invalid PPM dimensions: " + std::to_string(width) + "x" + std::to_string(height));
    }
    
    // Read max color value
    int max_color;
    file >> max_color;
    if (max_color != 255) {
        throw std::runtime_error("PPM max color value must be 255, got: " + std::to_string(max_color));
    }
    
    // Skip whitespace after max color value
    file.ignore(1);
    
    // Read pixel data
    spectre::ColorData image(width, height);
    std::vector<uint8_t> buffer(width * height * 3);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    
    if (file.gcount() != static_cast<std::streamsize>(buffer.size())) {
        throw std::runtime_error("Failed to read complete pixel data from PPM file");
    }
    
    // Fill image data (row-major order)
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 3;
            spectre::Color color(buffer[idx], buffer[idx + 1], buffer[idx + 2]);
            image.set_pixel(x, y, color);
        }
    }
    
    return image;
}

// ============================================================================
// PPM Exporter Implementation
// ============================================================================

void PPMExporter::save(const spectre::ColorData& color_data, const std::string& file_path) const {
    std::ofstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + file_path);
    }
    
    uint32_t width = color_data.get_width();
    uint32_t height = color_data.get_height();
    
    // Write PPM header
    file << "P6\n";
    file << width << " " << height << "\n";
    file << "255\n";
    
    // Write pixel data
    const auto& pixels = color_data.get_pixels();
    for (const auto& pixel : pixels) {
        file.put(static_cast<char>(pixel.r));
        file.put(static_cast<char>(pixel.g));
        file.put(static_cast<char>(pixel.b));
    }
    
    if (!file.good()) {
        throw std::runtime_error("Failed to write PPM file: " + file_path);
    }
}

// ============================================================================
// PNG Loader Implementation (using libpng)
// ============================================================================

spectre::ColorData PNGLoader::load(const std::string& file_path) const {
    FILE* fp = fopen(file_path.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Cannot open PNG file: " + file_path);
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        fclose(fp);
        throw std::runtime_error("Failed to create PNG read structure");
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        fclose(fp);
        throw std::runtime_error("Failed to create PNG info structure");
    }
    
    // Set error handling using setjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);
        throw std::runtime_error("Error reading PNG file: " + file_path);
    }
    
    // Set up file I/O
    png_init_io(png_ptr, fp);
    
    // Read PNG info
    png_read_info(png_ptr, info_ptr);
    
    uint32_t width = png_get_image_width(png_ptr, info_ptr);
    uint32_t height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    
    if (width == 0 || height == 0) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        fclose(fp);
        throw std::runtime_error("Invalid PNG dimensions: " + std::to_string(width) + "x" + std::to_string(height));
    }
    
    // Convert to RGB if necessary
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    
    // Convert grayscale to RGB
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    
    // Add alpha channel if missing (but we'll ignore it)
    if (color_type == PNG_COLOR_TYPE_RGB ||
        color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }
    
    png_read_update_info(png_ptr, info_ptr);
    
    // Create image data
    spectre::ColorData image(width, height);
    
    // Allocate row pointers
    std::vector<png_bytep> row_pointers(height);
    std::vector<uint8_t> image_data(width * height * 4);  // RGBA
    
    for (uint32_t y = 0; y < height; ++y) {
        row_pointers[y] = &image_data[y * width * 4];
    }
    
    // Read PNG data
    png_read_image(png_ptr, row_pointers.data());
    
    // Copy RGB data to ColorData (ignoring alpha channel)
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = (y * width + x) * 4;
            spectre::Color color(image_data[idx], image_data[idx + 1], image_data[idx + 2]);
            image.set_pixel(x, y, color);
        }
    }
    
    // Cleanup
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    fclose(fp);
    
    return image;
}

// ============================================================================
// PNG Exporter Implementation (using libpng)
// ============================================================================

void PNGExporter::save(const spectre::ColorData& color_data, const std::string& file_path) const {
    uint32_t width = color_data.get_width();
    uint32_t height = color_data.get_height();
    
    FILE* fp = fopen(file_path.c_str(), "wb");
    if (!fp) {
        throw std::runtime_error("Cannot write to PNG file: " + file_path);
    }
    
    // Initialize PNG structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        fclose(fp);
        throw std::runtime_error("Failed to create PNG write structure");
    }
    
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        throw std::runtime_error("Failed to create PNG info structure");
    }
    
    // Set error handling using setjmp
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        throw std::runtime_error("Error writing PNG file: " + file_path);
    }
    
    // Set up file I/O
    png_init_io(png_ptr, fp);
    
    // Set PNG image info
    png_set_IHDR(
        png_ptr,
        info_ptr,
        width, height,
        8,  // bit depth
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    
    // Write PNG info
    png_write_info(png_ptr, info_ptr);
    
    // Prepare pixel data
    const auto& pixels = color_data.get_pixels();
    std::vector<png_bytep> row_pointers(height);
    std::vector<uint8_t> image_data(width * height * 3);  // RGB
    
    // Copy ColorData pixels to RGB format
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t src_idx = y * width + x;
            size_t dst_idx = (y * width + x) * 3;
            
            image_data[dst_idx] = pixels[src_idx].r;
            image_data[dst_idx + 1] = pixels[src_idx].g;
            image_data[dst_idx + 2] = pixels[src_idx].b;
        }
        row_pointers[y] = &image_data[y * width * 3];
    }
    
    // Write PNG data
    png_write_image(png_ptr, row_pointers.data());
    png_write_end(png_ptr, info_ptr);
    
    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string detect_image_format(const std::string& file_path) {
    // Convert to lowercase for case-insensitive comparison
    std::string lower_path = file_path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (lower_path.size() >= 4) {
        std::string ext = lower_path.substr(lower_path.size() - 4);
        if (ext == ".ppm") return "ppm";
        
        if (lower_path.size() >= 4) {
            ext = lower_path.substr(lower_path.size() - 4);
            if (ext == ".png") return "png";
        }
    }
    
    throw std::runtime_error("Unsupported image format: " + file_path + 
                             " (supported: .ppm, .png)");
}

spectre::ColorData load_image(const std::string& file_path) {
    std::string format = detect_image_format(file_path);
    
    if (format == "ppm") {
        PPMLoader loader;
        return loader.load(file_path);
    } else if (format == "png") {
        PNGLoader loader;
        return loader.load(file_path);
    }
    
    throw std::runtime_error("Unsupported image format: " + format);
}

void save_image(const spectre::ColorData& color_data, const std::string& file_path) {
    std::string format = detect_image_format(file_path);
    
    if (format == "ppm") {
        PPMExporter exporter;
        exporter.save(color_data, file_path);
    } else if (format == "png") {
        PNGExporter exporter;
        exporter.save(color_data, file_path);
    } else {
        throw std::runtime_error("Unsupported image format: " + format);
    }
}

} // namespace etca
