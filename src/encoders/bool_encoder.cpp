#include "bool_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void BoolEncoder::_bind_methods() {}

void BoolEncoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    bit_write(bytes, bit_pos, static_cast<bool>(value) ? 1 : 0, 1);
}

Variant BoolEncoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    return Variant(bit_read(bytes, bit_pos, 1) != 0);
}

void BoolEncoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    bool v = static_cast<bool>(value);
    changed_out = !_has_prev || (v != _prev_value);
    if (changed_out)
        bit_write(bytes, bit_pos, v ? 1 : 0, 1);
    _prev_value = v;
    _has_prev = true;
}

void BoolEncoder::reset_delta()
{
    _has_prev = false;
}
