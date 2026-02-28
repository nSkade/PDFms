cmake_minimum_required(VERSION 3.17)

set(VCPKG_TARGET_TRIPLET x64-mingw-dynamic CACHE STRING "" FORCE)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg-clang-mingw.cmake" CACHE STRING "" FORCE)
set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)

set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed")
set(ENV{PKG_CONFIG_PATH} "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig")

project(pdfms LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(VCPKG_BOOTSTRAP_OPTIONS "-disableMetrics")
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

add_compile_options("-stdlib=libc++")
add_link_options("-stdlib=libc++")

find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)

add_executable(pdfms main.cpp)

target_link_libraries(pdfms PRIVATE 
    PkgConfig::POPPLER_CPP
)

target_precompile_headers(pdfms PRIVATE
	pch.h
)

# Find FTXUI installed via vcpkg
#find_package(ftxui CONFIG REQUIRED)

# Link libraries
#target_link_libraries(pdfms PRIVATE
#  ftxui::dom
#  ftxui::screen
#  ftxui::component
#)
# this is required for ftxui to work on vscode clang with windows >:(
## Add additional compile options you want
#target_compile_options(pdfms PRIVATE -Wno-deprecated-declarations)
#
## Remove /utf-8 flag from global flags (important for Clang on Windows)
#string(REPLACE "/utf-8" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#string(REPLACE "/utf-8" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
#
## Remove /utf-8 from the executable target compile options if present
#get_target_property(existing_opts pdfms COMPILE_OPTIONS)
#if(existing_opts)
#  list(REMOVE_ITEM existing_opts "/utf-8")
#  set_target_properties(pdfms PROPERTIES COMPILE_OPTIONS "${existing_opts}")
#endif()
#
## Also attempt to remove /utf-8 from FTXUI interface targets (optional, in case they add it)
#foreach(target ftxui::screen ftxui::dom ftxui::component)
#  get_target_property(opts ${target} INTERFACE_COMPILE_OPTIONS)
#  if(opts)
#    list(REMOVE_ITEM opts "/utf-8")
#    set_target_properties(${target} PROPERTIES INTERFACE_COMPILE_OPTIONS "${opts}")
#  endif()
#endforeach()


# Enable AddressSanitizer for debug builds
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#	# Enable AddressSanitizer
#	target_compile_options(pdfms PRIVATE -fsanitize=address -g)
#	target_link_options(pdfms PRIVATE -fsanitize=address)
#	target_compile_definitions(pdfms PRIVATE _ITERATOR_DEBUG_LEVEL=0)
#	set_property(TARGET pdfms PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
#endif()
