#include "bit_reader.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

using namespace godot;

void BitReader::_bind_methods()
{
    ClassDB::bind_static_method("BitReader", D_METHOD("create", "bytes"), &BitReader::create);
    ClassDB::bind_method(D_METHOD("reset"), &BitReader::reset);
    ClassDB::bind_method(D_METHOD("bits_remaining"), &BitReader::bits_remaining);
    ClassDB::bind_method(D_METHOD("read_bits", "count"), &BitReader::read_bits);
    ClassDB::bind_method(D_METHOD("read_bool"), &BitReader::read_bool);
    ClassDB::bind_method(D_METHOD("read_quantized_float", "min_val", "max_val", "bits"),
                         &BitReader::read_quantized_float);
}

Ref<BitReader> BitReader::create(PackedByteArray bytes)
{
    Ref<BitReader> r;
    r.instantiate();
    r->_bytes = bytes;
    r->_bit_pos = 0;
    r->_total_bits = bytes.size() * 8;
    return r;
}

void BitReader::reset()
{
    _bit_pos = 0;
}

int BitReader::bits_remaining() const
{
    return _total_bits - _bit_pos;
}

// LSB-first: reconstructs the value in the same bit order BitWriter wrote it.
int64_t BitReader::read_bits(int count)
{
    ERR_FAIL_COND_V_MSG(count < 1 || count > 63, 0, "count must be in [1, 63]");
    ERR_FAIL_COND_V_MSG(_bit_pos + count > _total_bits, 0, "not enough bits remaining");

    int64_t result = 0;
    int read = 0;

    while (read < count)
    {
        int byte_idx = _bit_pos / 8;
        int bit_in_byte = _bit_pos % 8;

        int bits_available = 8 - bit_in_byte;
        int bits_to_read = MIN(count - read, bits_available);

        // Extract `bits_to_read` bits starting at `bit_in_byte`
        int64_t chunk = (static_cast<int64_t>(_bytes[byte_idx]) >> bit_in_byte) & ((int64_t(1) << bits_to_read) - 1);

        // Place them at position `read` in the result
        result |= (chunk << read);
        _bit_pos += bits_to_read;
        read += bits_to_read;
    }

    return result;
}

bool BitReader::read_bool()
{
    return read_bits(1) != 0;
}

double BitReader::read_quantized_float(double min_val, double max_val, int bits)
{
    ERR_FAIL_COND_V_MSG(bits < 1 || bits > 32, 0.0, "bits must be in [1, 32]");

    int64_t max_int = (int64_t(1) << bits) - 1;
    int64_t quantized = read_bits(bits);
    return min_val + (static_cast<double>(quantized) / static_cast<double>(max_int)) * (max_val - min_val);
}