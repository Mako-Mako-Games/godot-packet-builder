#!/usr/bin/env python
import os

env = SConscript("godot-cpp/SConstruct")

# -----------------------------------------------------------------------------
# Config
# -----------------------------------------------------------------------------
LIB_NAME = "packet_builder"

# Where the addon *template* lives (gdextension file, plugin.cfg, scripts, etc.)
# This gets copied 1:1 into the build output, then we drop bin/ next to it.
ADDON_TEMPLATE_DIR = "addon"

# Final addon location that ships to users / gets zipped for release.
ADDON_BUILD_DIR = "build/addons/packet-builder"
BIN_BUILD_DIR = os.path.join(ADDON_BUILD_DIR, "bin")

# -----------------------------------------------------------------------------
# Custom options (mirrors the CACHE STRING option from the old CMakeLists)
# -----------------------------------------------------------------------------
opts = Variables([], ARGUMENTS)
opts.Add(
    PathVariable(
        "godot_project_dir",
        "Directory of a Godot project folder to also copy the built addon into",
        ".",
        PathVariable.PathIsDirCreate,
    )
)
opts.Update(env)
Help(opts.GenerateHelpText(env))

# -----------------------------------------------------------------------------
# Sources / includes
# -----------------------------------------------------------------------------
env.Append(CPPPATH=["src/", "src/encoders/"])

sources = [
    "src/register_types.cpp",
    "src/field_encoder.cpp",
    "src/packet_builder.cpp",
    "src/encoders/int_encoder.cpp",
    "src/encoders/bool_encoder.cpp",
    "src/encoders/float_encoder.cpp",
    "src/encoders/vector3_encoder.cpp",
    "src/encoders/transform_encoder.cpp",
    "src/encoders/string_encoder.cpp",
    "src/bit_writer.cpp",
    "src/bit_reader.cpp",
]

# -----------------------------------------------------------------------------
# Docs (equivalent of the DOC_XML / target_doc_sources block in CMake)
# -----------------------------------------------------------------------------
doc_data = None
if os.path.isdir("doc_classes") and env["target"] in ("editor", "template_debug"):
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Warning: godot-cpp doc data generation not available, skipping docs.")

# -----------------------------------------------------------------------------
# Build the shared library
# -----------------------------------------------------------------------------
# godot-cpp's env already knows platform/suffix/arch, so we use its own naming
# convention: packet_builder.<platform>.<target>.<arch>[.suffix]
file_name = "{}{}{}".format(LIB_NAME, env["suffix"], env["SHLIBSUFFIX"])

library = env.SharedLibrary(
    target=os.path.join(BIN_BUILD_DIR, env["platform"], file_name),
    source=sources,
)

# -----------------------------------------------------------------------------
# Copy the addon template (gdextension file, plugin.cfg, scripts, etc.) into
# the build output, 1:1, every build. bin/ lives alongside it (built above).
# -----------------------------------------------------------------------------
addon_files = env.Install(ADDON_BUILD_DIR, Glob(os.path.join(ADDON_TEMPLATE_DIR, "*")))

# Also drop a copy directly into a Godot project folder if one was specified,
# matching the old GODOT_PROJECT_DIR behavior.
project_targets = []
if env["godot_project_dir"] != ".":
    project_addon_dir = os.path.join(env["godot_project_dir"], "addons", "packet-builder")
    project_targets.append(env.Install(os.path.join(project_addon_dir, "bin", env["platform"]), library))
    project_targets.append(env.Install(project_addon_dir, Glob(os.path.join(ADDON_TEMPLATE_DIR, "*"))))

default_targets = [library, addon_files] + project_targets
Default(*default_targets)
