#ifndef HIERARCHICAL_ADDRESS_H
#define HIERARCHICAL_ADDRESS_H

#include <vector>
#include <string>
#include <cstdint>

namespace spectre {

/**
 * @brief Represents a hierarchical address for Spectre tiles
 * 
 * Since aperiodic tiles don't align to a Cartesian (x,y) grid,
 * we use hierarchical addressing instead: e.g., "1.4.2.0"
 * Each segment represents a choice at each inflation level.
 */
class HierarchicalAddress {
public:
    using AddressSegment = uint32_t;
    
    /**
     * @brief Constructor for a hierarchical address
     * @param address Vector of address segments
     */
    explicit HierarchicalAddress(const std::vector<AddressSegment>& address = {});
    
    /**
     * @brief Create an address from string representation (e.g., "1.4.2.0")
     */
    static HierarchicalAddress from_string(const std::string& str);
    
    /**
     * @brief Convert address to string representation
     */
    std::string to_string() const;
    
    /**
     * @brief Get the complete address
     */
    const std::vector<AddressSegment>& get_address() const { return address_; }
    
    /**
     * @brief Get the depth of this address (number of segments)
     */
    size_t get_depth() const { return address_.size(); }
    
    /**
     * @brief Add a child segment to create a child address
     * @param segment The child index
     */
    HierarchicalAddress get_child_address(AddressSegment segment) const;
    
    /**
     * @brief Get the parent address by removing the last segment
     */
    HierarchicalAddress get_parent_address() const;
    
    /**
     * @brief Check if this is the root address
     */
    bool is_root() const { return address_.empty(); }
    
    /**
     * @brief Check if this address is a descendant of another
     */
    bool is_descendant_of(const HierarchicalAddress& parent) const;
    
    /**
     * @brief Equality comparison
     */
    bool operator==(const HierarchicalAddress& other) const;
    
    /**
     * @brief Less-than comparison (for sorting/maps)
     */
    bool operator<(const HierarchicalAddress& other) const;

private:
    std::vector<AddressSegment> address_;
};

} // namespace spectre

#endif // HIERARCHICAL_ADDRESS_H
