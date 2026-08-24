cmake_minimum_required(VERSION 3.20)
project(multilife-qt-client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network Test)

include(FetchContent)
if(NOT TARGET GTest::gtest_main)
    set(GTEST_LOCAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../build/_deps/googletest-src")
    if(EXISTS "${GTEST_LOCAL_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(
            "${GTEST_LOCAL_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/googletest-local-build"
            EXCLUDE_FROM_ALL
        )
    else()
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
            GIT_SHALLOW TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
endif()

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/GameWindow.cpp
    src/InfiniteGridView.cpp
    src/WorldModel.cpp
    src/GameNetworkClient.cpp
    src/ClientRuntime.cpp
    src/ClientParsers.cpp
)

set(HEADERS
    include/GameWindow.h
    include/InfiniteGridView.h
    include/ClientProtocol.h
    include/ClientParsers.h
    include/WorldModel.h
    include/GameNetworkClient.h
    include/ClientRuntime.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
)


if(WIN32 AND MINGW)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

enable_testing()
if(BUILD_TESTING)
    add_executable(multilife-qt-client-tests
        tests/test_world_model.cpp
        include/WorldModel.h
        src/WorldModel.cpp
    )

    target_include_directories(multilife-qt-client-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(multilife-qt-client-tests
        Qt6::Core
        GTest::gtest_main
    )

    add_executable(multilife-qt-client-network-tests
        tests/test_client_parsers.cpp
        include/ClientParsers.h
        src/ClientParsers.cpp
    )

    target_include_directories(multilife-qt-client-network-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(multilife-qt-client-network-tests
        Qt6::Core
        GTest::gtest_main
    )

    add_executable(multilife-qt-client-threading-tests
        tests/test_network_threading_qt.cpp
        include/GameNetworkClient.h
        include/ClientParsers.h
        include/ClientProtocol.h
        src/GameNetworkClient.cpp
        src/ClientParsers.cpp
    )

    target_include_directories(multilife-qt-client-threading-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(multilife-qt-client-threading-tests
        Qt6::Core
        Qt6::Network
        Qt6::Test
    )

    add_executable(multilife-qt-client-ui-tests
        tests/test_game_window_ui_qt.cpp
        include/GameWindow.h
        include/InfiniteGridView.h
        include/WorldModel.h
        include/GameNetworkClient.h
        include/ClientRuntime.h
        src/GameWindow.cpp
        src/InfiniteGridView.cpp
        src/WorldModel.cpp
        src/GameNetworkClient.cpp
        src/ClientRuntime.cpp
        src/ClientParsers.cpp
    )

    target_include_directories(multilife-qt-client-ui-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(multilife-qt-client-ui-tests
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
        Qt6::Network
        Qt6::Test
    )

    include(GoogleTest)
    gtest_discover_tests(multilife-qt-client-tests)
    gtest_discover_tests(multilife-qt-client-network-tests)
    add_test(NAME multilife-qt-client-threading-tests COMMAND multilife-qt-client-threading-tests)
    add_test(NAME multilife-qt-client-ui-tests COMMAND multilife-qt-client-ui-tests)
endif()
