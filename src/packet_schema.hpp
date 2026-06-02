#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/array.hpp>
#include <vector>

namespace godot
{

    class PacketSchema : public RefCounted
    {
        GDCLASS(PacketSchema, RefCounted)

    public:
        enum FieldType
        {
            FIELD_UINT,
            FIELD_BOOL,
            FIELD_QUANTIZED_FLOAT,
        };

    private:
        struct Field
        {
            FieldType type;
            int bits;
            double min_val; // quantized_float only
            double max_val; // quantized_float only

            // last known value for delta — stored as int64 (quantized or raw)
            int64_t prev_value = -1;
            bool has_prev = false;
        };

        std::vector<Field> _fields;
        bool _built = false;
        int _total_bits = 0;

        // Scratch buffers pre-allocated in build() to avoid per-call heap allocs
        mutable std::vector<int64_t> _scratch_vals;
        mutable std::vector<uint8_t> _scratch_changed; // uint8_t avoids std::vector<bool> bit-packing

        // Internal helpers
        int64_t _quantize(double value, double min_val, double max_val, int bits) const;
        double _dequantize(int64_t quantized, double min_val, double max_val, int bits) const;
        int64_t _variant_to_int(const Field &field, const Variant &value) const;
        Variant _int_to_variant(const Field &field, int64_t value) const;

        // Low-level bit write/read (LSB-first)
        static void _write_bits(PackedByteArray &bytes, int &bit_count, int64_t value, int bits);
        static int64_t _read_bits(const PackedByteArray &bytes, int &bit_pos, int bits);

    protected:
        static void _bind_methods();

    public:
        // Schema definition — call before build()
        void add_uint(const String &name, int bits);
        void add_bool(const String &name);
        void add_quantized_float(const String &name, double min_val, double max_val, int bits);

        // Lock the schema. Must be called before encode/decode.
        void build();

        bool is_built() const { return _built; }
        int field_count() const { return static_cast<int>(_fields.size()); }

        // Full encode — values Array ordered by field definition index
        PackedByteArray encode(const Array &values) const;

        // Delta encode — only changed fields written; delta mask prepended.
        PackedByteArray encode_delta(const Array &values);

        // Decode a full packet — returns Array ordered by field definition index
        Array decode(const PackedByteArray &data) const;

        // Decode a delta packet — prev is the Array from the last decode
        Array decode_delta(const PackedByteArray &data, const Array &prev) const;

        // Reset stored delta state (call when a peer reconnects etc.)
        void reset_delta();
    };

} // namespace godot