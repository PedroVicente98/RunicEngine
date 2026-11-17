// The world registry (WorldRegistry.hpp).

#include "WorldRegistry.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "Assert.hpp"

namespace Runic::World {

namespace {

struct Registered {
    Core::WorldId id;
    std::string name;
    CoordinateSpace space;
};

} // namespace

struct WorldRegistry::State {
    std::vector<Registered> worlds;

    const Registered *Find(Core::WorldId id) const noexcept {
        const auto found = std::ranges::find(worlds, id, &Registered::id);
        return found != worlds.end() ? &*found : nullptr;
    }

    // The registered world, or the error every lookup of an unknown world gives.
    Core::Result<const Registered *> Require(Core::WorldId id) const {
        if (const Registered *world = Find(id)) {
            return world;
        }
        return Core::Fail(std::format("world {} is not registered", id.Value()));
    }
};

WorldRegistry::WorldRegistry() : m_State(std::make_unique<State>()) {}

WorldRegistry::~WorldRegistry() = default;

Core::Result<void> WorldRegistry::Register(Core::WorldId id, const WorldDescription &description) {
    if (!id.IsValid()) {
        return Core::Fail(std::format("world '{}' has no valid WorldId", description.name));
    }
    if (description.name.empty()) {
        return Core::Fail(std::format("world {} has no name", id.Value()));
    }
    if (const Registered *existing = m_State->Find(id)) {
        return Core::Fail(
            std::format("world {} is already registered as '{}'", id.Value(), existing->name));
    }
    if (std::ranges::find(m_State->worlds, description.name, &Registered::name) !=
        m_State->worlds.end()) {
        return Core::Fail(std::format("the world name '{}' is already taken", description.name));
    }
    if (description.space.cellSizeM == 0 || description.space.cellsPerSector == 0) {
        return Core::Fail(
            std::format("world '{}' has a cell or sector size of zero", description.name));
    }
    m_State->worlds.push_back(Registered{id, std::string(description.name), description.space});
    return {};
}

bool WorldRegistry::Knows(Core::WorldId id) const noexcept { return m_State->Find(id) != nullptr; }

Core::Result<CoordinateSpace> WorldRegistry::SpaceOf(Core::WorldId id) const {
    const Core::Result<const Registered *> world = m_State->Require(id);
    if (!world) {
        return world.GetError();
    }
    return world.Value()->space;
}

Core::Result<CellKey> WorldRegistry::CellAt(Core::WorldId world,
                                            Core::WorldPosition position) const {
    const Core::Result<const Registered *> found = m_State->Require(world);
    if (!found) {
        return found.GetError();
    }
    return CellKey{world, found.Value()->space.CellOf(position)};
}

Core::Result<SectorKey> WorldRegistry::SectorAt(Core::WorldId world,
                                                Core::WorldPosition position) const {
    const Core::Result<const Registered *> found = m_State->Require(world);
    if (!found) {
        return found.GetError();
    }
    return SectorKey{world, found.Value()->space.SectorOf(position)};
}

Core::Result<Core::WorldPosition> WorldRegistry::CellMin(const CellKey &cell) const {
    const Core::Result<const Registered *> found = m_State->Require(cell.world);
    if (!found) {
        return found.GetError();
    }
    return found.Value()->space.CellMin(cell.cell);
}

Core::Result<Core::WorldPosition> WorldRegistry::CellCenter(const CellKey &cell) const {
    const Core::Result<const Registered *> found = m_State->Require(cell.world);
    if (!found) {
        return found.GetError();
    }
    return found.Value()->space.CellCenter(cell.cell);
}

Core::Result<SectorKey> WorldRegistry::SectorOf(const CellKey &cell) const {
    const Core::Result<const Registered *> found = m_State->Require(cell.world);
    if (!found) {
        return found.GetError();
    }
    return SectorKey{cell.world, found.Value()->space.SectorOf(cell.cell)};
}

std::uint32_t WorldRegistry::Count() const noexcept {
    return static_cast<std::uint32_t>(m_State->worlds.size());
}

WorldInfo WorldRegistry::Describe(std::uint32_t index) const {
    RUNIC_ASSERT(index < m_State->worlds.size());
    const Registered &world = m_State->worlds[index];
    return WorldInfo{world.id, world.name, world.space};
}

} // namespace Runic::World
