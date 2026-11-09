# SCons profile for libgodot, TEMPLATE target: the shipping GameClient (runtime only: rendering, audio,
# input and resource loading from the exported .pck). Used by cmake/RunicGodot.cmake:
#
#   scons platform=linuxbsd target=template_release profile=scripts/godot/runic_template.py \
#         build_profile=<absolute path to scripts/godot/runic_client.gdbuild>
#
# The development GameClient uses the EDITOR library instead, so it reads the project folder directly
# (RunicDocs › Godot integration › Executables and Godot targets). Pinned Godot: external/godot.

library_type = "shared_library"
precision = "single"
accesskit = "no"

modules_enabled_by_default = "no"

# Text and fonts (player UI, nameplates, Label3D with MSDF).
module_freetype_enabled = "yes"
module_text_server_adv_enabled = "yes"
module_msdfgen_enabled = "yes"
module_svg_enabled = "yes"           # SVG-in-OpenType fonts; small

# Rendering.
module_glslang_enabled = "yes"
module_raycast_enabled = "yes"       # runtime occlusion culling

# Texture decoders for what Godot's importer put in the .pck.
module_webp_enabled = "yes"
module_bcdec_enabled = "yes"         # BCn fallback on GPUs without BC support

# Audio decoders: sound effects from the .pck and the player's own soundtrack files.
module_ogg_enabled = "yes"
module_vorbis_enabled = "yes"
module_mp3_enabled = "yes"

# Resource classes that materials may use.
module_noise_enabled = "yes"
module_visual_shader_enabled = "yes"

# Off, always, in the client: gdscript, mono, every physics and navigation module, multiplayer, enet,
# webrtc, websocket, jsonrpc, upnp, mbedtls, zip, regex, gltf, fbx, every importer and texture compressor,
# lightmapper_rd, xatlas_unwrap, openxr, mobile_vr, webxr, camera, theora, csg, gridmap, vhacd.

# ---- Whole engine features (template builds only) ------------------------------------------------------
disable_physics_2d = "yes"           # Runic runs its own Jolt; Godot's physics never runs in the client
disable_physics_3d = "yes"
disable_navigation_2d = "yes"
disable_navigation_3d = "yes"
disable_xr = "yes"
minizip = "no"
# 2D (UI is CanvasItem and Control), 3D (Label3D, Sprite3D, MultiMesh world UI) and the advanced GUI
# (RichTextLabel, Tree, ItemList) stay.

# The fine trim (single classes) is the build profile runic_client.gdbuild, passed on the command line as
# build_profile=<absolute path>: a relative path in this file would resolve from the Godot checkout.
