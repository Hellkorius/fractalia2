cmake_minimum_required(VERSION 3.20)
project(fractalia2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Cross-compilation setup for Windows using mingw
if(WIN32 OR MINGW)
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
endif()

# Vendor library paths
set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/../vendored)
set(SDL3_DIR ${VENDOR_DIR}/SDL3-3.1.6/x86_64-w64-mingw32)
set(GLM_DIR ${VENDOR_DIR}/glm)
set(FLECS_DIR ${VENDOR_DIR}/flecs-4.0.2)
set(VULKAN_LOADER_DIR ${VENDOR_DIR}/Vulkan-Loader)

# SDL3 - directly specify the library path since we know where it is
set(SDL3_LIBRARY ${SDL3_DIR}/lib/libSDL3.dll.a)

# Vulkan setup for cross-compilation
if(WIN32 OR MINGW)
    # For Windows cross-compilation, manually set up Vulkan
    set(VULKAN_DIR ${VENDOR_DIR}/vulkan)
    set(Vulkan_INCLUDE_DIRS ${VULKAN_DIR}/include)
    # We don't link against Vulkan library - it's loaded dynamically
    set(Vulkan_LIBRARIES "")
    
    # Add Vulkan include directory
    if(NOT EXISTS ${Vulkan_INCLUDE_DIRS})
        message(FATAL_ERROR "Vulkan headers not found at ${Vulkan_INCLUDE_DIRS}. Please download Vulkan headers to ../vendored/vulkan/include/")
    endif()
else()
    find_package(Vulkan REQUIRED)
endif()

# Include directories
include_directories(
    ${SDL3_DIR}/include
    ${GLM_DIR}
    ${FLECS_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Fast build option - create separate Flecs library
option(FAST_BUILD "Enable fast build mode (precompiled Flecs)" OFF)

if(FAST_BUILD)
    # Check if Flecs library already exists
    if(NOT TARGET flecs_lib)
        message(STATUS "Fast build: Creating Flecs library target")
        file(GLOB_RECURSE FLECS_SOURCES "${FLECS_DIR}/src/*.c")
        add_library(flecs_lib STATIC ${FLECS_SOURCES})
        target_include_directories(flecs_lib PUBLIC ${FLECS_DIR}/include)
        target_compile_options(flecs_lib PRIVATE -O2 -DNDEBUG)
        
        # Cache the library for future builds
        set_target_properties(flecs_lib PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        )
    endif()
    
    # Main executable sources (excluding Flecs)
    file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
    add_executable(${PROJECT_NAME} ${SOURCES})
else()
    # Normal build - include everything
    file(GLOB_RECURSE SOURCES
        "src/*.cpp"
        "src/*.c"
        "${FLECS_DIR}/src/*.c"
    )
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()


# Link libraries
if(FAST_BUILD)
    target_link_libraries(${PROJECT_NAME}
        flecs_lib
        ${SDL3_LIBRARY}
        ${Vulkan_LIBRARIES}
        gdi32
        winmm
        ole32
        oleaut32
        imm32
        version
        setupapi
        ws2_32
    )
else()
    target_link_libraries(${PROJECT_NAME}
        ${SDL3_LIBRARY}
        ${Vulkan_LIBRARIES}
        gdi32
        winmm
        ole32
        oleaut32
        imm32
        version
        setupapi
        ws2_32
    )
endif()

# For Windows cross-compilation, we need to handle Vulkan loading differently
if(WIN32 OR MINGW)
    # Add custom build step to generate Vulkan function pointers
    # This approach loads Vulkan functions dynamically at runtime
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
endif()

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -O2
)

# Windows-specific settings
if(WIN32 OR MINGW)
    # Use console application for easier debugging
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE FALSE
    )
    
    # Add static linking flags to reduce DLL dependencies
    target_link_options(${PROJECT_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
    )
endif()