cmake_minimum_required(VERSION 3.16)

project(kryptic-daemon VERSION 0.1.0 LANGUAGES CXX)

add_library(kryptic-daemon
    src/json.cpp
    src/kryptic.cpp
)
add_library(kryptic::daemon ALIAS kryptic-daemon)

target_include_directories(kryptic-daemon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(kryptic-daemon PUBLIC cxx_std_17)

if(WIN32)
    target_sources(kryptic-daemon PRIVATE src/transport_windows.cpp)
else()
    target_sources(kryptic-daemon PRIVATE src/transport_unix.cpp)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(KRYPTIC_DAEMON_BUILD_TESTS "Build kryptic-daemon tests" ON)
else()
    option(KRYPTIC_DAEMON_BUILD_TESTS "Build kryptic-daemon tests" OFF)
endif()

if(KRYPTIC_DAEMON_BUILD_TESTS)
    enable_testing()
    add_executable(kryptic-daemon-tests tests/kryptic_test.cpp)
    target_link_libraries(kryptic-daemon-tests PRIVATE kryptic-daemon)
    target_compile_features(kryptic-daemon-tests PRIVATE cxx_std_17)
    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(kryptic-daemon-tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME kryptic-daemon-tests COMMAND kryptic-daemon-tests)
endif()

include(GNUInstallDirs)
install(TARGETS kryptic-daemon
    EXPORT kryptic-daemonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT kryptic-daemonTargets
    FILE kryptic-daemonTargets.cmake
    NAMESPACE kryptic::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kryptic-daemon
)
