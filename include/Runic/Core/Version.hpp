#pragma once

namespace Runic {

// Disposable bootstrap API: proves that a consumer links the engine library.
[[nodiscard]] const char *Version() noexcept;

} // namespace Runic
