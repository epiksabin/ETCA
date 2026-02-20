#include "etca_format.h"
#include "image_io.h"
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
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

// when bro says c++ is easy
// the easy in question

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
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            input_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if (arg == "--lossless") {
            lossless = true;
        } else if (arg == "--quality" && i + 1 < argc) {
            quality = std::stof(argv[++i]);
        } else if (arg == "--author" && i + 1 < argc) {
            author = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
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
        std::cout << "Compressing '" << input_file << "' to '" << output_file << "' This might take a while\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        etca::EtcaMetadata metadata;
        if (!author.empty()) {
            metadata.set("author", author);
        }
        metadata.set("compression_mode", lossless ? "lossless" : "lossy");
        
        etca::EtcaWriter::write_from_file(input_file, output_file, lossless, quality, metadata);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        std::cout << "Successfully compressed image to .etca format\n";
        std::cout << "Compression time: " << format_time(elapsed) << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
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
            num_threads = std::stoi(argv[++i]);
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
        std::cout << "Decompressing '" << input_file << "' to '" << output_file << "'...\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        etca::EtcaReader::read_to_file(input_file, output_file);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();
        
        std::cout << "Successfully decompressed .etca file\n";
        std::cout << "Decompression time: " << format_time(elapsed) << "\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
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
        
        std::cout << "ETCA File Information\n";
        std::cout << "====================\n";
        std::cout << "File: " << input_file << "\n";
        std::cout << "Format version: " << static_cast<int>(etca_file.header.format_version) << "\n";
        std::cout << "Compression mode: " 
                  << (etca_file.header.compression_mode == etca::CompressionMode::LOSSY ? "Lossy" : "Lossless") << "\n";
        std::cout << "Image dimensions: " << etca_file.header.width << " x " << etca_file.header.height << "\n";
        std::cout << "Color depth: " << std::hex << static_cast<int>(etca_file.header.color_depth) << std::dec << "-bit\n";
        
        if (etca_file.header.metadata_size > 0) {
            std::cout << "\nMetadata:\n";
            std::cout << "------------------\n";
            std::cout << "Size: " << etca_file.header.metadata_size << " bytes\n";
            // TODO: Print individual metadata entries
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
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
