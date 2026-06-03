#include "vector3_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <cstring>

using namespace godot;

void Vector3Encoder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_quantize", "q"), &Vector3Encoder::set_quantize);
    ClassDB::bind_method(D_METHOD("get_quantize"), &Vector3Encoder::get_quantize);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quantize"), "set_quantize", "get_quantize");

    ClassDB::bind_method(D_METHOD("set_min_value", "v"), &Vector3Encoder::set_min_value);
    ClassDB::bind_method(D_METHOD("get_min_value"), &Vector3Encoder::get_min_value);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value"), "set_min_value", "get_min_value");

    ClassDB::bind_method(D_METHOD("set_max_value", "v"), &Vector3Encoder::set_max_value);
    ClassDB::bind_method(D_METHOD("get_max_value"), &Vector3Encoder::get_max_value);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value"), "set_max_value", "get_max_value");

    ClassDB::bind_method(D_METHOD("set_bits", "b"), &Vector3Encoder::set_bits);
    ClassDB::bind_method(D_METHOD("get_bits"), &Vector3Encoder::get_bits);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "bits"), "set_bits", "get_bits");
}

int64_t Vector3Encoder::_to_raw(float v) const
{
    if (quantize)
        return quantize_float(static_cast<double>(v), min_value, max_value, bits);
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    return static_cast<int64_t>(u);
}

float Vector3Encoder::_from_raw(int64_t raw) const
{
    if (quantize)
        return static_cast<float>(dequantize_float(raw, min_value, max_value, bits));
    uint32_t u = static_cast<uint32_t>(raw);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

void Vector3Encoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    Vector3 v = static_cast<Vector3>(value);
    int cb = _component_bits();
    bit_write(bytes, bit_pos, _to_raw(v.x), cb);
    bit_write(bytes, bit_pos, _to_raw(v.y), cb);
    bit_write(bytes, bit_pos, _to_raw(v.z), cb);
}

Variant Vector3Encoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    int cb = _component_bits();
    float x = _from_raw(bit_read(bytes, bit_pos, cb));
    float y = _from_raw(bit_read(bytes, bit_pos, cb));
    float z = _from_raw(bit_read(bytes, bit_pos, cb));
    return Variant(Vector3(x, y, z));
}

void Vector3Encoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    Vector3 v = static_cast<Vector3>(value);
    int64_t rx = _to_raw(v.x);
    int64_t ry = _to_raw(v.y);
    int64_t rz = _to_raw(v.z);
    changed_out = !_has_prev || (rx != _prev_raw[0]) || (ry != _prev_raw[1]) || (rz != _prev_raw[2]);
    if (changed_out)
    {
        int cb = _component_bits();
        bit_write(bytes, bit_pos, rx, cb);
        bit_write(bytes, bit_pos, ry, cb);
        bit_write(bytes, bit_pos, rz, cb);
    }
    _prev_raw[0] = rx;
    _prev_raw[1] = ry;
    _prev_raw[2] = rz;
    _has_prev = true;
}

void Vector3Encoder::reset_delta()
{
    _has_prev = false;
}
