#include "transform_encoder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <cstring>
#include <cmath>

using namespace godot;

void TransformEncoder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("set_quantize", "q"), &TransformEncoder::set_quantize);
    ClassDB::bind_method(D_METHOD("get_quantize"), &TransformEncoder::get_quantize);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quantize"), "set_quantize", "get_quantize");

    ClassDB::bind_method(D_METHOD("set_pos_min", "v"), &TransformEncoder::set_pos_min);
    ClassDB::bind_method(D_METHOD("get_pos_min"), &TransformEncoder::get_pos_min);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pos_min"), "set_pos_min", "get_pos_min");

    ClassDB::bind_method(D_METHOD("set_pos_max", "v"), &TransformEncoder::set_pos_max);
    ClassDB::bind_method(D_METHOD("get_pos_max"), &TransformEncoder::get_pos_max);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pos_max"), "set_pos_max", "get_pos_max");

    ClassDB::bind_method(D_METHOD("set_pos_bits", "b"), &TransformEncoder::set_pos_bits);
    ClassDB::bind_method(D_METHOD("get_pos_bits"), &TransformEncoder::get_pos_bits);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "pos_bits"), "set_pos_bits", "get_pos_bits");

    ClassDB::bind_method(D_METHOD("set_rot_bits", "b"), &TransformEncoder::set_rot_bits);
    ClassDB::bind_method(D_METHOD("get_rot_bits"), &TransformEncoder::get_rot_bits);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "rot_bits"), "set_rot_bits", "get_rot_bits");
}

// ─────────────────────────────────────────────
//  Position helpers
// ─────────────────────────────────────────────
static int64_t pos_to_raw(float v, bool quantize, double min_v, double max_v, int bits)
{
    if (quantize)
        return quantize_float(static_cast<double>(v), min_v, max_v, bits);
    uint32_t u;
    std::memcpy(&u, &v, sizeof(u));
    return static_cast<int64_t>(u);
}

static float pos_from_raw(int64_t raw, bool quantize, double min_v, double max_v, int bits)
{
    if (quantize)
        return static_cast<float>(dequantize_float(raw, min_v, max_v, bits));
    uint32_t u = static_cast<uint32_t>(raw);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

void TransformEncoder::_get_pos_raws(const Vector3 &pos, int64_t out[3]) const
{
    out[0] = pos_to_raw(pos.x, quantize, pos_min, pos_max, pos_bits);
    out[1] = pos_to_raw(pos.y, quantize, pos_min, pos_max, pos_bits);
    out[2] = pos_to_raw(pos.z, quantize, pos_min, pos_max, pos_bits);
}

void TransformEncoder::_encode_position(const Vector3 &pos, PackedByteArray &bytes, int &bit_pos) const
{
    int cb = _pos_component_bits();
    bit_write(bytes, bit_pos, pos_to_raw(pos.x, quantize, pos_min, pos_max, pos_bits), cb);
    bit_write(bytes, bit_pos, pos_to_raw(pos.y, quantize, pos_min, pos_max, pos_bits), cb);
    bit_write(bytes, bit_pos, pos_to_raw(pos.z, quantize, pos_min, pos_max, pos_bits), cb);
}

Vector3 TransformEncoder::_decode_position(const PackedByteArray &bytes, int &bit_pos) const
{
    int cb = _pos_component_bits();
    float x = pos_from_raw(bit_read(bytes, bit_pos, cb), quantize, pos_min, pos_max, pos_bits);
    float y = pos_from_raw(bit_read(bytes, bit_pos, cb), quantize, pos_min, pos_max, pos_bits);
    float z = pos_from_raw(bit_read(bytes, bit_pos, cb), quantize, pos_min, pos_max, pos_bits);
    return Vector3(x, y, z);
}

// ─────────────────────────────────────────────
//  Rotation helpers — smallest-3 quaternion
// ─────────────────────────────────────────────
//
// Smallest-3: find the component with the largest absolute value (the dropped one),
// store its sign and the index (2 bits), then quantize the remaining 3 components
// from [-1/sqrt(2), 1/sqrt(2)] into rot_bits each.
// The dropped component is reconstructed from sqrt(1 - a² - b² - c²) with the stored sign.

static constexpr double SQRT2_OVER_2 = 0.70710678118654752440;

void TransformEncoder::_get_rot_raws(const Quaternion &q, int64_t out[4]) const
{
    if (!quantize)
    {
        // out[0..3] = raw bits of x, y, z, w
        float comps[4] = {q.x, q.y, q.z, q.w};
        for (int i = 0; i < 4; i++)
        {
            uint32_t u;
            std::memcpy(&u, &comps[i], sizeof(u));
            out[i] = static_cast<int64_t>(u);
        }
        return;
    }

    // Find largest component
    float abs_comps[4] = {std::abs(q.x), std::abs(q.y), std::abs(q.z), std::abs(q.w)};
    int drop_idx = 0;
    for (int i = 1; i < 4; i++)
        if (abs_comps[i] > abs_comps[drop_idx])
            drop_idx = i;

    float comps[4] = {q.x, q.y, q.z, q.w};
    // Ensure dropped component is positive (we store its sign)
    int sign = (comps[drop_idx] >= 0.0f) ? 0 : 1;

    // Pack: out[0] = (drop_idx << 1) | sign, out[1..3] = quantized remaining
    out[0] = static_cast<int64_t>((drop_idx << 1) | sign);
    int write_idx = 1;
    for (int i = 0; i < 4; i++)
    {
        if (i == drop_idx)
            continue;
        float v = comps[i];
        if (sign)
            v = -v; // flip so dropped was positive
        out[write_idx++] = quantize_float(static_cast<double>(v), -SQRT2_OVER_2, SQRT2_OVER_2, rot_bits);
    }
}

void TransformEncoder::_encode_rotation(const Quaternion &q, PackedByteArray &bytes, int &bit_pos) const
{
    if (!quantize)
    {
        float comps[4] = {q.x, q.y, q.z, q.w};
        for (int i = 0; i < 4; i++)
        {
            uint32_t u;
            std::memcpy(&u, &comps[i], sizeof(u));
            bit_write(bytes, bit_pos, static_cast<int64_t>(u), 32);
        }
        return;
    }

    int64_t raws[4];
    _get_rot_raws(q, raws);
    bit_write(bytes, bit_pos, raws[0], 3); // 2 bits drop_idx + 1 bit sign
    bit_write(bytes, bit_pos, raws[1], rot_bits);
    bit_write(bytes, bit_pos, raws[2], rot_bits);
    bit_write(bytes, bit_pos, raws[3], rot_bits);
}

Quaternion TransformEncoder::_decode_rotation(const PackedByteArray &bytes, int &bit_pos) const
{
    if (!quantize)
    {
        float comps[4];
        for (int i = 0; i < 4; i++)
        {
            uint32_t u = static_cast<uint32_t>(bit_read(bytes, bit_pos, 32));
            std::memcpy(&comps[i], &u, sizeof(comps[i]));
        }
        return Quaternion(comps[0], comps[1], comps[2], comps[3]);
    }

    int64_t hdr = bit_read(bytes, bit_pos, 3);
    int drop_idx = static_cast<int>(hdr >> 1);
    int sign = static_cast<int>(hdr & 1);

    float small[3];
    for (int i = 0; i < 3; i++)
        small[i] = static_cast<float>(dequantize_float(bit_read(bytes, bit_pos, rot_bits),
                                                       -SQRT2_OVER_2, SQRT2_OVER_2, rot_bits));

    // Reconstruct dropped component
    double sq = 1.0;
    for (int i = 0; i < 3; i++)
        sq -= static_cast<double>(small[i]) * static_cast<double>(small[i]);
    if (sq < 0.0)
        sq = 0.0;
    float dropped = static_cast<float>(std::sqrt(sq));
    if (sign)
    {
        dropped = -dropped;
        for (int i = 0; i < 3; i++)
            small[i] = -small[i];
    }

    float out[4];
    int write_idx = 0;
    for (int i = 0; i < 4; i++)
        out[i] = (i == drop_idx) ? dropped : small[write_idx++];

    return Quaternion(out[0], out[1], out[2], out[3]);
}

// ─────────────────────────────────────────────
//  FieldEncoder interface
// ─────────────────────────────────────────────
int TransformEncoder::max_bits() const
{
    int pos = _pos_component_bits() * 3;
    int rot = quantize ? (3 + rot_bits * 3) : 128;
    return pos + rot;
}

void TransformEncoder::encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const
{
    Transform3D t = static_cast<Transform3D>(value);
    _encode_position(t.origin, bytes, bit_pos);
    _encode_rotation(t.basis.get_rotation_quaternion(), bytes, bit_pos);
}

Variant TransformEncoder::decode_value(const PackedByteArray &bytes, int &bit_pos) const
{
    Vector3 origin = _decode_position(bytes, bit_pos);
    Quaternion rot = _decode_rotation(bytes, bit_pos);
    return Variant(Transform3D(Basis(rot), origin));
}

void TransformEncoder::encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    Transform3D t = static_cast<Transform3D>(value);
    int64_t pos_r[3], rot_r[4];
    _get_pos_raws(t.origin, pos_r);
    _get_rot_raws(t.basis.get_rotation_quaternion(), rot_r);

    int rot_count = quantize ? 4 : 4; // always 4 entries in _prev_rot
    changed_out = !_has_prev || (pos_r[0] != _prev_pos[0]) || (pos_r[1] != _prev_pos[1]) || (pos_r[2] != _prev_pos[2]) || (rot_r[0] != _prev_rot[0]) || (rot_r[1] != _prev_rot[1]) || (rot_r[2] != _prev_rot[2]) || (rot_r[3] != _prev_rot[3]);

    if (changed_out)
    {
        // Write position directly from pre-computed raw values
        int cb = _pos_component_bits();
        bit_write(bytes, bit_pos, pos_r[0], cb);
        bit_write(bytes, bit_pos, pos_r[1], cb);
        bit_write(bytes, bit_pos, pos_r[2], cb);

        // Write rotation directly from pre-computed raw values
        if (!quantize)
        {
            // Raw: write all 4 components as-is (32 bits each)
            bit_write(bytes, bit_pos, rot_r[0], 32);
            bit_write(bytes, bit_pos, rot_r[1], 32);
            bit_write(bytes, bit_pos, rot_r[2], 32);
            bit_write(bytes, bit_pos, rot_r[3], 32);
        }
        else
        {
            // Quantized smallest-3: rot_r is [header, comp1, comp2, comp3]
            bit_write(bytes, bit_pos, rot_r[0], 3);  // 2 bits drop_idx + 1 bit sign
            bit_write(bytes, bit_pos, rot_r[1], rot_bits);
            bit_write(bytes, bit_pos, rot_r[2], rot_bits);
            bit_write(bytes, bit_pos, rot_r[3], rot_bits);
        }
    }

    for (int i = 0; i < 3; i++)
        _prev_pos[i] = pos_r[i];
    for (int i = 0; i < 4; i++)
        _prev_rot[i] = rot_r[i];
    _has_prev = true;
    (void)rot_count;
}

void TransformEncoder::reset_delta()
{
    _has_prev = false;
}
