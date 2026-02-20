#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "spectre_tree.h"
#include "color_data.h"
#include "entropy_coding.h"
#include <vector>
#include <cstdint>

namespace spectre {

/**
 * @brief Configuration parameters for compression
 */
struct CompressionConfig {
    double variance_threshold = 0.05;  // Range: 0.0-1.0 (lower = more detail, more tiles)
    int max_tree_depth = 12;  // Allow deeper trees for detail with index-based compression
    bool enable_mipmap = true;
    bool prefer_speed = false;  // If true, skip slower entropy codecs
    bool use_adaptive_encoding = true;  // Use best-fit codec selection
    
    CompressionConfig() = default;
};

/**
 * @brief Compressed representation of an image
 */
struct CompressedImage {
    uint32_t width, height;
    std::vector<uint8_t> data;  // Compressed data
    CompressionConfig config;
};

/**
 * @brief Main compression engine for the Spectre Tiles algorithm
 * 
 * Process:
 * 1. Build a Spectre-Tree from the input image
 * 2. Store tile hierarchy and color data
 * 3. Apply entropy coding to reduce size
 */
class Compressor {
public:
    /**
     * @brief Constructor for the compressor
     * @param config Compression configuration parameters
     */
    explicit Compressor(const CompressionConfig& config = CompressionConfig());
    
    /**
     * @brief Compress an image using the Spectre Tiles algorithm
     * @param image The input image data
     * @return A compressed representation of the image
     */
    CompressedImage compress(const ColorData& image);
    
    /**
     * @brief Get compression statistics (tree size, depth, etc.)
     */
    struct Statistics {
        size_t tile_count;
        int max_depth;
        uint32_t leaf_count;
        double compression_ratio;  // Original size / Compressed size
    };
    
    /**
     * @brief Get statistics from the last compression
     */
    const Statistics& get_last_statistics() const { return last_stats_; }
    
    /**
     * @brief Get entropy coding statistics from last compression
     */
    const CompressionStats& get_entropy_statistics() const { return entropy_stats_; }

private:
    CompressionConfig config_;
    Statistics last_stats_;
    CompressionStats entropy_stats_;
    
    /**
     * @brief Serialize the tree into a byte stream
     */
    void serialize_tree(
        const SpectreTree& tree,
        const ColorData& image,
        std::vector<uint8_t>& output
    );
    
    /**
     * @brief Apply entropy coding to reduce further
     */
    void apply_entropy_coding(std::vector<uint8_t>& data);
};

} // namespace spectre

#endif // COMPRESSOR_H
