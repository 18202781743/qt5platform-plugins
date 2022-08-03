cmake_minimum_required(VERSION 3.14)

set(VERSION 4.0)

project(qt5platform-plugins)

add_subdirectory(xcb)
add_subdirectory(wayland)
