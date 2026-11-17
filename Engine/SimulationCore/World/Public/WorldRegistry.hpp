#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "CellKeys.hpp"
#include "CoordinateSpace.hpp"
#include "PlainIds.hpp"
#include "Result.hpp"
#include "WorldSpace.hpp"

// The world registry: what each WorldId is. A WorldId names one seamless world: one coordinate
// space, one origin, one partition. A place that is not part of the seamless world (a dungeon
// run, an arena) is an instance of a world: an InstanceId that shares the world's space and has
// its own simulation. Everything that turns a position into a cell of a world asks the registry,
// and a WorldId it does not know is refused (RunicDocs › World space coordinates, cells and
// activation › Coordinate system).

namespace Runic::World {

// The InstanceId of a world's own shared copy: no instance. A dungeon run has another.
inline constexpr Core::InstanceId kSharedInstance{};

// Which copy of which world: where a simulation, and every entity in it, is.
struct WorldInstance {
    Core::WorldId world;
    Core::InstanceId instance = kSharedInstance;

    constexpr bool IsShared() const noexcept { return instance == kSharedInstance; }

    friend constexpr bool operator==(const WorldInstance &,
                                     const WorldInstance &) noexcept = default;
};

struct WorldDescription {
    std::string_view name; // for tools and logs, unique in the registry
    CoordinateSpace space;
};

// A registered world. The name stays valid while the registry lives.
struct WorldInfo {
    Core::WorldId id;
    std::string_view name;
    CoordinateSpace space;
};

class WorldRegistry {
  public:
    WorldRegistry();
    ~WorldRegistry();

    WorldRegistry(const WorldRegistry &) = delete;
    WorldRegistry &operator=(const WorldRegistry &) = delete;

    // Registers a world, at start. Fails, changing nothing, when the ID is invalid or already
    // registered, the name is empty or taken, or a size is zero.
    Core::Result<void> Register(Core::WorldId id, const WorldDescription &description);

    bool Knows(Core::WorldId id) const noexcept;

    // The world's coordinate space; an instance uses its world's. Fails for a WorldId the
    // registry does not know, as every function below does.
    Core::Result<CoordinateSpace> SpaceOf(Core::WorldId id) const;

    // The cell and the sector of a position in a world.
    Core::Result<CellKey> CellAt(Core::WorldId world, Core::WorldPosition position) const;
    Core::Result<SectorKey> SectorAt(Core::WorldId world, Core::WorldPosition position) const;

    // Where a cell is, and the sector it belongs to.
    Core::Result<Core::WorldPosition> CellMin(const CellKey &cell) const;
    Core::Result<Core::WorldPosition> CellCenter(const CellKey &cell) const;
    Core::Result<SectorKey> SectorOf(const CellKey &cell) const;

    // The worlds, in the order they were registered, for tools.
    std::uint32_t Count() const noexcept;
    WorldInfo Describe(std::uint32_t index) const;

  private:
    struct State;
    std::unique_ptr<State> m_State;
};

} // namespace Runic::World
