// etca_cli.cpp
#include "etca_format.h"
#include "image_io.h"
#include "progress_bar.h"
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <thread>
#include <atomic>
#include <fstream>
#if ETCA_OPENMP
#include <omp.h>
#endif

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <command> [options]\n\n"
              << "Commands:\n"
              << "  compress    Compress an image to .etca format\n"
              << "  decompress  Decompress a .etca file to image format\n"
              << "  info        Display information about a .etca file\n"
              << "\nCompress options:\n"
              << "  -i, --input <file>          Input image file (PPM or PNG)\n"
              << "  -o, --output <file>         Output .etca file (auto-generated if omitted)\n"
              << "  --lossless                  Use lossless compression (default: lossy)\n"
              << "  --quality <0.0-100.0>       Compression quality (default: 10.0)\n"
              << "  --fast                      Faster compression (skip slower codecs, may be slightly larger)\n"
              << "  --author <name>             Author metadata\n"
              << "  --threads <number>          Number of threads to use (default: all available)\n"
              << "\nDecompress options:\n"
              << "  -i, --input <file>          Input .etca file\n"
              << "  -o, --output <file>         Output image file (PPM or PNG)\n"
              << "  --threads <number>          Number of threads to use (default: all available)\n"
              << "\nInfo options:\n"
              << "  -i, --input <file>          Input .etca file\n"
              << "\nExamples:\n"
              << "  " << program_name << " compress -i photo.ppm -o photo.etca --quality 20\n"
              << "  " << program_name << " decompress -i photo.etca -o output.ppm\n"
              << "  " << program_name << " info -i photo.etca\n";
}

std::string format_bytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

std::string format_time(double seconds) {
    if (seconds < 60) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << seconds << "s";
        return oss.str();
    }
    int minutes = static_cast<int>(seconds / 60);
    double secs = seconds - (minutes * 60);
    std::ostringstream oss;
    oss << minutes << "m " << std::fixed << std::setprecision(1) << secs << "s";
    return oss.str();
}

std::string estimate_eta(double elapsed, double progress) {
    if (progress <= 0.0) return "calculating...";
    double total_time = elapsed / progress;
    double remaining = total_time - elapsed;
    return format_time(remaining);
}

int cmd_compress(int argc, char** argv) {
    std::string input_file, output_file, author;
    bool lossless = false;
    float quality = 10.0f;
    int num_threads = -1;  // -1 = use all available
    bool prefer_speed = false;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--lossless") {
            lossless = true;
        } else if (arg == "--fast") {
            prefer_speed = true;
        } else if (arg == "--quality" && i + 1 < argc) {
            try {
                quality = std::stof(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Error: invalid --quality value (expected float 0.0-100.0)\n";
                return 1;
            }
        } else if (arg == "--author" && i + 1 < argc) {
            author = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                num_threads = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Error: invalid --threads value (expected integer)\n";
                return 1;
            }
        }
    }
    
    if (num_threads > 0) {
#if ETCA_OPENMP
        omp_set_num_threads(num_threads);
#endif
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: --input is required\n";
        return 1;
    }
    
    if (output_file.empty()) {
        size_t dot_pos = input_file.find_last_of('.');
        if (dot_pos != std::string::npos) {
            output_file = input_file.substr(0, dot_pos) + ".etca";
        } else {
            output_file = input_file + ".etca";
        }
    }
    
    try {
        std::cout << "\033[1m\033[36m" << "ETCA Compression" << "\033[0m\n";
        std::cout << "Input:  " << input_file << "\n";
        std::cout << "Output: " << output_file << "\n";
        std::cout << "Mode:   " << (lossless ? "\033[32mLossless\033[0m" : "\033[33mLossy\033[0m") << "\n";
        if (!lossless) {
            std::cout << "Quality: " << quality << "\n";
        }
        std::cout << "\n";
        
        etca::ProgressBar progress("Compressing", 100);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        std::atomic<bool> compression_done(false);
        
        // Simulate progress updates in a separate thread
        std::thread progress_thread([&progress, &compression_done]() {
            while (!compression_done) {
                // Simulate progress: start fast, slow down, then speed up
                double elapsed = progress.get_elapsed();
                double estimated_total = 5.0;  // Rough estimate, will adjust
                
                if (elapsed < estimated_total) {
                    // Exponential progress simulation
                    double simulated_progress = 1.0 - std::exp(-elapsed / (estimated_total * 0.7));
                    progress.update(simulated_progress);
                } else {
                    progress.update(0.95);  // Almost done
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        
        etca::EtcaMetadata metadata;
        if (!author.empty()) {
            metadata.set("author", author);
        }
        metadata.set("compression_mode", lossless ? "lossless" : "lossy");
        
        etca::EtcaWriter::write_from_file(input_file, output_file, lossless, quality, metadata, prefer_speed);
        
        compression_done = true;
        progress.complete();
        progress_thread.join();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        // Get file sizes for stats
        std::ifstream in_file(input_file, std::ios::binary | std::ios::ate);
        std::ifstream out_file(output_file, std::ios::binary | std::ios::ate);
        size_t input_size = in_file.tellg();
        size_t output_size = out_file.tellg();
        in_file.close();
        out_file.close();
        
        std::cout << "\033[1m\033[32m✓ Successfully compressed!\033[0m\n";
        std::cout << "─────────────────────────────────────────\n";
        std::cout << "Time:        " << format_time(elapsed) << "\n";
        std::cout << "Input size:  " << format_bytes(input_size) << "\n";
        std::cout << "Output size: " << format_bytes(output_size) << "\n";
        if (output_size > 0 && input_size > 0) {
            double ratio = static_cast<double>(input_size) / output_size;
            std::cout << "Ratio:       " << std::fixed << std::setprecision(2) << ratio << "x";
            if (ratio > 1.0) {
                std::cout << " \033[32m(compressed)\033[0m";
            } else {
                std::cout << " \033[33m(expanded)\033[0m";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[1m\033[31m✗ Error:\033[0m " << e.what() << "\n";
        return 1;
    }
}

int cmd_decompress(int argc, char** argv) {
    std::string input_file, output_file;
    int num_threads = -1;  // -1 = use all available
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            try {
                num_threads = std::stoi(argv[++i]);
            } catch (const std::exception&) {
                std::cerr << "Error: invalid --threads value (expected integer)\n";
                return 1;
            }
        }
    }
    
    if (num_threads > 0) {
#if ETCA_OPENMP
        omp_set_num_threads(num_threads);
#endif
    }
    
    if (input_file.empty() || output_file.empty()) {
        std::cerr << "Error: --input and --output are required\n";
        return 1;
    }
    
    try {
        std::cout << "\033[1m\033[36m" << "ETCA Decompression" << "\033[0m\n";
        std::cout << "Input:  " << input_file << "\n";
        std::cout << "Output: " << output_file << "\n";
        std::cout << "\n";
        
        etca::ProgressBar progress("Decompressing", 100);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        std::atomic<bool> decompression_done(false);
        
        // Simulate progress updates
        std::thread progress_thread([&progress, &decompression_done]() {
            while (!decompression_done) {
                double elapsed = progress.get_elapsed();
                double estimated_total = 3.0;  // Rough estimate
                
                if (elapsed < estimated_total) {
                    double simulated_progress = 1.0 - std::exp(-elapsed / (estimated_total * 0.7));
                    progress.update(simulated_progress);
                } else {
                    progress.update(0.95);
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
        
        etca::EtcaReader::read_to_file(input_file, output_file);
        
        decompression_done = true;
        progress.complete();
        progress_thread.join();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        std::cout << "\033[1m\033[32m✓ Successfully decompressed!\033[0m\n";
        std::cout << "─────────────────────────────────────────\n";
        std::cout << "Time: " << format_time(elapsed) << "\n";
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[1m\033[31m✗ Error:\033[0m " << e.what() << "\n";
        return 1;
    }
}

int cmd_info(int argc, char** argv) {
    std::string input_file;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            input_file = argv[++i];
        }
    }
    
    if (input_file.empty()) {
        std::cerr << "Error: --input is required\n";
        return 1;
    }
    
    try {
        auto etca_file = etca::EtcaReader::read_header_and_metadata(input_file);
        
        // Get file size
        std::ifstream file(input_file, std::ios::binary | std::ios::ate);
        size_t file_size = file.tellg();
        file.close();
        
        std::cout << "\033[1m\033[36m" << "ETCA File Information" << "\033[0m\n";
        std::cout << "════════════════════════════════════════\n";
        std::cout << "\033[1mFile:\033[0m           " << input_file << "\n";
        std::cout << "\033[1mSize:\033[0m           " << format_bytes(file_size) << "\n";
        std::cout << "\033[1mFormat version:\033[0m  " << static_cast<int>(etca_file.header.format_version) << "\n";
        std::cout << "\033[1mCompression:\033[0m     " 
                  << (etca_file.header.compression_mode == etca::CompressionMode::LOSSY 
                      ? "\033[33mLossy\033[0m" : "\033[32mLossless\033[0m") << "\n";
        std::cout << "\033[1mDimensions:\033[0m     " << etca_file.header.width 
                  << " × " << etca_file.header.height << " px\n";
        std::cout << "\033[1mColor depth:\033[0m     " << std::hex << static_cast<int>(etca_file.header.color_depth) 
                  << std::dec << "-bit RGB\n";
        
        if (etca_file.header.metadata_size > 0) {
            std::cout << "\n\033[1mMetadata:\033[0m\n";
            std::cout << "─────────────────────────────────────────\n";
            std::cout << "Size: " << format_bytes(etca_file.header.metadata_size) << "\n";
            // TODO: Print individual metadata entries
        }
        
        std::cout << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[1m\033[31m✗ Error:\033[0m " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "compress") {
        return cmd_compress(argc, argv);
    } else if (command == "decompress") {
        return cmd_decompress(argc, argv);
    } else if (command == "info") {
        return cmd_info(argc, argv);
    } else if (command == "--help" || command == "-h") {
        print_usage(argv[0]);
        return 0;
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        print_usage(argv[0]);
        return 1;
    }
}
