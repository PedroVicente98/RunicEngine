# Engine module catalog: one row per module of docs/system/03-dependency-rules.md § Engine modules.
# The "Depends on" column is a ceiling (ALLOWS); third parties are PRIVATE unless listed in PUBLIC_EXCEPTIONS.
# Third parties are library IDs that cmake/Dependencies.cmake maps to targets when it adds each library.
# Changing a row is an owner decision recorded in ARCH first; checks.catalog_sync keeps this file equal to 03.

runic_catalog_module(Core
    GROUP Foundation
    THIRD_PARTY glm
    PUBLIC_EXCEPTIONS glm
    HEADLESS yes)
runic_catalog_module(Application
    GROUP Foundation
    ALLOWS Runic::Core
    THIRD_PARTY glfw
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
runic_catalog_module(ECS
    GROUP SimulationCore
    ALLOWS Runic::Core
    THIRD_PARTY flecs
    PUBLIC_EXCEPTIONS flecs-world
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
    PUBLIC_EXCEPTIONS flecs-world behaviortree-nodes
    HEADLESS yes)
runic_catalog_module(Environment
    GROUP WorldSystems
    ALLOWS Runic::Core Runic::World Runic::Simulation
    HEADLESS yes)
runic_catalog_module(Water
    GROUP WorldSystems
    ALLOWS Runic::World Runic::Physics Runic::Simulation Runic::Gameplay Runic::Environment Runic::DistributedSimulation
    HEADLESS yes)
runic_catalog_module(Network
    GROUP Networking
    ALLOWS Runic::Application Runic::Simulation Runic::DistributedSimulation Runic::Gameplay Runic::Perception
    HEADLESS yes)
runic_catalog_module(DistributedSimulation
    GROUP Networking
    ALLOWS Runic::Simulation Runic::Events Runic::World
    HEADLESS yes)
runic_catalog_module(Client
    GROUP Presentation
    ALLOWS Runic::Application Runic::Simulation Runic::Network Runic::Perception Runic::Environment Runic::Water Runic::Renderer Runic::Animation Runic::Audio Runic::UI Runic::Assets
    HEADLESS no)
runic_catalog_module(Renderer
    GROUP Presentation
    ALLOWS Runic::Core Runic::Assets
    THIRD_PARTY bgfx
    HEADLESS no)
runic_catalog_module(Animation
    GROUP Presentation
    ALLOWS Runic::Core Runic::Assets
    HEADLESS no)
runic_catalog_module(UI
    GROUP Presentation
    ALLOWS Runic::Application Runic::Renderer Runic::Assets
    THIRD_PARTY rmlui imgui
    HEADLESS no)
runic_catalog_module(Audio
    GROUP Presentation
    ALLOWS Runic::Core Runic::Assets
    THIRD_PARTY audio-backend
    HEADLESS no)
runic_catalog_module(Editor
    GROUP Tools
    ALLOWS_ANY_MODULE
    THIRD_PARTY imgui imnodes
    HEADLESS never-shipped)
runic_catalog_module(Cooker
    GROUP Tools
    ALLOWS Runic::Core Runic::Content Runic::Assets Runic::World Runic::Physics Runic::Navigation Runic::Water
    THIRD_PARTY fastgltf meshoptimizer mikktspace xatlas ktx basisu jolt recast zstd
    HEADLESS tool)

# Header-only interface targets the Platform implements; each may link Runic::Core only.
runic_catalog_interface(NetworkTransport FROM Network)
runic_catalog_interface(SimulationPort FROM Simulation)
