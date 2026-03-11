cmake_minimum_required(VERSION 3.16)
project(multilife-server
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "MultiLife server (C++20, Boost, GTest)"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_definitions(-D_WIN32_WINNT=0x0A00)
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE -D_UNICODE)
endif()

# Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.70 REQUIRED)

if(Boost_FOUND)
  message(STATUS "Boost found: ${Boost_VERSION}")
  message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

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
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  Boost::headers
  # Link specific Boost libs when needed, e.g.:
  # Boost::system
  # Boost::filesystem
  # Boost::asio
)


# Unit tests
enable_testing()

add_executable(${PROJECT_NAME}_tests
  tests/example_test.cpp
  # Add more test sources here
)

target_include_directories(${PROJECT_NAME}_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}_tests PRIVATE
  GTest::gtest_main
  Boost::headers
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME}_tests)
