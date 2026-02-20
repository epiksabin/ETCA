#ifndef SPECTRUM_ANALYZER_H
#define SPECTRUM_ANALYZER_H

#include "spectre_tree.h"
#include "hierarchical_address.h"
#include <vector>
#include <complex>
#include <cstdint>

namespace spectre {

/**
 * @brief Analyzes the frequency spectrum of tilings
 * 
 * Demonstrates that aperiodic tilings (Spectre) have continuous spectrum like white noise,
 * while periodic tilings (squares) have discrete frequency peaks.
 * 
 * This supports the mathematical claim:
 * "Aperiodic tilings have continuous spectrum like white noise vs. periodic tilings with discrete spikes."
 */
class SpectrumAnalyzer {
public:
    /**
     * @brief Represents a frequency spectrum
     */
    struct Spectrum {
        std::vector<double> magnitude;  // Magnitude at each frequency
        std::vector<double> frequencies; // Frequency bins
        double peak_frequency = 0.0;
        double peak_magnitude = 0.0;
        bool has_discrete_peaks = false;  // True for periodic, false for aperiodic
        uint32_t peak_count = 0;  // Number of distinct peaks
    };

    /**
     * @brief Generate tile positions for aperiodic (Spectre) tiling
     * @param grid_size Size of the region to tile (grid_size x grid_size)
     * @param depth Inflation depth for Spectre tiling
     * @return Vector of tile center positions (x, y)
     */
    static std::vector<std::pair<double, double>> generate_aperiodic_tile_positions(
        uint32_t grid_size,
        int depth
    );

    /**
     * @brief Generate tile positions for periodic (square) tiling
     * @param grid_size Size of the region to tile (grid_size x grid_size)
     * @param tile_size Size of each square tile
     * @return Vector of tile center positions (x, y)
     */
    static std::vector<std::pair<double, double>> generate_periodic_tile_positions(
        uint32_t grid_size,
        uint32_t tile_size
    );

    /**
     * @brief Compute 1D spatial frequency spectrum via Fourier analysis
     * @param tile_positions Vector of tile center positions
     * @param num_frequencies Number of frequency bins to compute
     * @return Spectrum with magnitude, frequencies, and spectral characteristics
     */
    static Spectrum compute_spatial_spectrum(
        const std::vector<std::pair<double, double>>& tile_positions,
        uint32_t num_frequencies = 128
    );

    /**
     * @brief Detect discrete peaks in the spectrum
     * @param spectrum The computed spectrum
     * @param threshold Peak detection sensitivity (0.0-1.0)
     * @return Peak frequencies and their magnitudes
     */
    static std::vector<std::pair<double, double>> detect_peaks(
        const Spectrum& spectrum,
        double threshold = 0.3
    );

    /**
     * @brief Calculate spectral entropy (continuous vs. discrete indicator)
     * @param spectrum The computed spectrum
     * @return Entropy value: high = continuous (aperiodic), low = discrete (periodic)
     */
    static double calculate_spectral_entropy(const Spectrum& spectrum);

    /**
     * @brief Compare aperiodic vs periodic spectrum visually (text-based histogram)
     * @param aperiodic_spectrum Spectrum from Spectre tiling
     * @param periodic_spectrum Spectrum from square tiling
     */
    static void print_spectrum_comparison(
        const Spectrum& aperiodic_spectrum,
        const Spectrum& periodic_spectrum
    );

    /**
     * @brief Export spectrum data to file for visualization
     * @param spectrum The computed spectrum
     * @param filename Output file path
     */
    static void export_spectrum_to_csv(
        const Spectrum& spectrum,
        const std::string& filename
    );

private:
    /**
     * @brief Compute 1D Discrete Fourier Transform
     * @param data Input signal samples
     * @return Complex DFT values
     */
    static std::vector<std::complex<double>> dft_1d(const std::vector<double>& data);

    /**
     * @brief Sample tile positions along a line to create 1D signal for FFT
     */
    static std::vector<double> sample_positions_to_1d_signal(
        const std::vector<std::pair<double, double>>& positions,
        uint32_t num_samples
    );
};

} // namespace spectre

#endif // SPECTRUM_ANALYZER_H
