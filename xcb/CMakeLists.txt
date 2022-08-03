PROJECT(dxcb)
CMAKE_MINIMUM_REQUIRED(VERSION 3.14)

set(REQUIRED_QT_VERSION 5.15.0)
find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core DBus X11Extras)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(AUTOMOC_COMPILER_PREDEFINES ON)

# NOTE(sbw): 禁止语法树上的 vrp 优化，-O2/-O3 默认开启，会导致测试虚析构函数 HOOK 失败
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXXFLAGS_RELEASE} -fno-tree-vrp")

add_definitions(-DXDG_ICON_VERSION_MAR=${qt5xdgiconloader_VERSION_MAJOR})
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT=True)
endif()

if(NOT VERSION)
    set(VERSION $$system(git describe --tags --abbrev=0))
    string(REGEX REPLACE "[^0-9.]" "" VERSION ${VERSION})
    if (NOT VERSION)
        set(VERSION 1.1.11)
    endif()
endif()

add_definitions(-DDXCB_VERSION=\"${VERSION}\")

set(HEADERS
    dplatformintegration.h
    utility.h
    dplatformnativeinterfacehook.h
    dforeignplatformwindow.h
    dwmsupport.h
    dhighdpi.h
    dnotitlebarwindowhelper.h
    3rdparty/dsimple.h
    3rdparty/clientwin.h
    dplatforminputcontexthook.h
)
set(SOURCES
    dplatformintegration.cpp
    dplatformnativeinterfacehook.cpp
    dhighdpi.cpp
    dnotitlebarwindowhelper.cpp
    3rdparty/dsimple.c
    3rdparty/clientwin.c
    dplatforminputcontexthook.cpp
)
set(RCC_QRCS
    cursors/cursor.qrc
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DQT_NO_DEBUG_OUTPUT -DUSE_NEW_IMPLEMENTING)
    set(DEFINED_DUSE_NEW_IMPLEMENTING True)
else()
    add_definitions(-DUSE_NEW_IMPLEMENTING)
    set(DEFINED_DUSE_NEW_IMPLEMENTING True)
endif()

if (DEFINED DEFINED_DUSE_NEW_IMPLEMENTING)
    list(APPEND HEADERS
        dframewindow.h
        dplatformwindowhelper.h
        dplatformbackingstorehelper.h
        dplatformopenglcontexthelper.h
    )
    list(APPEND SOURCES
        dframewindow.cpp
        dplatformwindowhelper.cpp
        dplatformbackingstorehelper.cpp
        dplatformopenglcontexthelper.cpp
    )
else()
    list(APPEND HEADERS
        dplatformbackingstore.h
        dplatformwindowhook.h
    )
    list(APPEND SOURCES
        dplatformbackingstore.cpp
        dplatformwindowhook.cpp
    )
endif()


qt5_add_dbus_interface(DBUS_INTERFACE_XMLS ../misc/com.deepin.im.xml im_interface)
qt5_add_dbus_interface(DBUS_INTERFACE_XMLS ../misc/org.freedesktop.DBus.xml dbus_interface)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin/plugins/platforms)
add_library(${PROJECT_NAME} SHARED main.cpp ${SOURCES} ${HEADERS} ${RCC_QRCS} ${DBUS_INTERFACE_XMLS})

include(linux.cmake)

set(COMMON_LIBS
    Qt5::Core
    Qt5::X11Extras
)

target_link_libraries(${PROJECT_NAME} PUBLIC ${COMMON_LIBS})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}
    ${Qt5Core_PRIVATE_INCLUDE_DIRS}
    Qt5::X11Extras
    ../src
)

if (${Qt5_VERSION_MAJOR} GREATER 4)
    # Qt >= 5.8
    if (${Qt5_VERSION_MINOR} GREATER 7)
        find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Gui)
        target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Gui)
        target_include_directories(${PROJECT_NAME} PUBLIC
            ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        )
    else()
        find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS PlatformSupport)
        target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::PlatformSupport)
        target_include_directories(${PROJECT_NAME} PUBLIC
            ${Qt5PlatformSupport_INCLUDE_DIRS}
        )
    endif()

    # Qt >= 5.10
    if (${Qt5_VERSION_MINOR} GREATER 9)
        find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS EdidSupport)
        target_link_libraries(${PROJECT_NAME} PUBLIC ${Qt5EdidSupport_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PUBLIC
            ${Qt5EdidSupport_PRIVATE_INCLUDE_DIRS}
        )
    endif()

    # Qt >= 5.13
    if (${Qt5_VERSION_MINOR} GREATER 12)
        find_package(Qt5 ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS XkbCommonSupport)
        target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::XkbCommonSupport)
        target_include_directories(${PROJECT_NAME} PUBLIC
            ${Qt5XkbCommonSupport_INCLUDE_DIRS}
        )
    endif()
endif()

if (NOT INSTALL_PATH)
    set(INSTALL_PATH ${CMAKE_INSTALL_LIBDIR}/qt5/plugins/platforms)
endif()
install(TARGETS ${PROJECT_NAME} DESTINATION ${INSTALL_PATH})
