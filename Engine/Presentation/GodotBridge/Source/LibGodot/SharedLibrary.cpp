#include "LibGodot/SharedLibrary.hpp"

#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace Runic::GodotBridge::Detail {

#if defined(_WIN32)
namespace {
std::wstring Widen(const std::string& utf8) {
    const int count = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(count > 0 ? count - 1 : 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), count);
    return wide;
}
std::string Narrow(const std::wstring& wide) {
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), bytes, nullptr, nullptr);
    return utf8;
}
} // namespace
#endif

SharedLibrary::~SharedLibrary() { Close(); }

bool SharedLibrary::Open(const std::string& path, std::string& error) {
    Close();
#if defined(_WIN32)
    m_Handle = reinterpret_cast<void*>(LoadLibraryW(Widen(path).c_str()));
    if (m_Handle == nullptr) {
        error = "cannot open " + path + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
#else
    // RTLD_LOCAL: libgodot's symbols stay private to it; the host reaches it only through its C entry points.
    m_Handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (m_Handle == nullptr) {
        const char* reason = dlerror();
        error = "cannot open " + path + (reason != nullptr ? std::string(": ") + reason : std::string());
        return false;
    }
#endif
    return true;
}

void* SharedLibrary::Symbol(const char* name) const {
    if (m_Handle == nullptr) {
        return nullptr;
    }
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), name));
#else
    return dlsym(m_Handle, name);
#endif
}

void SharedLibrary::Close() {
    if (m_Handle == nullptr) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(m_Handle));
#else
    dlclose(m_Handle);
#endif
    m_Handle = nullptr;
}

std::string ExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return Narrow(buffer);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    return std::string(buffer.data());
#else
    std::vector<char> buffer(4096, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    return std::string(buffer.data(), static_cast<size_t>(length));
#endif
}

} // namespace Runic::GodotBridge::Detail
