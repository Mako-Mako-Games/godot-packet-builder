#include "int_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void IntEncoder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_bits", "bits"), &IntEncoder::set_bits);
    ClassDB::bind_method(D_METHOD("get_bits"), &IntEncoder::get_bits);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "bits"), "set_bits", "get_bits");
}

void IntEncoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    bit_write(bytes, bit_pos, static_cast<int64_t>(value), bits);
}

Variant IntEncoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    return Variant(bit_read(bytes, bit_pos, bits));
}

void IntEncoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    int64_t v = static_cast<int64_t>(value);
    changed_out = !_has_prev || (v != _prev_value);
    if (changed_out)
        bit_write(bytes, bit_pos, v, bits);
    _prev_value = v;
    _has_prev = true;
}

void IntEncoder::reset_delta()
{
    _has_prev = false;
}
