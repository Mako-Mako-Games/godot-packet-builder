#include "packet_builder.hpp"
#include "bit_utils.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void PacketBuilder::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("add", "encoder"), &PacketBuilder::add);
    ClassDB::bind_method(D_METHOD("build"), &PacketBuilder::build);
    ClassDB::bind_method(D_METHOD("is_built"), &PacketBuilder::is_built);
    ClassDB::bind_method(D_METHOD("field_count"), &PacketBuilder::field_count);
    ClassDB::bind_method(D_METHOD("encode", "values"), &PacketBuilder::encode);
    ClassDB::bind_method(D_METHOD("encode_delta", "values"), &PacketBuilder::encode_delta);
    ClassDB::bind_method(D_METHOD("decode", "data"), &PacketBuilder::decode);
    ClassDB::bind_method(D_METHOD("decode_delta", "data", "prev"), &PacketBuilder::decode_delta);
    ClassDB::bind_method(D_METHOD("reset_delta"), &PacketBuilder::reset_delta);
}

void PacketBuilder::add(const Ref<FieldEncoder> &encoder)
{
    ERR_FAIL_COND_MSG(_built, "Cannot add encoders after build()");
    ERR_FAIL_COND_MSG(encoder.is_null(), "Encoder must not be null");
    _encoders.push_back(encoder);
}

void PacketBuilder::build()
{
    ERR_FAIL_COND_MSG(_encoders.empty(), "PacketBuilder has no encoders");
    _max_bits = 0;
    for (const Ref<FieldEncoder> &enc : _encoders)
        _max_bits += enc->max_bits();
    _built = true;
}

PackedByteArray PacketBuilder::encode(const Array &values) const
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(), "encode(): wrong number of values");

    PackedByteArray bytes;
    bytes.resize((_max_bits + 7) / 8);
    bytes.fill(0);
    int bit_pos = 0;

    for (int i = 0; i < n; i++)
        _encoders[i]->encode_value(values[i], bytes, bit_pos);

    // Trim to actual bytes written (bit_pos may be less than _max_bits)
    bytes.resize((bit_pos + 7) / 8);
    return bytes;
}

PackedByteArray PacketBuilder::encode_delta(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(), "encode_delta(): wrong number of values");

    // Max: n mask bits + all field bits
    PackedByteArray bytes;
    bytes.resize((_max_bits + n + 7) / 8);
    bytes.fill(0);

    // Reserve space for mask — written after we know which fields changed
    // Strategy: write mask bits first as zeros, then fill them in during field loop.
    // mask_bit_start tracks where in the bit stream the mask lives.
    int mask_start = 0;
    int bit_pos = n; // data starts after n mask bits

    int64_t mask = 0;
    for (int i = 0; i < n; i++)
    {
        bool changed = false;
        _encoders[i]->encode_delta(values[i], bytes, bit_pos, changed);
        if (changed)
            mask |= (int64_t(1) << i);
    }

    // Write mask into the reserved space at the start
    bit_write(bytes, mask_start, mask, n);

    bytes.resize((bit_pos + 7) / 8);
    return bytes;
}

Array PacketBuilder::decode(const PackedByteArray &data) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());

    Array result;
    result.resize(n);
    int bit_pos = 0;

    for (int i = 0; i < n; i++)
        result[i] = _encoders[i]->decode_value(data, bit_pos);

    return result;
}

Array PacketBuilder::decode_delta(const PackedByteArray &data, const Array &prev) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());

    int bit_pos = 0;
    int64_t mask = bit_read(data, bit_pos, n); // bit_pos now = n after mask read

    Array result;
    result.resize(n);

    for (int i = 0; i < n; i++)
    {
        if (mask & (int64_t(1) << i))
            result[i] = _encoders[i]->decode_value(data, bit_pos);
        else
            result[i] = prev[i];
    }

    return result;
}

void PacketBuilder::reset_delta()
{
    for (Ref<FieldEncoder> &enc : _encoders)
        enc->reset_delta();
}
