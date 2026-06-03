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
    ClassDB::bind_method(D_METHOD("encode_keyframe", "values"), &PacketBuilder::encode_keyframe);
    ClassDB::bind_method(D_METHOD("encode_delta", "values"), &PacketBuilder::encode_delta);
    ClassDB::bind_method(D_METHOD("decode", "data", "prev"), &PacketBuilder::decode);
    ClassDB::bind_method(D_METHOD("reset_delta"), &PacketBuilder::reset_delta);
}

void PacketBuilder::add(const Ref<FieldEncoder> &encoder)
{
    ERR_FAIL_COND_MSG(_built, "Cannot add encoders after build()");
    ERR_FAIL_COND_MSG(encoder.is_null(), "Encoder must not be null");
    // mask is int64_t — hard limit of 63 fields
    ERR_FAIL_COND_MSG(_encoders.size() >= 63, "PacketBuilder: maximum 63 fields (mask is int64)");
    _encoders.push_back(encoder);
}

void PacketBuilder::build()
{
    ERR_FAIL_COND_MSG(_encoders.empty(), "PacketBuilder has no encoders");
    _max_bits = 0;
    for (const Ref<FieldEncoder> &enc : _encoders)
    {
        ERR_FAIL_COND_MSG(enc.is_null(), "Encoder must not be null");
        // max_bits() is const and called only at build time — virtual call cost is fine
        _max_bits += enc->max_bits();
    }
    _built = true;
    _needs_keyframe = true; // First encode after build() is always a keyframe
}

PackedByteArray PacketBuilder::_encode_internal(const Array &values, bool force_keyframe)
{
    int n = static_cast<int>(_encoders.size());
    bool actual_needs_keyframe = force_keyframe || _needs_keyframe;

    // For keyframe: reset all encoder state so encode_delta naturally reports
    // every field as changed (no prev state after reset).
    if (actual_needs_keyframe)
    {
        for (Ref<FieldEncoder> &enc : _encoders)
            enc->reset_delta();
    }

    // Pre-allocate for worst case: 2 type bits + n mask bits + all field data bits.
    // encode_delta may write fewer bits when fields are unchanged (delta path),
    // so this size is always sufficient. Trimmed at the end.
    PackedByteArray bytes;
    bytes.resize((_max_bits + n + 2 + 7) / 8);
    bytes.fill(0);

    int bit_pos = 0;

    // 2-bit packet type tag
    PacketType type = actual_needs_keyframe ? PacketType::PACKET_KEYFRAME : PacketType::PACKET_DELTA;
    bit_write(bytes, bit_pos, static_cast<uint8_t>(type), 2);

    // Reserve space for the n-bit mask; backfill it once all fields are encoded.
    int mask_bit_pos = bit_pos;
    bit_pos += n;

    // Single pass: encode_delta writes field data only if changed, and always
    // commits prev state. For keyframe (reset above), every encoder sees no prev
    // and reports changed=true, so every field is written and mask is all 1s.
    int64_t mask = 0;
    for (int i = 0; i < n; i++)
    {
        bool changed = false;
        _encoders[i]->encode_delta(values[i], bytes, bit_pos, changed);
        if (changed)
            mask |= (int64_t(1) << i);
    }

    // Backfill mask into its reserved slot (mask_bit_pos is not advanced by field writes)
    bit_write(bytes, mask_bit_pos, mask, n);

    _needs_keyframe = false;

    // Trim to actual bytes written
    bytes.resize((bit_pos + 7) / 8);
    return bytes;
}

PackedByteArray PacketBuilder::encode_keyframe(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(),
                        "encode_keyframe(): wrong number of values");
    return _encode_internal(values, true);
}

PackedByteArray PacketBuilder::encode_delta(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(),
                        "encode_delta(): wrong number of values");
    return _encode_internal(values, false);
}

Array PacketBuilder::decode(const PackedByteArray &data, const Array &prev) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "PacketBuilder not built");
    int n = static_cast<int>(_encoders.size());

    int bit_pos = 0;

    // Read and validate packet type tag (2 bits)
    PacketType type = static_cast<PacketType>(bit_read(data, bit_pos, 2));
    ERR_FAIL_COND_V_MSG(type > PacketType::PACKET_KEYFRAME, Array(),
        vformat("decode(): unknown packet type %d — version mismatch or corruption",
                static_cast<int>(type)));

    bool is_keyframe = (type == PacketType::PACKET_KEYFRAME);

    // prev must be empty or exactly n elements — no other size is valid
    ERR_FAIL_COND_V_MSG(prev.size() != 0 && prev.size() != n, Array(),
        "decode(): prev must be empty or exactly field_count() elements");

    // Delta requires prev — a missing prev means a keyframe was dropped
    ERR_FAIL_COND_V_MSG(!is_keyframe && prev.size() != n, Array(),
        "decode(): delta packet received but prev is empty or wrong size — "
        "was a keyframe dropped?");

    // Read n-bit field mask
    int64_t mask = bit_read(data, bit_pos, n);

    Array result;
    result.resize(n);

    for (int i = 0; i < n; i++)
    {
        if (mask & (int64_t(1) << i))
        {
            result[i] = _encoders[i]->decode_value(data, bit_pos);
        }
        else
        {
            // Keyframe must have all mask bits set — if we get here, the encoder
            // produced a malformed packet
            ERR_FAIL_COND_V_MSG(is_keyframe, Array(),
                vformat("decode(): keyframe missing field %d — encoder bug", i));
            result[i] = prev[i];
        }
    }

    return result;
}

void PacketBuilder::reset_delta()
{
    for (Ref<FieldEncoder> &enc : _encoders)
        enc->reset_delta();
    // Ensure the next encode produces a keyframe so the receiver can
    // re-sync without relying on dropped state.
    _needs_keyframe = true;
}
