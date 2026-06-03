#pragma once
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstdint>

namespace godot
{

    // LSB-first bit writer into a pre-sized PackedByteArray.
    // bytes must already be sized and zero-filled before first write.
    inline void bit_write(PackedByteArray &bytes, int &bit_pos, int64_t value, int bits)
    {
        int written = 0;
        while (written < bits)
        {
            int byte_idx = bit_pos / 8;
            int bit_in_byte = bit_pos % 8;
            int available = 8 - bit_in_byte;
            int to_write = (bits - written) < available ? (bits - written) : available;
            uint8_t chunk = static_cast<uint8_t>((value >> written) & ((int64_t(1) << to_write) - 1));
            bytes.set(byte_idx, bytes[byte_idx] | (chunk << bit_in_byte));
            bit_pos += to_write;
            written += to_write;
        }
    }

    // LSB-first bit reader from a PackedByteArray.
    inline int64_t bit_read(const PackedByteArray &bytes, int &bit_pos, int bits)
    {
        int64_t result = 0;
        int done = 0;
        while (done < bits)
        {
            int byte_idx = bit_pos / 8;
            int bit_in_byte = bit_pos % 8;
            int available = 8 - bit_in_byte;
            int to_read = (bits - done) < available ? (bits - done) : available;
            int64_t chunk = (static_cast<int64_t>(bytes[byte_idx]) >> bit_in_byte) & ((int64_t(1) << to_read) - 1);
            result |= (chunk << done);
            bit_pos += to_read;
            done += to_read;
        }
        return result;
    }

    // Quantize a float to an integer in [0, 2^bits - 1]
    inline int64_t quantize_float(double value, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        double normalized = (value - min_val) / (max_val - min_val);
        if (normalized < 0.0)
            normalized = 0.0;
        if (normalized > 1.0)
            normalized = 1.0;
        return static_cast<int64_t>(normalized * static_cast<double>(max_int));
    }

    // Dequantize back to float
    inline double dequantize_float(int64_t quantized, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        return min_val + (static_cast<double>(quantized) / static_cast<double>(max_int)) * (max_val - min_val);
    }

} // namespace godot
