cmake_minimum_required(VERSION 3.12)

project(rjallanDOTme)

find_package(Boost 1.54.0 COMPONENTS program_options REQUIRED)

FIND_PACKAGE(LimeSuite REQUIRED)

FIND_PACKAGE(ALSA REQUIRED)

#FIND_PACKAGE(ZLIB REQUIRED) zlib only needed for png implementation
FIND_PACKAGE(JPEG REQUIRED)

set(CMAKE_CXX_STANDARD 17)

message( STATUS "ALSA LIBRRARIES: ${ALSA_LIBRARIES}" )

add_subdirectory(Simple-WebSocket-Server)

add_executable(main main.cpp IQWebSocketServer.cpp WAVWriter.cpp AlsaStreamer.cpp CommandLine.cpp IQWebSocketServer.hpp)
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(main PRIVATE  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Simple-WebSocket-Server ${CMAKE_CURRENT_SOURCE_DIR}/json/single_include ${Boost_INCLUDE_DIR})
target_link_libraries(main simple-websocket-server LimeSuite ALSA::ALSA fftw3 #[[ZLIB::ZLIB]] JPEG::JPEG ${Boost_LIBRARIES})
