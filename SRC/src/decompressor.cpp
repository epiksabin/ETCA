// decompressor.cpp
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
        data[0] == static_cast<uint8_t>(EntropyCodec::ADVANCED) ||
        data[0] == static_cast<uint8_t>(EntropyCodec::HUFFMAN)) {
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
                        // RLE sequence: value | count - need count byte
                        uint8_t count = data[data_offset++];
                        for (uint8_t i = 0; i < count; ++i) {
                            decoded_data.push_back(next_byte);
                        }
                    } else {
                        // Truncated RLE: consumed RLE_MARKER and value but no count - abort to avoid corruption
                        break;
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
    
    // Parse tile records - format MUST match compressor:
    // Format: index(4) | depth(2) | parent_index(4) | r(1) | g(1) | b(1) | child_count(1) | [child_index(4)]...
    std::vector<uint64_t> index_to_id(tile_count);  // Map tile index to actual tile ID
    std::map<uint64_t, std::pair<uint64_t, uint32_t>> tile_to_parent_and_position;  // Map: tile_id -> (parent_id, child_position)
    
    const uint32_t NO_PARENT = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < tile_count && data_offset < decoded_data.size(); ++i) {
        // Each tile needs at least: index(4) + depth(2) + parent_index(4) + r(1) + g(1) + b(1) + child_count(1) = 14 bytes
        if (data_offset + 14 > decoded_data.size()) {
            break;  // Not enough data for this tile
        }
        
        // Parse tile index (uint32_t) - 4 bytes
        uint32_t tile_index = (static_cast<uint32_t>(decoded_data[data_offset] & 0xFF) << 24) |
                             (static_cast<uint32_t>(decoded_data[data_offset+1] & 0xFF) << 16) |
                             (static_cast<uint32_t>(decoded_data[data_offset+2] & 0xFF) << 8) |
                             static_cast<uint32_t>(decoded_data[data_offset+3] & 0xFF);
        data_offset += 4;
        
        // Bounds check: prevent out-of-bounds write from corrupt/malicious input
        if (tile_index >= tile_count) {
            break;
        }
        
        // Convert index to tile ID (use index + 1 to match compressor's ID scheme where root=1)
        uint64_t tile_id = static_cast<uint64_t>(tile_index) + 1;
        index_to_id[tile_index] = tile_id;
        
        // Parse depth (uint16_t) - 2 bytes
        int tile_depth = static_cast<int>((static_cast<uint16_t>(decoded_data[data_offset] & 0xFF) << 8) |
                         static_cast<uint16_t>(decoded_data[data_offset+1] & 0xFF));
        data_offset += 2;
        
        // Parse parent index (uint32_t) - 4 bytes
        uint32_t parent_index = (static_cast<uint32_t>(decoded_data[data_offset] & 0xFF) << 24) |
                               (static_cast<uint32_t>(decoded_data[data_offset+1] & 0xFF) << 16) |
                               (static_cast<uint32_t>(decoded_data[data_offset+2] & 0xFF) << 8) |
                               static_cast<uint32_t>(decoded_data[data_offset+3] & 0xFF);
        data_offset += 4;
        
        // Convert parent index to parent ID
        uint64_t parent_id = (parent_index == NO_PARENT) ? 0 : (static_cast<uint64_t>(parent_index) + 1);
        
        // Parse color (r, g, b)
        uint8_t r = decoded_data[data_offset++];
        uint8_t g = decoded_data[data_offset++];
        uint8_t b = decoded_data[data_offset++];
        
        // Parse child count
        uint8_t child_count = decoded_data[data_offset++];
        
        // Parse child indices - each is 4 bytes (uint32_t)
        std::vector<uint64_t> children;
        if (data_offset + (child_count * 4) > decoded_data.size()) {
            break;  // Not enough data for all children
        }
        
        for (uint8_t j = 0; j < child_count; ++j) {
            uint32_t child_index = (static_cast<uint32_t>(decoded_data[data_offset] & 0xFF) << 24) |
                                  (static_cast<uint32_t>(decoded_data[data_offset+1] & 0xFF) << 16) |
                                  (static_cast<uint32_t>(decoded_data[data_offset+2] & 0xFF) << 8) |
                                  static_cast<uint32_t>(decoded_data[data_offset+3] & 0xFF);
            data_offset += 4;
            uint64_t child_id = (child_index == NO_PARENT) ? 0 : (static_cast<uint64_t>(child_index) + 1);
            
            // Only track valid children (skip NO_PARENT/invalid)
            if (child_id > 0) {
                children.push_back(child_id);
                // Track: child appears at position j in parent tile_id
                tile_to_parent_and_position[child_id] = {tile_id, j};
            }
        }
        
        // Add tile to tree
        tree->add_deserialized_tile(tile_id, tile_depth, parent_id, r, g, b, children);
    }
    
    // Reconstruct hierarchical addresses for ALL tiles from parent-child relationships
    // Build address for each tile by traversing up to root
    std::unordered_map<uint64_t, bool> address_set;
    
    for (const auto& [tile_id, parent_and_pos] : tile_to_parent_and_position) {
        std::vector<uint32_t> address_path;
        uint64_t current_id = tile_id;
        
        // Traverse up to root, collecting positions
        // Validate that parent-child chains are valid
        int traversal_depth = 0;
        const int MAX_TRAVERSAL = 1000;  // Safety limit to detect circular references
        
        while (tile_to_parent_and_position.count(current_id)) {
            const auto& [parent_id, position] = tile_to_parent_and_position[current_id];
            address_path.push_back(position);
            current_id = parent_id;
            
            if (++traversal_depth > MAX_TRAVERSAL) {
                break;  // Circular reference or invalid chain - abort
            }
        }
        
        // Verify we reached root (no parent for current_id or current_id == root)
        if (current_id == 1 || !tile_to_parent_and_position.count(current_id)) {
            // Valid chain - reverse to get path from root to tile
            std::reverse(address_path.begin(), address_path.end());
            
            // Set the reconstructed address
            HierarchicalAddress reconstructed_address(address_path);
            tree->set_tile_address(tile_id, reconstructed_address);
            address_set[tile_id] = true;
        }
        // else: invalid chain - address will remain default (root bounds)
    }
    
    // Ensure root tile (ID=1) has empty address
    tree->set_tile_address(1, HierarchicalAddress());
    address_set[1] = true;
    
    // Verify all tiles have addresses - flag any tiles without valid addresses
    const auto& all_tiles = tree->get_all_tiles();
    for (SpectreTile::ID tile_id : all_tiles) {
        if (!address_set[tile_id]) {
            // Tile address reconstruction failed - this indicates corrupt data
            // Set to empty address (root bounds) to minimize visual corruption
            tree->set_tile_address(tile_id, HierarchicalAddress());
        }
    }
    
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
    // Use sequential outer loop with inner parallelization to avoid race conditions
    // and ensure proper variable capture
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
        uint32_t end_x = std::min(tile_x + tile_width, width);
        uint32_t end_y = std::min(tile_y + tile_height, height);
        
        // Capture bounds locally to avoid race conditions
        const uint32_t local_tile_x = tile_x;
        const uint32_t local_tile_y = tile_y;
        const uint32_t local_end_x = end_x;
        const uint32_t local_end_y = end_y;
        const uint8_t tile_r = r;
        const uint8_t tile_g = g;
        const uint8_t tile_b = b;
        
        // Parallelize pixel assignment within each tile
        // Only one level of parallelism to avoid thread oversubscription
#if ETCA_OPENMP
        #pragma omp parallel for collapse(2)
#endif
        for (uint32_t x = local_tile_x; x < local_end_x; ++x) {
            for (uint32_t y = local_tile_y; y < local_end_y; ++y) {
                image.set_pixel(x, y, Color(tile_r, tile_g, tile_b));
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
    
    // Cache for the source image to avoid redundant get_pixel calls
    const auto& pixels = image.get_pixels();
    std::vector<Color> output = pixels;  // Efficient copy
    
    // Apply interpolation kernel: average nearby pixels with weighted blending
    // Use reduced blend strength for subtle smoothing
    const float BLEND_STRENGTH = 0.3f;  // Reduced from 0.5f for less blur
    const float CENTER_WEIGHT = 1.0f - BLEND_STRENGTH;
    const float NEIGHBOR_WEIGHT = BLEND_STRENGTH / 8.0f;
    
    // 8-connected neighbor offsets
    const int NEIGHBOR_DX[] = {-1,  0,  1, -1,  1, -1,  0,  1};
    const int NEIGHBOR_DY[] = {-1, -1, -1,  0,  0,  1,  1,  1};
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t center_idx = y * width + x;
            const Color& center = pixels[center_idx];
            
            // Accumulate weighted colors from neighbors
            float blend_r = center.r * CENTER_WEIGHT;
            float blend_g = center.g * CENTER_WEIGHT;
            float blend_b = center.b * CENTER_WEIGHT;
            float weight_sum = CENTER_WEIGHT;
            
            // Sample 8-connected neighbors
            for (int i = 0; i < 8; ++i) {
                int nx = static_cast<int>(x) + NEIGHBOR_DX[i];
                int ny = static_cast<int>(y) + NEIGHBOR_DY[i];
                
                if (nx >= 0 && nx < static_cast<int>(width) &&
                    ny >= 0 && ny < static_cast<int>(height)) {
                    const Color& neighbor = pixels[static_cast<size_t>(ny) * width + static_cast<size_t>(nx)];
                    blend_r += neighbor.r * NEIGHBOR_WEIGHT;
                    blend_g += neighbor.g * NEIGHBOR_WEIGHT;
                    blend_b += neighbor.b * NEIGHBOR_WEIGHT;
                    weight_sum += NEIGHBOR_WEIGHT;
                }
            }
            
            // Normalize and write result
            if (weight_sum > 0.0f) {
                output[center_idx].r = static_cast<uint8_t>(blend_r / weight_sum);
                output[center_idx].g = static_cast<uint8_t>(blend_g / weight_sum);
                output[center_idx].b = static_cast<uint8_t>(blend_b / weight_sum);
            }
        }
    }
    
    // Replace image pixels with interpolated values
    image.get_pixels() = output;
}

} // namespace spectre
