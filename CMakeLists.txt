cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)
project(ReverseEngineeringUtils VERSION 0.1.0 LANGUAGES C CXX)

# Allows usage of configure time content
include(FetchContent)

add_subdirectory(Utils)

set(_ReverseEngineeringUtils_cpp_headers
	Address.hpp
	Assembly.hpp
	BaseAddress.hpp
	Memory.hpp
	Shader.hpp
	third_party/DXBCChecksum/DXBCChecksum.h
)

set(_ReverseEngineeringUtils_cpp_sources
	Assembly.cpp
	Memory.cpp
	third_party/DXBCChecksum/DXBCChecksum.cpp
)

add_library(BeaEngine STATIC IMPORTED)

set_target_properties(BeaEngine PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/third_party/BeaEngine/lib/BeaEngine.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/BeaEngine/include"
)

add_library(ReverseEngineeringUtils
	STATIC
	${_ReverseEngineeringUtils_cpp_headers}
	${_ReverseEngineeringUtils_cpp_sources}
)

# Prevent windows.h from defining min and max macros
target_compile_definitions(ReverseEngineeringUtils PUBLIC NOMINMAX)

target_compile_features(ReverseEngineeringUtils PUBLIC cxx_std_17)

target_link_libraries(ReverseEngineeringUtils PRIVATE Utils BeaEngine)