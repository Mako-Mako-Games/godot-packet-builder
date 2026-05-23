// src/bit_writer.cpp
#include "bit_writer.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void BitWriter::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("reset"), &BitWriter::reset);
    ClassDB::bind_method(D_METHOD("write_bits", "value", "count"), &BitWriter::write_bits);
    ClassDB::bind_method(D_METHOD("write_bool", "value"), &BitWriter::write_bool);
    ClassDB::bind_method(D_METHOD("get_bytes"), &BitWriter::get_bytes);
    ClassDB::bind_method(D_METHOD("get_bit_count"), &BitWriter::get_bit_count);
}

void BitWriter::reset()
{
    _bytes.clear();
    _bit_count = 0;
}

void BitWriter::write_bits(int64_t value, int count)
{
    ERR_FAIL_COND_MSG(count < 1, "count must be >= 1");
    ERR_FAIL_COND_MSG(value < 0, "value must be unsigned");

    int remaining = count;
    while (remaining > 0)
    {
        if (_bit_count % 8 == 0)
            _bytes.append(0);

        int bits_left_in_byte = 8 - (_bit_count % 8);
        int bits_to_write = MIN(remaining, bits_left_in_byte);
        remaining -= bits_to_write;

        uint8_t chunk = (value >> remaining) & ((1 << bits_to_write) - 1);
        int bit_position = bits_left_in_byte - bits_to_write;
        _bytes.set(_bytes.size() - 1, _bytes[_bytes.size() - 1] | (chunk << bit_position));

        _bit_count += bits_to_write;
    }
}

void BitWriter::write_bool(bool value)
{
    write_bits(value ? 1 : 0, 1);
}

PackedByteArray BitWriter::get_bytes() const
{
    return _bytes;
}

int BitWriter::get_bit_count() const
{
    return _bit_count;
}