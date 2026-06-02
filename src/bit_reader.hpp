#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/ref.hpp>

namespace godot
{

    class BitReader : public RefCounted
    {
        GDCLASS(BitReader, RefCounted)

        PackedByteArray _bytes;
        int _bit_pos = 0;
        int _total_bits = 0;

    protected:
        static void _bind_methods();

    public:
        // Factory — mirrors the GDScript BitReader.new() + init pattern
        static Ref<BitReader> create(PackedByteArray bytes);

        void reset();
        int bits_remaining() const;

        // Read `count` LSB-first bits, returned as a non-negative int64
        int64_t read_bits(int count);

        // Convenience helpers
        bool read_bool();
        double read_quantized_float(double min_val, double max_val, int bits);
    };

} // namespace godot