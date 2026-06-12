#pragma once
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <cstdint>
#include <algorithm>
#include <cstring>

namespace godot
{
    // ─────────────────────────────────────────────────────────────────────────────
    //  bit_write — LSB-first, shift-based (no div/mod per iteration)
    //
    //  Writes `bits` bits from `value` into `bytes` starting at `bit_pos`.
    //  bytes must be pre-sized and zero-filled for the region being written.
    //  Uses ptrw() once outside the loop to avoid repeated COW checks.
    // ─────────────────────────────────────────────────────────────────────────────
    inline void bit_write(PackedByteArray &bytes, int &bit_pos, int64_t value, int bits)
    {
        // Single bounds check before entering the loop — no vformat in hot path.
        ERR_FAIL_COND_MSG((bit_pos + bits + 7) / 8 > bytes.size(),
            "bit_write: would write out of bounds");

        uint8_t *ptr = bytes.ptrw();
        int remaining = bits;
        int src_shift = 0;           // how many bits of value we've consumed
        int byte_idx  = bit_pos >> 3;
        int bit_in_byte = bit_pos & 7;

        while (remaining > 0)
        {
            int available  = 8 - bit_in_byte;
            int to_write   = remaining < available ? remaining : available;

            // Extract `to_write` bits from value at position src_shift
            uint8_t chunk = static_cast<uint8_t>(
                (value >> src_shift) & ((int64_t(1) << to_write) - 1));
            ptr[byte_idx] |= static_cast<uint8_t>(chunk << bit_in_byte);

            src_shift   += to_write;
            remaining   -= to_write;
            bit_pos     += to_write;
            byte_idx    += (bit_in_byte + to_write) >> 3;  // advance byte only when crossing
            bit_in_byte  = (bit_in_byte + to_write) & 7;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  bit_read — LSB-first, shift-based (no div/mod per iteration)
    // ─────────────────────────────────────────────────────────────────────────────
    inline int64_t bit_read(const PackedByteArray &bytes, int &bit_pos, int bits)
    {
        ERR_FAIL_COND_V_MSG((bit_pos + bits + 7) / 8 > bytes.size(), 0,
            "bit_read: would read out of bounds");

        const uint8_t *ptr = bytes.ptr();
        int64_t result   = 0;
        int remaining    = bits;
        int dst_shift    = 0;
        int byte_idx     = bit_pos >> 3;
        int bit_in_byte  = bit_pos & 7;

        while (remaining > 0)
        {
            int available = 8 - bit_in_byte;
            int to_read   = remaining < available ? remaining : available;

            int64_t chunk = (static_cast<int64_t>(ptr[byte_idx]) >> bit_in_byte)
                            & ((int64_t(1) << to_read) - 1);
            result |= (chunk << dst_shift);

            dst_shift   += to_read;
            remaining   -= to_read;
            bit_pos     += to_read;
            byte_idx    += (bit_in_byte + to_read) >> 3;
            bit_in_byte  = (bit_in_byte + to_read) & 7;
        }
        return result;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Quantization helpers — unchanged
    // ─────────────────────────────────────────────────────────────────────────────
    inline int64_t quantize_float(double value, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        double normalized = (value - min_val) / (max_val - min_val);
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;
        return static_cast<int64_t>(normalized * static_cast<double>(max_int));
    }

    inline double dequantize_float(int64_t quantized, double min_val, double max_val, int bits)
    {
        int64_t max_int = (int64_t(1) << bits) - 1;
        return min_val + (static_cast<double>(quantized) / static_cast<double>(max_int))
                         * (max_val - min_val);
    }

} // namespace godot
