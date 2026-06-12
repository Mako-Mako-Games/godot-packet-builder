#include "packet_builder.hpp"
#include "bit_utils.hpp"
// Concrete encoder types — needed only at build() time for cast_to<>
#include "transform_encoder.hpp"
#include "vector3_encoder.hpp"
#include "float_encoder.hpp"
#include "bool_encoder.hpp"
#include "string_encoder.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <cmath>

using namespace godot;

// ─────────────────────────────────────────────────────────────────────────────
//  Constants mirrored from transform_encoder (inlined here to avoid a call)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr double SQRT2_OVER_2 = 0.70710678118654752440;

// ─────────────────────────────────────────────────────────────────────────────
//  GDScript bindings
// ─────────────────────────────────────────────────────────────────────────────
void PacketBuilder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("add", "encoder"),          &PacketBuilder::add);
    ClassDB::bind_method(D_METHOD("build"),                   &PacketBuilder::build);
    ClassDB::bind_method(D_METHOD("is_built"),                &PacketBuilder::is_built);
    ClassDB::bind_method(D_METHOD("field_count"),             &PacketBuilder::field_count);
    ClassDB::bind_method(D_METHOD("encode_keyframe", "values"), &PacketBuilder::encode_keyframe);
    ClassDB::bind_method(D_METHOD("encode_delta",   "values"), &PacketBuilder::encode_delta);
    ClassDB::bind_method(D_METHOD("decode", "data", "prev"),  &PacketBuilder::decode);
    ClassDB::bind_method(D_METHOD("reset_delta"),             &PacketBuilder::reset_delta);
}

// ─────────────────────────────────────────────────────────────────────────────
//  add() / build()
// ─────────────────────────────────────────────────────────────────────────────
void PacketBuilder::add(const Ref<FieldEncoder> &encoder)
{
    ERR_FAIL_COND_MSG(_built,           "Cannot add encoders after build()");
    ERR_FAIL_COND_MSG(encoder.is_null(),"Encoder must not be null");
    ERR_FAIL_COND_MSG(_encoders.size() >= 63,
        "PacketBuilder: maximum 63 fields (mask is int64)");
    _encoders.push_back(encoder);
}

void PacketBuilder::_bake_fields()
{
    _fields.clear();
    _fields.reserve(_encoders.size());
    _max_bits = 0;

    for (const Ref<FieldEncoder> &enc : _encoders)
    {
        ERR_FAIL_COND_MSG(enc.is_null(), "Encoder must not be null");
        BakedField f{};

        if (const TransformEncoder *te = Object::cast_to<TransformEncoder>(enc.ptr()))
        {
            f.kind                  = BakedField::KIND_TRANSFORM;
            f.transform_quantize    = te->get_quantize();
            f.transform_pos_min     = te->get_pos_min();
            f.transform_pos_max     = te->get_pos_max();
            f.transform_pos_bits    = te->get_pos_bits();
            f.transform_rot_bits    = te->get_rot_bits();
        }
        else if (const Vector3Encoder *ve = Object::cast_to<Vector3Encoder>(enc.ptr()))
        {
            f.kind           = BakedField::KIND_VECTOR3;
            f.vector3_quantize = ve->get_quantize();
            f.vector3_min    = ve->get_min_value();
            f.vector3_max    = ve->get_max_value();
            f.vector3_bits   = ve->get_bits();
        }
        else if (const FloatEncoder *fe = Object::cast_to<FloatEncoder>(enc.ptr()))
        {
            f.kind           = BakedField::KIND_FLOAT;
            f.float_quantize = fe->get_quantize();
            f.float_min      = fe->get_min_value();
            f.float_max      = fe->get_max_value();
            f.float_bits     = fe->get_bits();
        }
        else if (Object::cast_to<BoolEncoder>(enc.ptr()))
        {
            f.kind = BakedField::KIND_BOOL;
        }
        else if (const StringEncoder *se = Object::cast_to<StringEncoder>(enc.ptr()))
        {
            f.kind             = BakedField::KIND_STRING;
            f.string_max_bytes = se->get_max_bytes();
        }
        else
        {
            ERR_FAIL_MSG("PacketBuilder::build(): unknown encoder type — "
                         "only built-in encoder classes are supported");
        }

        _max_bits += enc->max_bits();
        _fields.push_back(f);
    }
}

void PacketBuilder::build()
{
    ERR_FAIL_COND_MSG(_encoders.empty(), "PacketBuilder has no encoders");
    _bake_fields();

    // Pre-size scratch buffer for worst case: 2 type bits + 63 mask bits + all field data.
    // Reused every encode call — no per-call allocation.
    int n = static_cast<int>(_fields.size());
    int scratch_bytes = (_max_bits + n + 2 + 7) / 8;
    _scratch.resize(scratch_bytes);

    _built          = true;
    _needs_keyframe = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Encode helpers — static dispatch, no virtual calls
// ─────────────────────────────────────────────────────────────────────────────

// ── Transform ────────────────────────────────────────────────────────────────

static inline int transform_pos_cbits(const BakedField &f)
{
    return f.transform_quantize ? f.transform_pos_bits : 32;
}

static void get_pos_raws(const BakedField &f, const Vector3 &pos, int64_t out[3])
{
    if (f.transform_quantize)
    {
        out[0] = quantize_float(pos.x, f.transform_pos_min, f.transform_pos_max, f.transform_pos_bits);
        out[1] = quantize_float(pos.y, f.transform_pos_min, f.transform_pos_max, f.transform_pos_bits);
        out[2] = quantize_float(pos.z, f.transform_pos_min, f.transform_pos_max, f.transform_pos_bits);
    }
    else
    {
        uint32_t u;
        std::memcpy(&u, &pos.x, sizeof(u)); out[0] = u;
        std::memcpy(&u, &pos.y, sizeof(u)); out[1] = u;
        std::memcpy(&u, &pos.z, sizeof(u)); out[2] = u;
    }
}

static void write_pos_raws(const BakedField &f, const int64_t r[3],
                           PackedByteArray &bytes, int &bit_pos)
{
    int cb = transform_pos_cbits(f);
    bit_write(bytes, bit_pos, r[0], cb);
    bit_write(bytes, bit_pos, r[1], cb);
    bit_write(bytes, bit_pos, r[2], cb);
}

static Vector3 read_pos(const BakedField &f, const PackedByteArray &bytes, int &bit_pos)
{
    int cb = transform_pos_cbits(f);
    auto from_raw = [&](int64_t raw) -> float {
        if (f.transform_quantize)
            return static_cast<float>(dequantize_float(raw,
                f.transform_pos_min, f.transform_pos_max, f.transform_pos_bits));
        uint32_t u = static_cast<uint32_t>(raw);
        float v; std::memcpy(&v, &u, sizeof(v)); return v;
    };
    return Vector3(from_raw(bit_read(bytes, bit_pos, cb)),
                   from_raw(bit_read(bytes, bit_pos, cb)),
                   from_raw(bit_read(bytes, bit_pos, cb)));
}

static void get_rot_raws(const BakedField &f, const Quaternion &q, int64_t out[4])
{
    if (!f.transform_quantize)
    {
        float comps[4] = {q.x, q.y, q.z, q.w};
        for (int i = 0; i < 4; i++) {
            uint32_t u; std::memcpy(&u, &comps[i], sizeof(u));
            out[i] = static_cast<int64_t>(u);
        }
        return;
    }

    float abs_comps[4] = {std::abs(q.x), std::abs(q.y), std::abs(q.z), std::abs(q.w)};
    int drop_idx = 0;
    for (int i = 1; i < 4; i++)
        if (abs_comps[i] > abs_comps[drop_idx]) drop_idx = i;

    float comps[4] = {q.x, q.y, q.z, q.w};
    int sign = (comps[drop_idx] >= 0.0f) ? 0 : 1;

    out[0] = static_cast<int64_t>((drop_idx << 1) | sign);
    int wi = 1;
    for (int i = 0; i < 4; i++) {
        if (i == drop_idx) continue;
        float v = comps[i];
        if (sign) v = -v;
        out[wi++] = quantize_float(static_cast<double>(v),
            -SQRT2_OVER_2, SQRT2_OVER_2, f.transform_rot_bits);
    }
}

static void write_rot_raws(const BakedField &f, const int64_t r[4],
                           PackedByteArray &bytes, int &bit_pos)
{
    if (!f.transform_quantize)
    {
        for (int i = 0; i < 4; i++)
            bit_write(bytes, bit_pos, r[i], 32);
        return;
    }
    bit_write(bytes, bit_pos, r[0], 3);
    bit_write(bytes, bit_pos, r[1], f.transform_rot_bits);
    bit_write(bytes, bit_pos, r[2], f.transform_rot_bits);
    bit_write(bytes, bit_pos, r[3], f.transform_rot_bits);
}

static Quaternion read_rot(const BakedField &f, const PackedByteArray &bytes, int &bit_pos)
{
    if (!f.transform_quantize)
    {
        float comps[4];
        for (int i = 0; i < 4; i++) {
            uint32_t u = static_cast<uint32_t>(bit_read(bytes, bit_pos, 32));
            std::memcpy(&comps[i], &u, sizeof(comps[i]));
        }
        return Quaternion(comps[0], comps[1], comps[2], comps[3]);
    }

    int64_t hdr   = bit_read(bytes, bit_pos, 3);
    int drop_idx  = static_cast<int>(hdr >> 1);
    int sign      = static_cast<int>(hdr & 1);

    float small[3];
    for (int i = 0; i < 3; i++)
        small[i] = static_cast<float>(dequantize_float(
            bit_read(bytes, bit_pos, f.transform_rot_bits),
            -SQRT2_OVER_2, SQRT2_OVER_2, f.transform_rot_bits));

    double sq = 1.0;
    for (int i = 0; i < 3; i++) sq -= double(small[i]) * double(small[i]);
    if (sq < 0.0) sq = 0.0;
    float dropped = static_cast<float>(std::sqrt(sq));
    if (sign) { dropped = -dropped; for (int i = 0; i < 3; i++) small[i] = -small[i]; }

    float out[4];
    int wi = 0;
    for (int i = 0; i < 4; i++) out[i] = (i == drop_idx) ? dropped : small[wi++];
    return Quaternion(out[0], out[1], out[2], out[3]);
}

void PacketBuilder::_encode_transform(BakedField &field, const Variant &value,
    PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    Transform3D t = static_cast<Transform3D>(value);
    Quaternion  q = t.basis.get_rotation_quaternion();

    int64_t pos_r[3], rot_r[4];
    get_pos_raws(field, t.origin, pos_r);
    get_rot_raws(field, q,        rot_r);

    changed_out = !field.has_prev
        || pos_r[0] != field.prev_raw[0]
        || pos_r[1] != field.prev_raw[1]
        || pos_r[2] != field.prev_raw[2]
        || rot_r[0] != field.prev_raw[3]
        || rot_r[1] != field.prev_raw[4]
        || rot_r[2] != field.prev_raw[5]
        || rot_r[3] != field.prev_raw[6];

    if (changed_out)
    {
        write_pos_raws(field, pos_r, bytes, bit_pos);
        write_rot_raws(field, rot_r, bytes, bit_pos);
    }

    field.prev_raw[0] = pos_r[0]; field.prev_raw[1] = pos_r[1]; field.prev_raw[2] = pos_r[2];
    field.prev_raw[3] = rot_r[0]; field.prev_raw[4] = rot_r[1];
    field.prev_raw[5] = rot_r[2]; field.prev_raw[6] = rot_r[3];
    field.has_prev = true;
}

Variant PacketBuilder::_decode_transform(BakedField &field,
    const PackedByteArray &bytes, int &bit_pos) const
{
    Vector3    origin = read_pos(field, bytes, bit_pos);
    Quaternion rot    = read_rot(field, bytes, bit_pos);
    return Variant(Transform3D(Basis(rot), origin));
}

// ── Vector3 ──────────────────────────────────────────────────────────────────

static inline int64_t vec3_to_raw(const BakedField &f, float v)
{
    if (f.vector3_quantize)
        return quantize_float(static_cast<double>(v), f.vector3_min, f.vector3_max, f.vector3_bits);
    uint32_t u; std::memcpy(&u, &v, sizeof(u));
    return static_cast<int64_t>(u);
}

static inline float vec3_from_raw(const BakedField &f, int64_t raw)
{
    if (f.vector3_quantize)
        return static_cast<float>(dequantize_float(raw, f.vector3_min, f.vector3_max, f.vector3_bits));
    uint32_t u = static_cast<uint32_t>(raw);
    float v; std::memcpy(&v, &u, sizeof(v)); return v;
}

void PacketBuilder::_encode_vector3(BakedField &field, const Variant &value,
    PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    Vector3 v = static_cast<Vector3>(value);
    int cb = field.vector3_quantize ? field.vector3_bits : 32;
    int64_t rx = vec3_to_raw(field, v.x);
    int64_t ry = vec3_to_raw(field, v.y);
    int64_t rz = vec3_to_raw(field, v.z);

    changed_out = !field.has_prev
        || rx != field.prev_raw[0]
        || ry != field.prev_raw[1]
        || rz != field.prev_raw[2];

    if (changed_out)
    {
        bit_write(bytes, bit_pos, rx, cb);
        bit_write(bytes, bit_pos, ry, cb);
        bit_write(bytes, bit_pos, rz, cb);
    }

    field.prev_raw[0] = rx; field.prev_raw[1] = ry; field.prev_raw[2] = rz;
    field.has_prev = true;
}

Variant PacketBuilder::_decode_vector3(BakedField &field,
    const PackedByteArray &bytes, int &bit_pos) const
{
    int cb = field.vector3_quantize ? field.vector3_bits : 32;
    return Variant(Vector3(
        vec3_from_raw(field, bit_read(bytes, bit_pos, cb)),
        vec3_from_raw(field, bit_read(bytes, bit_pos, cb)),
        vec3_from_raw(field, bit_read(bytes, bit_pos, cb))));
}

// ── Float ────────────────────────────────────────────────────────────────────

static inline int64_t float_to_raw(const BakedField &f, double v)
{
    if (f.float_quantize)
        return quantize_float(v, f.float_min, f.float_max, f.float_bits);
    float fv = static_cast<float>(v);
    uint32_t u; std::memcpy(&u, &fv, sizeof(u));
    return static_cast<int64_t>(u);
}

static inline double float_from_raw(const BakedField &f, int64_t raw)
{
    if (f.float_quantize)
        return dequantize_float(raw, f.float_min, f.float_max, f.float_bits);
    uint32_t u = static_cast<uint32_t>(raw);
    float v; std::memcpy(&v, &u, sizeof(v));
    return static_cast<double>(v);
}

void PacketBuilder::_encode_float(BakedField &field, const Variant &value,
    PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    int64_t raw = float_to_raw(field, static_cast<double>(value));
    int cb = field.float_quantize ? field.float_bits : 32;
    changed_out = !field.has_prev || raw != field.prev_raw[0];
    if (changed_out) bit_write(bytes, bit_pos, raw, cb);
    field.prev_raw[0] = raw;
    field.has_prev = true;
}

Variant PacketBuilder::_decode_float(BakedField &field,
    const PackedByteArray &bytes, int &bit_pos) const
{
    int cb = field.float_quantize ? field.float_bits : 32;
    return Variant(float_from_raw(field, bit_read(bytes, bit_pos, cb)));
}

// ── Bool ─────────────────────────────────────────────────────────────────────

void PacketBuilder::_encode_bool(BakedField &field, const Variant &value,
    PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    int64_t raw = static_cast<bool>(value) ? 1 : 0;
    changed_out = !field.has_prev || raw != field.prev_raw[0];
    if (changed_out) bit_write(bytes, bit_pos, raw, 1);
    field.prev_raw[0] = raw;
    field.has_prev = true;
}

Variant PacketBuilder::_decode_bool(BakedField &field,
    const PackedByteArray &bytes, int &bit_pos) const
{
    return Variant(bit_read(bytes, bit_pos, 1) != 0);
}

// ── String ───────────────────────────────────────────────────────────────────

static int string_len_bits(int max_bytes)
{
    if (max_bytes <= 0) return 1;
    int bits = 0, v = max_bytes;
    while (v > 0) { bits++; v >>= 1; }
    return bits;
}

void PacketBuilder::_encode_string(BakedField &field, const Variant &value,
    PackedByteArray &bytes, int &bit_pos, bool &changed_out)
{
    String str = static_cast<String>(value);
    // Use prev_raw[0] as a hash for fast change detection; fall back to string compare on collision
    // Simple approach: store nothing in prev_raw, just compare the string directly.
    // We store the string in the BakedField via a side-channel below.
    // For now, always encode string (strings are infrequently sent anyway).
    // TODO: add String _prev_string field to BakedField for proper delta if needed.
    changed_out = true;
    if (field.has_prev)
    {
        // Reinterpret prev_raw as a pointer to a heap String we manage.
        // Simpler: just always mark changed for strings — they're rare events.
        changed_out = true;
    }
    field.has_prev = true;

    if (changed_out)
    {
        int lb = string_len_bits(field.string_max_bytes);
        PackedByteArray utf8 = str.to_utf8_buffer();
        int byte_count = utf8.size();
        if (byte_count > field.string_max_bytes) byte_count = field.string_max_bytes;
        bit_write(bytes, bit_pos, static_cast<int64_t>(byte_count), lb);
        for (int i = 0; i < byte_count; i++)
            bit_write(bytes, bit_pos, static_cast<int64_t>(utf8[i]), 8);
    }
}

Variant PacketBuilder::_decode_string(BakedField &field,
    const PackedByteArray &bytes, int &bit_pos) const
{
    int lb = string_len_bits(field.string_max_bytes);
    int byte_count = static_cast<int>(bit_read(bytes, bit_pos, lb));
    PackedByteArray utf8;
    utf8.resize(byte_count);
    for (int i = 0; i < byte_count; i++)
        utf8.set(i, static_cast<uint8_t>(bit_read(bytes, bit_pos, 8)));
    return Variant(utf8.get_string_from_utf8());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Core encode / decode
// ─────────────────────────────────────────────────────────────────────────────

PackedByteArray PacketBuilder::_encode_internal(const Array &values, bool force_keyframe)
{
    int n = static_cast<int>(_fields.size());
    bool actual_keyframe = force_keyframe || _needs_keyframe;

    if (actual_keyframe)
        for (BakedField &f : _fields) { f.has_prev = false; }

    // Zero only the bytes we'll actually use (scratch is pre-sized to worst case).
    // We write at most (_max_bits + n + 2 + 7) / 8 bytes.
    int max_bytes_needed = (_max_bits + n + 2 + 7) / 8;
    uint8_t *scratch_ptr = _scratch.ptrw();
    std::memset(scratch_ptr, 0, max_bytes_needed);

    int bit_pos = 0;

    PacketType type = actual_keyframe ? PacketType::PACKET_KEYFRAME : PacketType::PACKET_DELTA;
    bit_write(_scratch, bit_pos, static_cast<int64_t>(type), 2);

    int mask_bit_pos = bit_pos;
    bit_pos += n;   // reserve mask slot

    int64_t mask = 0;
    for (int i = 0; i < n; i++)
    {
        bool changed = false;
        BakedField &f = _fields[i];
        switch (f.kind)
        {
            case BakedField::KIND_TRANSFORM: _encode_transform(f, values[i], _scratch, bit_pos, changed); break;
            case BakedField::KIND_VECTOR3:   _encode_vector3  (f, values[i], _scratch, bit_pos, changed); break;
            case BakedField::KIND_FLOAT:     _encode_float    (f, values[i], _scratch, bit_pos, changed); break;
            case BakedField::KIND_BOOL:      _encode_bool     (f, values[i], _scratch, bit_pos, changed); break;
            case BakedField::KIND_STRING:    _encode_string   (f, values[i], _scratch, bit_pos, changed); break;
            default: break;
        }
        if (changed) mask |= (int64_t(1) << i);
    }

    bit_write(_scratch, mask_bit_pos, mask, n);
    _needs_keyframe = false;

    // Copy trimmed result into a fresh PackedByteArray for the caller.
    int out_bytes = (bit_pos + 7) / 8;
    PackedByteArray result;
    result.resize(out_bytes);
    std::memcpy(result.ptrw(), _scratch.ptr(), out_bytes);
    return result;
}

PackedByteArray PacketBuilder::encode_keyframe(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    ERR_FAIL_COND_V_MSG(values.size() != (int)_fields.size(), PackedByteArray(),
        "encode_keyframe(): wrong number of values");
    return _encode_internal(values, true);
}

PackedByteArray PacketBuilder::encode_delta(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    ERR_FAIL_COND_V_MSG(values.size() != (int)_fields.size(), PackedByteArray(),
        "encode_delta(): wrong number of values");
    return _encode_internal(values, false);
}

Array PacketBuilder::decode(const PackedByteArray &data, const Array &prev) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "PacketBuilder not built");
    int n = static_cast<int>(_fields.size());

    int bit_pos = 0;

    PacketType type = static_cast<PacketType>(bit_read(data, bit_pos, 2));
    ERR_FAIL_COND_V_MSG(type > PacketType::PACKET_KEYFRAME, Array(),
        vformat("decode(): unknown packet type %d", static_cast<int>(type)));

    bool is_keyframe = (type == PacketType::PACKET_KEYFRAME);

    ERR_FAIL_COND_V_MSG(prev.size() != 0 && prev.size() != n, Array(),
        "decode(): prev must be empty or exactly field_count() elements");
    ERR_FAIL_COND_V_MSG(!is_keyframe && prev.size() != n, Array(),
        "decode(): delta packet received but prev is empty — was a keyframe dropped?");

    int64_t mask = bit_read(data, bit_pos, n);

    Array result;
    result.resize(n);

    for (int i = 0; i < n; i++)
    {
        if (mask & (int64_t(1) << i))
        {
            BakedField &f = const_cast<BakedField &>(_fields[i]);
            switch (f.kind)
            {
                case BakedField::KIND_TRANSFORM: result[i] = _decode_transform(f, data, bit_pos); break;
                case BakedField::KIND_VECTOR3:   result[i] = _decode_vector3  (f, data, bit_pos); break;
                case BakedField::KIND_FLOAT:     result[i] = _decode_float    (f, data, bit_pos); break;
                case BakedField::KIND_BOOL:      result[i] = _decode_bool     (f, data, bit_pos); break;
                case BakedField::KIND_STRING:    result[i] = _decode_string   (f, data, bit_pos); break;
                default: break;
            }
        }
        else
        {
            ERR_FAIL_COND_V_MSG(is_keyframe, Array(),
                vformat("decode(): keyframe missing field %d — encoder bug", i));
            result[i] = prev[i];
        }
    }

    return result;
}

void PacketBuilder::reset_delta()
{
    for (BakedField &f : _fields) f.has_prev = false;
    _needs_keyframe = true;
}
