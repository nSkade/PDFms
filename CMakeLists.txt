cmake_minimum_required(VERSION 3.17)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

project(pdfms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)

add_executable(pdfms main.cpp)
target_link_libraries(pdfms PRIVATE PkgConfig::POPPLER_CPP)
target_compile_options(pdfms PRIVATE -Wno-deprecated-declarations)

# Enable AddressSanitizer for debug builds
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#	# Enable AddressSanitizer
#	target_compile_options(pdfms PRIVATE -fsanitize=address -g)
#	target_link_options(pdfms PRIVATE -fsanitize=address)
#	target_compile_definitions(pdfms PRIVATE _ITERATOR_DEBUG_LEVEL=0)
#	set_property(TARGET pdfms PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
#endif()