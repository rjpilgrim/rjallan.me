cmake_minimum_required(VERSION 3.12)

project(spectrifyIcecast)

list(APPEND CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")

FIND_PACKAGE(JPEG REQUIRED)

set(CMAKE_CXX_STANDARD 17)


message( STATUS "CMAKE MODULE PATH: ${CMAKE_MODULE_PATH}" )
message( STATUS "CMAKE MODULE PATH: ${LIBUSB_INCLUDE_DIR}" )


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Simple-WebSocket-Server)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../fftw-3.3.9)

add_executable(main src/main.cpp
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(main PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR}/../fftw-3.3.9/api ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../Simple-WebSocket-Server ${CMAKE_CURRENT_SOURCE_DIR}/../json/single_include)
target_link_libraries(main  simple-websocket-server fftw3 JPEG::JPEG)
