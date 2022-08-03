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

add_definitions(-DQT5DWAYLANDPLUGIN_LIBRARY)

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
add_definitions(-DQT_DEPRECATED_WARNINGS)

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x060000)    # disables all the APIs deprecated before Qt 6.0.0

if(NOT VERSION)
    execute_process(COMMAND git describe --tags --abbrev=0 OUTPUT_VARIABLE TAG_VERSION)
    string(REGEX REPLACE "[^0-9.]" "" VERSION ${TAG_VERSION})
    if (NOT VERSION)
        set(VERSION 1.1.11)
    endif()
endif()

try_compile(DEEPIN_KWIN_TEST_COMPILE_RESULT ${CMAKE_CURRENT_BINARY_DIR}/deepin-kwin-test
    ${CMAKE_CURRENT_LIST_DIR}/config.tests/deepin-kwin-test deepin-kwin-test
)
if(DEEPIN_KWIN_TEST_COMPILE_RESULT)
    add_definitions(-DD_DEEPIN_KWIN)
endif()

try_compile(WAYLAND_TEST_COMPILE_RESULT ${CMAKE_CURRENT_BINARY_DIR}/wayland_test
    ${CMAKE_CURRENT_LIST_DIR}/config.tests/wayland_test wayland_test
)
if(WAYLAND_TEST_COMPILE_RESULT)
    add_definitions(-DDTHEMED_ICON_LOOKUP)
    set(GLOBAL_HEADERS
        dwaylandshellmanager.cpp
        dkeyboard.cpp
        ../../src/global.cpp
        ../../src/vtablehook.cpp
    )
    set(GLOBAL_SOURCES
        dwaylandshellmanager.cpp
        dkeyboard.cpp
        ../../src/global.cpp
        ../../src/vtablehook.cpp
    )
else()
    message(WARNING "QtWayland version incompatible")
endif()

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/plugins/wayland-shell-integration)
add_library(${PROJECT_NAME} MODULE main.cpp ${GLOBAL_SOURCES} ${GLOBAL_HEADERS})

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
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${Qt5Widgets_PRIVATE_INCLUDE_DIRS}
    )
else()
    find_package(Qt5 REQUIRED PlatformSupport)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${COMMON_LIBS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
    ${Qt5WaylandClient_PRIVATE_INCLUDE_DIRS}
    ${Qt5XkbCommonSupport_PRIVATE_INCLUDE_DIRS}
    ../../src
)

if (NOT INSTALL_PATH)
    set(INSTALL_PATH ${CMAKE_INSTALL_LIBDIR}/qt5/plugins/wayland-shell-integration)
endif()
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_PATH})
