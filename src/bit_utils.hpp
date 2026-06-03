#pragma once
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <cstdint>
#include <algorithm>

namespace godot
{

    // LSB-first bit writer into a pre-sized PackedByteArray.
    // bytes must already be sized and zero-filled before first write.
    // Uses direct pointer access (ptrw()) to avoid Variant layer dispatch overhead.
    inline void bit_write(PackedByteArray &bytes, int &bit_pos, int64_t value, int bits)
    {
        int written = 0;
        uint8_t *ptr = bytes.ptrw();
        int byte_count = bytes.size();
        
        while (written < bits)
        {
            int byte_idx = bit_pos / 8;
            int bit_in_byte = bit_pos % 8;
            int available = 8 - bit_in_byte;
            int to_write = std::min<int>(bits - written, available);
            
            ERR_FAIL_COND_MSG(byte_idx >= byte_count,
                vformat("bit_write out of bounds: byte %d, size %d", byte_idx, byte_count));
            
            uint8_t chunk = static_cast<uint8_t>((value >> written) & ((int64_t(1) << to_write) - 1));
            ptr[byte_idx] |= (chunk << bit_in_byte);
            bit_pos += to_write;
            written += to_write;
        }
    }

    // LSB-first bit reader from a PackedByteArray.
    // Uses direct pointer access (ptr()) to avoid Variant layer dispatch overhead.
    inline int64_t bit_read(const PackedByteArray &bytes, int &bit_pos, int bits)
    {
        int64_t result = 0;
        int done = 0;
        const uint8_t *ptr = bytes.ptr();
        int byte_count = bytes.size();
        
        while (done < bits)
        {
            int byte_idx = bit_pos / 8;
            int bit_in_byte = bit_pos % 8;
            int available = 8 - bit_in_byte;
            int to_read = std::min<int>(bits - done, available);
            
            ERR_FAIL_COND_V_MSG(byte_idx >= byte_count, 0,
                vformat("bit_read out of bounds: byte %d, size %d", byte_idx, byte_count));
            
            int64_t chunk = (static_cast<int64_t>(ptr[byte_idx]) >> bit_in_byte) & ((int64_t(1) << to_read) - 1);
            result |= (chunk << done);
            bit_pos += to_read;
            done += to_read;
        }
        return result;
    }

    // Quantize a float to an integer in [0, 2^bits - 1].
    // Clamps value to [min_val, max_val] before quantizing.
    inline int64_t quantize_float(double value, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        double normalized = (value - min_val) / (max_val - min_val);
        // Clamp to [0, 1] range
        normalized = std::max<double>(0.0, std::min<double>(normalized, 1.0));
        return static_cast<int64_t>(normalized * static_cast<double>(max_int));
    }

    // Dequantize back to float in [min_val, max_val] range.
    inline double dequantize_float(int64_t quantized, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        return min_val + (static_cast<double>(quantized) / static_cast<double>(max_int)) * (max_val - min_val);
    }

} // namespace godot
