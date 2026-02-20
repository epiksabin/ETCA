#ifndef ETCA_FORMAT_H
#define ETCA_FORMAT_H

#include "color_data.h"
#include "compressor.h"
#include <string>
#include <vector>
#include <map>

namespace etca {

/**
 * @brief Compression mode for .etca files
 */
enum class CompressionMode : uint8_t {
    LOSSY = 0x00,       // Lossy compression using variance-based tile subdivision
    LOSSLESS = 0x01     // Lossless compression with full-depth tree
};

/**
 * @brief .etca file header (20 bytes fixed)
 * 
 * Layout:
 *   Offset  Size    Field
 *   0       4       Magic bytes: "ETCA"
 *   4       1       Format version (0x01 for v1)
 *   5       1       Compression mode (0x00 = lossy, 0x01 = lossless)
 *   6       4       Original image width (uint32 big-endian)
 *   10      4       Original image height (uint32 big-endian)
 *   14      1       Color depth (0x18 = 24-bit RGB)
 *   15      4       Metadata section size in bytes (uint32 big-endian, 0 if no metadata)
 *   19      1       Reserved for future use
 */
struct EtcaHeader {
    static constexpr uint32_t MAGIC = 0x45544341;  // "ETCA" in big-endian
    static constexpr uint8_t VERSION = 0x01;
    static constexpr uint8_t COLOR_DEPTH_RGB24 = 0x18;
    static constexpr size_t HEADER_SIZE = 20;
    
    uint8_t format_version = VERSION;
    CompressionMode compression_mode = CompressionMode::LOSSY;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t color_depth = COLOR_DEPTH_RGB24;
    uint32_t metadata_size = 0;  // Size of metadata section (0 if none)
    
    /**
     * @brief Serialize header to binary format
     * @return Binary representation of header (20 bytes)
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize header from binary data
     * @param data Binary data at least HEADER_SIZE bytes
     * @return EtcaHeader object
     * @throws std::runtime_error if data is invalid or magic bytes don't match
     */
    static EtcaHeader deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief Metadata for .etca files (optional)
 * Key-value pairs: author, creation_date, original_format, quality_level, etc.
 */
class EtcaMetadata {
private:
    std::map<std::string, std::string> data_;
    
public:
    /**
     * @brief Set metadata key-value pair
     */
    void set(const std::string& key, const std::string& value);
    
    /**
     * @brief Get metadata value by key
     * @return Value associated with key, or empty string if not found
     */
    std::string get(const std::string& key) const;
    
    /**
     * @brief Check if key exists
     */
    bool has(const std::string& key) const;
    
    /**
     * @brief Serialize metadata to binary format (key=value pairs, newline-separated)
     * @return Binary representation
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * @brief Deserialize metadata from binary data
     * @param data Binary data containing key=value pairs
     * @return EtcaMetadata object
     */
    static EtcaMetadata deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief Complete .etca file with header, metadata, and compressed image data
 */
struct EtcaFile {
    EtcaHeader header;
    EtcaMetadata metadata;
    std::vector<uint8_t> compressed_data;  // Serialized SpectreTree + RLE-encoded payload
};

/**
 * @brief Writes images to .etca format
 */
class EtcaWriter {
public:
    /**
     * @brief Compress image and write to .etca file
     * @param image The image to compress
     * @param output_path Output file path
     * @param lossless If true, use lossless compression; otherwise use lossy
     * @param variance_threshold For lossy mode: only subdivide tiles with variance > threshold
     * @param max_depth Maximum tree depth (0 = auto-determine for lossless)
     * @throws std::runtime_error if file cannot be written
     */
    static void write(
        const spectre::ColorData& image,
        const std::string& output_path,
        bool lossless = false,
        float variance_threshold = 10.0f,
        uint16_t max_depth = 0
    );
    
    /**
     * @brief Write image file to .etca format with metadata
     * @param input_file Input image file (PPM or PNG)
     * @param output_path Output .etca file path
     * @param lossless If true, use lossless compression
     * @param variance_threshold For lossy mode: subdivision threshold
     * @param metadata Additional metadata to store (optional)
     * @throws std::runtime_error if file cannot be read/written
     */
    static void write_from_file(
        const std::string& input_file,
        const std::string& output_path,
        bool lossless = false,
        float variance_threshold = 10.0f,
        const EtcaMetadata& metadata = EtcaMetadata()
    );
};

/**
 * @brief Reads images from .etca format
 */
class EtcaReader {
public:
    /**
     * @brief Read and decompress .etca file
     * @param input_path Input .etca file path
     * @return ColorData object with decompressed image
     * @throws std::runtime_error if file cannot be read or is corrupt
     */
    static spectre::ColorData read(const std::string& input_path);
    
    /**
     * @brief Read .etca file and export to image format
     * @param input_path Input .etca file path
     * @param output_file Output image file path (PPM or PNG)
     * @throws std::runtime_error if file cannot be read/written
     */
    static void read_to_file(const std::string& input_path, const std::string& output_file);
    
    /**
     * @brief Read .etca header and metadata without decompressing
     * @param input_path Input .etca file path
     * @return EtcaFile structure with header and metadata filled
     * @throws std::runtime_error if file cannot be read or is corrupt
     */
    static EtcaFile read_header_and_metadata(const std::string& input_path);
};

} // namespace etca

#endif // ETCA_FORMAT_H
