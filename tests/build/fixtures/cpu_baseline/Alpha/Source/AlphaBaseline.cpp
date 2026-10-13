#include "AlphaBaseline.hpp"

#if defined(FIXTURE_EXPECT_BASELINE)
#if !defined(__AVX2__) || !defined(__FMA__)
#error "the x86-64-v3 baseline (AVX2, FMA) is not applied"
#endif
#else
#if defined(__AVX2__)
#error "AVX2 is on although RUNIC_CPU_BASELINE=none"
#endif
#endif

bool Fixture::Alpha::BaselineChecked() { return true; }
