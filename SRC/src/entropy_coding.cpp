// entropy_coding.cpp
#include "entropy_coding.h"
#include <algorithm>
#include <queue>
#include <bitset>
#include <cstring>
#include <iostream>

namespace spectre {

// ============================================================================
// RLECodec Implementation
// ============================================================================

std::vector<uint8_t> RLECodec::encode(const std::vector<uint8_t>& input) {
    stats_.original_size = input.size();
    
    if (input.empty()) {
        stats_.compressed_size = 1;
        return {static_cast<uint8_t>(EntropyCodec::RLE)};
    }
    
    std::vector<uint8_t> encoded;
    encoded.push_back(static_cast<uint8_t>(EntropyCodec::RLE));
    
    const uint8_t RLE_MARKER = 0xFF;
    size_t i = 0;
    
    while (i < input.size()) {
        uint8_t current = input[i];
        size_t run_length = 1;
        
        // Count consecutive identical bytes (max 255 in single run)
        while (i + run_length < input.size() && 
               input[i + run_length] == current && 
               run_length < 255) {
            run_length++;
        }
        
        if (run_length >= 4) {
            // Encode as RLE: MARKER | byte_value | count
            encoded.push_back(RLE_MARKER);
            encoded.push_back(current);
            encoded.push_back(static_cast<uint8_t>(run_length));
            i += run_length;
        } else if (current == RLE_MARKER) {
            // Escape marker: MARKER | MARKER
            encoded.push_back(RLE_MARKER);
            encoded.push_back(RLE_MARKER);
            i++;
        } else {
            // Literal byte
            encoded.push_back(current);
            i++;
        }
    }
    
    stats_.compressed_size = encoded.size();
    stats_.codec_used = EntropyCodec::RLE;
    stats_.compression_ratio = static_cast<float>(stats_.original_size) / 
                               std::max(1.0f, static_cast<float>(stats_.compressed_size));
    
    return encoded;
}

std::vector<uint8_t> RLECodec::decode(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> decoded;
    
    if (input.empty() || input[0] != static_cast<uint8_t>(EntropyCodec::RLE)) {
        return decoded;
    }
    
    const uint8_t RLE_MARKER = 0xFF;
    size_t i = 1;  // Skip codec type byte
    
    while (i < input.size()) {
        if (input[i] == RLE_MARKER) {
            if (i + 1 >= input.size()) break;
            
            if (input[i + 1] == RLE_MARKER) {
                // Escaped marker
                decoded.push_back(RLE_MARKER);
                i += 2;
            } else if (i + 2 < input.size()) {
                // RLE sequence: MARKER | byte_value | count
                uint8_t value = input[i + 1];
                uint8_t count = input[i + 2];
                for (int j = 0; j < count; ++j) {
                    decoded.push_back(value);
                }
                i += 3;
            } else {
                break;
            }
        } else {
            // Literal byte
            decoded.push_back(input[i]);
            i++;
        }
    }
    
    return decoded;
}

// ============================================================================
// HuffmanCodec Implementation
// ============================================================================

/**
 * @brief Bit writer helper for Huffman encoding
 */
class BitWriter {
private:
    std::vector<uint8_t>& output_;
    uint8_t current_byte_;
    int bits_in_byte_;
    
public:
    BitWriter(std::vector<uint8_t>& output) 
        : output_(output), current_byte_(0), bits_in_byte_(0) {}
    
    void write_bit(bool bit) {
        if (bit) {
            current_byte_ |= (1 << (7 - bits_in_byte_));
        }
        bits_in_byte_++;
        
        if (bits_in_byte_ == 8) {
            output_.push_back(current_byte_);
            current_byte_ = 0;
            bits_in_byte_ = 0;
        }
    }
    
    void write_bits(const std::string& bits) {
        for (char c : bits) {
            write_bit(c == '1');
        }
    }
    
    void flush() {
        if (bits_in_byte_ > 0) {
            output_.push_back(current_byte_);
            current_byte_ = 0;
            bits_in_byte_ = 0;
        }
    }
    
    int get_pending_bits() const {
        return bits_in_byte_;
    }
};

/**
 * @brief Bit reader helper for Huffman decoding
 */
class BitReader {
private:
    const std::vector<uint8_t>& input_;
    size_t byte_pos_;
    int bit_pos_;
    
public:
    BitReader(const std::vector<uint8_t>& input, size_t start_pos = 0)
        : input_(input), byte_pos_(start_pos), bit_pos_(0) {}
    
    bool read_bit() {
        if (byte_pos_ >= input_.size()) {
            return false;  // End of stream
        }
        
        bool bit = (input_[byte_pos_] >> (7 - bit_pos_)) & 1;
        bit_pos_++;
        
        if (bit_pos_ == 8) {
            byte_pos_++;
            bit_pos_ = 0;
        }
        
        return bit;
    }
    
    bool eof() const {
        return byte_pos_ >= input_.size();
    }
    
    size_t get_position() const {
        return byte_pos_;
    }
};

std::shared_ptr<HuffmanNode> HuffmanCodec::build_tree(const std::vector<uint32_t>& frequencies) {
    // Priority queue: {frequency, node}
    auto cmp = [](const std::pair<uint32_t, std::shared_ptr<HuffmanNode>>& a,
                  const std::pair<uint32_t, std::shared_ptr<HuffmanNode>>& b) {
        return a.first > b.first;  // Min-heap
    };
    std::priority_queue<std::pair<uint32_t, std::shared_ptr<HuffmanNode>>,
                        std::vector<std::pair<uint32_t, std::shared_ptr<HuffmanNode>>>,
                        decltype(cmp)> pq(cmp);
    
    // Create leaf nodes for all bytes that appear in the data
    for (size_t i = 0; i < frequencies.size(); ++i) {
        if (frequencies[i] > 0) {
            auto node = std::make_shared<HuffmanNode>(static_cast<uint8_t>(i), frequencies[i]);
            pq.push({frequencies[i], node});
        }
    }
    
    // Special case: only one unique byte
    if (pq.size() == 1) {
        auto node = pq.top().second;
        auto root = std::make_shared<HuffmanNode>(0, node->frequency);
        root->left = node;
        return root;
    }
    
    // Build tree bottom-up
    while (pq.size() > 1) {
        auto left = pq.top().second;
        pq.pop();
        auto right = pq.top().second;
        pq.pop();
        
        auto parent = std::make_shared<HuffmanNode>(0, left->frequency + right->frequency);
        parent->left = left;
        parent->right = right;
        
        pq.push({parent->frequency, parent});
    }
    
    return pq.empty() ? nullptr : pq.top().second;
}

void HuffmanCodec::generate_codes(const std::shared_ptr<HuffmanNode>& node,
                                   const std::string& code,
                                   std::map<uint8_t, std::string>& codes) {
    if (!node) return;
    
    if (!node->left && !node->right) {
        // Leaf node
        codes[node->value] = code.empty() ? "0" : code;
        return;
    }
    
    if (node->left) {
        generate_codes(node->left, code + "0", codes);
    }
    if (node->right) {
        generate_codes(node->right, code + "1", codes);
    }
}

void HuffmanCodec::serialize_tree(const std::shared_ptr<HuffmanNode>& node, BitWriter& writer) {
    if (!node) return;
    
    if (!node->left && !node->right) {
        // Leaf node: write 1 bit + 8 bits for value
        writer.write_bit(true);
        for (int i = 7; i >= 0; --i) {
            writer.write_bit((node->value >> i) & 1);
        }
    } else {
        // Internal node: write 0 bit, then serialize children
        writer.write_bit(false);
        serialize_tree(node->left, writer);
        serialize_tree(node->right, writer);
    }
}

std::shared_ptr<HuffmanNode> HuffmanCodec::deserialize_tree(BitReader& reader) {
    if (reader.eof()) {
        return nullptr;
    }
    
    bool is_leaf = reader.read_bit();
    
    if (is_leaf) {
        // Read 8-bit value
        uint8_t value = 0;
        for (int i = 7; i >= 0; --i) {
            if (reader.eof()) return nullptr;
            if (reader.read_bit()) {
                value |= (1 << i);
            }
        }
        return std::make_shared<HuffmanNode>(value, 0);
    } else {
        // Internal node: recursively deserialize children
        auto node = std::make_shared<HuffmanNode>(0, 0);
        node->left = deserialize_tree(reader);
        node->right = deserialize_tree(reader);
        return node;
    }
}

std::vector<uint8_t> HuffmanCodec::encode(const std::vector<uint8_t>& input) {
    stats_.original_size = input.size();
    
    if (input.empty()) {
        stats_.compressed_size = 1;
        return {static_cast<uint8_t>(EntropyCodec::NONE)};
    }
    
    // Analyze byte frequencies
    std::vector<uint32_t> frequencies(256, 0);
    for (uint8_t byte : input) {
        frequencies[byte]++;
    }
    
    // Build Huffman tree
    auto root = build_tree(frequencies);
    if (!root) {
        stats_.compressed_size = input.size() + 1;
        return input;  // Fallback
    }
    
    // Generate codes
    huffman_codes_.clear();
    generate_codes(root, "", huffman_codes_);
    
    // Check if we have any codes (should always have at least one)
    if (huffman_codes_.empty()) {
        stats_.compressed_size = input.size() + 1;
        stats_.codec_used = EntropyCodec::NONE;
        return input;
    }
    
    // Build output: [codec_type(1)] [tree_size(4)] [tree_data] [data_size(4)] [encoded_bits]
    std::vector<uint8_t> output;
    output.push_back(static_cast<uint8_t>(EntropyCodec::HUFFMAN));
    
    // Serialize tree to temporary buffer to get size
    std::vector<uint8_t> tree_data;
    BitWriter tree_writer(tree_data);
    serialize_tree(root, tree_writer);
    tree_writer.flush();
    
    // Write tree size (4 bytes)
    output.push_back((tree_data.size() >> 24) & 0xFF);
    output.push_back((tree_data.size() >> 16) & 0xFF);
    output.push_back((tree_data.size() >> 8) & 0xFF);
    output.push_back(tree_data.size() & 0xFF);
    
    // Write tree data
    output.insert(output.end(), tree_data.begin(), tree_data.end());
    
    // Encode input data using Huffman codes
    std::vector<uint8_t> encoded_bits;
    BitWriter bit_writer(encoded_bits);
    
    for (uint8_t byte : input) {
        auto it = huffman_codes_.find(byte);
        if (it != huffman_codes_.end()) {
            bit_writer.write_bits(it->second);
        } else {
            // Should not happen if tree was built correctly
            stats_.compressed_size = input.size() + 1;
            stats_.codec_used = EntropyCodec::NONE;
            return input;
        }
    }
    
    // Get padding bits before flushing (bits remaining in current byte)
    int padding_bits = bit_writer.get_pending_bits();
    if (padding_bits > 0) {
        padding_bits = 8 - padding_bits;  // Convert to padding count
    }
    bit_writer.flush();
    
    // Write encoded data size (4 bytes)
    output.push_back((encoded_bits.size() >> 24) & 0xFF);
    output.push_back((encoded_bits.size() >> 16) & 0xFF);
    output.push_back((encoded_bits.size() >> 8) & 0xFF);
    output.push_back(encoded_bits.size() & 0xFF);
    
    // Write encoded bits
    output.insert(output.end(), encoded_bits.begin(), encoded_bits.end());
    
    // Store number of padding bits in last byte (1 byte)
    output.push_back(static_cast<uint8_t>(padding_bits));
    
    stats_.compressed_size = output.size();
    stats_.codec_used = EntropyCodec::HUFFMAN;
    stats_.compression_ratio = static_cast<float>(stats_.original_size) / 
                               std::max(1.0f, static_cast<float>(stats_.compressed_size));
    
    return output;
}

std::vector<uint8_t> HuffmanCodec::decode(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> decoded;
    
    if (input.empty() || input[0] != static_cast<uint8_t>(EntropyCodec::HUFFMAN)) {
        return decoded;
    }
    
    if (input.size() < 10) {  // Minimum: codec(1) + tree_size(4) + data_size(4) + padding(1)
        return decoded;
    }
    
    size_t pos = 1;
    
    // Read tree size (4 bytes)
    uint32_t tree_size = (static_cast<uint32_t>(input[pos]) << 24) |
                        (static_cast<uint32_t>(input[pos+1]) << 16) |
                        (static_cast<uint32_t>(input[pos+2]) << 8) |
                        static_cast<uint32_t>(input[pos+3]);
    pos += 4;
    
    if (pos + tree_size > input.size()) {
        return decoded;  // Invalid tree size
    }
    
    // Deserialize tree
    std::vector<uint8_t> tree_data(input.begin() + pos, input.begin() + pos + tree_size);
    BitReader tree_reader(tree_data);
    auto root = deserialize_tree(tree_reader);
    pos += tree_size;
    
    if (!root) {
        return decoded;  // Failed to deserialize tree
    }
    
    if (pos + 4 > input.size()) {
        return decoded;  // Not enough data for size field
    }
    
    // Read encoded data size (4 bytes)
    uint32_t encoded_size = (static_cast<uint32_t>(input[pos]) << 24) |
                           (static_cast<uint32_t>(input[pos+1]) << 16) |
                           (static_cast<uint32_t>(input[pos+2]) << 8) |
                           static_cast<uint32_t>(input[pos+3]);
    pos += 4;
    
    if (pos + encoded_size + 1 > input.size()) {
        return decoded;  // Invalid encoded data size
    }
    
    // Read padding bits (last byte)
    uint8_t padding_bits = input[input.size() - 1];
    
    // Decode using tree traversal
    std::vector<uint8_t> encoded_data(input.begin() + pos, input.begin() + pos + encoded_size);
    BitReader bit_reader(encoded_data);
    
    auto current = root;
    int bits_read = 0;
    int total_bits = encoded_size * 8 - padding_bits;
    
    while (bits_read < total_bits) {
        if (bit_reader.eof()) break;
        
        bool bit = bit_reader.read_bit();
        bits_read++;
        
        if (bit) {
            current = current->right;
        } else {
            current = current->left;
        }
        
        if (!current) {
            return decoded;  // Invalid tree traversal
        }
        
        // Check if we reached a leaf
        if (!current->left && !current->right) {
            decoded.push_back(current->value);
            current = root;  // Reset to root for next symbol
        }
    }
    
    return decoded;
}

// ============================================================================
// DeflateCodec Implementation
// ============================================================================

DeflateCodec::DeflateCodec(uint16_t window_size, uint16_t max_match_len)
    : window_size_(window_size), max_match_len_(max_match_len) {
}

std::pair<uint16_t, uint16_t> DeflateCodec::find_match(
    const std::vector<uint8_t>& data,
    size_t pos,
    uint16_t window_size) {
    
    if (pos == 0) return {0, 0};
    
    size_t window_start = (pos > window_size) ? (pos - window_size) : 0;
    size_t match_len = 0;
    size_t best_distance = 0;
    
    // Try to find matches in the sliding window
    for (size_t i = window_start; i < pos; ++i) {
        size_t len = 0;
        while (pos + len < data.size() && 
               i + len < pos &&
               data[i + len] == data[pos + len] &&
               len < max_match_len_) {
            len++;
        }
        
        if (len > match_len) {
            match_len = len;
            best_distance = pos - i;
        }
    }
    
    // Only return match if it's at least 3 bytes
    if (match_len >= 3) {
        return {static_cast<uint16_t>(match_len), static_cast<uint16_t>(best_distance)};
    } else {
        return {0, 0};
    }
}

std::vector<uint8_t> DeflateCodec::encode(const std::vector<uint8_t>& input) {
    stats_.original_size = input.size();
    
    std::vector<uint8_t> encoded;
    encoded.push_back(static_cast<uint8_t>(EntropyCodec::DEFLATE));
    
    if (input.empty()) {
        stats_.compressed_size = 1;
        return encoded;
    }
    
    size_t i = 0;
    const uint8_t MATCH_MARKER = 0xFF;
    
    while (i < input.size()) {
        auto [match_len, distance] = find_match(input, i, window_size_);
        
        if (match_len > 0) {
            // Encode match: MARKER | length_hi | length_lo | dist_hi | dist_lo
            encoded.push_back(MATCH_MARKER);
            encoded.push_back((match_len >> 8) & 0xFF);
            encoded.push_back(match_len & 0xFF);
            encoded.push_back((distance >> 8) & 0xFF);
            encoded.push_back(distance & 0xFF);
            i += match_len;
        } else if (input[i] == MATCH_MARKER) {
            // Escape marker
            encoded.push_back(MATCH_MARKER);
            encoded.push_back(MATCH_MARKER);
            i++;
        } else {
            // Literal byte
            encoded.push_back(input[i]);
            i++;
        }
    }
    
    stats_.compressed_size = encoded.size();
    stats_.codec_used = EntropyCodec::DEFLATE;
    stats_.compression_ratio = static_cast<float>(stats_.original_size) / 
                               std::max(1.0f, static_cast<float>(stats_.compressed_size));
    
    return encoded;
}

std::vector<uint8_t> DeflateCodec::decode(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> decoded;
    
    if (input.empty() || input[0] != static_cast<uint8_t>(EntropyCodec::DEFLATE)) {
        return decoded;
    }
    
    const uint8_t MATCH_MARKER = 0xFF;
    size_t i = 1;
    
    while (i < input.size()) {
        if (input[i] == MATCH_MARKER) {
            if (i + 1 >= input.size()) break;
            
            if (input[i + 1] == MATCH_MARKER) {
                // Escaped marker
                decoded.push_back(MATCH_MARKER);
                i += 2;
            } else if (i + 4 < input.size()) {
                // Match: MARKER | length_hi | length_lo | dist_hi | dist_lo
                uint16_t len = static_cast<uint16_t>((static_cast<uint16_t>(input[i + 1] & 0xFF) << 8) | static_cast<uint16_t>(input[i + 2] & 0xFF));
                uint16_t dist = static_cast<uint16_t>((static_cast<uint16_t>(input[i + 3] & 0xFF) << 8) | static_cast<uint16_t>(input[i + 4] & 0xFF));
                
                // Prevent distance underflow: dist must not exceed decoded size
                if (dist > decoded.size()) {
                    i += 5;  // Skip invalid match, avoid undefined behavior
                    continue;
                }
                
                // Copy from history at distance 'dist' back, 'len' bytes forward
                size_t src = decoded.size() - dist;
                for (uint16_t j = 0; j < len; ++j) {
                    if (src + j < decoded.size()) {
                        decoded.push_back(decoded[src + j]);
                    }
                }
                
                i += 5;
            } else {
                break;
            }
        } else {
            // Literal byte
            decoded.push_back(input[i]);
            i++;
        }
    }
    
    return decoded;
}

// ============================================================================
// AdvancedCodec Implementation
// ============================================================================

std::vector<uint8_t> AdvancedCodec::delta_encode(const std::vector<uint8_t>& input) {
    if (input.empty()) return input;
    
    std::vector<uint8_t> encoded;
    encoded.push_back(input[0]);  // Store first byte as-is
    
    for (size_t i = 1; i < input.size(); ++i) {
        // Store difference (with wrap-around for uint8)
        uint8_t delta = input[i] - input[i - 1];
        encoded.push_back(delta);
    }
    
    return encoded;
}

std::vector<uint8_t> AdvancedCodec::delta_decode(const std::vector<uint8_t>& input) {
    if (input.empty()) return input;
    
    std::vector<uint8_t> decoded;
    decoded.push_back(input[0]);
    
    for (size_t i = 1; i < input.size(); ++i) {
        // Reconstruct original value from delta
        uint8_t value = decoded[i - 1] + input[i];
        decoded.push_back(value);
    }
    
    return decoded;
}

std::vector<uint8_t> AdvancedCodec::encode(const std::vector<uint8_t>& input) {
    stats_.original_size = input.size();
    
    if (input.empty()) {
        stats_.compressed_size = 1;
        return {static_cast<uint8_t>(EntropyCodec::ADVANCED)};
    }
    
    // Apply delta encoding first
    auto delta_encoded = delta_encode(input);
    
    // Then apply deflate compression
    DeflateCodec deflate(32768, 258);
    auto deflate_result = deflate.encode(delta_encoded);
    
    // Prepend Advanced codec marker
    std::vector<uint8_t> result;
    result.push_back(static_cast<uint8_t>(EntropyCodec::ADVANCED));
    result.insert(result.end(), deflate_result.begin() + 1, deflate_result.end());  // Skip DEFLATE marker
    
    stats_.compressed_size = result.size();
    stats_.codec_used = EntropyCodec::ADVANCED;
    stats_.compression_ratio = static_cast<float>(stats_.original_size) / 
                               std::max(1.0f, static_cast<float>(stats_.compressed_size));
    
    return result;
}

std::vector<uint8_t> AdvancedCodec::decode(const std::vector<uint8_t>& input) {
    if (input.empty() || input[0] != static_cast<uint8_t>(EntropyCodec::ADVANCED)) {
        return {};
    }
    
    // Reconstruct Deflate-encoded data
    std::vector<uint8_t> deflate_input;
    deflate_input.push_back(static_cast<uint8_t>(EntropyCodec::DEFLATE));
    deflate_input.insert(deflate_input.end(), input.begin() + 1, input.end());
    
    // Decode with deflate
    DeflateCodec deflate;
    auto delta_decoded = deflate.decode(deflate_input);
    
    // Decode delta
    return delta_decode(delta_decoded);
}

// ============================================================================
// AdaptiveEncoder Implementation
// ============================================================================

CompressionStats AdaptiveEncoder::last_stats_;

std::vector<uint8_t> AdaptiveEncoder::encode(const std::vector<uint8_t>& input, bool prefer_speed) {
    if (input.empty()) {
        last_stats_ = {0, 1, 0.0f, EntropyCodec::NONE};
        return {static_cast<uint8_t>(EntropyCodec::NONE)};
    }
    
    std::vector<std::pair<std::vector<uint8_t>, const CompressionStats*>> results;
    
    // Always consider raw (no entropy): prevents expansion when no codec compresses well
    std::vector<uint8_t> raw_result;
    raw_result.push_back(static_cast<uint8_t>(EntropyCodec::NONE));
    raw_result.insert(raw_result.end(), input.begin(), input.end());
    static CompressionStats raw_stats;
    raw_stats.original_size = input.size();
    raw_stats.compressed_size = raw_result.size();
    raw_stats.compression_ratio = static_cast<float>(input.size()) / static_cast<float>(raw_result.size());
    raw_stats.codec_used = EntropyCodec::NONE;
    results.push_back({raw_result, &raw_stats});
    
    // Try codecs that may reduce size
    RLECodec rle;
    auto rle_result = rle.encode(input);
    results.push_back({rle_result, &rle.get_stats()});
    
    if (!prefer_speed) {
        HuffmanCodec huffman;
        auto huffman_result = huffman.encode(input);
        results.push_back({huffman_result, &huffman.get_stats()});
        
        DeflateCodec deflate;
        auto deflate_result = deflate.encode(input);
        results.push_back({deflate_result, &deflate.get_stats()});
        
        AdvancedCodec advanced;
        auto advanced_result = advanced.encode(input);
        results.push_back({advanced_result, &advanced.get_stats()});
    }
    
    // Pick the smallest output size (never expand beyond raw)
    size_t best_idx = 0;
    size_t best_size = results[0].first.size();
    
    for (size_t i = 1; i < results.size(); ++i) {
        if (results[i].first.size() < best_size) {
            best_size = results[i].first.size();
            best_idx = i;
        }
    }
    
    last_stats_ = *results[best_idx].second;
    last_stats_.compressed_size = results[best_idx].first.size();
    last_stats_.compression_ratio = static_cast<float>(input.size()) / std::max(1.0f, static_cast<float>(last_stats_.compressed_size));
    return results[best_idx].first;
}

std::vector<uint8_t> AdaptiveEncoder::decode(const std::vector<uint8_t>& input) {
    if (input.empty()) {
        return {};
    }
    
    EntropyCodec codec_type = static_cast<EntropyCodec>(input[0]);
    
    switch (codec_type) {
        case EntropyCodec::RLE: {
            RLECodec rle;
            return rle.decode(input);
        }
        case EntropyCodec::HUFFMAN: {
            HuffmanCodec huffman;
            return huffman.decode(input);
        }
        case EntropyCodec::DEFLATE: {
            DeflateCodec deflate;
            return deflate.decode(input);
        }
        case EntropyCodec::ADVANCED: {
            AdvancedCodec advanced;
            return advanced.decode(input);
        }
        default:
            return input.size() > 1 ? std::vector<uint8_t>(input.begin() + 1, input.end())
                                    : std::vector<uint8_t>();
    }
}

const CompressionStats& AdaptiveEncoder::get_stats() {
    return last_stats_;
}

} // namespace spectre
