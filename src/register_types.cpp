// src/register_types.cpp
#include "register_types.hpp"
#include "bit_writer.hpp"
#include "bit_reader.hpp"
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