#include "decompressor.h"
#include "tile_inflater.h"
#include <functional>
#include <algorithm>
#if ETCA_OPENMP
#include <omp.h>
#endif

namespace spectre {

// Helper function to calculate spatial bounds of a tile from its hierarchical address
static void calculate_tile_bounds(
    const SpectreTree& tree,
    SpectreTile::ID tile_id,
    uint32_t image_width,
    uint32_t image_height,
    uint32_t& out_x,
    uint32_t& out_y,
    uint32_t& out_width,
    uint32_t& out_height) {
    
    // Get the hierarchical address for this tile
    HierarchicalAddress address = tree.get_address(tile_id);
    const auto& address_segments = address.get_address();
    
    // Start with the root tile covering the entire image
    uint32_t current_x = 0;
    uint32_t current_y = 0;
    uint32_t current_width = image_width;
    uint32_t current_height = image_height;
    
    // Traverse the address to get the final bounds
    for (uint32_t segment : address_segments) {
        uint32_t child_x, child_y, child_width, child_height;
        
        // Use TileInflater to get child bounds
        TileInflater::get_child_bounds(
            current_width, current_height,
            static_cast<int>(segment),
            child_x, child_y, child_width, child_height
        );
        
        // Update current bounds (relative to current region)
        current_x += child_x;
        current_y += child_y;
        current_width = child_width;
        current_height = child_height;
    }
    
    out_x = current_x;
    out_y = current_y;
    out_width = current_width;
    out_height = current_height;
}

ColorData Decompressor::decompress(const CompressedImage& compressed) {
    return decompress(compressed, false, -1);
}

ColorData Decompressor::decompress(
    const CompressedImage& compressed,
    bool should_interpolate,
    int /*max_depth*/) {
    
    // Deserialize the tree from compressed data
    auto tree = deserialize_tree(compressed.data, compressed.width, compressed.height);
    
    if (!tree) {
        // Return blank image on failure
        return ColorData(compressed.width, compressed.height);
    }
    
    // Reconstruct the image from the tree
    ColorData image = reconstruct_image(*tree, should_interpolate);
    
    return image;
}

std::unique_ptr<SpectreTree> Decompressor::deserialize_tree(
    const std::vector<uint8_t>& data,
    uint32_t width,
    uint32_t height) {
    
    // Create empty tree
    auto tree = std::make_unique<SpectreTree>(width, height);
    
    if (data.empty()) {
        return tree;
    }
    
    // Try to decode with the new entropy decoding system
    // Check if the first byte is an entropy codec marker
    std::vector<uint8_t> decoded_data;
    
    if (data[0] == static_cast<uint8_t>(EntropyCodec::NONE) ||
        data[0] == static_cast<uint8_t>(EntropyCodec::RLE) ||
        data[0] == static_cast<uint8_t>(EntropyCodec::DEFLATE) ||
        data[0] == static_cast<uint8_t>(EntropyCodec::ADVANCED)) {
        // New entropy encoding detected - decode it
        decoded_data = AdaptiveEncoder::decode(data);
    } else if (data[0] == 0x01 || data[0] == 0x00) {
        // Old format encoding (legacy RLE support)
        if (data[0] == 0x01) {
            // Data is RLE encoded - decode it
            decoded_data.reserve(data.size() * 2);
            size_t data_offset = 1;  // Skip marker
            
            const uint8_t RLE_MARKER = 0xFF;
            
            while (data_offset < data.size()) {
                uint8_t byte = data[data_offset++];
                
                if (byte == RLE_MARKER) {
                    if (data_offset >= data.size()) break;
                    
                    uint8_t next_byte = data[data_offset++];
                    
                    if (next_byte == RLE_MARKER) {
                        // Escaped marker byte - add literal marker
                        decoded_data.push_back(RLE_MARKER);
                    } else if (data_offset < data.size()) {
                        // RLE sequence: value | count
                        uint8_t count = data[data_offset++];
                        for (uint8_t i = 0; i < count; ++i) {
                            decoded_data.push_back(next_byte);
                        }
                    }
                } else {
                    // Regular byte
                    decoded_data.push_back(byte);
                }
            }
        } else {
            // Data is not encoded - use as-is
            decoded_data.assign(data.begin() + 1, data.end());
        }
    } else {
        // Unknown format - assume unencoded
        decoded_data = data;
    }
    
    if (decoded_data.size() < 14) {
        // Not enough data for header
        return tree;
    }
    
    // Initialize offset for parsing the tree structure
    size_t data_offset = 0;
    
    // Parse header: width(4) | height(4) | tile_count(4) | max_depth(2)
    uint32_t stored_width = (static_cast<uint32_t>(decoded_data[data_offset]) << 24) |
                           (static_cast<uint32_t>(decoded_data[data_offset+1]) << 16) |
                           (static_cast<uint32_t>(decoded_data[data_offset+2]) << 8) |
                           static_cast<uint32_t>(decoded_data[data_offset+3]);
    data_offset += 4;
    
    uint32_t stored_height = (static_cast<uint32_t>(decoded_data[data_offset]) << 24) |
                            (static_cast<uint32_t>(decoded_data[data_offset+1]) << 16) |
                            (static_cast<uint32_t>(decoded_data[data_offset+2]) << 8) |
                            static_cast<uint32_t>(decoded_data[data_offset+3]);
    data_offset += 4;
    
    uint32_t tile_count = (static_cast<uint32_t>(decoded_data[data_offset]) << 24) |
                         (static_cast<uint32_t>(decoded_data[data_offset+1]) << 16) |
                         (static_cast<uint32_t>(decoded_data[data_offset+2]) << 8) |
                         static_cast<uint32_t>(decoded_data[data_offset+3]);
    data_offset += 4;
    
    data_offset += 2;  // Skip max_depth field (not used in reconstruction)
    
    // Validate header
    if (stored_width != width || stored_height != height) {
        // Dimension mismatch - return empty tree
        return tree;
    }
    
    // Parse tile records using new compact index-based format
    // Format: index(2) | depth(1) | parent_index(2) | r(1) | g(1) | b(1) | child_count(1) | [child_index(2)]...
    std::vector<uint64_t> index_to_id(tile_count);  // Map tile index to actual tile ID
    std::map<uint64_t, std::pair<uint64_t, uint32_t>> tile_to_parent_and_position;  // Map: tile_id -> (parent_id, child_position)
    
    for (uint32_t i = 0; i < tile_count && data_offset < decoded_data.size(); ++i) {
        // Each tile needs at least: index(2) + depth(1) + parent_index(2) + r(1) + g(1) + b(1) + child_count(1) = 9 bytes
        if (data_offset + 9 > decoded_data.size()) {
            break;  // Not enough data for this tile
        }
        
        // Parse tile index (uint16_t)
        uint16_t tile_index = static_cast<uint16_t>((static_cast<uint16_t>(decoded_data[data_offset] & 0xFF) << 8) |
                             static_cast<uint16_t>(decoded_data[data_offset+1] & 0xFF));
        data_offset += 2;
        
        // Convert index to tile ID (use index + 1 to match compressor's ID scheme where root=1)
        uint64_t tile_id = static_cast<uint64_t>(tile_index) + 1;
        index_to_id[tile_index] = tile_id;
        
        // Parse depth (uint8_t)
        int tile_depth = static_cast<int>(decoded_data[data_offset++]);
        
        // Parse parent index (uint16_t)
        uint16_t parent_index = static_cast<uint16_t>((static_cast<uint16_t>(decoded_data[data_offset] & 0xFF) << 8) |
                               static_cast<uint16_t>(decoded_data[data_offset+1] & 0xFF));
        data_offset += 2;
        
        // Convert parent index to parent ID
        uint64_t parent_id = (parent_index == 0xFFFF) ? 0 : (static_cast<uint64_t>(parent_index) + 1);
        
        // Parse color (r, g, b)
        uint8_t r = decoded_data[data_offset++];
        uint8_t g = decoded_data[data_offset++];
        uint8_t b = decoded_data[data_offset++];
        
        // Parse child count
        uint8_t child_count = decoded_data[data_offset++];
        
        // Parse child indices and convert to IDs, track their positions
        std::vector<uint64_t> children;
        if (data_offset + (child_count * 2) > decoded_data.size()) {
            // Not enough data for all children
            break;
        }
        
        for (uint8_t j = 0; j < child_count; ++j) {
            uint16_t child_index = static_cast<uint16_t>((static_cast<uint16_t>(decoded_data[data_offset] & 0xFF) << 8) |
                                  static_cast<uint16_t>(decoded_data[data_offset+1] & 0xFF));
            data_offset += 2;
            uint64_t child_id = (child_index == 0xFFFF) ? 0 : (static_cast<uint64_t>(child_index) + 1);
            children.push_back(child_id);
            
            // Track: child appears at position j in parent tile_id
            tile_to_parent_and_position[child_id] = {tile_id, j};
        }
        
        // Add tile to tree
        tree->add_deserialized_tile(tile_id, tile_depth, parent_id, r, g, b, children);
    }
    
    // Reconstruct hierarchical addresses for ALL tiles from parent-child relationships
    // Build address for each tile by traversing up to root
    for (const auto& [tile_id, parent_and_pos] : tile_to_parent_and_position) {
        std::vector<uint32_t> address_path;
        uint64_t current_id = tile_id;
        
        // Traverse up to root, collecting positions
        while (tile_to_parent_and_position.count(current_id)) {
            const auto& [parent_id, position] = tile_to_parent_and_position[current_id];
            address_path.push_back(position);
            current_id = parent_id;
        }
        
        // Reverse to get path from root to tile
        std::reverse(address_path.begin(), address_path.end());
        
        // Set the reconstructed address
        HierarchicalAddress reconstructed_address(address_path);
        tree->set_tile_address(tile_id, reconstructed_address);
    }
    
    // Root tile should have empty address (already set correctly)
    return tree;
}

ColorData Decompressor::reconstruct_image(
    const SpectreTree& tree,
    bool should_interpolate) {
    
    uint32_t width, height;
    tree.get_dimensions(width, height);
    
    ColorData image(width, height);
    
    // Get all leaf tiles
    auto leaves = tree.get_leaf_nodes();
    
    // For each leaf, paint its region with its color using proper spatial mapping
    // Parallelize leaf processing since each tile is independent
#if ETCA_OPENMP
    #pragma omp parallel for
#endif
    for (size_t leaf_idx = 0; leaf_idx < leaves.size(); ++leaf_idx) {
        auto leaf_id = leaves[leaf_idx];
        const SpectreTile* tile = tree.get_tile(leaf_id);
        if (!tile) continue;
        
        uint8_t r, g, b;
        tile->get_color(r, g, b);
        Color tile_color(r, g, b);
        
        // Calculate tile bounds based on hierarchical address
        uint32_t tile_x, tile_y, tile_width, tile_height;
        calculate_tile_bounds(tree, leaf_id, width, height,
                             tile_x, tile_y, tile_width, tile_height);
        
        // Fill the tile region with its color
        uint32_t end_x = tile_x + tile_width;
        uint32_t end_y = tile_y + tile_height;
        
        // Clamp to image bounds
        if (end_x > width) end_x = width;
        if (end_y > height) end_y = height;
        
        // Parallelize pixel assignment within each tile
#if ETCA_OPENMP
        #pragma omp parallel for collapse(2)
#endif
        for (uint32_t x = tile_x; x < end_x; ++x) {
            for (uint32_t y = tile_y; y < end_y; ++y) {
                image.set_pixel(x, y, tile_color);
            }
        }
    }
    
    // Apply interpolation if requested
    if (should_interpolate) {
        apply_interpolation(image);
    }
    
    return image;
}

void Decompressor::apply_interpolation(ColorData& image) {
    // Apply bilinear interpolation at tile boundaries to reduce blocking artifacts
    // This smooths transitions between tiles with different colors
    
    uint32_t width = image.get_width();
    uint32_t height = image.get_height();
    
    // Create a duplicate for reading while we write
    ColorData interpolated = image;
    
    // Apply interpolation kernel: average nearby pixels with weighted blending
    // For simplicity, use a 3x3 kernel that blurs boundaries slightly
    const float BLEND_STRENGTH = 0.5f;
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            // Get center pixel
            Color center = image.get_pixel(x, y);
            
            // Accumulate weighted colors from neighbors
            float blend_r = center.r * (1.0f - BLEND_STRENGTH);
            float blend_g = center.g * (1.0f - BLEND_STRENGTH);
            float blend_b = center.b * (1.0f - BLEND_STRENGTH);
            
            float weight_sum = 1.0f - BLEND_STRENGTH;
            
            // Sample neighbors with reduced weight
            const float neighbor_weight = BLEND_STRENGTH / 8.0f;
            
            // 8-connected neighbors
            int neighbors_x[] = {-1,  0,  1, -1,  1, -1,  0,  1};
            int neighbors_y[] = {-1, -1, -1,  0,  0,  1,  1,  1};
            
            for (int i = 0; i < 8; ++i) {
                int nx = static_cast<int>(x) + neighbors_x[i];
                int ny = static_cast<int>(y) + neighbors_y[i];
                
                if (nx >= 0 && nx < static_cast<int>(width) &&
                    ny >= 0 && ny < static_cast<int>(height)) {
                    Color neighbor = image.get_pixel(static_cast<uint32_t>(nx), static_cast<uint32_t>(ny));
                    blend_r += neighbor.r * neighbor_weight;
                    blend_g += neighbor.g * neighbor_weight;
                    blend_b += neighbor.b * neighbor_weight;
                    weight_sum += neighbor_weight;
                }
            }
            
            // Normalize and clamp to [0, 255]
            if (weight_sum > 0) {
                uint8_t final_r = static_cast<uint8_t>(blend_r / weight_sum);
                uint8_t final_g = static_cast<uint8_t>(blend_g / weight_sum);
                uint8_t final_b = static_cast<uint8_t>(blend_b / weight_sum);
                
                interpolated.set_pixel(x, y, Color(final_r, final_g, final_b));
            }
        }
    }
    
    // Replace original with interpolated
    image = interpolated;
}

} // namespace spectre
