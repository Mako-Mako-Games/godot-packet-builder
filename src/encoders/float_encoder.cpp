#include "float_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <cstring>

using namespace godot;

void FloatEncoder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_quantize", "q"), &FloatEncoder::set_quantize);
    ClassDB::bind_method(D_METHOD("get_quantize"), &FloatEncoder::get_quantize);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quantize"), "set_quantize", "get_quantize");

    ClassDB::bind_method(D_METHOD("set_min_value", "v"), &FloatEncoder::set_min_value);
    ClassDB::bind_method(D_METHOD("get_min_value"), &FloatEncoder::get_min_value);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value"), "set_min_value", "get_min_value");

    ClassDB::bind_method(D_METHOD("set_max_value", "v"), &FloatEncoder::set_max_value);
    ClassDB::bind_method(D_METHOD("get_max_value"), &FloatEncoder::get_max_value);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value"), "set_max_value", "get_max_value");

    ClassDB::bind_method(D_METHOD("set_bits", "b"), &FloatEncoder::set_bits);
    ClassDB::bind_method(D_METHOD("get_bits"), &FloatEncoder::get_bits);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "bits"), "set_bits", "get_bits");
}

int64_t FloatEncoder::_to_raw(double v) const
{
    if (quantize)
        return quantize_float(v, min_value, max_value, bits);
    // Reinterpret float32 bits as int32
    float f = static_cast<float>(v);
    int32_t raw;
    std::memcpy(&raw, &f, sizeof(raw));
    return static_cast<int64_t>(static_cast<uint32_t>(raw));
}

double FloatEncoder::_from_raw(int64_t raw) const
{
    if (quantize)
        return dequantize_float(raw, min_value, max_value, bits);
    uint32_t u = static_cast<uint32_t>(raw);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return static_cast<double>(f);
}

int FloatEncoder::max_bits() const
{
    return quantize ? bits : 32;
}

void FloatEncoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    bit_write(bytes, bit_pos, _to_raw(static_cast<double>(value)), max_bits());
}

Variant FloatEncoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    return Variant(_from_raw(bit_read(bytes, bit_pos, max_bits())));
}

void FloatEncoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    int64_t raw = _to_raw(static_cast<double>(value));
    changed_out = !_has_prev || (raw != _prev_raw);
    if (changed_out)
        bit_write(bytes, bit_pos, raw, max_bits());
    _prev_raw = raw;
    _has_prev = true;
}

void FloatEncoder::reset_delta()
{
    _has_prev = false;
}
