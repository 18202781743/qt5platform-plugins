PROJECT(kwayland-shell)
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

set(REQUIRED_QT_VERSION 5.15.1)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS WaylandClient XkbCommonSupport)
find_package(KF5Wayland REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(AUTOMOC_COMPILER_PREDEFINES ON)

add_definitions(-DQT5DWAYLANDPLUGIN_LIBRARY -DQT_DEPRECATED_WARNINGS)

if(NOT VERSION)
    set(VERSION $$system(git describe --tags --abbrev=0))
    string(REGEX REPLACE "[^0-9.]" "" VERSION ${VERSION})
    if (NOT VERSION)
        set(VERSION 1.1.11)
    endif()
endif()

add_definitions(-DDXCB_VERSION=\"${VERSION}\")

try_compile(WAYLAND_TEST_COMPILE_RESULT ${CMAKE_CURRENT_BINARY_DIR}/wayland_test
    ${CMAKE_CURRENT_LIST_DIR}/config.tests/wayland_test wayland_test
)
if(WAYLAND_TEST_COMPILE_RESULT)
    add_definitions(-DDTHEMED_ICON_LOOKUP)
    set(HEADERS
        dwaylandshellmanager.cpp
        dkeyboard.cpp
        ../../src/global.cpp
        ../../src/vtablehook.cpp
    )
    set(SOURCES
        dwaylandshellmanager.cpp
        dkeyboard.cpp
        ../../src/global.cpp
        ../../src/vtablehook.cpp
    )
else()
    message(WARNING "QtWayland version incompatible")
endif()

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/plugins/wayland-shell-integration)
add_library(${PROJECT_NAME} SHARED main.cpp ${SOURCES} ${HEADERS})

set(COMMON_LIBS
    Qt5::Core
    ${Qt5WaylandClient_LIBRARIES}
    KF5::WaylandClient
)

# Qt >= 5.8
if (${Qt5_VERSION_MINOR} GREATER 7)
    find_package(Qt5 REQUIRED Gui Widgets)
    target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui Qt5::Widgets)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${QtGui_PRIVATE_INCLUDE_DIRS}
        ${QtWidget_PRIVATE_INCLUDE_DIRS}
    )
else()
    find_package(Qt5 REQUIRED PlatformSupport)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${COMMON_LIBS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${Qt5Widgets_PRIVATE_INCLUDE_DIRS}
    ${Qt5WaylandClient_PRIVATE_INCLUDE_DIRS}
    ${Qt5XkbCommonSupport_PRIVATE_INCLUDE_DIRS}
    ../../src
)

if (NOT INSTALL_PATH)
    set(INSTALL_PATH ${CMAKE_INSTALL_LIBDIR}/qt5/plugins/wayland-shell-integration)
endif()
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_PATH})
