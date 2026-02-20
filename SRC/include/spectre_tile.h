#ifndef SPECTRE_TILE_H
#define SPECTRE_TILE_H

#include <vector>
#include <cstdint>

namespace spectre {

/**
 * @brief Represents a single Spectre tile with geometric and numerical properties
 * 
 * Unlike squares in a quadtree, the Spectre tile has unique properties:
 * - Non-Cartesian coordinate system
 * - Mathematical inflation rules for subdivision
 * - Hierarchical addressing instead of (x,y) coordinates
 */
class SpectreTile {
public:
    using ID = uint64_t;
    
    /**
     * @brief Constructor for a Spectre tile
     * @param id Unique identifier for this tile
     * @param depth Depth in the Spectre-Tree hierarchy
     * @param parent_id ID of the parent tile (0 if root)
     */
    SpectreTile(ID id, int depth, ID parent_id);
    
    /**
     * @brief Get the unique ID of this tile
     */
    ID get_id() const { return id_; }
    
    /**
     * @brief Get the depth in the tree (0 for root)
     */
    int get_depth() const { return depth_; }
    
    /**
     * @brief Get the parent tile ID
     */
    ID get_parent_id() const { return parent_id_; }
    
    /**
     * @brief Get all child tile IDs resulting from inflation
     */
    const std::vector<ID>& get_children() const { return children_; }
    
    /**
     * @brief Add a child tile (result of inflation)
     */
    void add_child(ID child_id);
    
    /**
     * @brief Check if this tile has been subdivided
     */
    bool is_subdivided() const { return !children_.empty(); }
    
    /**
     * @brief Set the color average for this tile
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void set_color(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Get the stored color average
     */
    void get_color(uint8_t& r, uint8_t& g, uint8_t& b) const;
    
private:
    ID id_;
    int depth_;
    ID parent_id_;
    std::vector<ID> children_;
    
    // Color storage (RGB)
    uint8_t color_r_, color_g_, color_b_;
};

} // namespace spectre

#endif // SPECTRE_TILE_H
