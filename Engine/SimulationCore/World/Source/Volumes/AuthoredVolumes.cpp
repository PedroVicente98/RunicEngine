// Authored volumes (AuthoredVolumes.hpp): kinds and their backends, volumes, the rule areas'
// index and the exact point test.

#include "AuthoredVolumes.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>

#include "Assert.hpp"
#include "SpatialIndex/SparseCells.hpp"
#include "Volumes/VolumeShapes.hpp"

namespace Runic::World {

namespace Detail {

namespace {

constexpr char kNamespaceSeparator = ':';
constexpr double kNoSize = 0.0;

} // namespace

DVec3 PartHalfSize(const VolumePart &part) noexcept {
    switch (part.shape) {
    case VolumePartShape::Sphere:
        return DVec3(static_cast<double>(part.radius));
    case VolumePartShape::Cylinder:
        return DVec3(part.radius, part.halfHeight, part.radius);
    case VolumePartShape::Box:
        break;
    }
    return ToDouble(part.halfExtents);
}

bool PartContains(const VolumePart &part, DVec3 local) noexcept {
    switch (part.shape) {
    case VolumePartShape::Sphere: {
        const double radius = part.radius;
        return glm::dot(local, local) <= radius * radius;
    }
    case VolumePartShape::Cylinder: {
        const double radius = part.radius;
        return std::abs(local.y) <= static_cast<double>(part.halfHeight) &&
               local.x * local.x + local.z * local.z <= radius * radius;
    }
    case VolumePartShape::Box:
        break;
    }
    return std::abs(local.x) <= static_cast<double>(part.halfExtents.x) &&
           std::abs(local.y) <= static_cast<double>(part.halfExtents.y) &&
           std::abs(local.z) <= static_cast<double>(part.halfExtents.z);
}

DVec3 TurnedHalfSize(DQuat rotation, DVec3 half) noexcept {
    const glm::dmat3 turn = glm::mat3_cast(rotation);
    // Each world axis gathers the absolute share of every turned axis.
    DVec3 out(kNoSize);
    for (int column = 0; column < DVec3::length(); ++column) {
        out += glm::abs(turn[column]) * half[column];
    }
    return out;
}

SpatialBounds BoundsOfParts(Core::WorldPosition origin, DQuat rotation,
                            const std::vector<VolumePart> &parts) noexcept {
    DVec3 low(std::numeric_limits<double>::max());
    DVec3 high(std::numeric_limits<double>::lowest());
    for (const VolumePart &part : parts) {
        const DVec3 center = rotation * ToDouble(part.offset);
        const DVec3 half = TurnedHalfSize(rotation * ToDouble(part.rotation), PartHalfSize(part));
        low = glm::min(low, center - half);
        high = glm::max(high, center + half);
    }
    const DVec3 middle = (low + high) * 0.5;
    const DVec3 half = (high - low) * 0.5;
    SpatialBounds bounds;
    bounds.center = origin + Core::WorldDelta{middle.x, middle.y, middle.z};
    // A half size a float cannot hold is rounded up, so the box never shrinks inside a part.
    const auto up = [](double value) {
        const auto rounded = static_cast<float>(value);
        return static_cast<double>(rounded) < value ? std::nextafter(rounded, HUGE_VALF) : rounded;
    };
    bounds.halfExtents = Core::Vec3(up(half.x), up(half.y), up(half.z));
    return bounds;
}

namespace {

bool IsNamespaced(std::string_view name) {
    const std::size_t separator = name.find(kNamespaceSeparator);
    return separator != std::string_view::npos && separator > 0 && separator + 1 < name.size();
}

bool PartIsLargeEnough(const VolumePart &part) {
    const DVec3 half = PartHalfSize(part);
    const double smallest = std::min({half.x, half.y, half.z});
    // A part's size is its full width along each axis.
    return smallest + smallest >= static_cast<double>(kMinVolumePartSizeM);
}

} // namespace

} // namespace Detail

namespace {

constexpr int kGenerationShift = 32;
constexpr std::uint64_t kSlotMask = 0xFFFF'FFFFull;
constexpr std::uint32_t kFirstGeneration = 1;

VolumeBackend BackendOf(const VolumeKindDesc &desc) {
    if (desc.solid) {
        return VolumeBackend::StaticBody;
    }
    return desc.followsEntity ? VolumeBackend::SensorBody : VolumeBackend::SpatialIndex;
}

} // namespace

struct VolumeSet::State {
    explicit State(const CoordinateSpace &of) : space(of) {}

    struct Kind {
        std::string name;
        VolumeBackend backend = VolumeBackend::SpatialIndex;
    };
    struct Volume {
        bool live = false;
        std::uint32_t generation = 0;
        VolumeKindId kind;
        VolumeBackend backend = VolumeBackend::SpatialIndex;
        Core::WorldPosition origin;
        Core::Quat rotation = kNoRotation;
        Detail::DQuat turn{};
        std::vector<VolumePart> parts;
        std::vector<Detail::DQuat> untwists; // each part's rotation, undone, in 64-bit
        bool indivisible = false;
        std::uint64_t owner = 0;
        SpatialBounds bounds;
        CellRange cells;
    };

    CoordinateSpace space;
    std::vector<Kind> kinds; // VolumeKindId n is kinds[n - 1]
    std::vector<Volume> volumes;
    std::vector<std::uint32_t> freeSlots;
    Detail::SparseCells cells; // the rule areas only
    std::uint32_t live = 0;

    static VolumeId IdOf(std::uint32_t slot, std::uint32_t generation) {
        return VolumeId((static_cast<std::uint64_t>(generation) << kGenerationShift) | slot);
    }
    static std::uint32_t SlotOf(VolumeId id) {
        return static_cast<std::uint32_t>(id.Value() & kSlotMask);
    }
    const Volume *Find(VolumeId id) const {
        const std::uint32_t slot = SlotOf(id);
        if (slot >= volumes.size()) {
            return nullptr;
        }
        const Volume &volume = volumes[slot];
        const auto generation = static_cast<std::uint32_t>(id.Value() >> kGenerationShift);
        return volume.live && volume.generation == generation ? &volume : nullptr;
    }
    const Volume &Get(VolumeId id) const {
        const Volume *volume = Find(id);
        RUNIC_ASSERT_MSG(volume != nullptr, "the ID is not a volume of this set");
        return *volume;
    }
    bool InsideLocked(const Volume &volume, Core::WorldPosition point) const {
        const Detail::DVec3 inVolume = Detail::ToVolumeSpace(volume.origin, volume.turn, point);
        for (std::size_t i = 0; i < volume.parts.size(); ++i) {
            const VolumePart &part = volume.parts[i];
            if (Detail::PartContains(part, Detail::ToPartSpace(part, volume.untwists[i], inVolume))) {
                return true;
            }
        }
        return false;
    }
};

VolumeSet::VolumeSet(const CoordinateSpace &space) : m_State(std::make_unique<State>(space)) {}

VolumeSet::~VolumeSet() = default;

Core::Result<VolumeKindId> VolumeSet::AddKind(const VolumeKindDesc &desc) {
    State &state = *m_State;
    if (!Detail::IsNamespaced(desc.name)) {
        return Core::Fail(std::format("volume kind '{}' needs a namespace, such as 'game:{}'",
                                      desc.name, desc.name));
    }
    if (FindKind(desc.name).IsValid()) {
        return Core::Fail(std::format("volume kind '{}' is already added", desc.name));
    }
    if (desc.solid && desc.followsEntity) {
        return Core::Fail(std::format("volume kind '{}' is solid and follows an entity: a moving "
                                      "solid volume has no backend yet",
                                      desc.name));
    }
    state.kinds.push_back({std::string(desc.name), BackendOf(desc)});
    return VolumeKindId(static_cast<std::uint32_t>(state.kinds.size()));
}

VolumeKindId VolumeSet::FindKind(std::string_view name) const noexcept {
    const std::vector<State::Kind> &kinds = m_State->kinds;
    for (std::size_t i = 0; i < kinds.size(); ++i) {
        if (kinds[i].name == name) {
            return VolumeKindId(static_cast<std::uint32_t>(i + 1));
        }
    }
    return VolumeKindId::Invalid();
}

VolumeKindInfo VolumeSet::DescribeKind(VolumeKindId kind) const {
    RUNIC_ASSERT_MSG(kind.IsValid() && kind.Value() <= m_State->kinds.size(),
                     "the ID is not a volume kind of this set");
    const State::Kind &entry = m_State->kinds[kind.Value() - 1];
    return {entry.name, entry.backend};
}

Core::Result<VolumeId> VolumeSet::Add(const VolumeDesc &desc) {
    State &state = *m_State;
    if (!desc.kind.IsValid() || desc.kind.Value() > state.kinds.size()) {
        return Core::Fail(
            std::format("volume kind {} is not a kind of this set", desc.kind.Value()));
    }
    if (desc.parts.empty()) {
        return Core::Fail("a volume needs at least one part");
    }
    for (std::size_t i = 0; i < desc.parts.size(); ++i) {
        if (!Detail::PartIsLargeEnough(desc.parts[i])) {
            return Core::Fail(
                std::format("part {} of the volume is smaller than {} m", i, kMinVolumePartSizeM));
        }
    }
    std::uint32_t slot = 0;
    if (state.freeSlots.empty()) {
        slot = static_cast<std::uint32_t>(state.volumes.size());
        state.volumes.emplace_back();
    } else {
        slot = state.freeSlots.back();
        state.freeSlots.pop_back();
    }
    State::Volume &volume = state.volumes[slot];
    volume.live = true;
    volume.generation = volume.generation == 0 ? kFirstGeneration : volume.generation + 1;
    volume.kind = desc.kind;
    volume.backend = state.kinds[desc.kind.Value() - 1].backend;
    volume.origin = desc.origin;
    volume.rotation = desc.rotation;
    volume.turn = Detail::ToDouble(desc.rotation);
    volume.parts = desc.parts;
    volume.untwists.clear();
    for (const VolumePart &part : volume.parts) {
        volume.untwists.push_back(glm::conjugate(Detail::ToDouble(part.rotation)));
    }
    volume.indivisible = desc.indivisible;
    volume.owner = desc.owner;
    volume.bounds = Detail::BoundsOfParts(volume.origin, volume.turn, volume.parts);
    volume.cells = CellsTouched(state.space, volume.bounds);
    if (volume.backend == VolumeBackend::SpatialIndex) {
        state.cells.Add(volume.cells, slot);
    }
    ++state.live;
    return State::IdOf(slot, volume.generation);
}

void VolumeSet::Remove(VolumeId id) {
    State &state = *m_State;
    state.Get(id);
    const std::uint32_t slot = State::SlotOf(id);
    State::Volume &volume = state.volumes[slot];
    if (volume.backend == VolumeBackend::SpatialIndex) {
        state.cells.Remove(volume.cells, slot);
    }
    volume.live = false;
    volume.parts.clear();
    volume.untwists.clear();
    state.freeSlots.push_back(slot);
    --state.live;
}

bool VolumeSet::IsVolume(VolumeId id) const noexcept { return m_State->Find(id) != nullptr; }

void VolumeSet::SetPose(VolumeId id, Core::WorldPosition origin, Core::Quat rotation) {
    State &state = *m_State;
    RUNIC_ASSERT_MSG(state.Get(id).backend == VolumeBackend::SensorBody,
                     "the volume does not move: only a volume that follows an entity has a pose "
                     "to set");
    State::Volume &volume = state.volumes[State::SlotOf(id)];
    volume.origin = origin;
    volume.rotation = rotation;
    volume.turn = Detail::ToDouble(rotation);
    volume.bounds = Detail::BoundsOfParts(origin, volume.turn, volume.parts);
    volume.cells = CellsTouched(state.space, volume.bounds);
}

VolumeInfo VolumeSet::Describe(VolumeId id) const {
    const State::Volume &volume = m_State->Get(id);
    VolumeInfo info;
    info.kind = volume.kind;
    info.backend = volume.backend;
    info.indivisible = volume.indivisible;
    info.owner = volume.owner;
    info.origin = volume.origin;
    info.rotation = volume.rotation;
    info.parts = static_cast<std::uint32_t>(volume.parts.size());
    info.bounds = volume.bounds;
    return info;
}

const std::vector<VolumePart> &VolumeSet::PartsOf(VolumeId id) const {
    return m_State->Get(id).parts;
}

CellRange VolumeSet::CellsOf(VolumeId id) const { return m_State->Get(id).cells; }

void VolumeSet::Volumes(std::vector<VolumeId> &out) const {
    const State &state = *m_State;
    out.clear();
    for (std::uint32_t slot = 0; slot < state.volumes.size(); ++slot) {
        if (state.volumes[slot].live) {
            out.push_back(State::IdOf(slot, state.volumes[slot].generation));
        }
    }
    std::sort(out.begin(), out.end());
}

bool VolumeSet::IsInside(VolumeId id, Core::WorldPosition point) const {
    return m_State->InsideLocked(m_State->Get(id), point);
}

void VolumeSet::VolumesAt(Core::WorldPosition point, std::vector<VolumeId> &out) const {
    const State &state = *m_State;
    const std::size_t first = out.size();
    for (const std::uint32_t slot : state.cells.SlotsIn(state.space.CellOf(point))) {
        const State::Volume &volume = state.volumes[slot];
        if (state.InsideLocked(volume, point)) {
            out.push_back(State::IdOf(slot, volume.generation));
        }
    }
    std::sort(out.begin() + static_cast<std::ptrdiff_t>(first), out.end());
}

void VolumeSet::PhysicsVolumes(std::vector<VolumeId> &out) const {
    const State &state = *m_State;
    out.clear();
    for (std::uint32_t slot = 0; slot < state.volumes.size(); ++slot) {
        const State::Volume &volume = state.volumes[slot];
        if (volume.live && volume.backend != VolumeBackend::SpatialIndex) {
            out.push_back(State::IdOf(slot, volume.generation));
        }
    }
    std::sort(out.begin(), out.end());
}

VolumeSetStats VolumeSet::Stats() const noexcept {
    const State &state = *m_State;
    VolumeSetStats stats;
    stats.kinds = static_cast<std::uint32_t>(state.kinds.size());
    stats.volumes = state.live;
    for (const State::Volume &volume : state.volumes) {
        if (!volume.live) {
            continue;
        }
        if (volume.backend == VolumeBackend::SpatialIndex) {
            ++stats.indexed;
        } else {
            ++stats.physics;
        }
        if (volume.indivisible) {
            ++stats.indivisible;
        }
    }
    stats.cells = state.cells.Cells();
    return stats;
}

} // namespace Runic::World
