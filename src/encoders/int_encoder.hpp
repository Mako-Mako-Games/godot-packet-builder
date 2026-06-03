#pragma once
#include "field_encoder.hpp"

namespace godot
{

    // Encodes an integer value.
    // bits: number of bits to write (1–64). Default 64 = full int64 (8 bytes, no quantization).
    // Delta: always on. Compares raw bit patterns.
    class IntEncoder : public FieldEncoder
    {
        GDCLASS(IntEncoder, FieldEncoder)

    public:
        int bits = 64;

    private:
        bool _has_prev = false;
        int64_t _prev_value = 0;

    protected:
        static void _bind_methods();

    public:
        void set_bits(int b) { bits = b; }
        int get_bits() const { return bits; }

        int max_bits() const override { return bits; }
        void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const override;
        Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const override;
        void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) override;
        void reset_delta() override;
    };

} // namespace godot
