find_package(PkgConfig REQUIRED)

pkg_check_modules(XCB REQUIRED
    x11-xcb xi xcb-renderutil sm ice xcb-render dbus-1 xcb
    xcb-image xcb-icccm xcb-sync xcb-xfixes xcb-shm xcb-randr
    xcb-shape xcb-keysyms xcb-xkb xcb-composite xkbcommon-x11
    xcb-damage mtdev egl
)

# Don't link cairo library
execute_process(COMMAND pkg-config --cflags-only-I cairo OUTPUT_VARIABLE CAIRO_INCLUDE_DIRS)
string(STRIP ${CAIRO_INCLUDE_DIRS} CAIRO_INCLUDE_DIRS)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CAIRO_INCLUDE_DIRS}")

if (${Qt5_VERSION_MINOR} GREATER 5)
    pkg_check_modules(XCB-XINERAMA REQUIRED xcb-xinerama)
endif()

if (${Qt5_VERSION_MINOR} GREATER 4)
    find_package(Qt5 REQUIRED XcbQpa)
endif()

list(APPEND HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/windoweventhook.h
    ${CMAKE_CURRENT_LIST_DIR}/xcbnativeeventfilter.h
    ${CMAKE_CURRENT_LIST_DIR}/dxcbwmsupport.h
)
list(APPEND SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/windoweventhook.cpp
    ${CMAKE_CURRENT_LIST_DIR}/xcbnativeeventfilter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/utility_x11.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dxcbwmsupport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/dforeignplatformwindow_x11.cpp
)

if (NOT xcb-xlib)
    add_definitions(-DXCB_USE_XLIB)
    list(APPEND XCB_USE xcb_xlib)
    if (${Qt5_VERSION_MINOR} GREATER 14)
        add_definitions(-DXCB_USE_XINPUT2 -DXCB_USE_XINPUT21 -DXCB_USE_XINPUT22)
        list(APPEND XCB_USE xcb_xinput)
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
endif()

add_definitions(-DXCB_USE_XLIB)

if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/libqt5xcbqpa-dev/${Qt5_VERSION})
    include_directories(${CMAKE_CURRENT_LIST_DIR}/libqt5xcbqpa-dev/${Qt5_VERSION})
else()
    message(FATAL_ERROR "Not support Qt Version: ${Qt5_VERSION}")
endif()

include_directories(${CMAKE_CURRENT_LIST_DIR}/../xcb)
include(${CMAKE_SOURCE_DIR}/src/src.cmake)

