# Transitional bundle (OQ-8): the headless third parties RunicGame's executables link until they link
# per-module targets. It is not a module and has no code of its own. Runic::Core is created by CORE-1.
add_library(runic_runtime INTERFACE)
add_library(Runic::Runtime ALIAS runic_runtime)
target_compile_features(runic_runtime INTERFACE cxx_std_20)
