cmake_minimum_required(VERSION 3.1)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_DISABLE_SOURCE_CHANGES  ON)

# Use std=c++14
set(CMAKE_CXX_STANDARD 14)

set (GCC_COVERAGE_COMPILE_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

# Enable sanitizers for debug builds
if (CMAKE_BUILD_TYPE MATCHES Debug)
    # Enable extra debug info in binary
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-4 -g3 -Og")
    # Enable UB / address sanitizers
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fsanitize=address -fno-sanitize-recover")
else()
    set (GCC_OPTIMIZATION "-O9")
endif ()

# Start our project
project(yostat CXX)

# Don't bother building the json tests every time
set(JSON_BuildTests OFF CACHE INTERNAL "")
# Add JSON dependency
add_subdirectory("vendor/json")

# Add Wx dependencies for gui
set(wxWidgets_CONFIG_OPTIONS --toolkit=gtk3)
find_package(wxWidgets COMPONENTS core base adv REQUIRED)
include_directories(${wxWidgets_INCLUDE_DIRS})
include(${wxWidgets_USE_FILE})
include(FindOpenGL)
include_directories(${OPENGL_INCLUDE_DIRS})

# Set our local include directory for all targets
include_directories("include/")

add_executable(yostat
    src/main.cpp
    src/yostat_wx_panel.cpp
    src/yostat_parse.cpp
)
target_link_libraries(yostat
    PRIVATE nlohmann_json::nlohmann_json
    ${wxWidgets_LIBRARIES}
    ${OPENGL_LIBRARIES}
)
