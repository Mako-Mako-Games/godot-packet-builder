// src/bit_reader.cpp
#include "bit_reader.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void BitReader::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("_init", "bytes"), &BitReader::_init);
    ClassDB::bind_method(D_METHOD("reset"), &BitReader::reset);
    ClassDB::bind_method(D_METHOD("bits_remaining"), &BitReader::bits_remaining);
    ClassDB::bind_method(D_METHOD("read_bits", "count"), &BitReader::read_bits);
    ClassDB::bind_method(D_METHOD("read_bool"), &BitReader::read_bool);
}

void BitReader::_init(PackedByteArray bytes)
{
    _bytes = bytes;
    _bit_pos = 0;
    _total_bits = bytes.size() * 8;
}

void BitReader::reset()
{
    _bit_pos = 0;
}

int BitReader::bits_remaining() const
{
    return _total_bits - _bit_pos;
}

int64_t BitReader::read_bits(int count)
{
    ERR_FAIL_COND_V_MSG(count < 1, 0, "count must be >= 1");
    ERR_FAIL_COND_V_MSG(_bit_pos + count > _total_bits, 0, "not enough bits remaining");

    int64_t result = 0;
    int remaining = count;

    while (remaining > 0)
    {
        int byte_idx = _bit_pos / 8;
        int bits_left_in_byte = 8 - (_bit_pos % 8);
        int bits_to_read = MIN(remaining, bits_left_in_byte);

        int bit_position = bits_left_in_byte - bits_to_read;
        int64_t chunk = (_bytes[byte_idx] >> bit_position) & ((1 << bits_to_read) - 1);

        result = (result << bits_to_read) | chunk;
        _bit_pos += bits_to_read;
        remaining -= bits_to_read;
    }

    return result;
}

bool BitReader::read_bool()
{
    return read_bits(1) == 1;
}