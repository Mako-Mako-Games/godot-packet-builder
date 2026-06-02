#include "bit_writer.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>

using namespace godot;

void BitWriter::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("reset"), &BitWriter::reset);
    ClassDB::bind_method(D_METHOD("write_bits", "value", "count"), &BitWriter::write_bits);
    ClassDB::bind_method(D_METHOD("write_bool", "value"), &BitWriter::write_bool);
    ClassDB::bind_method(D_METHOD("write_quantized_float", "value", "min_val", "max_val", "bits"),
                         &BitWriter::write_quantized_float);
    ClassDB::bind_method(D_METHOD("get_bytes"), &BitWriter::get_bytes);
    ClassDB::bind_method(D_METHOD("get_bit_count"), &BitWriter::get_bit_count);
}

void BitWriter::reset()
{
    _bytes.clear();
    _bit_count = 0;
}

// LSB-first: bit 0 of value goes into the lowest available bit of the current byte.
// This matches how x86 naturally shifts and makes the delta-mask header trivial to inspect.
void BitWriter::write_bits(int64_t value, int count)
{
    ERR_FAIL_COND_MSG(count < 1 || count > 63, "count must be in [1, 63]");
    ERR_FAIL_COND_MSG(value < 0, "value must be >= 0");
    ERR_FAIL_COND_MSG(value >= (int64_t(1) << count),
                      vformat("value %d does not fit in %d bits", value, count));

    int written = 0;
    while (written < count)
    {
        // Ensure there is a byte to write into
        int byte_idx = _bit_count / 8;
        int bit_in_byte = _bit_count % 8;

        if (bit_in_byte == 0)
            _bytes.append(0);

        int bits_available = 8 - bit_in_byte;
        int bits_to_write = MIN(count - written, bits_available);

        // Extract the next `bits_to_write` bits from value, starting at `written`
        uint8_t chunk = static_cast<uint8_t>((value >> written) & ((int64_t(1) << bits_to_write) - 1));

        // Place them at `bit_in_byte` within the current byte
        _bytes.set(byte_idx, _bytes[byte_idx] | (chunk << bit_in_byte));

        _bit_count += bits_to_write;
        written += bits_to_write;
    }
}

void BitWriter::write_bool(bool value)
{
    write_bits(value ? 1 : 0, 1);
}

void BitWriter::write_quantized_float(double value, double min_val, double max_val, int bits)
{
    ERR_FAIL_COND_MSG(bits < 1 || bits > 32, "bits must be in [1, 32]");
    ERR_FAIL_COND_MSG(max_val <= min_val, "max_val must be > min_val");

    int64_t max_int = (int64_t(1) << bits) - 1;
    double normalized = (value - min_val) / (max_val - min_val);
    normalized = CLAMP(normalized, 0.0, 1.0);
    write_bits(static_cast<int64_t>(normalized * static_cast<double>(max_int)), bits);
}

PackedByteArray BitWriter::get_bytes() const
{
    return _bytes;
}

int BitWriter::get_bit_count() const
{
    return _bit_count;
}