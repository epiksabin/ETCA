#ifndef IMAGE_IO_H
#define IMAGE_IO_H

#include "color_data.h"
#include <string>
#include <memory>
#include <vector>

namespace etca {

/**
 * @brief Abstract base class for loading images from file formats
 */
class ImageLoader {
public:
    virtual ~ImageLoader() = default;
    
    /**
     * @brief Load image from file
     * @param file_path File path to load from
     * @return ColorData object containing the loaded image
     * @throws std::runtime_error if file cannot be read or parsed
     */
    virtual spectre::ColorData load(const std::string& file_path) const = 0;
};

/**
 * @brief PPM (Portable Pixmap) image loader
 * Supports P6 (binary) format with RGB 24-bit color
 */
class PPMLoader : public ImageLoader {
public:
    spectre::ColorData load(const std::string& file_path) const override;
};

/**
 * @brief PNG image loader using libpng
 */
class PNGLoader : public ImageLoader {
public:
    spectre::ColorData load(const std::string& file_path) const override;
};

/**
 * @brief Abstract base class for exporting images to file formats
 */
class ImageExporter {
public:
    virtual ~ImageExporter() = default;
    
    /**
     * @brief Save image to file
     * @param color_data ColorData object to save
     * @param file_path Output file path
     * @throws std::runtime_error if file cannot be written
     */
    virtual void save(const spectre::ColorData& color_data, const std::string& file_path) const = 0;
};

/**
 * @brief PPM (Portable Pixmap) image exporter
 * Exports to P6 (binary) format with RGB 24-bit color
 */
class PPMExporter : public ImageExporter {
public:
    void save(const spectre::ColorData& color_data, const std::string& file_path) const override;
};

/**
 * @brief PNG image exporter using libpng
 */
class PNGExporter : public ImageExporter {
public:
    void save(const spectre::ColorData& color_data, const std::string& file_path) const override;
};

/**
 * @brief Detect image format from file extension
 * @param file_path File path to check
 * @return Format string: "ppm", "png", or throws std::runtime_error for unsupported formats
 */
std::string detect_image_format(const std::string& file_path);

/**
 * @brief Load image from file with automatic format detection
 * @param file_path File path to load from
 * @return ColorData object containing the loaded image
 * @throws std::runtime_error if format is unsupported or file cannot be read
 */
spectre::ColorData load_image(const std::string& file_path);

/**
 * @brief Save image to file with automatic format detection
 * @param color_data ColorData object to save
 * @param file_path Output file path
 * @throws std::runtime_error if format is unsupported or file cannot be written
 */
void save_image(const spectre::ColorData& color_data, const std::string& file_path);

} // namespace etca

#endif // IMAGE_IO_H
