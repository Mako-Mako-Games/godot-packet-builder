#pragma once
#include "field_encoder.hpp"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>

namespace godot
{

    // Packet type tag written in first 2 bits of encoded data.
    enum class PacketType : uint8_t
    {
        PACKET_DELTA    = 0,
        PACKET_KEYFRAME = 1,
        PACKET_INVALID  = 3,
    };

    // Self-contained baked field configuration.
    // Extracted from encoder objects at build() time.
    // No virtual calls or GDExtension dispatch in hot path.
    struct BakedField
    {
        enum Kind : uint8_t
        {
            KIND_TRANSFORM = 0,
            KIND_VECTOR3   = 1,
            KIND_FLOAT     = 2,
            KIND_INT       = 3,
            KIND_BOOL      = 4,
            KIND_STRING    = 5,
        };

        Kind kind;

        // TransformEncoder config
        bool   transform_quantize  = false;
        double transform_pos_min   = 0, transform_pos_max = 0;
        int    transform_pos_bits  = 0, transform_rot_bits = 0;

        // Vector3Encoder config
        bool   vector3_quantize = false;
        double vector3_min = 0, vector3_max = 0;
        int    vector3_bits = 0;

        // FloatEncoder config
        bool   float_quantize = false;
        double float_min = 0, float_max = 0;
        int    float_bits = 0;

        // IntEncoder config (reserved)
        int64_t int_min = 0, int_max = 0;
        int     int_bits = 0;

        // StringEncoder config
        int string_max_bytes = 0;

        // Delta state — owned here, not in encoder objects.
        // Sized to 7: Transform needs 3 (pos x/y/z) + 4 (rot drop/a/b/c).
        // All other types use prev_raw[0..2] at most.
        bool    has_prev = false;
        int64_t prev_raw[7] = {0, 0, 0, 0, 0, 0, 0};
    };

    class PacketBuilder : public RefCounted
    {
        GDCLASS(PacketBuilder, RefCounted)

    private:
        std::vector<Ref<FieldEncoder>> _encoders;  // kept alive for refcount; not used in hot path
        std::vector<BakedField>        _fields;    // baked config; used in hot path
        bool             _built         = false;
        int              _max_bits      = 0;
        bool             _needs_keyframe = true;
        PackedByteArray  _scratch;                 // reused every encode call — no per-call alloc

        void _bake_fields();
        PackedByteArray _encode_internal(const Array &values, bool force_keyframe);

        // Static-dispatch encode/decode — one function per concrete encoder type.
        // No virtual calls, no GDExtension dispatch.
        void    _encode_transform(BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);
        void    _encode_vector3  (BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);
        void    _encode_float    (BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);
        void    _encode_int      (BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);
        void    _encode_bool     (BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);
        void    _encode_string   (BakedField &f, const Variant &v, PackedByteArray &bytes, int &bit_pos, bool &changed_out);

        Variant _decode_transform(BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;
        Variant _decode_vector3  (BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;
        Variant _decode_float    (BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;
        Variant _decode_int      (BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;
        Variant _decode_bool     (BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;
        Variant _decode_string   (BakedField &f, const PackedByteArray &bytes, int &bit_pos) const;

    protected:
        static void _bind_methods();

    public:
        void add(const Ref<FieldEncoder> &encoder);
        void build();

        bool is_built()    const { return _built; }
        int  field_count() const { return static_cast<int>(_encoders.size()); }

        PackedByteArray encode_keyframe(const Array &values);
        PackedByteArray encode_delta   (const Array &values);
        Array           decode         (const PackedByteArray &data, const Array &prev) const;
        void            reset_delta    ();
    };

} // namespace godot
