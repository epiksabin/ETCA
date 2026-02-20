#include "etca_format.h"
#include "decompressor.h"
#include "image_io.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace etca {

// ============================================================================
// EtcaHeader Implementation
// ============================================================================

std::vector<uint8_t> EtcaHeader::serialize() const {
    std::vector<uint8_t> data(HEADER_SIZE, 0);
    
    // Magic bytes
    data[0] = (MAGIC >> 24) & 0xFF;
    data[1] = (MAGIC >> 16) & 0xFF;
    data[2] = (MAGIC >> 8) & 0xFF;
    data[3] = MAGIC & 0xFF;
    
    // Format version
    data[4] = format_version;
    
    // Compression mode
    data[5] = static_cast<uint8_t>(compression_mode);
    
    // Width (big-endian)
    data[6] = (width >> 24) & 0xFF;
    data[7] = (width >> 16) & 0xFF;
    data[8] = (width >> 8) & 0xFF;
    data[9] = width & 0xFF;
    
    // Height (big-endian)
    data[10] = (height >> 24) & 0xFF;
    data[11] = (height >> 16) & 0xFF;
    data[12] = (height >> 8) & 0xFF;
    data[13] = height & 0xFF;
    
    // Color depth
    data[14] = color_depth;
    
    // Metadata section size (big-endian)
    data[15] = (metadata_size >> 24) & 0xFF;
    data[16] = (metadata_size >> 16) & 0xFF;
    data[17] = (metadata_size >> 8) & 0xFF;
    data[18] = metadata_size & 0xFF;
    
    // Reserved
    data[19] = 0x00;
    
    return data;
}

EtcaHeader EtcaHeader::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < HEADER_SIZE) {
        throw std::runtime_error("Invalid .etca file: header too small");
    }
    
    // Check magic bytes
    uint32_t magic = (static_cast<uint32_t>(data[0]) << 24) |
                     (static_cast<uint32_t>(data[1]) << 16) |
                     (static_cast<uint32_t>(data[2]) << 8) |
                     static_cast<uint32_t>(data[3]);
    
    if (magic != MAGIC) {
        throw std::runtime_error("Invalid .etca file: magic bytes do not match");
    }
    
    EtcaHeader header;
    
    // Format version
    header.format_version = data[4];
    if (header.format_version != VERSION) {
        throw std::runtime_error("Unsupported .etca format version: " + 
                                 std::to_string(header.format_version));
    }
    
    // Compression mode
    header.compression_mode = static_cast<CompressionMode>(data[5]);
    
    // Width (big-endian)
    header.width = (static_cast<uint32_t>(data[6]) << 24) |
                   (static_cast<uint32_t>(data[7]) << 16) |
                   (static_cast<uint32_t>(data[8]) << 8) |
                   static_cast<uint32_t>(data[9]);
    
    // Height (big-endian)
    header.height = (static_cast<uint32_t>(data[10]) << 24) |
                    (static_cast<uint32_t>(data[11]) << 16) |
                    (static_cast<uint32_t>(data[12]) << 8) |
                    static_cast<uint32_t>(data[13]);
    
    // Color depth
    header.color_depth = data[14];
    
    // Metadata section size (big-endian)
    header.metadata_size = (static_cast<uint32_t>(data[15]) << 24) |
                           (static_cast<uint32_t>(data[16]) << 16) |
                           (static_cast<uint32_t>(data[17]) << 8) |
                           static_cast<uint32_t>(data[18]);
    
    if (header.width == 0 || header.height == 0) {
        throw std::runtime_error("Invalid image dimensions in .etca file: " +
                                 std::to_string(header.width) + "x" +
                                 std::to_string(header.height));
    }
    
    return header;
}

// ============================================================================
// EtcaMetadata Implementation
// ============================================================================

void EtcaMetadata::set(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::string EtcaMetadata::get(const std::string& key) const {
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return "";
}

bool EtcaMetadata::has(const std::string& key) const {
    return data_.find(key) != data_.end();
}

std::vector<uint8_t> EtcaMetadata::serialize() const {
    std::ostringstream oss;
    for (const auto& pair : data_) {
        oss << pair.first << "=" << pair.second << "\n";
    }
    
    std::string str = oss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

EtcaMetadata EtcaMetadata::deserialize(const std::vector<uint8_t>& data) {
    EtcaMetadata metadata;
    
    std::string str(data.begin(), data.end());
    std::istringstream iss(str);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            metadata.set(key, value);
        }
    }
    
    return metadata;
}

// ============================================================================
// EtcaWriter Implementation
// ============================================================================

void EtcaWriter::write(
    const spectre::ColorData& image,
    const std::string& output_path,
    bool lossless,
    float variance_threshold,
    uint16_t max_depth) {
    
    // Create compression configuration
    spectre::CompressionConfig config;
    config.variance_threshold = variance_threshold / 255.0f;  // Normalize to 0.0-1.0
    
    if (max_depth > 0) {
        config.max_tree_depth = max_depth;
    } else if (lossless) {
        // For lossless, use a very high depth to capture all details
        config.max_tree_depth = 32;
    }
    
    // Create and use compressor
    spectre::Compressor compressor(config);
    spectre::CompressedImage compressed = compressor.compress(image);
    
    // Create header
    EtcaHeader header;
    header.format_version = EtcaHeader::VERSION;
    header.compression_mode = lossless ? CompressionMode::LOSSLESS : CompressionMode::LOSSY;
    header.width = image.get_width();
    header.height = image.get_height();
    header.color_depth = EtcaHeader::COLOR_DEPTH_RGB24;
    header.metadata_size = 0;  // No metadata for now
    
    // Serialize and write to file
    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    }
    
    // Write header
    auto header_bytes = header.serialize();
    file.write(reinterpret_cast<const char*>(header_bytes.data()), static_cast<std::streamsize>(header_bytes.size()));
    
    // Write compressed data
    file.write(reinterpret_cast<const char*>(compressed.data.data()), static_cast<std::streamsize>(compressed.data.size()));
    
    if (!file.good()) {
        throw std::runtime_error("Failed to write .etca file: " + output_path);
    }
}

void EtcaWriter::write_from_file(
    const std::string& input_file,
    const std::string& output_path,
    bool lossless,
    float variance_threshold,
    const EtcaMetadata& metadata) {
    
    // Load image from file
    spectre::ColorData image(input_file);
    
    // Create compression configuration
    spectre::CompressionConfig config;
    
    if (lossless) {
        // For lossless, use extremely low variance threshold to capture all details
        config.variance_threshold = 0.001;  // Very aggressive subdivision for perfect reconstruction
        config.max_tree_depth = 24;  // Allow very deep trees for detail
    } else {
        // For lossy, use the quality parameter to determine variance threshold
        config.variance_threshold = variance_threshold / 255.0f;  // Normalize to 0.0-1.0
        config.max_tree_depth = 12;
    }
    
    // Create and use compressor
    spectre::Compressor compressor(config);
    spectre::CompressedImage compressed = compressor.compress(image);
    
    // Create header
    EtcaHeader header;
    header.format_version = EtcaHeader::VERSION;
    header.compression_mode = lossless ? CompressionMode::LOSSLESS : CompressionMode::LOSSY;
    header.width = image.get_width();
    header.height = image.get_height();
    header.color_depth = EtcaHeader::COLOR_DEPTH_RGB24;
    
    // Serialize metadata
    auto metadata_bytes = const_cast<EtcaMetadata&>(metadata).serialize();
    header.metadata_size = static_cast<uint32_t>(metadata_bytes.size());
    
    // Serialize and write to file
    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + output_path);
    }
    
    // Write header
    auto header_bytes = header.serialize();
    file.write(reinterpret_cast<const char*>(header_bytes.data()), static_cast<std::streamsize>(header_bytes.size()));
    
    // Write metadata
    if (header.metadata_size > 0) {
        file.write(reinterpret_cast<const char*>(metadata_bytes.data()), static_cast<std::streamsize>(metadata_bytes.size()));
    }
    
    // Write compressed data
    file.write(reinterpret_cast<const char*>(compressed.data.data()), static_cast<std::streamsize>(compressed.data.size()));
    
    if (!file.good()) {
        throw std::runtime_error("Failed to write .etca file: " + output_path);
    }
}

// ============================================================================
// EtcaReader Implementation
// ============================================================================

spectre::ColorData EtcaReader::read(const std::string& input_path) {
    std::ifstream file(input_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + input_path);
    }
    
    // Read header
    std::vector<uint8_t> header_bytes(EtcaHeader::HEADER_SIZE);
    file.read(reinterpret_cast<char*>(header_bytes.data()), EtcaHeader::HEADER_SIZE);
    if (file.gcount() != EtcaHeader::HEADER_SIZE) {
        throw std::runtime_error("Failed to read .etca header");
    }
    
    EtcaHeader header = EtcaHeader::deserialize(header_bytes);
    
    // Skip metadata if present
    if (header.metadata_size > 0) {
        file.ignore(header.metadata_size);
    }
    
    // Read remaining compressed data
    std::vector<uint8_t> compressed_data;
    char byte;
    while (file.read(&byte, 1)) {
        compressed_data.push_back(static_cast<uint8_t>(byte));
    }
    
    // Decompress image
    spectre::CompressedImage compressed;
    compressed.width = header.width;
    compressed.height = header.height;
    compressed.data = compressed_data;
    
    return spectre::Decompressor::decompress(compressed);
}

void EtcaReader::read_to_file(const std::string& input_path, const std::string& output_file) {
    spectre::ColorData image = read(input_path);
    image.save_to_file(output_file);
}

EtcaFile EtcaReader::read_header_and_metadata(const std::string& input_path) {
    std::ifstream file(input_path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + input_path);
    }
    
    // Read header
    std::vector<uint8_t> header_bytes(EtcaHeader::HEADER_SIZE);
    file.read(reinterpret_cast<char*>(header_bytes.data()), EtcaHeader::HEADER_SIZE);
    if (file.gcount() != EtcaHeader::HEADER_SIZE) {
        throw std::runtime_error("Failed to read .etca header");
    }
    
    EtcaFile etca_file;
    etca_file.header = EtcaHeader::deserialize(header_bytes);
    
    // Read metadata if present
    if (etca_file.header.metadata_size > 0) {
        std::vector<uint8_t> metadata_bytes(etca_file.header.metadata_size);
        file.read(reinterpret_cast<char*>(metadata_bytes.data()), etca_file.header.metadata_size);
        if (file.gcount() != static_cast<std::streamsize>(etca_file.header.metadata_size)) {
            throw std::runtime_error("Failed to read .etca metadata");
        }
        etca_file.metadata = EtcaMetadata::deserialize(metadata_bytes);
    }
    
    return etca_file;
}

} // namespace etca
