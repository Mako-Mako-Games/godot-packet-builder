#pragma once
#include "field_encoder.hpp"

namespace godot
{

    // Encodes a float (double).
    // Default (quantize = false): writes raw 32-bit IEEE 754 float (4 bytes).
    // Quantized (quantize = true): requires min_value, max_value, bits to be set.
    class FloatEncoder : public FieldEncoder
    {
        GDCLASS(FloatEncoder, FieldEncoder)

    public:
        bool quantize = false;
        double min_value = 0.0;
        double max_value = 1.0;
        int bits = 16;

    private:
        bool _has_prev = false;
        int64_t _prev_raw = 0;

        int64_t _to_raw(double v) const;
        double _from_raw(int64_t raw) const;

    protected:
        static void _bind_methods();

    public:
        void set_quantize(bool q) { quantize = q; }
        bool get_quantize() const { return quantize; }
        void set_min_value(double v) { min_value = v; }
        double get_min_value() const { return min_value; }
        void set_max_value(double v) { max_value = v; }
        double get_max_value() const { return max_value; }
        void set_bits(int b) { bits = b; }
        int get_bits() const { return bits; }

        int max_bits() const override;
        void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const override;
        Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const override;
        void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) override;
        void reset_delta() override;
    };

} // namespace godot
