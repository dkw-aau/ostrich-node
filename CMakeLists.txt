cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For development with CLion and/or Visual Studio Code
# Setup CMake.js for regular CMake
if(NOT DEFINED CMAKE_JS_INC)
    include("cmakejs.cmake")
    setup_cmakejs()
endif()

# Include BoostLib module
set(NODE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/node_modules")
if(NOT EXISTS "${NODE_MODULES_DIR}/boost-lib/cmake/BoostLib.cmake")
    set(NODE_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
    if(NOT EXISTS "${NODE_MODULES_DIR}/boost-lib/cmake/BoostLib.cmake")
        message(FATAL_ERROR "Couldn't find boost-lib cmake file at ${NODE_MODULES_DIR}")
    endif()
endif()
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${NODE_MODULES_DIR}/boost-lib/cmake")
include(BoostLib)

# Set project
project(ostrich)

# Include cmake-js requirements
include_directories(${CMAKE_JS_INC} ../nan)

# Add directories
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/src/main/cpp/
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/hdt/libcds/includes/
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/deps/hdt/libhdt/include/
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/deps/hdt/libhdt/src/
)

# Add source files
file(GLOB_RECURSE OSTRICH_SOURCE_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/src/main/cpp/*/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/src/main/cpp/*/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/src/main/cpp/simpleprogresslistener.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/src/main/cpp/simpleprogresslistener.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/deps/hdt/libhdt/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ostrich/deps/hdt/libhdt/src/*.cpp"
)

# Compile OSTRICH as a static lib
add_library(ostrich_core STATIC ${OSTRICH_SOURCE_FILES})
target_compile_definitions(ostrich_core PUBLIC -DCOMPRESSED_ADD_VALUES -DCOMPRESSED_DEL_VALUES -DUSE_VSI -DUSE_VSI_T)
set_property(TARGET ostrich_core PROPERTY POSITION_INDEPENDENT_CODE ON)

# Source for OSTRICH node bindings
set(SOURCE_OSTRICH_NODE
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/ostrich.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/OstrichStore.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/OstrichStore.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/LiteralsUtils.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/LiteralsUtils.cc")

# Source for OSTRICH node bindings with triple buffering during querying
set(SOURCE_BUFFERED_OSTRICH_NODE
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/bufferedOstrich.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/BufferedOstrichStore.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/BufferedOstrichStore.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/LiteralsUtils.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/LiteralsUtils.cc")

# Set cmake-js binary for bindings
add_library(${PROJECT_NAME} SHARED ${SOURCE_OSTRICH_NODE})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME} ostrich_core)
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

# Set cmake-js binary for buffered bindings
add_library(${PROJECT_NAME}-buffered SHARED ${SOURCE_BUFFERED_OSTRICH_NODE})
set_target_properties(${PROJECT_NAME}-buffered PROPERTIES PREFIX "" SUFFIX ".node")
target_link_libraries(${PROJECT_NAME}-buffered ostrich_core)
target_link_libraries(${PROJECT_NAME}-buffered ${CMAKE_JS_LIB})

# Add boost libs
require_boost_libs(1.70.0 iostreams)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME}-buffered ${Boost_LIBRARIES})

# Kyoto Cabinet dependencies
find_library(LZMA lzma REQUIRED)
find_library(LZO lzo2 REQUIRED)
find_package(ZLIB REQUIRED)

# Add Kyoto Cabinet
find_library(KYOTO_CABINET libkyotocabinet.a REQUIRED)
find_path(KYOTO_INCLUDE_DIR kcplantdb.h REQUIRED)
include_directories(${KYOTO_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${KYOTO_CABINET} ${LZMA} ${LZO})
target_link_libraries(${PROJECT_NAME} ZLIB::ZLIB)
target_link_libraries(${PROJECT_NAME}-buffered ${KYOTO_CABINET} ${LZMA} ${LZO})
target_link_libraries(${PROJECT_NAME}-buffered ZLIB::ZLIB)

# Add pthreads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)
target_link_libraries(${PROJECT_NAME}-buffered Threads::Threads)

if(NOT MSVC)
    set(PThreadLib -pthread)
endif()
