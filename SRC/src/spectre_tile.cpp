#include "spectre_tile.h"

namespace spectre {

SpectreTile::SpectreTile(ID id, int depth, ID parent_id)
    : id_(id), depth_(depth), parent_id_(parent_id),
      color_r_(0), color_g_(0), color_b_(0) {
}

void SpectreTile::add_child(ID child_id) {
    children_.push_back(child_id);
}

void SpectreTile::set_color(uint8_t r, uint8_t g, uint8_t b) {
    color_r_ = r;
    color_g_ = g;
    color_b_ = b;
}

void SpectreTile::get_color(uint8_t& r, uint8_t& g, uint8_t& b) const {
    r = color_r_;
    g = color_g_;
    b = color_b_;
}

} // namespace spectre
