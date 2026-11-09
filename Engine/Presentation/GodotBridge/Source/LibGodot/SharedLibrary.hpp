#pragma once

#include <string>

namespace Runic::GodotBridge::Detail {

// A shared library opened at runtime: dlopen on Linux and macOS, LoadLibraryW on Windows.
class SharedLibrary {
public:
    SharedLibrary() = default;
    ~SharedLibrary();
    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    bool Open(const std::string& path, std::string& error);
    void* Symbol(const char* name) const;
    void Close();
    bool IsOpen() const { return m_Handle != nullptr; }

private:
    void* m_Handle = nullptr;
};

std::string ExecutablePath();

} // namespace Runic::GodotBridge::Detail
