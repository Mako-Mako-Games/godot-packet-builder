#include "packet_schema.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ─────────────────────────────────────────────
//  Binding
// ─────────────────────────────────────────────
void PacketSchema::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("add_uint", "name", "bits"), &PacketSchema::add_uint);
    ClassDB::bind_method(D_METHOD("add_bool", "name"), &PacketSchema::add_bool);
    ClassDB::bind_method(D_METHOD("add_quantized_float", "name", "min_val", "max_val", "bits"), &PacketSchema::add_quantized_float);
    ClassDB::bind_method(D_METHOD("build"), &PacketSchema::build);
    ClassDB::bind_method(D_METHOD("is_built"), &PacketSchema::is_built);
    ClassDB::bind_method(D_METHOD("field_count"), &PacketSchema::field_count);
    ClassDB::bind_method(D_METHOD("encode", "values"), &PacketSchema::encode);
    ClassDB::bind_method(D_METHOD("encode_delta", "values"), &PacketSchema::encode_delta);
    ClassDB::bind_method(D_METHOD("decode", "data"), &PacketSchema::decode);
    ClassDB::bind_method(D_METHOD("decode_delta", "data", "prev"), &PacketSchema::decode_delta);
    ClassDB::bind_method(D_METHOD("reset_delta"), &PacketSchema::reset_delta);
}

// ─────────────────────────────────────────────
//  Schema definition
// ─────────────────────────────────────────────
void PacketSchema::add_uint(const String &name, int bits)
{
    ERR_FAIL_COND_MSG(_built, "Cannot add fields after build()");
    ERR_FAIL_COND_MSG(bits < 1 || bits > 63, "bits must be in [1, 63]");
    Field f;
    f.type = FIELD_UINT;
    f.bits = bits;
    f.min_val = 0;
    f.max_val = 0;
    _fields.push_back(f);
}

void PacketSchema::add_bool(const String &name)
{
    ERR_FAIL_COND_MSG(_built, "Cannot add fields after build()");
    Field f;
    f.type = FIELD_BOOL;
    f.bits = 1;
    f.min_val = 0;
    f.max_val = 0;
    _fields.push_back(f);
}

void PacketSchema::add_quantized_float(const String &name, double min_val, double max_val, int bits)
{
    ERR_FAIL_COND_MSG(_built, "Cannot add fields after build()");
    ERR_FAIL_COND_MSG(bits < 1 || bits > 32, "bits must be in [1, 32]");
    ERR_FAIL_COND_MSG(max_val <= min_val, "max_val must be > min_val");
    Field f;
    f.type = FIELD_QUANTIZED_FLOAT;
    f.bits = bits;
    f.min_val = min_val;
    f.max_val = max_val;
    _fields.push_back(f);
}

void PacketSchema::build()
{
    ERR_FAIL_COND_MSG(_fields.empty(), "Schema has no fields");
    int n = static_cast<int>(_fields.size());
    _total_bits = 0;
    for (const Field &f : _fields)
        _total_bits += f.bits;
    _scratch_vals.resize(n);
    _scratch_changed.resize(n);
    _built = true;
}

void PacketSchema::reset_delta()
{
    for (Field &f : _fields)
    {
        f.has_prev = false;
        f.prev_value = -1;
    }
}

// ─────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────
int64_t PacketSchema::_quantize(double value, double min_val, double max_val, int bits) const
{
    int64_t max_int = (int64_t(1) << bits) - 1;
    double normalized = (value - min_val) / (max_val - min_val);
    normalized = CLAMP(normalized, 0.0, 1.0);
    return static_cast<int64_t>(normalized * static_cast<double>(max_int));
}

double PacketSchema::_dequantize(int64_t quantized, double min_val, double max_val, int bits) const
{
    int64_t max_int = (int64_t(1) << bits) - 1;
    return min_val + (static_cast<double>(quantized) / static_cast<double>(max_int)) * (max_val - min_val);
}

int64_t PacketSchema::_variant_to_int(const Field &field, const Variant &value) const
{
    switch (field.type)
    {
    case FIELD_BOOL:
        return static_cast<bool>(value) ? 1 : 0;
    case FIELD_UINT:
        return static_cast<int64_t>(value);
    case FIELD_QUANTIZED_FLOAT:
        return _quantize(static_cast<double>(value), field.min_val, field.max_val, field.bits);
    default:
        return 0;
    }
}

Variant PacketSchema::_int_to_variant(const Field &field, int64_t value) const
{
    switch (field.type)
    {
    case FIELD_BOOL:
        return Variant(value != 0);
    case FIELD_UINT:
        return Variant(value);
    case FIELD_QUANTIZED_FLOAT:
        return Variant(_dequantize(value, field.min_val, field.max_val, field.bits));
    default:
        return Variant();
    }
}

// LSB-first bit writer — writes into pre-sized `bytes`
void PacketSchema::_write_bits(PackedByteArray &bytes, int &bit_count, int64_t value, int bits)
{
    int written = 0;
    while (written < bits)
    {
        int byte_idx = bit_count / 8;
        int bit_in_byte = bit_count % 8;
        int bits_available = 8 - bit_in_byte;
        int bits_to_write = MIN(bits - written, bits_available);
        uint8_t chunk = static_cast<uint8_t>((value >> written) & ((int64_t(1) << bits_to_write) - 1));
        bytes.set(byte_idx, bytes[byte_idx] | (chunk << bit_in_byte));
        bit_count += bits_to_write;
        written += bits_to_write;
    }
}

// LSB-first bit reader
int64_t PacketSchema::_read_bits(const PackedByteArray &bytes, int &bit_pos, int bits)
{
    int64_t result = 0;
    int read = 0;
    while (read < bits)
    {
        int byte_idx = bit_pos / 8;
        int bit_in_byte = bit_pos % 8;
        int bits_available = 8 - bit_in_byte;
        int bits_to_read = MIN(bits - read, bits_available);
        int64_t chunk = (static_cast<int64_t>(bytes[byte_idx]) >> bit_in_byte) & ((int64_t(1) << bits_to_read) - 1);
        result |= (chunk << read);
        bit_pos += bits_to_read;
        read += bits_to_read;
    }
    return result;
}

// ─────────────────────────────────────────────
//  Encode / Decode
// ─────────────────────────────────────────────
PackedByteArray PacketSchema::encode(const Array &values) const
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "Schema not built");

    int n = static_cast<int>(_fields.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(), "encode(): wrong number of values");

    PackedByteArray bytes;
    bytes.resize((_total_bits + 7) / 8);
    bytes.fill(0);
    int bit_count = 0;

    for (int i = 0; i < n; i++)
        _write_bits(bytes, bit_count, _variant_to_int(_fields[i], values[i]), _fields[i].bits);

    return bytes;
}

PackedByteArray PacketSchema::encode_delta(const Array &values)
{
    ERR_FAIL_COND_V_MSG(!_built, PackedByteArray(), "Schema not built");

    int n = static_cast<int>(_fields.size());
    ERR_FAIL_COND_V_MSG(values.size() != n, PackedByteArray(), "encode_delta(): wrong number of values");

    // Quantize, detect changes, build mask, compute exact output bit count in one pass
    int64_t mask = 0;
    int exact_bits = n; // mask occupies n bits
    for (int i = 0; i < n; i++)
    {
        _scratch_vals[i] = _variant_to_int(_fields[i], values[i]);
        _scratch_changed[i] = !_fields[i].has_prev || (_scratch_vals[i] != _fields[i].prev_value);
        if (_scratch_changed[i])
        {
            mask |= (int64_t(1) << i);
            exact_bits += _fields[i].bits;
        }
    }

    PackedByteArray bytes;
    bytes.resize((exact_bits + 7) / 8);
    bytes.fill(0);
    int bit_count = 0;

    // Write entire mask in one call
    _write_bits(bytes, bit_count, mask, n);

    for (int i = 0; i < n; i++)
    {
        if (_scratch_changed[i])
            _write_bits(bytes, bit_count, _scratch_vals[i], _fields[i].bits);
    }

    for (int i = 0; i < n; i++)
    {
        _fields[i].prev_value = _scratch_vals[i];
        _fields[i].has_prev = true;
    }

    return bytes;
}

Array PacketSchema::decode(const PackedByteArray &data) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "Schema not built");

    int n = static_cast<int>(_fields.size());
    Array result;
    result.resize(n);
    int bit_pos = 0;

    for (int i = 0; i < n; i++)
        result[i] = _int_to_variant(_fields[i], _read_bits(data, bit_pos, _fields[i].bits));

    return result;
}

Array PacketSchema::decode_delta(const PackedByteArray &data, const Array &prev) const
{
    ERR_FAIL_COND_V_MSG(!_built, Array(), "Schema not built");

    int n = static_cast<int>(_fields.size());
    int bit_pos = 0;

    // Read entire mask in one call
    int64_t mask = _read_bits(data, bit_pos, n);

    Array result;
    result.resize(n);

    for (int i = 0; i < n; i++)
    {
        if (mask & (int64_t(1) << i))
            result[i] = _int_to_variant(_fields[i], _read_bits(data, bit_pos, _fields[i].bits));
        else
            result[i] = prev[i];
    }

    return result;
}