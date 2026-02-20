#include "compressor.h"
#include <algorithm>

namespace spectre {

Compressor::Compressor(const CompressionConfig& config)
    : config_(config), last_stats_{0, 0, 0, 0.0} {
}

CompressedImage Compressor::compress(const ColorData& image) {
    CompressedImage result;
    result.width = image.get_width();
    result.height = image.get_height();
    result.config = config_;
    
    // Build the Spectre-Tree
    SpectreTree tree(image.get_width(), image.get_height());
    tree.build(image, config_.variance_threshold, config_.max_tree_depth);
    
    // Record statistics
    last_stats_.tile_count = tree.get_tile_count();
    last_stats_.max_depth = tree.get_max_depth();
    last_stats_.leaf_count = static_cast<uint32_t>(tree.get_leaf_nodes().size());
    
    // Estimate compression ratio
    // Original: width * height * 3 bytes (RGB)
    // Compressed: tree structure + tile colors + metadata
    size_t original_size = image.get_width() * image.get_height() * 3;
    size_t estimated_compressed_size = tree.get_tile_count() * 10;  // ~10 bytes per tile
    last_stats_.compression_ratio = static_cast<double>(original_size) / 
                                    static_cast<double>(std::max(size_t(1), estimated_compressed_size));
    
    // Serialize the tree
    serialize_tree(tree, image, result.data);
    
    // Apply entropy coding
    apply_entropy_coding(result.data);
    
    return result;
}

void Compressor::serialize_tree(
    const SpectreTree& tree,
    const ColorData& image,
    std::vector<uint8_t>& output) {
    
    // Simple serialization format:
    // [Header: width(4) | height(4) | tile_count(4) | depth(2)]
    // [Tile Data: id(4) | depth(2) | parent_id(4) | r(1) | g(1) | b(1) | child_count(1) | child_ids...]
    
    output.clear();
    
    // Write header
    uint32_t width = image.get_width();
    uint32_t height = image.get_height();
    uint32_t tile_count = static_cast<uint32_t>(tree.get_tile_count());
    uint16_t max_depth = static_cast<uint16_t>(tree.get_max_depth());
    
    output.push_back((width >> 24) & 0xFF);
    output.push_back((width >> 16) & 0xFF);
    output.push_back((width >> 8) & 0xFF);
    output.push_back(width & 0xFF);
    
    output.push_back((height >> 24) & 0xFF);
    output.push_back((height >> 16) & 0xFF);
    output.push_back((height >> 8) & 0xFF);
    output.push_back(height & 0xFF);
    
    output.push_back((tile_count >> 24) & 0xFF);
    output.push_back((tile_count >> 16) & 0xFF);
    output.push_back((tile_count >> 8) & 0xFF);
    output.push_back(tile_count & 0xFF);
    
    output.push_back((max_depth >> 8) & 0xFF);
    output.push_back(max_depth & 0xFF);
    
    // Serialize individual tiles using indexed format for efficiency
    // Format for each tile: index(2) | depth(1) | parent_index(2) | r(1) | g(1) | b(1) | child_count(1) | [child_index(2)]...
    // This reduces overhead from 22+ bytes to ~14 bytes per tile
    auto all_tiles = tree.get_all_tiles();
    std::map<SpectreTile::ID, uint16_t> id_to_index;
    
    // First pass: create index mapping
    uint16_t index = 0;
    for (SpectreTile::ID tile_id : all_tiles) {
        id_to_index[tile_id] = index++;
    }
    
    // Second pass: serialize tiles using indices
    for (SpectreTile::ID tile_id : all_tiles) {
        const SpectreTile* tile_ptr = tree.get_tile(tile_id);
        if (!tile_ptr) continue;
        
        // Write tile index (uint16_t) - 2 bytes instead of 8
        uint16_t tile_index = id_to_index[tile_id];
        output.push_back((tile_index >> 8) & 0xFF);
        output.push_back(tile_index & 0xFF);
        
        // Write depth (uint8_t) - 1 byte instead of 2 (max 255 levels)
        int depth = tile_ptr->get_depth();
        output.push_back(static_cast<uint8_t>(depth & 0xFF));
        
        // Write parent index (uint16_t) - 2 bytes instead of 8
        SpectreTile::ID parent_id = tile_ptr->get_parent_id();
        uint16_t parent_index = (parent_id > 0 && id_to_index.count(parent_id)) 
                               ? id_to_index[parent_id] 
                               : 0xFFFF;  // 0xFFFF means no parent (root)
        output.push_back((parent_index >> 8) & 0xFF);
        output.push_back(parent_index & 0xFF);
        
        // Write color (3 bytes: r, g, b)
        uint8_t r, g, b;
        tile_ptr->get_color(r, g, b);
        output.push_back(r);
        output.push_back(g);
        output.push_back(b);
        
        // Write child count and child IDs
        const auto& children = tile_ptr->get_children();
        uint8_t child_count = static_cast<uint8_t>(children.size());
        output.push_back(child_count);
        
        for (SpectreTile::ID child_id : children) {
            // Write child index (uint16_t) - 2 bytes instead of 8
            uint16_t child_index = id_to_index.count(child_id) ? id_to_index[child_id] : 0xFFFF;
            output.push_back((child_index >> 8) & 0xFF);
            output.push_back(child_index & 0xFF);
        }
    }
}

void Compressor::apply_entropy_coding(std::vector<uint8_t>& data) {
    // Use the new adaptive entropy encoder to select the best compression strategy
    if (data.empty()) {
        return;
    }
    
    // Apply adaptive encoding that tries multiple codecs and picks the best one
    data = AdaptiveEncoder::encode(data, config_.prefer_speed);
    
    // Store entropy statistics
    entropy_stats_ = AdaptiveEncoder::get_stats();
}

} // namespace spectre
