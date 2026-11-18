// Reading a scene Godot imported into the Cooker's input (EditorCook.hpp: ReadImportedScene).

#include "EditorCook.hpp"

#include <cctype>
#include <format>
#include <map>
#include <string>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>

#include "AssetCatalog.hpp"
#include "ContentId.hpp"
#include "MarkerNodes/RunicCollider3D.hpp"

namespace Runic::GodotTools {

namespace {

using Detail::RunicCollider3D;

constexpr std::string_view kResPrefix = "res://";
constexpr std::string_view kGameNamespacePrefix = "game:";
constexpr std::string_view kPackedSceneType = "PackedScene";
constexpr std::string_view kCollisionPresetHint =
    "The mesh itself,None,Collision meshes (-rcol),Convex hull (not cooked yet),Simplified mesh "
    "(not cooked yet)";
constexpr char kPathSeparator = '/';
constexpr char kWordSeparator = '_';
constexpr char kExtensionMark = '.';
constexpr int64_t kNoUid = -1;
constexpr std::size_t kTriangleCorners = 3;

godot::String ToGodot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}
std::string ToStd(const godot::String &text) { return text.utf8().get_data(); }

Core::Vec3 ToVec3(const godot::Vector3 &v) { return {v.x, v.y, v.z}; }

bool IsCollisionOnly(const godot::Node *node) {
    return node->has_meta(ToGodot(kCollisionOnlyMeta)) ||
           ToStd(node->get_name()).find(kCollisionOnlySuffix) != std::string::npos;
}

// "Platform-rcol", "Platform-rcol2": collision for the mesh node named Platform.
std::string OwnerNameOf(const godot::Node *collisionOnly) {
    const std::string name = ToStd(collisionOnly->get_name());
    return name.substr(0, name.find(kCollisionOnlySuffix));
}

// A path of node names from an ancestor to a node (empty for the ancestor itself).
std::string PathFrom(const godot::Node *ancestor, const godot::Node *node) {
    std::string path;
    for (const godot::Node *at = node; at != nullptr && at != ancestor; at = at->get_parent()) {
        const std::string name = ToStd(at->get_name());
        path = path.empty() ? name : name + kPathSeparator + path;
    }
    return path;
}

// The pose of a node relative to an ancestor (outside the scene tree, so composed by hand).
godot::Transform3D TransformFrom(const godot::Node *ancestor, const godot::Node *node) {
    godot::Transform3D transform;
    for (const godot::Node *at = node; at != nullptr && at != ancestor; at = at->get_parent()) {
        if (const auto *spatial = godot::Object::cast_to<godot::Node3D>(at)) {
            transform = spatial->get_transform() * transform;
        }
    }
    return transform;
}

// The imported file a node comes from: the nearest of itself and its ancestors that a file
// instanced, up to the scene's root.
const godot::Node *FileNodeOf(const godot::Node *root, const godot::Node *node) {
    for (const godot::Node *at = node; at != nullptr; at = at->get_parent()) {
        if (at == root || !at->get_scene_file_path().is_empty()) {
            return at;
        }
    }
    return root;
}

// Godot's triangles are clockwise seen from the front; the Runic formats store them counter-
// clockwise, as glTF and Jolt do.
Core::Result<Cooker::InputMesh> ReadMesh(const godot::Ref<godot::Mesh> &mesh,
                                         const godot::Transform3D &into, std::string_view node) {
    Cooker::InputMesh read;
    if (mesh.is_null()) {
        return read;
    }
    const godot::Ref<godot::ArrayMesh> arrays = mesh;
    const godot::Basis normalBasis = into.basis.inverse().transposed();
    for (int32_t s = 0; s < mesh->get_surface_count(); ++s) {
        if (arrays.is_valid() &&
            arrays->surface_get_primitive_type(s) != godot::Mesh::PRIMITIVE_TRIANGLES) {
            return Core::Fail(
                std::format("node {}'s surface {} is not made of triangles", node, s));
        }
        const godot::Array surface = mesh->surface_get_arrays(s);
        const godot::PackedVector3Array positions = surface[godot::Mesh::ARRAY_VERTEX];
        const godot::PackedVector3Array normals = surface[godot::Mesh::ARRAY_NORMAL];
        const godot::PackedInt32Array indices = surface[godot::Mesh::ARRAY_INDEX];
        const auto base = static_cast<std::uint32_t>(read.positions.size());
        for (int64_t i = 0; i < positions.size(); ++i) {
            read.positions.push_back(ToVec3(into.xform(positions[i])));
        }
        if (normals.size() == positions.size()) {
            for (int64_t i = 0; i < normals.size(); ++i) {
                read.normals.push_back(ToVec3(normalBasis.xform(normals[i]).normalized()));
            }
        }
        const int64_t count = indices.is_empty() ? positions.size() : indices.size();
        for (int64_t t = 0; t + static_cast<int64_t>(kTriangleCorners) <= count;
             t += kTriangleCorners) {
            const auto corner = [&](int64_t i) {
                return base + static_cast<std::uint32_t>(indices.is_empty() ? i : indices[i]);
            };
            read.indices.insert(read.indices.end(), {corner(t), corner(t + 2), corner(t + 1)});
        }
    }
    if (read.normals.size() != read.positions.size()) {
        read.normals.clear(); // a surface without normals: the Cooker computes them all
    }
    return read;
}

Cooker::CollisionSource SourceOf(const godot::Node *node) {
    const godot::Variant meta = node->get_meta(ToGodot(kCollisionPresetMeta),
                                               static_cast<int64_t>(CollisionPreset::MeshItself));
    switch (static_cast<CollisionPreset>(static_cast<int64_t>(meta))) {
    case CollisionPreset::None:
        return Cooker::CollisionSource::None;
    case CollisionPreset::CollisionMeshes:
        return Cooker::CollisionSource::CollisionMeshes;
    case CollisionPreset::ConvexHull:
        return Cooker::CollisionSource::ConvexHull;
    case CollisionPreset::SimplifiedMesh:
        return Cooker::CollisionSource::SimplifiedMesh;
    case CollisionPreset::MeshItself:
        break;
    }
    return Cooker::CollisionSource::VisualMesh;
}

class SceneWalk {
  public:
    SceneWalk(const godot::Node *root, Cooker::CookScene &scene, std::string_view content)
        : m_Root(root), m_Scene(scene), m_Content(content) {}

    Core::Result<void> Walk(const godot::Node *node) {
        if (node != m_Root) {
            if (auto read = Visit(node); !read) {
                return read;
            }
        }
        for (int32_t i = 0; i < node->get_child_count(); ++i) {
            if (auto read = Walk(node->get_child(i)); !read) {
                return read;
            }
        }
        return {};
    }

  private:
    Core::Result<void> Visit(const godot::Node *node) {
        const std::string path = PathFrom(m_Root, node);
        if (const auto *collider = godot::Object::cast_to<RunicCollider3D>(node)) {
            m_Scene.colliders.push_back(
                {path, collider->ToCookedShape(TransformFrom(m_Root, node))});
            return {};
        }
        const auto *mesh = godot::Object::cast_to<godot::MeshInstance3D>(node);
        if (mesh == nullptr) {
            return {};
        }
        if (IsCollisionOnly(node)) {
            return CheckOwned(node, path);
        }
        auto asset = AssetOf(mesh);
        if (!asset) {
            return asset.GetError();
        }
        const godot::Transform3D pose = TransformFrom(m_Root, node);
        const godot::Quaternion rotation = pose.basis.orthonormalized().get_rotation_quaternion();
        Cooker::InputPlacement placement;
        placement.node = path;
        placement.asset = asset.Value();
        placement.position = ToVec3(pose.origin);
        placement.rotation = Core::Quat(rotation.w, rotation.x, rotation.y, rotation.z);
        placement.scale = ToVec3(pose.basis.get_scale());
        m_Scene.placements.push_back(placement);
        return {};
    }

    // A collision-only mesh is cooked into its owner's collision: it must have one.
    Core::Result<void> CheckOwned(const godot::Node *node, std::string_view path) {
        if (OwnerOf(node) == nullptr) {
            return Core::Fail(std::format("{}: node {} is a collision-only mesh, and no mesh node "
                                          "named {} is its parent or its sibling",
                                          m_Scene.source, path, OwnerNameOf(node)));
        }
        return {};
    }

    static const godot::Node *OwnerOf(const godot::Node *collisionOnly) {
        const godot::String owner = ToGodot(OwnerNameOf(collisionOnly));
        const godot::Node *parent = collisionOnly->get_parent();
        if (parent == nullptr) {
            return nullptr;
        }
        if (parent->get_name() == owner && godot::Object::cast_to<godot::MeshInstance3D>(parent)) {
            return parent;
        }
        const godot::Node *sibling = parent->get_node_or_null(godot::NodePath(owner));
        return godot::Object::cast_to<godot::MeshInstance3D>(sibling) != nullptr ? sibling
                                                                                 : nullptr;
    }

    // The asset a mesh node is: read the first time a node of its file and path is seen.
    Core::Result<Core::AssetId> AssetOf(const godot::MeshInstance3D *mesh) {
        const godot::Node *file = FileNodeOf(m_Root, mesh);
        const std::string filePath =
            file == m_Root ? m_Scene.source : ToStd(file->get_scene_file_path());
        const std::string inFile = PathFrom(file, mesh);
        const std::string key = filePath + kPathSeparator + inFile;
        if (const auto known = m_Assets.find(key); known != m_Assets.end()) {
            return known->second;
        }
        const int64_t uid =
            godot::ResourceLoader::get_singleton()->get_resource_uid(ToGodot(filePath));
        const std::string identity =
            uid == kNoUid ? key
                          : std::format("{:016x}{}{}", static_cast<std::uint64_t>(uid),
                                        kPathSeparator, inFile);
        const std::uint64_t hash = Content::HashText(identity);
        Cooker::InputAsset asset;
        asset.id = Core::AssetId(hash == 0 ? 1 : hash);
        auto name = Content::ContentId::Parse(ContentNameOf(filePath, inFile, m_Content));
        if (!name) {
            return Core::Fail(std::format("{}: node {} cannot be named: {}", filePath, inFile,
                                          name.GetError().message));
        }
        asset.name = name.Value();
        asset.source = filePath;
        auto surfaces = ReadMesh(mesh->get_mesh(), godot::Transform3D(), inFile);
        if (!surfaces) {
            return Core::Fail(std::format("{}: {}", filePath, surfaces.GetError().message));
        }
        asset.surfaces.push_back(std::move(surfaces.Value()));
        asset.collision = SourceOf(mesh);
        // The naming convention: collision-only meshes beside it or under it, in its own space.
        const godot::Node *parent = mesh->get_parent();
        const godot::Transform3D toMesh = mesh->get_transform().affine_inverse();
        const auto gather = [&](const godot::Node *candidate, const godot::Transform3D &into) {
            const auto *piece = godot::Object::cast_to<godot::MeshInstance3D>(candidate);
            if (piece == nullptr || !IsCollisionOnly(piece) || OwnerOf(piece) != mesh) {
                return Core::Result<void>();
            }
            auto read = ReadMesh(piece->get_mesh(), into, PathFrom(file, piece));
            if (!read) {
                return Core::Result<void>(
                    Core::Fail(std::format("{}: {}", filePath, read.GetError().message)));
            }
            asset.collisionMeshes.push_back(std::move(read.Value()));
            return Core::Result<void>();
        };
        for (int32_t i = 0; i < mesh->get_child_count(); ++i) {
            const auto *child = godot::Object::cast_to<godot::Node3D>(mesh->get_child(i));
            if (child != nullptr) {
                if (auto read = gather(child, child->get_transform()); !read) {
                    return read.GetError();
                }
            }
        }
        if (parent != nullptr) {
            for (int32_t i = 0; i < parent->get_child_count(); ++i) {
                const auto *sibling = godot::Object::cast_to<godot::Node3D>(parent->get_child(i));
                if (sibling != nullptr && sibling != mesh) {
                    if (auto read = gather(sibling, toMesh * sibling->get_transform()); !read) {
                        return read.GetError();
                    }
                }
            }
        }
        // Collision meshes the artist made say what the collision is, whatever the file's preset.
        if (!asset.collisionMeshes.empty()) {
            asset.collision = Cooker::CollisionSource::CollisionMeshes;
        }
        m_Assets.emplace(key, asset.id);
        m_Scene.assets.push_back(std::move(asset));
        return m_Scene.assets.back().id;
    }

    const godot::Node *m_Root;
    Cooker::CookScene &m_Scene;
    std::string_view m_Content;
    std::map<std::string, Core::AssetId> m_Assets;
};

} // namespace

std::string_view CollisionPresetHint() noexcept { return kCollisionPresetHint; }

std::string ContentNameOf(std::string_view resPath, std::string_view nodePath,
                          std::string_view contentFolder) {
    std::string_view path = resPath;
    if (path.starts_with(kResPrefix)) {
        path.remove_prefix(kResPrefix.size());
    }
    if (!contentFolder.empty() && path.starts_with(contentFolder) &&
        path.size() > contentFolder.size() && path[contentFolder.size()] == kPathSeparator) {
        path.remove_prefix(contentFolder.size() + 1);
    }
    const std::size_t lastSlash = path.rfind(kPathSeparator);
    const std::size_t dot = path.rfind(kExtensionMark);
    if (dot != std::string_view::npos && (lastSlash == std::string_view::npos || dot > lastSlash)) {
        path = path.substr(0, dot);
    }
    std::string source(path);
    if (!nodePath.empty()) {
        source += kPathSeparator;
        source += nodePath;
    }
    // Snake case: a capital after a lowercase letter or a digit starts a word; anything that is not
    // a letter, a digit or '/' separates words; parts never start or end with '_'.
    std::string name(kGameNamespacePrefix);
    bool partStarted = false;
    bool pendingSeparator = false;
    char previous = kPathSeparator;
    for (const char c : source) {
        if (c == kPathSeparator) {
            if (partStarted) {
                name += kPathSeparator;
            }
            partStarted = false;
            pendingSeparator = false;
            previous = c;
            continue;
        }
        const auto u = static_cast<unsigned char>(c);
        if (!std::isalnum(u)) {
            pendingSeparator = partStarted;
            previous = c;
            continue;
        }
        const bool wordStart =
            std::isupper(u) && (std::islower(static_cast<unsigned char>(previous)) ||
                                std::isdigit(static_cast<unsigned char>(previous)));
        if (partStarted && (pendingSeparator || wordStart)) {
            name += kWordSeparator;
        }
        name += static_cast<char>(std::tolower(u));
        partStarted = true;
        pendingSeparator = false;
        previous = c;
    }
    if (!name.empty() && name.back() == kPathSeparator) {
        name.pop_back();
    }
    return name;
}

Core::Result<Cooker::CookScene>
ReadImportedScene(std::string_view scene, Core::WorldPosition origin, std::string_view content) {
    // Read afresh, the scenes it instances too: a cook reads what was imported last, never a copy
    // the editor cached before a reimport.
    const godot::Ref<godot::PackedScene> packed = godot::ResourceLoader::get_singleton()->load(
        ToGodot(scene), ToGodot(kPackedSceneType), godot::ResourceLoader::CACHE_MODE_REPLACE_DEEP);
    if (packed.is_null()) {
        return Core::Fail(std::format("{} cannot be loaded as a scene (is it imported?)", scene));
    }
    godot::Node *root = packed->instantiate();
    if (root == nullptr) {
        return Core::Fail(std::format("{} cannot be instantiated", scene));
    }
    Cooker::CookScene read;
    read.source = std::string(scene);
    read.origin = origin;
    auto name = Content::ContentId::Parse(ContentNameOf(scene, {}, content));
    Core::Result<void> walked = Core::Fail(name ? std::string() : name.GetError().message);
    if (name) {
        read.name = name.Value();
        walked = SceneWalk(root, read, content).Walk(root);
    }
    memdelete(root);
    if (!walked) {
        return walked.GetError();
    }
    return read;
}

} // namespace Runic::GodotTools
