#pragma once
#include "field_encoder.hpp"
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/quaternion.hpp>

namespace godot
{

    // Encodes a Transform3D (position + rotation).
    //
    // Position: 3x float, raw or quantized (pos_min, pos_max, pos_bits).
    // Rotation: quaternion, raw (4x 32-bit float = 128 bits) or
    //           smallest-3 compressed (quantize = true): 2 bits for dropped index
    //           + 3x rot_bits each (default 10 bits each = 32 bits total).
    //
    // quantize flag controls both position and rotation compression.
    class TransformEncoder : public FieldEncoder
    {
        GDCLASS(TransformEncoder, FieldEncoder)

    public:
        bool quantize = false;

        // Position config (used when quantize = true)
        double pos_min = -1000.0;
        double pos_max = 1000.0;
        int pos_bits = 16;

        // Rotation config: bits per smallest-3 component (used when quantize = true)
        int rot_bits = 10;

    private:
        bool _has_prev = false;
        int64_t _prev_pos[3] = {0, 0, 0};
        int64_t _prev_rot[4] = {0, 0, 0, 0}; // raw: 4x float bits; quantized: [drop_idx, a, b, c]

        int _pos_component_bits() const { return quantize ? pos_bits : 32; }

        // Helpers
        void _encode_position(const Vector3 &pos, PackedByteArray &bytes, int &bit_pos) const;
        Vector3 _decode_position(const PackedByteArray &bytes, int &bit_pos) const;
        void _encode_rotation(const Quaternion &q, PackedByteArray &bytes, int &bit_pos) const;
        Quaternion _decode_rotation(const PackedByteArray &bytes, int &bit_pos) const;

        void _get_pos_raws(const Vector3 &pos, int64_t out[3]) const;
        void _get_rot_raws(const Quaternion &q, int64_t out[4]) const;

    protected:
        static void _bind_methods();

    public:
        void set_quantize(bool q) { quantize = q; }
        bool get_quantize() const { return quantize; }
        void set_pos_min(double v) { pos_min = v; }
        double get_pos_min() const { return pos_min; }
        void set_pos_max(double v) { pos_max = v; }
        double get_pos_max() const { return pos_max; }
        void set_pos_bits(int b) { pos_bits = b; }
        int get_pos_bits() const { return pos_bits; }
        void set_rot_bits(int b) { rot_bits = b; }
        int get_rot_bits() const { return rot_bits; }

        int max_bits() const override;
        void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const override;
        Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const override;
        void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) override;
        void reset_delta() override;
    };

} // namespace godot
