#include "BetaValue.hpp"

// Must not compile: Alpha's Source/ is not an include directory for other modules.
#include "AlphaInternal.hpp"

int Fixture::Beta::Answer() { return Fixture::Alpha::Detail::kAnswer; }
