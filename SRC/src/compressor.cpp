// compressor.cpp
#include "compressor.h"
#include <algorithm>
#include <unordered_map> // Replaced std::map with std::unordered_map for O(1) lookups
#include <mutex> // Included for potential thread-safety fix

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
    
    // Record initial statistics
    last_stats_.max_depth = tree.get_max_depth();
    last_stats_.leaf_count = static_cast<uint32_t>(tree.get_leaf_nodes().size());
    
    // Serialize the tree 
    // (This also accurately calculates valid tile_count)
    serialize_tree(tree, image, result.data);
    
    // Apply entropy coding
    apply_entropy_coding(result.data);
    
    // [FIX 3]: Calculate the real compression ratio using the ACTUAL compressed vector size
    size_t original_size = image.get_width() * image.get_height() * 3;
    last_stats_.compression_ratio = static_cast<double>(original_size) / 
                                    static_cast<double>(std::max<size_t>(1, result.data.size()));
    
    return result;
}

void Compressor::serialize_tree(
    const SpectreTree& tree,
    const ColorData& image,
    std::vector<uint8_t>& output) {
    
    output.clear();
    
    // [FIX 5]: Pass by const reference to avoid a massive deep copy
    const auto& all_tiles = tree.get_all_tiles();
    
    // [FIX 1 & FIX 5]: Use uint32_t to avoid overflow, and unordered_map for O(N) performance
    std::unordered_map<SpectreTile::ID, uint32_t> id_to_index;
    uint32_t index = 0;
    
    // First pass: create index mapping
    for (SpectreTile::ID tile_id : all_tiles) {
        // [FIX 2]: Only count and index valid tiles to prevent corrupted headers later
        if (tree.get_tile(tile_id) != nullptr) {
            id_to_index[tile_id] = index++;
        }
    }
    
    uint32_t width = image.get_width();
    uint32_t height = image.get_height();
    uint32_t valid_tile_count = index; 
    uint16_t max_depth = static_cast<uint16_t>(tree.get_max_depth());
    
    // Update structural stats with verified count
    last_stats_.tile_count = valid_tile_count;
    
    // OPTIMIZATION: Pre-allocate output buffer to avoid repeated reallocations
    // Estimate: header(14 bytes) + tiles(~25-50 bytes each) + some overhead
    size_t estimated_size = 14 + (valid_tile_count * 50) + 1000;
    output.reserve(estimated_size);
    
    // Write header (14 bytes)
    output.push_back((width >> 24) & 0xFF);
    output.push_back((width >> 16) & 0xFF);
    output.push_back((width >> 8) & 0xFF);
    output.push_back(width & 0xFF);
    
    output.push_back((height >> 24) & 0xFF);
    output.push_back((height >> 16) & 0xFF);
    output.push_back((height >> 8) & 0xFF);
    output.push_back(height & 0xFF);
    
    output.push_back((valid_tile_count >> 24) & 0xFF);
    output.push_back((valid_tile_count >> 16) & 0xFF);
    output.push_back((valid_tile_count >> 8) & 0xFF);
    output.push_back(valid_tile_count & 0xFF);
    
    output.push_back((max_depth >> 8) & 0xFF);
    output.push_back(max_depth & 0xFF);
    
    // [FIX 1]: Proper 32-bit Sentinel value
    const uint32_t NO_PARENT = 0xFFFFFFFF;

    // Second pass: serialize tiles using validated indices
    for (SpectreTile::ID tile_id : all_tiles) {
        const SpectreTile* tile_ptr = tree.get_tile(tile_id);
        if (!tile_ptr) continue;
        
        // Write tile index (uint32_t) - 4 bytes
        uint32_t tile_index = id_to_index[tile_id];
        output.push_back((tile_index >> 24) & 0xFF);
        output.push_back((tile_index >> 16) & 0xFF);
        output.push_back((tile_index >> 8) & 0xFF);
        output.push_back(tile_index & 0xFF);
        
        // [FIX 4]: Write depth as uint16_t (2 bytes) to match max_depth header sizing safely
        uint16_t depth = static_cast<uint16_t>(tile_ptr->get_depth());
        output.push_back((depth >> 8) & 0xFF);
        output.push_back(depth & 0xFF);
        
        // Write parent index (uint32_t) - 4 bytes
        SpectreTile::ID parent_id = tile_ptr->get_parent_id();
        uint32_t parent_index = (parent_id > 0 && id_to_index.count(parent_id)) 
                               ? id_to_index[parent_id] 
                               : NO_PARENT;
        output.push_back((parent_index >> 24) & 0xFF);
        output.push_back((parent_index >> 16) & 0xFF);
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
            // Write child index (uint32_t) - 4 bytes
            uint32_t child_index = id_to_index.count(child_id) ? id_to_index[child_id] : NO_PARENT;
            output.push_back((child_index >> 24) & 0xFF);
            output.push_back((child_index >> 16) & 0xFF);
            output.push_back((child_index >> 8) & 0xFF);
            output.push_back(child_index & 0xFF);
        }
    }
    
    // Shrink to fit to reduce memory after serialization
    output.shrink_to_fit();
}

void Compressor::apply_entropy_coding(std::vector<uint8_t>& data) {
    if (data.empty()) {
        return;
    }
    
    // [FIX 6]: Provided two paths to handle the thread safety issue here.
    
    // PREFERRED PATH: 
    // Update AdaptiveEncoder::encode to take 'entropy_stats_' as an output parameter
    // so it doesn't rely on internal static variables being overwritten by other threads.
    // data = AdaptiveEncoder::encode(data, config_.prefer_speed, entropy_stats_);
    
    // FALLBACK PATH:
    // we must lock it here ig
    static std::mutex encoder_mutex;
    std::lock_guard<std::mutex> lock(encoder_mutex);
    
    data = AdaptiveEncoder::encode(data, config_.prefer_speed);
    entropy_stats_ = AdaptiveEncoder::get_stats();
}

} // namespace spectre