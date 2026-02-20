#ifndef COLOR_DATA_H
#define COLOR_DATA_H

#include <cstdint>
#include <vector>
#include <string>

namespace spectre {

/**
 * @brief Represents a color value (RGB)
 */
struct Color {
    uint8_t r, g, b;
    
    Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0) : r(r), g(g), b(b) {}
};

/**
 * @brief Manages image data and provides analysis methods
 */
class ColorData {
public:
    /**
     * @brief Constructor for image data
     * @param width Image width in pixels
     * @param height Image height in pixels
     */
    ColorData(uint32_t width, uint32_t height);
    
    /**
     * @brief Constructor that loads image from file (PPM or PNG)
     * @param file_path Path to image file
     * @throws std::runtime_error if file cannot be read or format is unsupported
     */
    explicit ColorData(const std::string& file_path);
    
    /**
     * @brief Get image width
     */
    uint32_t get_width() const { return width_; }
    
    /**
     * @brief Get image height
     */
    uint32_t get_height() const { return height_; }
    
    /**
     * @brief Set a pixel color
     * @param x X coordinate
     * @param y Y coordinate
     * @param color Color value
     */
    void set_pixel(uint32_t x, uint32_t y, const Color& color);
    
    /**
     * @brief Get a pixel color
     * @param x X coordinate
     * @param y Y coordinate
     */
    Color get_pixel(uint32_t x, uint32_t y) const;
    
    /**
     * @brief Get all pixels as a flat array
     */
    const std::vector<Color>& get_pixels() const { return pixels_; }
    
    /**
     * @brief Extract a sub-region of the image
     * @param x Starting X coordinate
     * @param y Starting Y coordinate
     * @param width Region width
     * @param height Region height
     */
    ColorData extract_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height) const;
    
    /**
     * @brief Calculate the average color over the entire image
     */
    Color calculate_average_color() const;
    
    /**
     * @brief Fill the entire image with a single color
     */
    void fill(const Color& color);
    
    /**
     * @brief Save image to file (PPM or PNG)
     * @param file_path Output file path
     * @throws std::runtime_error if file cannot be written or format is unsupported
     */
    void save_to_file(const std::string& file_path) const;

private:
    uint32_t width_, height_;
    std::vector<Color> pixels_;
    
    /**
     * @brief Helper to convert (x,y) to linear index
     */
    size_t xy_to_index(uint32_t x, uint32_t y) const {
        return y * width_ + x;
    }
};

} // namespace spectre

#endif // COLOR_DATA_H
