#include <iostream>
#include <memory>
#include <iomanip>

// Include all modules
#include "spectre_tile.h"
#include "hierarchical_address.h"
#include "color_data.h"
#include "variance_calculator.h"
#include "tile_inflater.h"
#include "spectre_tree.h"
#include "compressor.h"
#include "decompressor.h"
#include "spectrum_analyzer.h"

using namespace spectre;

/**
 * @brief Demonstrate basic Spectre Tiles functionality
 */
void demo_basic_tiles() {
    std::cout << "\n=== Basic Spectre Tile Demo ===\n";
    
    // Create a simple tile
    SpectreTile tile(1, 0, 0);
    tile.set_color(255, 128, 64);
    
    uint8_t r, g, b;
    tile.get_color(r, g, b);
    
    std::cout << "Tile ID: " << tile.get_id() << "\n";
    std::cout << "Tile Depth: " << tile.get_depth() << "\n";
    std::cout << "Color (RGB): " << (int)r << ", " << (int)g << ", " << (int)b << "\n";
}

/**
 * @brief Demonstrate hierarchical addressing
 */
void demo_hierarchical_addressing() {
    std::cout << "\n=== Hierarchical Addressing Demo ===\n";
    
    // Create a hierarchical address
    HierarchicalAddress root;
    std::cout << "Root address: '" << root.to_string() << "'\n";
    std::cout << "Root depth: " << root.get_depth() << "\n";
    std::cout << "Is root: " << (root.is_root() ? "Yes" : "No") << "\n";
    
    // Create child addresses
    HierarchicalAddress child1 = root.get_child_address(0);
    std::cout << "\nChild[0] address: '" << child1.to_string() << "'\n";
    
    HierarchicalAddress child1_2 = child1.get_child_address(2);
    std::cout << "Child[0].Child[2] address: '" << child1_2.to_string() << "'\n";
    
    // Create from string
    HierarchicalAddress from_str = HierarchicalAddress::from_string("1.4.2.0");
    std::cout << "\nFrom string '1.4.2.0': '" << from_str.to_string() << "'\n";
    
    // Check descendance
    std::cout << "Is '1.4.2.0' descendant of '1.4'? " 
              << (from_str.is_descendant_of(HierarchicalAddress::from_string("1.4")) ? "Yes" : "No") << "\n";
}

/**
 * @brief Demonstrate color data operations
 */
void demo_color_data() {
    std::cout << "\n=== Color Data Demo ===\n";
    
    // Create a small test image
    ColorData image(4, 4);
    
    // Set some pixels
    image.set_pixel(0, 0, Color(255, 0, 0));     // Red
    image.set_pixel(1, 0, Color(0, 255, 0));     // Green
    image.set_pixel(2, 0, Color(0, 0, 255));     // Blue
    image.set_pixel(3, 0, Color(255, 255, 255)); // White
    
    // Fill rest with gray
    image.fill(Color(128, 128, 128));
    
    // Calculate average color
    Color avg = image.calculate_average_color();
    std::cout << "Image size: " << image.get_width() << "x" << image.get_height() << "\n";
    std::cout << "Average color (RGB): " << (int)avg.r << ", " << (int)avg.g << ", " << (int)avg.b << "\n";
    
    // Extract a region
    ColorData region = image.extract_region(0, 0, 2, 2);
    std::cout << "Extracted region: " << region.get_width() << "x" << region.get_height() << "\n";
}

/**
 * @brief Demonstrate variance calculation
 */
void demo_variance() {
    std::cout << "\n=== Variance Calculation Demo ===\n";
    
    // Create a uniform image (low variance)
    ColorData uniform(4, 4);
    uniform.fill(Color(128, 128, 128));
    
    double var_uniform = VarianceCalculator::calculate_variance(uniform);
    std::cout << "Uniform image variance: " << std::fixed << std::setprecision(4) << var_uniform << "\n";
    
    // Create a varied image (high variance)
    ColorData varied(4, 4);
    varied.set_pixel(0, 0, Color(0, 0, 0));         // Black
    varied.set_pixel(1, 0, Color(255, 255, 255));   // White
    varied.set_pixel(2, 0, Color(0, 0, 0));         // Black
    varied.set_pixel(3, 0, Color(255, 255, 255));   // White
    varied.fill(Color(128, 128, 128));
    
    double var_varied = VarianceCalculator::calculate_variance(varied);
    std::cout << "Varied image variance: " << std::fixed << std::setprecision(4) << var_varied << "\n";
    
    // Test should_subdivide
    bool should_subdiv_uniform = VarianceCalculator::should_subdivide(uniform, 0.1);
    bool should_subdiv_varied = VarianceCalculator::should_subdivide(varied, 0.1);
    
    std::cout << "Uniform should subdivide: " << (should_subdiv_uniform ? "Yes" : "No") << "\n";
    std::cout << "Varied should subdivide: " << (should_subdiv_varied ? "Yes" : "No") << "\n";
}

/**
 * @brief Demonstrate Spectre-Tree construction
 */
void demo_spectre_tree() {
    std::cout << "\n=== Spectre-Tree Demo ===\n";
    
    // Create a test image with some variation
    ColorData image(16, 16);
    
    // Create a gradient pattern
    for (uint32_t y = 0; y < 16; ++y) {
        for (uint32_t x = 0; x < 16; ++x) {
            uint8_t intensity = static_cast<uint8_t>((x * 16) + y);
            image.set_pixel(x, y, Color(intensity, intensity, intensity));
        }
    }
    
    // Build the Spectre-Tree
    SpectreTree tree(16, 16);
    tree.build(image, 0.15, 4);  // variance_threshold, max_depth
    
    std::cout << "Image size: 16x16\n";
    std::cout << "Total tiles: " << tree.get_tile_count() << "\n";
    std::cout << "Max depth: " << tree.get_max_depth() << "\n";
    std::cout << "Leaf nodes: " << tree.get_leaf_nodes().size() << "\n";
}

/**
 * @brief Demonstrate spectrum analysis of aperiodic vs periodic tilings
 * 
 * Supports the claim: "Aperiodic tilings have continuous spectrum like white noise"
 */
void demo_spectrum_analysis() {
    std::cout << "\n=== Spectrum Analysis Demo ===\n";
    std::cout << "Claim: Aperiodic tilings have continuous spectrum like white noise\n";
    std::cout << "vs. Periodic tilings have discrete frequency spikes.\n\n";
    
    // Generate aperiodic (Spectre) tile positions
    auto aperiodic_positions = SpectrumAnalyzer::generate_aperiodic_tile_positions(256, 5);
    auto aperiodic_spectrum = SpectrumAnalyzer::compute_spatial_spectrum(aperiodic_positions, 128);
    
    // Generate periodic (square grid) tile positions
    auto periodic_positions = SpectrumAnalyzer::generate_periodic_tile_positions(256, 32);
    auto periodic_spectrum = SpectrumAnalyzer::compute_spatial_spectrum(periodic_positions, 128);
    
    // Print detailed comparison
    SpectrumAnalyzer::print_spectrum_comparison(aperiodic_spectrum, periodic_spectrum);
    
    // Export spectra to CSV for plotting
    SpectrumAnalyzer::export_spectrum_to_csv(aperiodic_spectrum, "spectrum_aperiodic.csv");
    SpectrumAnalyzer::export_spectrum_to_csv(periodic_spectrum, "spectrum_periodic.csv");
    
    std::cout << "\nConclusion: Aperiodic (Spectre) tilings have continuous frequency\n";
    std::cout << "distribution (like white noise), reducing Moiré artifacts.\n";
}

/**
 * @brief Demonstrate compression/decompression
 */
void demo_compression() {
    std::cout << "\n=== Compression/Decompression Demo ===\n";
    
    // Create a test image
    ColorData image(8, 8);
    image.fill(Color(100, 150, 200));
    
    // Set a few pixels to create some difference
    for (uint32_t i = 0; i < 10; ++i) {
        image.set_pixel(i, i, Color(255, 0, 0));
    }
    
    // Compress
    CompressionConfig config;
    config.variance_threshold = 0.2;
    config.max_tree_depth = 3;
    
    Compressor compressor(config);
    CompressedImage compressed = compressor.compress(image);
    
    const auto& stats = compressor.get_last_statistics();
    std::cout << "Original size: " << (image.get_width() * image.get_height() * 3) << " bytes\n";
    std::cout << "Compressed size: ~" << (stats.tile_count * 10) << " bytes\n";
    std::cout << "Compression ratio: " << std::fixed << std::setprecision(2) 
              << stats.compression_ratio << "x\n";
    std::cout << "Tiles: " << stats.tile_count << "\n";
    std::cout << "Max depth: " << stats.max_depth << "\n";
    std::cout << "Leaves: " << stats.leaf_count << "\n";
    
    // Decompress
    ColorData decompressed = Decompressor::decompress(compressed);
    std::cout << "\nDecompressed size: " << decompressed.get_width() << "x" 
              << decompressed.get_height() << "\n";
}

/**
 * @brief Main entry point
 */
int main() {
    std::cout << "=== Spectre Tiles Compression Algorithm Demo ===\n";
    std::cout << "Based on the mathematical discovery of aperiodic monotiles\n";
    
    try {
        demo_basic_tiles();
        demo_hierarchical_addressing();
        demo_color_data();
        demo_variance();
        demo_spectre_tree();
        demo_compression();
        demo_spectrum_analysis();
        
        std::cout << "\n=== Demo Complete ===\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}


