#pragma once
#include "field_encoder.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>

namespace godot
{

    // Packet type tag written in first 2 bits of encoded data.
    // (leaves room to add more types later without breaking the format)
    enum class PacketType : uint8_t
    {
        PACKET_DELTA    = 0,
        PACKET_KEYFRAME = 1,
        PACKET_INVALID  = 3,  // Sentinel for corruption/version mismatch
    };

    class PacketBuilder : public RefCounted
    {
        GDCLASS(PacketBuilder, RefCounted)

    private:
        std::vector<Ref<FieldEncoder>> _encoders;
        bool _built = false;
        int _max_bits = 0; // sum of all encoder max_bits — for full encode buffer sizing
        bool _needs_keyframe = true; // Set to false after first successful encode

        // Internal — force_keyframe=true sets all mask bits, ignores encoder prev state
        PackedByteArray _encode_internal(const Array &values, bool force_keyframe);

    protected:
        static void _bind_methods();

    public:
        // Add an encoder in field order.
        void add(const Ref<FieldEncoder> &encoder);

        // Lock the schema. Must be called before encode/decode.
        void build();

        bool is_built() const { return _built; }
        int field_count() const { return static_cast<int>(_encoders.size()); }

        // Full snapshot — all fields written, mask all 1s, tagged as keyframe.
        // decode() can consume this directly as a prev=[] first packet.
        PackedByteArray encode_keyframe(const Array &values);

        // Delta — only changed fields written. On first call with no prev state,
        // behaves identically to encode_keyframe automatically.
        PackedByteArray encode_delta(const Array &values);

        // Single decoder for both packet types.
        // For PACKET_KEYFRAME, prev is ignored and can be empty.
        // For PACKET_DELTA, prev must be the last decoded result.
        Array decode(const PackedByteArray &data, const Array &prev) const;

        // Reset all encoder delta states and mark next encode as keyframe.
        void reset_delta();
    };

} // namespace godot
