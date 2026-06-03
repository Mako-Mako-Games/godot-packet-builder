#include "string_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <cmath>

using namespace godot;

void StringEncoder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_max_bytes", "b"), &StringEncoder::set_max_bytes);
    ClassDB::bind_method(D_METHOD("get_max_bytes"), &StringEncoder::get_max_bytes);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_bytes"), "set_max_bytes", "get_max_bytes");
}

// Number of bits needed to store values in [0, max_bytes]
int StringEncoder::_len_bits() const
{
    if (max_bytes <= 0)
        return 1;
    int bits = 0;
    int v = max_bytes;
    while (v > 0)
    {
        bits++;
        v >>= 1;
    }
    return bits;
}

int StringEncoder::max_bits() const
{
    return _len_bits() + max_bytes * 8;
}

static void write_string(const String &str, int max_bytes, int len_bits,
                         PackedByteArray &bytes, int &bit_pos)
{
    PackedByteArray utf8 = str.to_utf8_buffer();
    int byte_count = utf8.size();
    if (byte_count > max_bytes)
        byte_count = max_bytes;
    bit_write(bytes, bit_pos, static_cast<int64_t>(byte_count), len_bits);
    for (int i = 0; i < byte_count; i++)
        bit_write(bytes, bit_pos, static_cast<int64_t>(utf8[i]), 8);
}

void StringEncoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    write_string(static_cast<String>(value), max_bytes, _len_bits(), bytes, bit_pos);
}

Variant StringEncoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    int len_bits = _len_bits();
    int byte_count = static_cast<int>(bit_read(bytes, bit_pos, len_bits));
    PackedByteArray utf8;
    utf8.resize(byte_count);
    for (int i = 0; i < byte_count; i++)
        utf8.set(i, static_cast<uint8_t>(bit_read(bytes, bit_pos, 8)));
    return Variant(utf8.get_string_from_utf8());
}

void StringEncoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    String v = static_cast<String>(value);
    changed_out = !_has_prev || (v != _prev_value);
    if (changed_out)
        write_string(v, max_bytes, _len_bits(), bytes, bit_pos);
    _prev_value = v;
    _has_prev = true;
}

void StringEncoder::reset_delta()
{
    _has_prev = false;
    _prev_value = String();
}
