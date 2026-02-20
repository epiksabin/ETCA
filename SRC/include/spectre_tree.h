#ifndef SPECTRE_TREE_H
#define SPECTRE_TREE_H

#include "spectre_tile.h"
#include "hierarchical_address.h"
#include "color_data.h"
#include <map>
#include <memory>
#include <vector>

namespace spectre {

/**
 * @brief The Spectre-Tree: Hierarchical tree structure for image compression
 * 
 * Similar to a quadtree, but using Spectre tiles instead of squares.
 * The root is one giant Spectre tile covering the entire image.
 * Each node's variance is checked; if high, it's inflated into children.
 */
class SpectreTree {
public:
    /**
     * @brief Constructor for a Spectre-Tree
     * @param image_width Width of the image
     * @param image_height Height of the image
     */
    SpectreTree(uint32_t image_width, uint32_t image_height);
    
    /**
     * @brief Get the root tile ID
     */
    SpectreTile::ID get_root_id() const { return root_id_; }
    
    /**
     * @brief Get a tile by ID
     */
    SpectreTile* get_tile(SpectreTile::ID id);
    const SpectreTile* get_tile(SpectreTile::ID id) const;
    
    /**
     * @brief Get a tile by hierarchical address
     */
    SpectreTile* get_tile_by_address(const HierarchicalAddress& address);
    const SpectreTile* get_tile_by_address(const HierarchicalAddress& address) const;
    
    /**
     * @brief Get the hierarchical address of a tile
     */
    HierarchicalAddress get_address(SpectreTile::ID id) const;
    
    /**
     * @brief Build the tree structure from image data with adaptive subdivision
     * @param image The image data
     * @param variance_threshold Threshold for subdivision (0.0-1.0)
     * @param max_depth Maximum tree depth
     */
    void build(const ColorData& image, double variance_threshold, int max_depth);
    
    /**
     * @brief Get all leaf nodes (tiles that weren't subdivided)
     */
    std::vector<SpectreTile::ID> get_leaf_nodes() const;
    
    /**
     * @brief Get all tiles (both leaf and internal nodes)
     */
    std::vector<SpectreTile::ID> get_all_tiles() const;
    
    /**
     * @brief Get total number of tiles
     */
    size_t get_tile_count() const { return tiles_.size(); }
    
    /**
     * @brief Get maximum depth reached
     */
    int get_max_depth() const { return max_depth_; }
    
    /**
     * @brief Get image dimensions
     */
    void get_dimensions(uint32_t& width, uint32_t& height) const {
        width = image_width_;
        height = image_height_;
    }
    
    /**
     * @brief Add a deserialized tile to the tree (for deserialization)
     * @param id Tile ID
     * @param depth Tile depth
     * @param parent_id Parent tile ID
     * @param r,g,b Color values
     * @param children IDs of child tiles
     */
    void add_deserialized_tile(
        SpectreTile::ID id,
        int depth,
        SpectreTile::ID parent_id,
        uint8_t r, uint8_t g, uint8_t b,
        const std::vector<SpectreTile::ID>& children
    );
    
    /**
     * @brief Set the hierarchical address of a tile (for deserialization)
     * @param id Tile ID
     * @param address Hierarchical address for the tile
     */
    void set_tile_address(SpectreTile::ID id, const HierarchicalAddress& address);

private:
    uint32_t image_width_, image_height_;
    SpectreTile::ID root_id_;
    int max_depth_;
    
    // Map from tile ID to tile object
    std::map<SpectreTile::ID, std::unique_ptr<SpectreTile>> tiles_;
    
    // Map from tile ID to hierarchical address
    std::map<SpectreTile::ID, HierarchicalAddress> id_to_address_;
    
    /**
     * @brief Recursively build the tree with variance-driven subdivision
     */
    void build_recursive(
        SpectreTile& tile,
        const ColorData& image,
        const HierarchicalAddress& address,
        double variance_threshold,
        int current_depth,
        int max_depth
    );
    
    /**
     * @brief Create and register a new tile
     */
    SpectreTile::ID create_tile(int depth, SpectreTile::ID parent_id);
};

} // namespace spectre

#endif // SPECTRE_TREE_H
