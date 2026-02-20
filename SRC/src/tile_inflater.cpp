#include "tile_inflater.h"
#include <cmath>

namespace spectre {

// Initialize static member
SpectreTile::ID TileInflater::next_tile_id_ = 1;

std::vector<SpectreTile::ID> TileInflater::inflate_tile(SpectreTile& tile) {
    std::vector<SpectreTile::ID> children;
    
    // Generate child tiles based on Spectre inflation rules
    // The Spectre tile inflates into 4 children with specific arrangement
    for (int i = 0; i < CHILDREN_PER_TILE; ++i) {
        SpectreTile::ID child_id = next_tile_id_++;
        tile.add_child(child_id);
        children.push_back(child_id);
    }
    
    return children;
}

void TileInflater::get_child_bounds(
    uint32_t parent_width,
    uint32_t parent_height,
    int child_index,
    uint32_t& child_x,
    uint32_t& child_y,
    uint32_t& child_width,
    uint32_t& child_height) {
    
    // Divide into a 2x2 grid, ensuring complete pixel coverage
    // For odd dimensions, distribute the extra pixel to left/top quadrants
    uint32_t left_width = (parent_width + 1) / 2;    // Left columns get the extra pixel if odd
    uint32_t right_width = parent_width - left_width; // Right gets the remainder
    uint32_t top_height = (parent_height + 1) / 2;   // Top rows get the extra pixel if odd
    uint32_t bottom_height = parent_height - top_height; // Bottom gets the remainder
    
    switch (child_index) {
        case 0:  // Top-left
            child_x = 0;
            child_y = 0;
            child_width = left_width;
            child_height = top_height;
            break;
        case 1:  // Top-right
            child_x = left_width;
            child_y = 0;
            child_width = right_width;
            child_height = top_height;
            break;
        case 2:  // Bottom-left
            child_x = 0;
            child_y = top_height;
            child_width = left_width;
            child_height = bottom_height;
            break;
        case 3:  // Bottom-right
            child_x = left_width;
            child_y = top_height;
            child_width = right_width;
            child_height = bottom_height;
            break;
        default:
            child_x = child_y = 0;
            child_width = child_height = 1;
            break;
    }
}

double TileInflater::calculate_tile_size(double initial_size, int depth) {
    // Each inflation level reduces tile size
    // Spectre tiles scale with a specific ratio (approximately 2/3)
    const double SCALE_FACTOR = 0.5;
    return initial_size * std::pow(SCALE_FACTOR, depth);
}

} // namespace spectre
