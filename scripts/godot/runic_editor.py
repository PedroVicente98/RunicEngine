# SCons profile for libgodot, EDITOR target: the RunicEditor, the development GameClient and the Cooker on
# build machines (RunicEditor --headless). Used by cmake/RunicGodot.cmake:
#
#   scons platform=linuxbsd target=editor profile=scripts/godot/runic_editor.py
#
# Pinned Godot: external/godot (4.7.2-stable). Module names change between versions: run
# `python3 scripts/godot/list_modules.py external/godot` at every Godot update and review this list and
# runic_template.py before changing the pin (RunicDocs › Godot integration › Building Godot).
# Arguments on the SCons command line override this file.

target = "editor"
library_type = "shared_library"     # LibGodot: a .so / .dll the host loads, not an executable
precision = "single"                 # Godot stays single precision; Runic keeps 64-bit positions itself
deprecated = "yes"                   # compatibility code that community GDExtension plugins may still use
accesskit = "no"                     # screen-reader driver: needs a separate prebuilt SDK; off until wanted

# ---- Modules: everything off, then only the editor's list -------------------------------------------------
modules_enabled_by_default = "no"

# Required by every editor build (SConstruct refuses to build the editor without them).
module_freetype_enabled = "yes"
module_regex_enabled = "yes"
module_svg_enabled = "yes"           # the editor's icons are SVG

# Text and fonts: shaping (HarfBuzz, ICU) and MSDF fonts for Label3D and world UI.
module_text_server_adv_enabled = "yes"
module_msdfgen_enabled = "yes"       # requires freetype

# Rendering.
module_glslang_enabled = "yes"       # Vulkan shaders compiled at runtime
module_raycast_enabled = "yes"       # occlusion culling and occluder baking
module_lightmapper_rd_enabled = "yes"
module_xatlas_unwrap_enabled = "yes" # UV2 unwrap on import (lightmaps)
module_meshoptimizer_enabled = "yes" # LODs and shadow meshes on import

# Importers: glTF is the border between Blender and Godot.
module_gltf_enabled = "yes"
module_fbx_enabled = "yes"           # requires gltf

# Importers: images, and the texture compressors used on import.
module_jpg_enabled = "yes"
module_webp_enabled = "yes"
module_bmp_enabled = "yes"
module_tga_enabled = "yes"
module_hdr_enabled = "yes"
module_tinyexr_enabled = "yes"
module_dds_enabled = "yes"
module_ktx_enabled = "yes"           # requires basis_universal
module_basis_universal_enabled = "yes"  # requires tinyexr in editor builds
module_etcpak_enabled = "yes"
module_cvtt_enabled = "yes"
module_betsy_enabled = "yes"
module_astcenc_enabled = "yes"
module_bcdec_enabled = "yes"

# Audio import and playback.
module_ogg_enabled = "yes"
module_vorbis_enabled = "yes"        # requires ogg
module_mp3_enabled = "yes"

# Materials and editor utilities.
module_noise_enabled = "yes"
module_visual_shader_enabled = "yes"
module_zip_enabled = "yes"           # ZIP packer and reader (templates, asset library downloads)
module_mbedtls_enabled = "yes"       # HTTPS for the asset library

# GDScript stays OFF in slices 0 to 19. It returns in this target only, with its locks, in slices 20-21
# (RunicDocs › Godot integration › GDScript: editor tools only). Its optional dependencies are jsonrpc
# (language server) and websocket (remote debugger):
# module_gdscript_enabled = "yes"
# module_jsonrpc_enabled = "yes"
# module_websocket_enabled = "yes"

# Off: mono (C#), text_server_fb, godot_physics_2d, godot_physics_3d, jolt_physics, navigation_2d,
# navigation_3d, csg, gridmap, multiplayer, enet, upnp, webrtc, websocket, jsonrpc, openxr, mobile_vr,
# webxr, camera, theora, vhacd, interactive_music, objectdb_profiler. With no physics module Godot falls
# back to its dummy physics server, so the process never holds a second copy of Jolt.

# ---- Feature options ---------------------------------------------------------------------------------------
# Editor builds refuse disable_3d, disable_advanced_gui, disable_physics_* and disable_navigation_*;
# only XR may be disabled here.
disable_xr = "yes"
