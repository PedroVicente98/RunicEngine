// Jolt's process-wide setup (JoltRuntime.hpp), counted, with Jolt's trace sent to the Runic log.

#include "PhysicsWorld/JoltRuntime.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

#include "Log.hpp"

namespace Runic::Physics::Detail {

namespace {

constexpr std::size_t kTraceBytes = 1024;

std::mutex g_Mutex;
int g_Users = 0;

void Trace(const char *format, ...) {
    char text[kTraceBytes];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    RUNIC_LOG_INFO("RUNIC: Jolt: {}", text);
}

} // namespace

JoltRuntime::JoltRuntime() {
    const std::lock_guard lock(g_Mutex);
    if (g_Users++ == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = &Trace;
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
}

JoltRuntime::~JoltRuntime() {
    const std::lock_guard lock(g_Mutex);
    if (--g_Users == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

} // namespace Runic::Physics::Detail
