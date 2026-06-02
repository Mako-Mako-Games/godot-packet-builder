#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

namespace godot
{

    class BitWriter : public RefCounted
    {
        GDCLASS(BitWriter, RefCounted)

        PackedByteArray _bytes;
        int _bit_count = 0;

    protected:
        static void _bind_methods();

    public:
        void reset();

        // Write `count` LSB-first bits from `value`.
        // Value must be non-negative and fit in `count` bits.
        void write_bits(int64_t value, int count);

        // Convenience helpers (implemented in terms of write_bits)
        void write_bool(bool value);
        void write_quantized_float(double value, double min_val, double max_val, int bits);

        PackedByteArray get_bytes() const;
        int get_bit_count() const;
    };

} // namespace godot