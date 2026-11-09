#include "GodotRuntime.hpp"

#include "LibGodot/SharedLibrary.hpp"

#include <godot_cpp/classes/godot_instance.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/godot.hpp>

#include <cstring>
#include <filesystem>
#include <thread>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <shellapi.h>
#include <windows.h>
#endif

namespace Runic::GodotBridge {

namespace {

// The two C functions of core/extension/libgodot.h.
using CreateInstanceFn = GDExtensionObjectPtr (*)(int, char*[], GDExtensionInitializationFunction);
using DestroyInstanceFn = void (*)(GDExtensionObjectPtr);

// A symbol address as a function pointer without a cast between object and function pointers, which ISO
// C++ leaves conditionally supported (and -Wpedantic reports).
template <typename Function>
Function ToFunction(void* symbol) {
    static_assert(sizeof(Function) == sizeof(void*));
    Function function = nullptr;
    std::memcpy(&function, &symbol, sizeof(function));
    return function;
}

// LibGodot keeps one instance per process; its initialization callbacks carry no user data, so the
// runtime being created is reachable from them through this pointer.
GodotRuntime* g_Active = nullptr;
std::vector<GodotRuntime::LevelHook>* g_InitializeHooks = nullptr;
std::vector<GodotRuntime::LevelHook>* g_DeinitializeHooks = nullptr;
bool g_InstanceCreatedInProcess = false;

void ClearActive() {
    g_Active = nullptr;
    g_InitializeHooks = nullptr;
    g_DeinitializeHooks = nullptr;
}

GodotInitLevel ToRunic(godot::ModuleInitializationLevel level) {
    switch (level) {
    case godot::MODULE_INITIALIZATION_LEVEL_CORE:
        return GodotInitLevel::Core;
    case godot::MODULE_INITIALIZATION_LEVEL_SERVERS:
        return GodotInitLevel::Servers;
    case godot::MODULE_INITIALIZATION_LEVEL_SCENE:
        return GodotInitLevel::Scene;
    default:
        return GodotInitLevel::Editor;
    }
}

void InitializeLevel(godot::ModuleInitializationLevel level) {
    if (g_InitializeHooks != nullptr) {
        for (const auto& hook : *g_InitializeHooks) {
            hook(ToRunic(level));
        }
    }
}

void DeinitializeLevel(godot::ModuleInitializationLevel level) {
    if (g_DeinitializeHooks != nullptr) {
        for (auto it = g_DeinitializeHooks->rbegin(); it != g_DeinitializeHooks->rend(); ++it) {
            (*it)(ToRunic(level));
        }
    }
}

// The GDExtension entry point of the host executable. LibGodot loads it as the extension
// "libgodot://main" during Main::setup and calls it once per initialization level.
extern "C" GDExtensionBool RunicGodotBridgeInit(GDExtensionInterfaceGetProcAddress getProcAddress,
                                                GDExtensionClassLibraryPtr library,
                                                GDExtensionInitialization* initialization) {
    godot::GDExtensionBinding::InitObject init(getProcAddress, library, initialization);
    init.register_initializer(&InitializeLevel);
    init.register_terminator(&DeinitializeLevel);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

} // namespace

struct GodotRuntime::Impl {
    Detail::SharedLibrary library;
    CreateInstanceFn create = nullptr;
    DestroyInstanceFn destroy = nullptr;
    GDExtensionObjectPtr raw = nullptr;
    godot::GodotInstance* instance = nullptr;
    bool started = false;
    std::uint64_t frames = 0;
    std::thread::id owner;
    std::vector<std::string> argumentStorage;  // Godot keeps pointers into argv during setup
    std::vector<char*> argv;
    std::vector<LevelHook> initializeHooks;
    std::vector<LevelHook> deinitializeHooks;
    std::string error;

    bool Fail(std::string message) {
        error = std::move(message);
        return false;
    }
    bool OnOwnerThread() const { return owner == std::this_thread::get_id(); }
};

GodotRuntime::GodotRuntime() : m_Impl(std::make_unique<Impl>()) {}

GodotRuntime::~GodotRuntime() { Destroy(); }

void GodotRuntime::AddInitializeHook(LevelHook hook) { m_Impl->initializeHooks.push_back(std::move(hook)); }

void GodotRuntime::AddDeinitializeHook(LevelHook hook) { m_Impl->deinitializeHooks.push_back(std::move(hook)); }

bool GodotRuntime::Load(const std::string& libraryPath) {
    Impl& impl = *m_Impl;
    if (impl.raw != nullptr) {
        return impl.Fail("Load: the Godot instance already exists");
    }
    if (!std::filesystem::exists(libraryPath)) {
        return impl.Fail("libgodot not found at " + libraryPath +
                         " (build it with the libgodot targets, or pass the path of a built library)");
    }
    std::string error;
    if (!impl.library.Open(libraryPath, error)) {
        return impl.Fail(error);
    }
    impl.create = ToFunction<CreateInstanceFn>(impl.library.Symbol("libgodot_create_godot_instance"));
    impl.destroy = ToFunction<DestroyInstanceFn>(impl.library.Symbol("libgodot_destroy_godot_instance"));
    if (impl.create == nullptr || impl.destroy == nullptr) {
        impl.library.Close();
        impl.create = nullptr;
        impl.destroy = nullptr;
        return impl.Fail(libraryPath + " has no LibGodot entry points; Godot must be built with "
                                       "library_type=shared_library");
    }
    return true;
}

bool GodotRuntime::Create(const std::vector<std::string>& arguments) {
    Impl& impl = *m_Impl;
    if (!IsLoaded()) {
        return impl.Fail("Create: Load() the library first");
    }
    if (impl.raw != nullptr || g_Active != nullptr) {
        return impl.Fail("Create: this process already has a Godot instance");
    }
    if (g_InstanceCreatedInProcess) {
        return impl.Fail("Create: LibGodot cannot create a second instance in a process after the first one");
    }
    if (arguments.empty()) {
        return impl.Fail("Create: arguments[0] must be the executable path");
    }

    impl.argumentStorage = arguments;
    impl.argv.clear();
    for (std::string& argument : impl.argumentStorage) {
        impl.argv.push_back(argument.data());
    }
    impl.argv.push_back(nullptr);

    impl.owner = std::this_thread::get_id();
    g_Active = this;
    g_InitializeHooks = &impl.initializeHooks;
    g_DeinitializeHooks = &impl.deinitializeHooks;
    g_InstanceCreatedInProcess = true;

    // Inside: the platform OS layer, Main::setup(argv), then RunicGodotBridgeInit as "libgodot://main".
    impl.raw = impl.create(static_cast<int>(impl.argumentStorage.size()), impl.argv.data(), &RunicGodotBridgeInit);
    if (impl.raw == nullptr) {
        ClearActive();
        return impl.Fail("Create: Godot refused the arguments or failed to set up (see Godot's output above)");
    }
    // The C object is a Godot Object; godot-cpp wraps it as godot::GodotInstance.
    impl.instance = static_cast<godot::GodotInstance*>(godot::internal::get_object_instance_binding(impl.raw));
    if (impl.instance == nullptr) {
        impl.destroy(impl.raw);
        impl.raw = nullptr;
        ClearActive();
        return impl.Fail("Create: the Godot instance has no godot-cpp binding");
    }
    return true;
}

bool GodotRuntime::Start() {
    Impl& impl = *m_Impl;
    if (impl.instance == nullptr) {
        return impl.Fail("Start: Create() the instance first");
    }
    if (!impl.OnOwnerThread()) {
        return impl.Fail("Start: called from a thread other than the one that created the instance");
    }
    if (impl.started) {
        return true;
    }
    // Main::setup2 and Main::start: servers, window unless headless, then the main loop's first scene.
    if (!impl.instance->start()) {
        return impl.Fail("Start: Godot did not start (no main scene, or a project error; see Godot's output)");
    }
    impl.started = true;
    return true;
}

bool GodotRuntime::Iterate() {
    Impl& impl = *m_Impl;
    if (!impl.started || !impl.OnOwnerThread()) {
        return true;
    }
    ++impl.frames;
    return impl.instance->iteration();
}

void GodotRuntime::Destroy() {
    Impl& impl = *m_Impl;
    if (impl.raw != nullptr) {
        // stop(), the instance's deletion and Main::cleanup(); godot-cpp's bindings are invalid afterwards.
        impl.destroy(impl.raw);
        impl.raw = nullptr;
        impl.instance = nullptr;
        impl.started = false;
    }
    if (g_Active == this) {
        ClearActive();
    }
    // The library stays mapped: Godot registers atexit handlers and thread-locals that live in it.
}

bool GodotRuntime::IsLoaded() const { return m_Impl->create != nullptr && m_Impl->destroy != nullptr; }
bool GodotRuntime::IsCreated() const { return m_Impl->raw != nullptr; }
bool GodotRuntime::IsStarted() const { return m_Impl->started; }
std::uint64_t GodotRuntime::FrameCount() const { return m_Impl->frames; }
const std::string& GodotRuntime::LastError() const { return m_Impl->error; }

std::string GodotRuntime::LibraryFileName(GodotTarget target) {
    const char* kind = target == GodotTarget::Editor ? "editor" : "template_release";
#if defined(__x86_64__) || defined(_M_X64)
    const char* arch = "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    const char* arch = "arm64";
#else
    const char* arch = "unknown";
#endif
#if defined(_WIN32)
    return std::string("godot.windows.") + kind + "." + arch + ".dll";
#elif defined(__APPLE__)
    return std::string("libgodot.macos.") + kind + "." + arch + ".dylib";
#else
    return std::string("libgodot.linuxbsd.") + kind + "." + arch + ".so";
#endif
}

std::string GodotRuntime::ExecutableDirectory() {
    const std::filesystem::path executable(Detail::ExecutablePath());
    return executable.parent_path().string();
}

std::string GodotRuntime::DefaultLibraryPath(GodotTarget target) {
    const std::filesystem::path directory(ExecutableDirectory());
    const std::filesystem::path release = directory / LibraryFileName(target);
    if (target == GodotTarget::Template && !std::filesystem::exists(release)) {
        std::string debugName = LibraryFileName(target);
        const auto at = debugName.find("template_release");
        debugName.replace(at, std::string("template_release").size(), "template_debug");
        const std::filesystem::path debug = directory / debugName;
        if (std::filesystem::exists(debug)) {
            return debug.string();
        }
    }
    return release.string();
}

std::vector<std::string> GodotRuntime::CommandLine(int argc, char** argv) {
    std::vector<std::string> arguments;
#if defined(_WIN32)
    (void)argc;
    (void)argv;
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; i < count; ++i) {
        const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, utf8.data(), bytes, nullptr, nullptr);
        arguments.push_back(std::move(utf8));
    }
    LocalFree(wide);
#else
    arguments.assign(argv, argv + argc);
#endif
    return arguments;
}

} // namespace Runic::GodotBridge
