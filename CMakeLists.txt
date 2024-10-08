CMAKE_MINIMUM_REQUIRED(VERSION 3.6)
PROJECT(terark-zip)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_VERBOSE_MAKEFILE ON)

OPTION(WITH_TESTS "Build with tests" OFF)

MESSAGE("[terark-zip] CMAKE_BUILD_TYPE : ${CMAKE_BUILD_TYPE}")
MESSAGE("[terark-zip] WITH_TESTS : ${WITH_TESTS}")

# Headers
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/boost-include)

#----------- Extra Compiler Flags
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -DHAVE_SSE42 -DHAVE_PCLMUL -fPIC")

SET(BUILD_SUFFIX "d")
IF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  SET(BUILD_SUFFIX "r")
ENDIF()

add_subdirectory(3rdparty/boost-include)

# CORE LIB
FILE(GLOB CORE_SRC src/terark/*.cpp
        src/terark/io/*.cpp
        src/terark/util/*.cpp
        src/terark/thread/*.cpp
        src/terark/succinct/*.cpp
        src/terark/c/*.c
        # boost-include/libs/coroutine/src/*.cpp
        # boost-include/libs/coroutine/src/detail/*.cpp
        # boost-include/libs/coroutine/src/posix/*.cpp
        # boost-include/libs/fiber/src/*.cpp
        # boost-include/libs/fiber/src/algo/*.cpp
        # boost-include/libs/fiber/src/numa/*.cpp
        # boost-include/libs/fiber/examples/*.cpp
        # boost-include/libs/fiber/examples/asio/*.cpp
        # boost-include/libs/fiber/examples/cuda/*.cpp
        # boost-include/libs/fiber/examples/hip/*.cpp
        # boost-include/libs/fiber/examples/numa/*.cpp
        )

# FSA LIB
FILE(GLOB FSA_SRC src/terark/fsa/*.cpp)

# Embbed ZSTD
FILE(GLOB ZSTD_SRC 3rdparty/zstd/zstd/common/*.c
        3rdparty/zstd/zstd/compress/*.c
        3rdparty/zstd/zstd/decompress/*.c
        3rdparty/zstd/zstd/decompress/*.S
        3rdparty/zstd/zstd/deprecated/*.c
        3rdparty/zstd/zstd/dictBuilder/*.c
        3rdparty/zstd/zstd/legacy/*.c)

# ZBS LIB
FILE(GLOB ZBS_SRC src/terark/entropy/*.cpp
        src/terark/zbs/*.cpp
        ${ZSTD_SRC})

# IDX LIB
FILE(GLOB IDX_SRC src/terark/idx/*.cpp)

SET(ALL_SRC ${CORE_SRC} ${IDX_SRC} ${ZBS_SRC} ${FSA_SRC})
# TODO fix other test code, now only check crit_bit_trie
FILE(GLOB_RECURSE ALL_SRC_TESTS 
  src/*_test.cpp
  ${CMAKE_SOURCE_DIR}/tests/tries/test_crit_bit_trie.cpp)

# Remove all test files
LIST(REMOVE_ITEM ALL_SRC ${ALL_SRC_TESTS})

# Combined all into libterark-zip.a
ADD_LIBRARY(terark-zip-${BUILD_SUFFIX} STATIC ${ALL_SRC})
TARGET_LINK_LIBRARIES(terark-zip-${BUILD_SUFFIX} boost-fiber boost-thread boost-system)
TARGET_INCLUDE_DIRECTORIES(terark-zip-${BUILD_SUFFIX} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
                                                              "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd>"
                                                              # expose boost headers in case someone need it
                                                              "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/boost-include>")

option(WITH_TBB "build with Threading Building Blocks (TBB)" OFF)
if(WITH_TBB)
  find_package(TBB REQUIRED)
  add_definitions(-DTERARK_WITH_TBB=1)
  include_directories(${TBB_INCLUDE_DIR})
endif()

############## Build gtests ###########
IF(WITH_TESTS)
  # Find exist gtest target
  IF(TARGET gtest)
    MESSAGE("[terark-zip] gtest target exist, skip local build")
  ELSE()
    MESSAGE("[terark-zip] gtest target not found, use local build")
    ADD_SUBDIRECTORY(3rdparty/googletest)
  ENDIF()
  get_target_property(gtest_INCLUDE_DIR gtest INCLUDE_DIRECTORIES)
  INCLUDE_DIRECTORIES(${gtest_INCLUDE_DIR})
  MESSAGE("[terark-zip] gtest include path: ${gtest_INCLUDE_DIR}")

  foreach(test_file ${ALL_SRC_TESTS})
    string(REPLACE ".cpp" "" test_name ${test_file})
    string(REGEX REPLACE "^((.+)/)+" "" test_name ${test_name})
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} gtest terark-zip-${BUILD_SUFFIX})
  endforeach()
ENDIF()


############## Install Targets ############

INSTALL(TARGETS terark-zip-${BUILD_SUFFIX}
        EXPORT terark-zip-${BUILD_SUFFIX}-targets
        ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}/lib
        )

INSTALL( EXPORT terark-zip-${BUILD_SUFFIX}-targets DESTINATION ${CMAKE_BINARY_DIR}/cmake)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/terark
        DESTINATION ${CMAKE_BINARY_DIR}/include
        FILES_MATCHING 
        PATTERN "*.h"
        PATTERN "*.hpp"
)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zstd/zstd
        DESTINATION ${CMAKE_BINARY_DIR}/include
        FILES_MATCHING 
        PATTERN "*.h"
        PATTERN "*.hpp"
)

INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/boost-include/boost
        DESTINATION ${CMAKE_BINARY_DIR}/include
        FILES_MATCHING 
        PATTERN "*.h"
        PATTERN "*.hpp"
)

INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/3rdparty/boost-include/lib
        DESTINATION ${CMAKE_BINARY_DIR}
        FILES_MATCHING
        PATTERN "libboost_fiber.a"
        PATTERN "libboost_context.a"
        PATTERN "libboost_system.a"
        PATTERN "libboost_filesystem.a"
        PATTERN "libboost_thread.a"
)



############## Legacy Tests (Deprecated!) ##############
IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # tests
    FILE(GLOB TESTS_SRC
            tests/core/*.cpp
            tests/tries/*.cpp
            tests/succinct/*.cpp)
        #ADD_LIBRARY(terark-tests SHARED ${TESTS_SRC})
        #TARGET_LINK_LIBRARIES(terark-tests terark-zip-${BUILD_SUFFIX})

    # tools
    FILE(GLOB TOOLS_SRC
            tools/general/*.cpp
            tools/fsa/*.cpp
            tools/zbs/*.cpp)
        #ADD_LIBRARY(terark-tools SHARED ${TOOLS_SRC})
        #TARGET_LINK_LIBRARIES(terark-tools terark-zip-${BUILD_SUFFIX})
ENDIF ()

############## Examples & Tools  ##############
# TODO
