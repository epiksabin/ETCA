#ifndef DECOMPRESSOR_H
#define DECOMPRESSOR_H

#include "color_data.h"
#include "compressor.h"
#include "entropy_coding.h"
#include <vector>
#include <cstdint>
#include <memory>

namespace spectre {

/**
 * @brief Main decompression engine
 * 
 * Reconstructs an image from compressed Spectre Tiles representation.
 * 
 * Process:
 * 1. Deserialize the tile tree structure
 * 2. Reconstruct the hierarchical tile arrangement
 * 3. Paint each tile with its average color
 * 4. Optionally apply interpolation for smooth gradients
 */
class Decompressor {
public:
    /**
     * @brief Decompress an image
     * @param compressed The compressed image data
     * @return The reconstructed image
     */
    static ColorData decompress(const CompressedImage& compressed);
    
    /**
     * @brief Decompress with quality options
     * @param compressed The compressed image data
     * @param apply_interpolation Apply interpolation between tiles
     * @param max_depth Limit decompression depth (for LOD)
     */
    static ColorData decompress(
        const CompressedImage& compressed,
        bool apply_interpolation,
        int max_depth = -1
    );

private:
    /**
     * @brief Deserialize tree from compressed data
     */
    static std::unique_ptr<SpectreTree> deserialize_tree(
        const std::vector<uint8_t>& data,
        uint32_t width,
        uint32_t height
    );
    
    /**
     * @brief Reconstruct image from tree
     */
    static ColorData reconstruct_image(
        const SpectreTree& tree,
        bool apply_interpolation
    );
    
    /**
     * @brief Apply bilinear or other interpolation between tile boundaries
     */
    static void apply_interpolation(ColorData& image);
};

} // namespace spectre

#endif // DECOMPRESSOR_H
