#pragma once
#include "field_encoder.hpp"
#include <godot_cpp/variant/string.hpp>

namespace godot
{

    // Encodes a String as a length-prefixed byte sequence.
    // max_bytes: maximum encoded UTF-8 byte count (default 256).
    //            The length prefix uses ceil(log2(max_bytes+1)) bits.
    // Strings longer than max_bytes are truncated at the nearest valid UTF-8 boundary.
    // Delta: always on. Compares the raw encoded bytes.
    class StringEncoder : public FieldEncoder
    {
        GDCLASS(StringEncoder, FieldEncoder)

    public:
        int max_bytes = 256;

    private:
        bool _has_prev = false;
        String _prev_value;

        int _len_bits() const;

    protected:
        static void _bind_methods();

    public:
        void set_max_bytes(int b) { max_bytes = b; }
        int get_max_bytes() const { return max_bytes; }

        int max_bits() const override;
        void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const override;
        Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const override;
        void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) override;
        void reset_delta() override;
    };

} // namespace godot
