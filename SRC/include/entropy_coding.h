#ifndef ENTROPY_CODING_H
#define ENTROPY_CODING_H

#include <vector>
#include <cstdint>
#include <memory>
#include <map>

namespace spectre {

/**
 * @brief Entropy coding format types
 */
enum class EntropyCodec : uint8_t {
    NONE = 0x00,           ///< No compression
    RLE = 0x01,            ///< Run-Length Encoding (legacy)
    DEFLATE = 0x02,        ///< LZ77 + Huffman (streaming)
    ADVANCED = 0x03        ///< LZ77 + Delta + Huffman adaptive
};

/**
 * @brief Compression statistics
 */
struct CompressionStats {
    size_t original_size = 0;
    size_t compressed_size = 0;
    float compression_ratio = 0.0f;
    EntropyCodec codec_used = EntropyCodec::NONE;
    
    float get_savings_percent() const {
        if (original_size == 0) return 0.0f;
        return (1.0f - (static_cast<float>(compressed_size) / static_cast<float>(original_size))) * 100.0f;
    }
};

/**
 * @brief Huffman tree node for encoding/decoding
 */
struct HuffmanNode {
    uint8_t value;
    uint32_t frequency;
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;
    
    HuffmanNode(uint8_t v, uint32_t f) : value(v), frequency(f), left(nullptr), right(nullptr) {}
};

/**
 * @brief Abstract entropy codec interface
 */
class EntropyCodec_Base {
public:
    virtual ~EntropyCodec_Base() = default;
    
    /**
     * @brief Encode/compress data
     * @param input Raw input data
     * @return Compressed data with codec type prepended
     */
    virtual std::vector<uint8_t> encode(const std::vector<uint8_t>& input) = 0;
    
    /**
     * @brief Decode/decompress data (expects codec type byte prefix)
     * @param input Compressed data with codec type
     * @return Original uncompressed data
     */
    virtual std::vector<uint8_t> decode(const std::vector<uint8_t>& input) = 0;
    
    /**
     * @brief Get compression statistics from last operation
     */
    virtual const CompressionStats& get_stats() const = 0;
};

/**
 * @brief Run-Length Encoding with variable-length runs
 */
class RLECodec : public EntropyCodec_Base {
public:
    std::vector<uint8_t> encode(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decode(const std::vector<uint8_t>& input) override;
    const CompressionStats& get_stats() const override { return stats_; }

private:
    CompressionStats stats_;
};

/**
 * @brief Huffman Coding
 * Single-pass Huffman encoder for high entropy reduction
 */
class HuffmanCodec : public EntropyCodec_Base {
public:
    std::vector<uint8_t> encode(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decode(const std::vector<uint8_t>& input) override;
    const CompressionStats& get_stats() const override { return stats_; }

private:
    CompressionStats stats_;
    std::map<uint8_t, std::string> huffman_codes_;
    
    /**
     * @brief Build Huffman tree from frequency analysis
     */
    std::shared_ptr<HuffmanNode> build_tree(const std::vector<uint32_t>& frequencies);
    
    /**
     * @brief Generate bit codes for each byte value
     */
    void generate_codes(const std::shared_ptr<HuffmanNode>& node, 
                        const std::string& code,
                        std::map<uint8_t, std::string>& codes);
};

/**
 * @brief Deflate-like compression (LZ77 + Huffman)
 * Combines sliding window pattern matching with Huffman coding
 */
class DeflateCodec : public EntropyCodec_Base {
public:
    DeflateCodec(uint16_t window_size = 32768, uint16_t max_match_len = 258);
    
    std::vector<uint8_t> encode(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decode(const std::vector<uint8_t>& input) override;
    const CompressionStats& get_stats() const override { return stats_; }

private:
    CompressionStats stats_;
    uint16_t window_size_;
    uint16_t max_match_len_;
    
    /**
     * @brief Find the best match in sliding window
     * @return {length, distance} of best match, or {0, 0} if no match found
     */
    std::pair<uint16_t, uint16_t> find_match(
        const std::vector<uint8_t>& data,
        size_t pos,
        uint16_t window_size
    );
};

/**
 * @brief Advanced codec with Delta encoding + LZ77 + Huffman
 * Optimized for tile-based image data with adaptive encoding
 */
class AdvancedCodec : public EntropyCodec_Base {
public:
    std::vector<uint8_t> encode(const std::vector<uint8_t>& input) override;
    std::vector<uint8_t> decode(const std::vector<uint8_t>& input) override;
    const CompressionStats& get_stats() const override { return stats_; }

private:
    CompressionStats stats_;
    
    /**
     * @brief Apply delta encoding to tile color data
     * Reduces entropy by storing differences instead of absolute values
     */
    std::vector<uint8_t> delta_encode(const std::vector<uint8_t>& input);
    
    /**
     * @brief Reverse delta encoding
     */
    std::vector<uint8_t> delta_decode(const std::vector<uint8_t>& input);
};

/**
 * @brief Adaptive entropy encoder - tries multiple codecs and picks the best
 */
class AdaptiveEncoder {
public:
    /**
     * @brief Encode data using the best available codec
     * @param input Raw data to compress
     * @param prefer_speed If true, skips slower codecs like Huffman
     * @return Compressed data with codec type prefix
     */
    static std::vector<uint8_t> encode(
        const std::vector<uint8_t>& input,
        bool prefer_speed = false
    );
    
    /**
     * @brief Decode data (automatically detects codec from prefix)
     * @param input Compressed data with codec type prefix
     * @return Original uncompressed data
     */
    static std::vector<uint8_t> decode(const std::vector<uint8_t>& input);
    
    /**
     * @brief Get statistics from last encoding
     */
    static const CompressionStats& get_stats();

private:
    static CompressionStats last_stats_;
};

} // namespace spectre

#endif // ENTROPY_CODING_H
