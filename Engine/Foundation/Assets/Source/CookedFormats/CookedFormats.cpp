// The native cooked formats (CookedFormats.hpp).

#include "CookedFormats.hpp"

#include <cmath>
#include <format>
#include <fstream>
#include <system_error>

#include "ContentId.hpp"
#include "CookedFormats/ByteStream.hpp"

namespace Runic::Assets {

namespace {

using Detail::ByteReader;
using Detail::ByteWriter;

// On disk: 3 floats a vector, 4 a rotation, 4 bytes an index; a shape, a placement and a collider
// have fixed sizes.
constexpr std::size_t kVec3Bytes = 12;
constexpr std::size_t kIndexBytes = 4;
constexpr std::size_t kShapeBytes = 56;
constexpr std::size_t kPlacementBytes = 64;
constexpr std::size_t kColliderBytes = 64;
constexpr std::size_t kTriangleIndices = 3;
constexpr std::uint32_t kShapeKindCount = 4;
// Where the header's fields start.
constexpr std::size_t kMagicBytes = 4;
constexpr std::string_view kTemporarySuffix = ".writing";

std::string_view KindPhrase(AssetKind kind) noexcept {
    switch (kind) {
    case AssetKind::Mesh:
        return "a mesh";
    case AssetKind::Collision:
        return "a collision";
    case AssetKind::Cell:
        return "a cell";
    }
    return "a mesh";
}

std::uint64_t Checksum(std::span<const std::byte> bytes) noexcept {
    return Content::HashText(
        std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

std::vector<std::byte> Seal(AssetKind kind, std::vector<std::byte> payload) {
    ByteWriter header;
    for (const char c : kCookedMagic) {
        header.U8(static_cast<std::uint8_t>(c));
    }
    header.U16(kCookedFormatVersion);
    header.U16(static_cast<std::uint16_t>(kind));
    header.U64(payload.size());
    header.U64(Checksum(payload));
    std::vector<std::byte> file = std::move(header.Bytes());
    file.insert(file.end(), payload.begin(), payload.end());
    return file;
}

// The header, checked: the payload's bytes, or why the file is not one of kind.
Core::Result<std::span<const std::byte>>
Open(std::span<const std::byte> bytes, std::optional<AssetKind> kind, AssetKind *found = nullptr) {
    if (bytes.size() < kCookedHeaderBytes) {
        return Core::Fail(std::format("{} bytes are too few for a cooked file's {}-byte header",
                                      bytes.size(), kCookedHeaderBytes));
    }
    ByteReader header(bytes.first(kCookedHeaderBytes));
    for (std::size_t i = 0; i < kMagicBytes; ++i) {
        if (header.U8() != static_cast<std::uint8_t>(kCookedMagic[i])) {
            return Core::Fail(
                std::format("not a cooked file: it does not start with \"{}\"", kCookedMagic));
        }
    }
    const std::uint16_t version = header.U16();
    if (version != kCookedFormatVersion) {
        return Core::Fail(std::format("cooked format version {}, this build reads version {}",
                                      version, kCookedFormatVersion));
    }
    const std::uint16_t code = header.U16();
    if (code >= kAssetKindCount) {
        return Core::Fail(std::format("cooked file of unknown kind {}", code));
    }
    const auto actual = static_cast<AssetKind>(code);
    if (kind.has_value() && actual != *kind) {
        return Core::Fail(
            std::format("the file holds {}, not {}", KindPhrase(actual), KindPhrase(*kind)));
    }
    const std::uint64_t size = header.U64();
    const std::uint64_t checksum = header.U64();
    const std::span<const std::byte> payload = bytes.subspan(kCookedHeaderBytes);
    if (size != payload.size()) {
        return Core::Fail(std::format("the header promises a {}-byte payload, the file has {}",
                                      size, payload.size()));
    }
    if (Checksum(payload) != checksum) {
        return Core::Fail("the payload does not match its checksum: the file is damaged");
    }
    if (found != nullptr) {
        *found = actual;
    }
    return payload;
}

bool Finite(const Core::Vec3 &v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool Unit(const Core::Quat &q) noexcept {
    const float length = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return std::isfinite(length) && std::abs(length - 1.0f) <= kUnitQuaternionTolerance;
}

Core::Error Truncated(std::string_view what) {
    return Core::Fail(std::format("the payload ends inside {}", what));
}

Core::Result<void> Ended(const ByteReader &reader) {
    if (reader.Remaining() != 0) {
        return Core::Fail(std::format("{} bytes follow the payload's end", reader.Remaining()));
    }
    return {};
}

void WriteVectors(ByteWriter &out, const std::vector<Core::Vec3> &vectors) {
    for (const Core::Vec3 &v : vectors) {
        out.Vec3(v);
    }
}

Core::Result<void> ReadPositions(ByteReader &in, std::uint32_t count, std::string_view what,
                                 std::vector<Core::Vec3> &out) {
    if (!in.Has(count, kVec3Bytes)) {
        return Truncated(what);
    }
    out.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        out[i] = in.Vec3();
        if (!Finite(out[i])) {
            return Core::Fail(std::format("{} {} is not finite", what, i));
        }
    }
    return {};
}

Core::Result<void> ReadIndices(ByteReader &in, std::uint32_t count, std::uint32_t vertices,
                               std::vector<std::uint32_t> &out) {
    if (count % kTriangleIndices != 0) {
        return Core::Fail(std::format("{} triangle indices: not a multiple of 3", count));
    }
    if (!in.Has(count, kIndexBytes)) {
        return Truncated("the triangle indices");
    }
    out.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        out[i] = in.U32();
        if (out[i] >= vertices) {
            return Core::Fail(std::format("triangle index {} at {} is past the {} vertices", out[i],
                                          i, vertices));
        }
    }
    return {};
}

void WriteShape(ByteWriter &out, const CollisionShape &shape) {
    out.U32(static_cast<std::uint32_t>(shape.kind));
    out.Vec3(shape.halfExtents);
    out.F32(shape.radius);
    out.F32(shape.halfHeight);
    out.U32(shape.mesh);
    out.Vec3(shape.position);
    out.Quat(shape.rotation);
}

// meshes: how many meshes a Mesh shape may name (0: none may).
Core::Result<CollisionShape> ReadShape(ByteReader &in, std::uint32_t meshes,
                                       std::string_view what) {
    CollisionShape shape;
    const std::uint32_t kind = in.U32();
    shape.halfExtents = in.Vec3();
    shape.radius = in.F32();
    shape.halfHeight = in.F32();
    shape.mesh = in.U32();
    shape.position = in.Vec3();
    shape.rotation = in.Quat();
    if (!in.Ok()) {
        return Truncated(what);
    }
    if (kind >= kShapeKindCount) {
        return Core::Fail(std::format("{} is of unknown kind {}", what, kind));
    }
    shape.kind = static_cast<CollisionShapeKind>(kind);
    if (!Finite(shape.halfExtents) || !std::isfinite(shape.radius) ||
        !std::isfinite(shape.halfHeight) || !Finite(shape.position)) {
        return Core::Fail(std::format("{} has a size or a position that is not finite", what));
    }
    if (!Unit(shape.rotation)) {
        return Core::Fail(std::format("{}'s rotation is not a unit quaternion", what));
    }
    switch (shape.kind) {
    case CollisionShapeKind::Box:
        if (shape.halfExtents.x <= 0.0f || shape.halfExtents.y <= 0.0f ||
            shape.halfExtents.z <= 0.0f) {
            return Core::Fail(std::format("{} is a box with a size that is not positive", what));
        }
        break;
    case CollisionShapeKind::Sphere:
    case CollisionShapeKind::Capsule:
        if (shape.radius <= 0.0f ||
            (shape.kind == CollisionShapeKind::Capsule && shape.halfHeight < 0.0f)) {
            return Core::Fail(std::format("{} has a size that is not positive", what));
        }
        break;
    case CollisionShapeKind::Mesh:
        if (shape.mesh >= meshes) {
            return Core::Fail(std::format("{} names mesh {} of {}", what, shape.mesh, meshes));
        }
        break;
    }
    return shape;
}

} // namespace

std::vector<std::byte> WriteCookedMesh(const CookedMesh &mesh) {
    ByteWriter out;
    out.U32(static_cast<std::uint32_t>(mesh.positions.size()));
    out.U32(static_cast<std::uint32_t>(mesh.normals.size()));
    out.U32(static_cast<std::uint32_t>(mesh.indices.size()));
    out.Vec3(mesh.boundsMin);
    out.Vec3(mesh.boundsMax);
    WriteVectors(out, mesh.positions);
    WriteVectors(out, mesh.normals);
    for (const std::uint32_t index : mesh.indices) {
        out.U32(index);
    }
    return Seal(AssetKind::Mesh, std::move(out.Bytes()));
}

Core::Result<CookedMesh> ReadCookedMesh(std::span<const std::byte> bytes) {
    auto payload = Open(bytes, AssetKind::Mesh);
    if (!payload) {
        return payload.GetError();
    }
    ByteReader in(payload.Value());
    CookedMesh mesh;
    const std::uint32_t vertices = in.U32();
    const std::uint32_t normals = in.U32();
    const std::uint32_t indices = in.U32();
    mesh.boundsMin = in.Vec3();
    mesh.boundsMax = in.Vec3();
    if (!in.Ok()) {
        return Truncated("the mesh's counts");
    }
    if (normals != 0 && normals != vertices) {
        return Core::Fail(
            std::format("the mesh has {} normals for {} vertices", normals, vertices));
    }
    if (auto read = ReadPositions(in, vertices, "position", mesh.positions); !read) {
        return read.GetError();
    }
    if (auto read = ReadPositions(in, normals, "normal", mesh.normals); !read) {
        return read.GetError();
    }
    if (auto read = ReadIndices(in, indices, vertices, mesh.indices); !read) {
        return read.GetError();
    }
    if (auto ended = Ended(in); !ended) {
        return ended.GetError();
    }
    return mesh;
}

std::vector<std::byte> WriteCookedCollision(const CookedCollision &collision) {
    ByteWriter out;
    out.U32(static_cast<std::uint32_t>(collision.meshes.size()));
    for (const CollisionMesh &mesh : collision.meshes) {
        out.U32(static_cast<std::uint32_t>(mesh.positions.size()));
        out.U32(static_cast<std::uint32_t>(mesh.indices.size()));
        WriteVectors(out, mesh.positions);
        for (const std::uint32_t index : mesh.indices) {
            out.U32(index);
        }
    }
    out.U32(static_cast<std::uint32_t>(collision.shapes.size()));
    for (const CollisionShape &shape : collision.shapes) {
        WriteShape(out, shape);
    }
    return Seal(AssetKind::Collision, std::move(out.Bytes()));
}

Core::Result<CookedCollision> ReadCookedCollision(std::span<const std::byte> bytes) {
    auto payload = Open(bytes, AssetKind::Collision);
    if (!payload) {
        return payload.GetError();
    }
    ByteReader in(payload.Value());
    CookedCollision collision;
    const std::uint32_t meshes = in.U32();
    if (!in.Ok()) {
        return Truncated("the collision's mesh count");
    }
    for (std::uint32_t m = 0; m < meshes; ++m) {
        CollisionMesh mesh;
        const std::uint32_t vertices = in.U32();
        const std::uint32_t indices = in.U32();
        if (!in.Ok()) {
            return Truncated(std::format("mesh {}'s counts", m));
        }
        if (auto read =
                ReadPositions(in, vertices, std::format("mesh {}'s position", m), mesh.positions);
            !read) {
            return read.GetError();
        }
        if (auto read = ReadIndices(in, indices, vertices, mesh.indices); !read) {
            return Core::Fail(std::format("mesh {}: {}", m, read.GetError().message));
        }
        collision.meshes.push_back(std::move(mesh));
    }
    const std::uint32_t shapes = in.U32();
    if (!in.Ok() || !in.Has(shapes, kShapeBytes)) {
        return Truncated("the collision's shapes");
    }
    for (std::uint32_t s = 0; s < shapes; ++s) {
        auto shape = ReadShape(in, meshes, std::format("shape {}", s));
        if (!shape) {
            return shape.GetError();
        }
        collision.shapes.push_back(shape.Value());
    }
    if (auto ended = Ended(in); !ended) {
        return ended.GetError();
    }
    return collision;
}

std::vector<std::byte> WriteCookedCell(const CookedCell &cell) {
    ByteWriter out;
    out.F64(cell.origin.x);
    out.F64(cell.origin.y);
    out.F64(cell.origin.z);
    out.U32(static_cast<std::uint32_t>(cell.placements.size()));
    for (const CellPlacement &placement : cell.placements) {
        out.U64(placement.id);
        out.U64(placement.mesh.Value());
        out.U64(placement.collision.Value());
        out.Vec3(placement.position);
        out.Quat(placement.rotation);
        out.Vec3(placement.scale);
    }
    out.U32(static_cast<std::uint32_t>(cell.colliders.size()));
    for (const CellCollider &collider : cell.colliders) {
        out.U64(collider.id);
        WriteShape(out, collider.shape);
    }
    return Seal(AssetKind::Cell, std::move(out.Bytes()));
}

Core::Result<CookedCell> ReadCookedCell(std::span<const std::byte> bytes) {
    auto payload = Open(bytes, AssetKind::Cell);
    if (!payload) {
        return payload.GetError();
    }
    ByteReader in(payload.Value());
    CookedCell cell;
    cell.origin.x = in.F64();
    cell.origin.y = in.F64();
    cell.origin.z = in.F64();
    const std::uint32_t placements = in.U32();
    if (!in.Ok() || !in.Has(placements, kPlacementBytes)) {
        return Truncated("the cell's placements");
    }
    if (!std::isfinite(cell.origin.x) || !std::isfinite(cell.origin.y) ||
        !std::isfinite(cell.origin.z)) {
        return Core::Fail("the cell's origin is not finite");
    }
    for (std::uint32_t p = 0; p < placements; ++p) {
        CellPlacement placement;
        placement.id = in.U64();
        placement.mesh = Core::AssetId(in.U64());
        placement.collision = Core::AssetId(in.U64());
        placement.position = in.Vec3();
        placement.rotation = in.Quat();
        placement.scale = in.Vec3();
        if (!Finite(placement.position) || !Finite(placement.scale)) {
            return Core::Fail(std::format("placement {} has a position or a scale that is not "
                                          "finite",
                                          p));
        }
        if (!Unit(placement.rotation)) {
            return Core::Fail(std::format("placement {}'s rotation is not a unit quaternion", p));
        }
        cell.placements.push_back(placement);
    }
    const std::uint32_t colliders = in.U32();
    if (!in.Ok() || !in.Has(colliders, kColliderBytes)) {
        return Truncated("the cell's colliders");
    }
    constexpr std::uint32_t kNoMeshes = 0; // a collider is never a mesh
    for (std::uint32_t c = 0; c < colliders; ++c) {
        CellCollider collider;
        collider.id = in.U64();
        auto shape = ReadShape(in, kNoMeshes, std::format("collider {}", c));
        if (!shape) {
            return shape.GetError();
        }
        collider.shape = shape.Value();
        cell.colliders.push_back(collider);
    }
    if (auto ended = Ended(in); !ended) {
        return ended.GetError();
    }
    return cell;
}

Core::Result<AssetKind> PeekCookedKind(std::span<const std::byte> bytes) {
    AssetKind kind = AssetKind::Mesh;
    auto payload = Open(bytes, std::nullopt, &kind);
    if (!payload) {
        return payload.GetError();
    }
    return kind;
}

Core::Result<void> WriteBytesToFile(const std::filesystem::path &path,
                                    std::span<const std::byte> bytes) {
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    const std::filesystem::path temporary = path.string() + std::string(kTemporarySuffix);
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            return Core::Fail(std::format("cannot write {}", temporary.string()));
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        return Core::Fail(std::format("cannot rename {} to {}: {}", temporary.string(),
                                      path.string(), error.message()));
    }
    return {};
}

Core::Result<std::vector<std::byte>> ReadBytesFromFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return Core::Fail(std::format("cannot open {}", path.string()));
    }
    const std::streamsize size = in.tellg();
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char *>(bytes.data()), size);
    if (!in) {
        return Core::Fail(std::format("cannot read {}", path.string()));
    }
    return bytes;
}

} // namespace Runic::Assets
