#include "variance_calculator.h"
#include <cmath>
#include <numeric>
#if ETCA_OPENMP
#include <omp.h>
#endif

namespace spectre {

double VarianceCalculator::calculate_variance(const ColorData& data) {
    double var_r, var_g, var_b;
    calculate_channel_variance(data, var_r, var_g, var_b);
    
    // Combined variance (average of channel variances)
    return (var_r + var_g + var_b) / 3.0;
}

void VarianceCalculator::calculate_channel_variance(
    const ColorData& data,
    double& var_r,
    double& var_g,
    double& var_b) {
    
    const auto& pixels = data.get_pixels();
    
    if (pixels.empty()) {
        var_r = var_g = var_b = 0.0;
        return;
    }
    
    // Calculate means (parallel reduction)
    double mean_r = 0, mean_g = 0, mean_b = 0;
#if ETCA_OPENMP
    #pragma omp parallel for reduction(+:mean_r,mean_g,mean_b)
#endif
    for (size_t i = 0; i < pixels.size(); ++i) {
        mean_r += pixels[i].r;
        mean_g += pixels[i].g;
        mean_b += pixels[i].b;
    }
    
    double count = static_cast<double>(pixels.size());
    mean_r /= count;
    mean_g /= count;
    mean_b /= count;
    
    // Calculate variances (parallel reduction)
    var_r = var_g = var_b = 0.0;
#if ETCA_OPENMP
    #pragma omp parallel for reduction(+:var_r,var_g,var_b)
#endif
    for (size_t i = 0; i < pixels.size(); ++i) {
        double dr = pixels[i].r - mean_r;
        double dg = pixels[i].g - mean_g;
        double db = pixels[i].b - mean_b;
        
        var_r += dr * dr;
        var_g += dg * dg;
        var_b += db * db;
    }
    
    var_r /= count;
    var_g /= count;
    var_b /= count;
    
    // Normalize to 0-1 range
    var_r = std::sqrt(var_r) / 255.0;
    var_g = std::sqrt(var_g) / 255.0;
    var_b = std::sqrt(var_b) / 255.0;
}

bool VarianceCalculator::should_subdivide(const ColorData& data, double threshold) {
    return calculate_variance(data) > threshold;
}

double VarianceCalculator::calculate_mean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
}

} // namespace spectre
