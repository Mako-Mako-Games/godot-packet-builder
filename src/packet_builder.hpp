#pragma once
#include "field_encoder.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>

namespace godot
{

    class PacketBuilder : public RefCounted
    {
        GDCLASS(PacketBuilder, RefCounted)

    private:
        std::vector<Ref<FieldEncoder>> _encoders;
        bool _built = false;
        int _max_bits = 0; // sum of all encoder max_bits — for full encode buffer sizing

    protected:
        static void _bind_methods();

    public:
        // Add an encoder in field order.
        void add(const Ref<FieldEncoder> &encoder);

        // Lock the schema. Must be called before encode/decode.
        void build();

        bool is_built() const { return _built; }
        int field_count() const { return static_cast<int>(_encoders.size()); }

        // Full encode — values Array ordered by encoder index.
        PackedByteArray encode(const Array &values) const;

        // Delta encode — only changed fields written; 1-bit-per-field mask prepended.
        PackedByteArray encode_delta(const Array &values);

        // Decode a full packet produced by encode().
        Array decode(const PackedByteArray &data) const;

        // Decode a delta packet — prev is the Array from the last decode/decode_delta.
        Array decode_delta(const PackedByteArray &data, const Array &prev) const;

        // Reset all encoder delta states.
        void reset_delta();
    };

} // namespace godot
