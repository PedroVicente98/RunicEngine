#pragma once

// Forbidden: a library type in a public header.
#include <flecs.h>

namespace Fixture::Alpha {
flecs::world* World();
}
