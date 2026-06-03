#pragma once
#include "field_encoder.hpp"

namespace godot
{

    // Encodes a bool as 1 bit.
    class BoolEncoder : public FieldEncoder
    {
        GDCLASS(BoolEncoder, FieldEncoder)

    private:
        bool _has_prev = false;
        bool _prev_value = false;

    protected:
        static void _bind_methods();

    public:
        int max_bits() const override { return 1; }
        void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const override;
        Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const override;
        void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) override;
        void reset_delta() override;
    };

} // namespace godot
