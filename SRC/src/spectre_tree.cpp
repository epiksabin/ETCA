#include "spectre_tree.h"
#include "variance_calculator.h"
#include "tile_inflater.h"
#if ETCA_OPENMP
#include <omp.h>
#endif

namespace spectre {

SpectreTree::SpectreTree(uint32_t image_width, uint32_t image_height)
    : image_width_(image_width), image_height_(image_height), root_id_(1), max_depth_(0) {
    
    // Create root tile
    auto root = std::make_unique<SpectreTile>(root_id_, 0, 0);
    tiles_[root_id_] = std::move(root);
    id_to_address_[root_id_] = HierarchicalAddress();  // Empty = root
}

SpectreTile* SpectreTree::get_tile(SpectreTile::ID id) {
    auto it = tiles_.find(id);
    if (it != tiles_.end()) {
        return it->second.get();
    }
    return nullptr;
}

const SpectreTile* SpectreTree::get_tile(SpectreTile::ID id) const {
    auto it = tiles_.find(id);
    if (it != tiles_.end()) {
        return it->second.get();
    }
    return nullptr;
}

SpectreTile* SpectreTree::get_tile_by_address(const HierarchicalAddress& address) {
    // Linear search through address map (could optimize with reverse map)
    for (auto& [id, addr] : id_to_address_) {
        if (addr == address) {
            return get_tile(id);
        }
    }
    return nullptr;
}

const SpectreTile* SpectreTree::get_tile_by_address(const HierarchicalAddress& address) const {
    for (const auto& [id, addr] : id_to_address_) {
        if (addr == address) {
            return get_tile(id);
        }
    }
    return nullptr;
}

HierarchicalAddress SpectreTree::get_address(SpectreTile::ID id) const {
    auto it = id_to_address_.find(id);
    if (it != id_to_address_.end()) {
        return it->second;
    }
    return HierarchicalAddress();
}

void SpectreTree::build(const ColorData& image, double variance_threshold, int max_depth) {
    SpectreTile* root = get_tile(root_id_);
    if (!root) return;
    
    build_recursive(*root, image, HierarchicalAddress(), variance_threshold, 0, max_depth);
}

std::vector<SpectreTile::ID> SpectreTree::get_leaf_nodes() const {
    std::vector<SpectreTile::ID> leaves;
    
    for (const auto& [id, tile_ptr] : tiles_) {
        if (!tile_ptr->is_subdivided()) {
            leaves.push_back(id);
        }
    }
    
    return leaves;
}

std::vector<SpectreTile::ID> SpectreTree::get_all_tiles() const {
    std::vector<SpectreTile::ID> all_tiles;
    
    for (const auto& [id, tile_ptr] : tiles_) {
        if (tile_ptr) {
            all_tiles.push_back(id);
        }
    }
    
    return all_tiles;
}

void SpectreTree::add_deserialized_tile(
    SpectreTile::ID id,
    int depth,
    SpectreTile::ID parent_id,
    uint8_t r, uint8_t g, uint8_t b,
    const std::vector<SpectreTile::ID>& children) {
    
    // Create a new tile with basic info
    auto tile = std::make_unique<SpectreTile>(id, depth, parent_id);
    
    // Set the color
    tile->set_color(r, g, b);
    
    // Add children
    for (SpectreTile::ID child_id : children) {
        tile->add_child(child_id);
    }
    
    // Register the tile
    tiles_[id] = std::move(tile);
    
    // Estimate hierarchical address based on depth (placeholder)
    // In full implementation, could reconstruct from parent-child relationships
    std::vector<uint32_t> address_segments;
    for (int i = 0; i < depth; ++i) {
        address_segments.push_back(0);  // Placeholder segment
    }
    id_to_address_[id] = HierarchicalAddress(address_segments);
    
    // Update max_depth if needed
    if (depth > max_depth_) {
        max_depth_ = depth;
    }
}

void SpectreTree::set_tile_address(SpectreTile::ID id, const HierarchicalAddress& address) {
    id_to_address_[id] = address;
}

void SpectreTree::build_recursive(
    SpectreTile& tile,
    const ColorData& image,
    const HierarchicalAddress& address,
    double variance_threshold,
    int current_depth,
    int max_depth) {
    
    // Update max depth
    if (current_depth > max_depth_) {
        max_depth_ = current_depth;
    }
    
    // Calculate color for this tile (average of all pixels)
    Color avg_color = image.calculate_average_color();
    tile.set_color(avg_color.r, avg_color.g, avg_color.b);
    
    // Check if we should subdivide
    if (current_depth >= max_depth || !VarianceCalculator::should_subdivide(image, variance_threshold)) {
        return;  // This tile is a leaf
    }
    
    // Inflate the tile
    std::vector<SpectreTile::ID> children = TileInflater::inflate_tile(tile);
    
    // Process each child
    uint32_t parent_width = image.get_width();
    uint32_t parent_height = image.get_height();
    
    for (size_t i = 0; i < children.size(); ++i) {
        uint32_t child_x, child_y, child_width, child_height;
        TileInflater::get_child_bounds(parent_width, parent_height, static_cast<int>(i),
                                       child_x, child_y, child_width, child_height);
        
        // Extract the region for this child
        ColorData child_image = image.extract_region(child_x, child_y, child_width, child_height);
        
        // Create child tile
        SpectreTile::ID child_id = children[i];
        auto child_tile = std::make_unique<SpectreTile>(child_id, current_depth + 1, tile.get_id());
        
        // Store hierarchical address
        HierarchicalAddress child_address = address.get_child_address(static_cast<uint32_t>(i));
        
        // Recursively build subtree
        SpectreTile* child_ptr = child_tile.get();
        tiles_[child_id] = std::move(child_tile);
        id_to_address_[child_id] = child_address;
        
        build_recursive(*child_ptr, child_image, child_address, variance_threshold,
                       current_depth + 1, max_depth);
    }
}

SpectreTile::ID SpectreTree::create_tile(int depth, SpectreTile::ID parent_id) {
    static SpectreTile::ID id_counter = 2;  // Start after root
    
    SpectreTile::ID new_id = id_counter++;
    auto tile = std::make_unique<SpectreTile>(new_id, depth, parent_id);
    tiles_[new_id] = std::move(tile);
    
    return new_id;
}

} // namespace spectre
