#ifndef VARIANCE_CALCULATOR_H
#define VARIANCE_CALCULATOR_H

#include "color_data.h"
#include <cstdint>

namespace spectre {

/**
 * @brief Calculates color variance for adaptive tile subdivision
 * 
 * Variance is used to determine if a tile should be further subdivided.
 * High variance means detail (needs more tiles), low variance means smooth (single tile ok).
 */
class VarianceCalculator {
public:
    /**
     * @brief Calculate the variance of colors in an image region
     * @param data The image data
     * @return The variance value (0.0 = uniform color, higher = more variation)
     */
    static double calculate_variance(const ColorData& data);
    
    /**
     * @brief Calculate variance per channel (R, G, B)
     * @param data The image data
     * @param var_r Output for red variance
     * @param var_g Output for green variance
     * @param var_b Output for blue variance
     */
    static void calculate_channel_variance(
        const ColorData& data,
        double& var_r,
        double& var_g,
        double& var_b
    );
    
    /**
     * @brief Determine if a tile should be subdivided
     * @param data The image data in this tile
     * @param threshold Variance threshold for subdivision
     */
    static bool should_subdivide(const ColorData& data, double threshold);

private:
    /**
     * @brief Calculate mean value of a set of numbers
     */
    static double calculate_mean(const std::vector<double>& values);
};

} // namespace spectre

#endif // VARIANCE_CALCULATOR_H
