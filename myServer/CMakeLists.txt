cmake_minimum_required(VERSION 3.12)

project(rjallanDOTme)

find_package(Boost 1.54.0 COMPONENTS program_options REQUIRED)

list(APPEND CMAKE_MODULE_PATH "/usr/local/share/cmake/Modules")

#FIND_PACKAGE(LimeSuite REQUIRED)
if(UNIX AND NOT APPLE)
	FIND_PACKAGE(ALSA REQUIRED)
	set(ALSASTREAMER_SRC src/AlsaStreamer.cpp)
	set(ALSASTREAMER_LIBNAME  ALSA::ALSA)
endif()

if(APPLE)
	set(LIBUSB_INCLUDE_DIR /usr/local/Cellar/libusb/1.0.24/include/libusb-1.0)
	set(LIBUSB_LIBRARIES /usr/local/Cellar/libusb/1.0.24/lib/libusb-1.0.a)
endif()


#FIND_PACKAGE(ZLIB REQUIRED) zlib only needed for png implementation
FIND_PACKAGE(JPEG REQUIRED)

set(CMAKE_CXX_STANDARD 17)


message( STATUS "CMAKE MODULE PATH: ${CMAKE_MODULE_PATH}" )
message( STATUS "CMAKE MODULE PATH: ${LIBUSB_INCLUDE_DIR}" )


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../Simple-WebSocket-Server ${CMAKE_CURRENT_BINARY_DIR}/Simple-WebSocket-Server)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../fftw-3.3.9 ${CMAKE_CURRENT_BINARY_DIR}/fftw-3.3.9)
add_subdirectory(LimeSuite)
add_subdirectory(hackrf/host/libhackrf/)

add_executable(main src/main.cpp src/IQWebSocketServer.cpp src/WAVWriter.cpp ${ALSASTREAMER_SRC} src/CommandLine.cpp src/LimeStreamer.cpp src/HackRFStreamer.cpp)
set_target_properties(main PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_include_directories(main PRIVATE hackrf/host/libhackrf/src ${CMAKE_CURRENT_SOURCE_DIR}/../fftw-3.3.9/api ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../Simple-WebSocket-Server ${CMAKE_CURRENT_SOURCE_DIR}/../json/single_include ${Boost_INCLUDE_DIR})
target_link_libraries(main hackrf simple-websocket-server LimeSuite ${ALSASTREAMER_LIBNAME} fftw3 #[[ZLIB::ZLIB]] JPEG::JPEG ${Boost_LIBRARIES})
