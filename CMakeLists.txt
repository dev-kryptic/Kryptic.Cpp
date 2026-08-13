cmake_minimum_required(VERSION 3.16)

project(krypticdev VERSION 0.1.0 LANGUAGES CXX)

add_library(krypticdev
    src/json.cpp
    src/krypticdev.cpp
)
add_library(krypticdev::krypticdev ALIAS krypticdev)

target_include_directories(krypticdev
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(krypticdev PUBLIC cxx_std_17)

if(WIN32)
    target_sources(krypticdev PRIVATE src/transport_windows.cpp)
else()
    target_sources(krypticdev PRIVATE src/transport_unix.cpp)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(KRYPTICDEV_BUILD_TESTS "Build krypticdev tests" ON)
else()
    option(KRYPTICDEV_BUILD_TESTS "Build krypticdev tests" OFF)
endif()

if(KRYPTICDEV_BUILD_TESTS)
    enable_testing()
    add_executable(krypticdev_tests tests/kryptic_test.cpp)
    target_link_libraries(krypticdev_tests PRIVATE krypticdev)
    target_compile_features(krypticdev_tests PRIVATE cxx_std_17)
    if(NOT WIN32)
        find_package(Threads REQUIRED)
        target_link_libraries(krypticdev_tests PRIVATE Threads::Threads)
    endif()
    add_test(NAME krypticdev_tests COMMAND krypticdev_tests)
endif()

include(GNUInstallDirs)
install(TARGETS krypticdev
    EXPORT krypticdevTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT krypticdevTargets
    FILE krypticdevTargets.cmake
    NAMESPACE krypticdev::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/krypticdev
)
