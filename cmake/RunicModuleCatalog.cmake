# Engine module catalog: one row per module of RunicDocs › Architecture › Repositories, build targets and processes
# › Engine modules (the architecture source of truth; there is no copy of it in this repository).
# The "Depends on" column is a ceiling (ALLOWS); third parties are PRIVATE unless listed in PUBLIC_EXCEPTIONS.
# Third parties are library IDs that cmake/Dependencies.cmake and cmake/RunicGodot.cmake map to targets.
# Changing a row is an owner decision recorded in RunicDocs first; checks.catalog_sync keeps this file equal to that
# table (read from the RunicDocs checkout, RUNIC_DOCS_DIR).

# ---- Foundation ----
runic_catalog_module(Core
    GROUP Foundation
    THIRD_PARTY glm
    PUBLIC_EXCEPTIONS glm
    HEADLESS yes)
runic_catalog_module(Application
    GROUP Foundation
    ALLOWS Runic::Core
    HEADLESS yes)
runic_catalog_module(Events
    GROUP Foundation
    ALLOWS Runic::Core
    HEADLESS yes)
runic_catalog_module(Content
    GROUP Foundation
    ALLOWS Runic::Core
    HEADLESS yes)
runic_catalog_module(Assets
    GROUP Foundation
    ALLOWS Runic::Core Runic::Content
    THIRD_PARTY zstd
    HEADLESS yes)

# ---- Simulation core ----
# Flecs stays private to ECS (decided 2026-11-12): modules use Runic types, and the Flecs world is reachable only
# through the opt-in Runic::ECSNative target that ECS-1 adds (discouraged; the dependency check lists its users).
runic_catalog_module(ECS
    GROUP SimulationCore
    ALLOWS Runic::Core
    THIRD_PARTY flecs
    HEADLESS yes)
runic_catalog_module(World
    GROUP SimulationCore
    ALLOWS Runic::Core Runic::ECS
    HEADLESS yes)
runic_catalog_module(Physics
    GROUP SimulationCore
    ALLOWS Runic::Core Runic::World
    THIRD_PARTY jolt
    HEADLESS yes)
runic_catalog_module(Simulation
    GROUP SimulationCore
    ALLOWS Runic::Core Runic::Application Runic::ECS Runic::Events Runic::World Runic::Physics Runic::Content
    HEADLESS yes)

# ---- GameplayAPI ----
runic_catalog_module(Gameplay
    GROUP GameplayAPI
    ALLOWS Runic::Simulation Runic::Environment
    HEADLESS yes)
runic_catalog_module(Combat
    GROUP GameplayAPI
    ALLOWS Runic::Gameplay Runic::Physics Runic::Environment Runic::DistributedSimulation
    HEADLESS yes)
runic_catalog_module(GameplayFlow
    GROUP GameplayAPI
    ALLOWS Runic::Gameplay Runic::DistributedSimulation
    HEADLESS yes)
runic_catalog_module(Quest
    GROUP GameplayAPI
    ALLOWS Runic::GameplayFlow Runic::DistributedSimulation
    HEADLESS yes)

# ---- World systems ----
runic_catalog_module(Perception
    GROUP WorldSystems
    ALLOWS Runic::Gameplay Runic::World Runic::Environment Runic::Water
    HEADLESS yes)
runic_catalog_module(Navigation
    GROUP WorldSystems
    ALLOWS Runic::World Runic::Simulation Runic::Assets
    THIRD_PARTY detour
    HEADLESS yes)
runic_catalog_module(AI
    GROUP WorldSystems
    ALLOWS Runic::Gameplay Runic::Combat Runic::Perception Runic::Navigation
    THIRD_PARTY behaviortree
    PUBLIC_EXCEPTIONS behaviortree-nodes
    HEADLESS yes)
runic_catalog_module(Environment
    GROUP WorldSystems
    ALLOWS Runic::Core Runic::World Runic::Simulation
    HEADLESS yes)
runic_catalog_module(Water
    GROUP WorldSystems
    ALLOWS Runic::World Runic::Physics Runic::Simulation Runic::Gameplay Runic::Environment Runic::DistributedSimulation
    HEADLESS yes)

# ---- Networking ----
runic_catalog_module(Network
    GROUP Networking
    ALLOWS Runic::Application Runic::Simulation Runic::DistributedSimulation Runic::Gameplay Runic::Perception
    HEADLESS yes)
runic_catalog_module(DistributedSimulation
    GROUP Networking
    ALLOWS Runic::Simulation Runic::Events Runic::World
    HEADLESS yes)

# ---- Presentation (never reached by a headless target) ----
runic_catalog_module(Client
    GROUP Presentation
    ALLOWS Runic::Application Runic::Simulation Runic::Network Runic::Perception Runic::Environment Runic::Water Runic::Animation Runic::Audio Runic::Assets
    HEADLESS no)
runic_catalog_module(GodotBridge
    GROUP Presentation
    ALLOWS Runic::Application Runic::Client Runic::Audio Runic::Assets
    THIRD_PARTY libgodot godot-cpp dl
    HEADLESS no)
runic_catalog_module(Animation
    GROUP Presentation
    ALLOWS Runic::Core Runic::Assets
    HEADLESS no)
runic_catalog_module(Audio
    GROUP Presentation
    ALLOWS Runic::Core Runic::Assets
    HEADLESS no)

# ---- Tools ----
runic_catalog_module(GodotTools
    GROUP Tools
    ALLOWS Runic::Cooker
    ALLOWS_ANY_MODULE
    THIRD_PARTY libgodot godot-cpp
    PUBLIC_EXCEPTIONS godot-cpp
    HEADLESS never-shipped)
runic_catalog_module(Cooker
    GROUP Tools
    ALLOWS Runic::Core Runic::Content Runic::Assets Runic::World Runic::Physics Runic::Navigation Runic::Water
    THIRD_PARTY jolt recast meshoptimizer zstd
    HEADLESS tool)

# Header-only interface targets the Platform implements; each may link Runic::Core only.
runic_catalog_interface(NetworkTransport FROM Network)
runic_catalog_interface(SimulationPort FROM Simulation)
