#include "register_types.hpp"
#include "bit_writer.hpp"
#include "bit_reader.hpp"
#include "field_encoder.hpp"
#include "packet_builder.hpp"
#include "encoders/int_encoder.hpp"
#include "encoders/bool_encoder.hpp"
#include "encoders/float_encoder.hpp"
#include "encoders/vector3_encoder.hpp"
#include "encoders/transform_encoder.hpp"
#include "encoders/string_encoder.hpp"
#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

void initialize_packet_builder(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
    ClassDB::register_class<BitWriter>();
    ClassDB::register_class<BitReader>();
    ClassDB::register_abstract_class<FieldEncoder>();
    ClassDB::register_class<PacketBuilder>();
    ClassDB::register_class<IntEncoder>();
    ClassDB::register_class<BoolEncoder>();
    ClassDB::register_class<FloatEncoder>();
    ClassDB::register_class<Vector3Encoder>();
    ClassDB::register_class<TransformEncoder>();
    ClassDB::register_class<StringEncoder>();
}

void uninitialize_packet_builder(ModuleInitializationLevel p_level)
{
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
        return;
}

extern "C"
{
    GDExtensionBool GDE_EXPORT packet_builder_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        const GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization)
    {
        godot::GDExtensionBinding::InitObject init_obj(
            p_get_proc_address, p_library, r_initialization);
        init_obj.register_initializer(initialize_packet_builder);
        init_obj.register_terminator(uninitialize_packet_builder);
        init_obj.set_minimum_library_initialization_level(
            MODULE_INITIALIZATION_LEVEL_SCENE);
        return init_obj.init();
    }
}