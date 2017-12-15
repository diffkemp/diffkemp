add_library(ParDepSlicer MODULE ParamDependencySlicer.cpp)

target_compile_features(ParDepSlicer PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(ParDepSlicer PROPERTIES COMPILE_FLAGS "-fno-rtti")
