// src/bit_writer.h
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
        void write_bits(int64_t value, int count);
        PackedByteArray get_bytes() const;
        int get_bit_count() const;
    };

}