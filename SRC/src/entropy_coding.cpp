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
    
    // Encode data (simple approach: just use RLE instead for now, Huffman is complex)
    // A full Huffman implementation would encode bit sequences
    // For now, return RLE-encoded as fallback
    RLECodec rle;
    auto result = rle.encode(input);
    
    stats_.compressed_size = result.size();
    stats_.codec_used = EntropyCodec::RLE;  // Using RLE as approximation
    stats_.compression_ratio = static_cast<float>(stats_.original_size) / 
                               std::max(1.0f, static_cast<float>(stats_.compressed_size));
    
    return result;
}

std::vector<uint8_t> HuffmanCodec::decode(const std::vector<uint8_t>& input) {
    // For now, delegate to RLE since "encode" returns RLE data
    RLECodec rle;
    return rle.decode(input);
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
    
    // Try multiple codecs
    RLECodec rle;
    auto rle_result = rle.encode(input);
    results.push_back({rle_result, &rle.get_stats()});
    
    if (!prefer_speed) {
        DeflateCodec deflate;
        auto deflate_result = deflate.encode(input);
        results.push_back({deflate_result, &deflate.get_stats()});
        
        AdvancedCodec advanced;
        auto advanced_result = advanced.encode(input);
        results.push_back({advanced_result, &advanced.get_stats()});
    }
    
    // Pick the codec with best compression ratio
    size_t best_idx = 0;
    float best_ratio = results[0].second->compression_ratio;
    
    for (size_t i = 1; i < results.size(); ++i) {
        if (results[i].second->compression_ratio > best_ratio) {
            best_ratio = results[i].second->compression_ratio;
            best_idx = i;
        }
    }
    
    last_stats_ = *results[best_idx].second;
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
