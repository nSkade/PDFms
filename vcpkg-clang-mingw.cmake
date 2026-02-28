# vcpkg-clang-mingw.cmake

set(CMAKE_SYSTEM_NAME Windows)

# 1. Compilers
set(CMAKE_C_COMPILER "C:/LLVM-MinGW/bin/clang.exe")
set(CMAKE_CXX_COMPILER "C:/LLVM-MinGW/bin/clang++.exe")

# 2. Tools
set(CMAKE_AR "C:/LLVM-MinGW/bin/llvm-ar.exe")
set(CMAKE_RANLIB "C:/LLVM-MinGW/bin/llvm-ranlib.exe")

# 3. The "Force libc++" Magic
# We use VCPKG_CXX_FLAGS to ensure these are applied during dependency compilation
set(VCPKG_C_FLAGS "-stdlib=libc++")
set(VCPKG_CXX_FLAGS "-stdlib=libc++")

# Also set the standard CMake variables for your main project
set(CMAKE_CXX_FLAGS "-stdlib=libc++" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "-stdlib=libc++" CACHE STRING "" FORCE)

# 4. Use LLD (Linker)
set(VCPKG_LINKER_FLAGS "-fuse-ld=lld")