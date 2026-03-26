cmake_minimum_required(VERSION 3.16)
project(multilife-server
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "MultiLife server (C++20, Boost, GTest)"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Suppress the removed FindBoost module warning on CMake 4.x
cmake_policy(SET CMP0167 NEW)

if(WIN32)
  add_compile_definitions(_WIN32_WINNT=0x0A00 NOMINMAX UNICODE _UNICODE)
endif()

# Boost
find_package(Boost 1.70 REQUIRED CONFIG COMPONENTS headers)
message(STATUS "Boost found: ${Boost_VERSION}")

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
  GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


# Server executable
add_executable(${PROJECT_NAME}
  src/main.cpp
  src/ThreadPool.cpp
  src/TickScheduler.cpp
  src/ResourceManager.cpp
  src/Chunk.cpp
  src/World.cpp
  src/GameServer.cpp
  src/BoostNetworkManager.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)


if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::headers
    ws2_32
    mswsock
  )
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::headers
  )
endif()


# Unit tests
enable_testing()

add_executable(${PROJECT_NAME}_tests
  tests/test_chunk.cpp
  tests/test_world.cpp
  tests/test_resource_manager.cpp
  tests/test_thread_safe_queue.cpp
  tests/test_thread_pool.cpp
  tests/test_game_server.cpp
  tests/test_network_manager.cpp
  src/ThreadPool.cpp
  src/TickScheduler.cpp
  src/ResourceManager.cpp
  src/Chunk.cpp
  src/World.cpp
  src/GameServer.cpp
  src/BoostNetworkManager.cpp
)

target_include_directories(${PROJECT_NAME}_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)


if(WIN32)
  target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    GTest::gtest_main
    Boost::headers
    ws2_32
    mswsock
  )
else()
  target_link_libraries(${PROJECT_NAME}_tests PRIVATE
    GTest::gtest_main
    Boost::headers
  )
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)