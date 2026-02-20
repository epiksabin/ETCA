#ifndef TILE_INFLATER_H
#define TILE_INFLATER_H

#include "spectre_tile.h"
#include <vector>
#include <cstdint>

namespace spectre {

/**
 * @brief Handles the inflation (subdivision) of Spectre tiles
 * 
 * According to the mathematical properties of Spectre tiles,
 * a single tile can be mathematically subdivided into specific smaller children.
 * This follows the hierarchical substitution system.
 */
class TileInflater {
public:
    /**
     * @brief Number of children produced by inflating a Spectre tile
     * This is determined by the mathematical properties of Spectre tiles.
     */
    static constexpr int CHILDREN_PER_TILE = 4;
    
    /**
     * @brief Inflate (subdivide) a Spectre tile into its children
     * @param tile The tile to inflate
     * @return Vector of child tile IDs
     */
    static std::vector<SpectreTile::ID> inflate_tile(SpectreTile& tile);
    
    /**
     * @brief Get the bounding region of a child tile within parent
     * 
     * Since we don't use Cartesian coordinates, this provides
     * the relative positioning information for a child.
     * 
     * @param parent_width Parent tile width
     * @param parent_height Parent tile height
     * @param child_index Which child (0-3)
     * @param child_x Output: child X offset
     * @param child_y Output: child Y offset
     * @param child_width Output: child width
     * @param child_height Output: child height
     */
    static void get_child_bounds(
        uint32_t parent_width,
        uint32_t parent_height,
        int child_index,
        uint32_t& child_x,
        uint32_t& child_y,
        uint32_t& child_width,
        uint32_t& child_height
    );
    
    /**
     * @brief Calculate the approximate size of a tile at a given depth
     * @param initial_size Size at root (depth 0)
     * @param depth Current depth
     */
    static double calculate_tile_size(double initial_size, int depth);

private:
    // Counter for generating unique IDs
    static SpectreTile::ID next_tile_id_;
};

} // namespace spectre

#endif // TILE_INFLATER_H
