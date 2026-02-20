#include "hierarchical_address.h"
#include <sstream>
#include <algorithm>

namespace spectre {

HierarchicalAddress::HierarchicalAddress(const std::vector<AddressSegment>& address)
    : address_(address) {
}

HierarchicalAddress HierarchicalAddress::from_string(const std::string& str) {
    std::vector<AddressSegment> segments;
    
    if (str.empty() || str == ".") {
        return HierarchicalAddress(segments);  // root
    }
    
    std::istringstream iss(str);
    std::string segment;
    
    while (std::getline(iss, segment, '.')) {
        if (!segment.empty()) {
            segments.push_back(static_cast<AddressSegment>(std::stoul(segment)));
        }
    }
    
    return HierarchicalAddress(segments);
}

std::string HierarchicalAddress::to_string() const {
    if (address_.empty()) {
        return ".";  // root
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < address_.size(); ++i) {
        if (i > 0) oss << ".";
        oss << address_[i];
    }
    return oss.str();
}

HierarchicalAddress HierarchicalAddress::get_child_address(AddressSegment segment) const {
    auto child_addr = address_;
    child_addr.push_back(segment);
    return HierarchicalAddress(child_addr);
}

HierarchicalAddress HierarchicalAddress::get_parent_address() const {
    if (address_.empty()) {
        return HierarchicalAddress();  // Can't go above root
    }
    
    auto parent_addr = address_;
    parent_addr.pop_back();
    return HierarchicalAddress(parent_addr);
}

bool HierarchicalAddress::is_descendant_of(const HierarchicalAddress& parent) const {
    if (parent.address_.size() >= address_.size()) {
        return false;
    }
    
    return std::equal(parent.address_.begin(), parent.address_.end(), address_.begin());
}

bool HierarchicalAddress::operator==(const HierarchicalAddress& other) const {
    return address_ == other.address_;
}

bool HierarchicalAddress::operator<(const HierarchicalAddress& other) const {
    return address_ < other.address_;
}

} // namespace spectre
