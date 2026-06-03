#pragma once
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot
{

    // Abstract base for all field encoders.
    // Subclassing from GDScript is not supported — use the built-in encoder classes.
    class FieldEncoder : public RefCounted
    {
        GDCLASS(FieldEncoder, RefCounted)

    protected:
        static void _bind_methods() {}

    public:

        // Maximum number of bits this encoder can write in a single encode call.
        // Used by PacketBuilder to pre-size the output buffer.
        virtual int max_bits() const = 0;

        // Write value into bytes at bit_pos. Advances bit_pos.
        virtual void encode_value(const Variant &value, PackedByteArray &bytes, int &bit_pos) const = 0;

        // Read a value from bytes at bit_pos. Advances bit_pos.
        virtual Variant decode_value(const PackedByteArray &bytes, int &bit_pos) const = 0;

        // Write value only if changed since last call. Sets changed_out accordingly.
        // Still responsible for updating internal prev state (is_changed + write + commit in one go).
        virtual void encode_delta(const Variant &value, PackedByteArray &bytes, int &bit_pos, bool &changed_out) = 0;

        // Reset internal delta state (call when a peer reconnects).
        virtual void reset_delta() = 0;
    };

} // namespace godot
