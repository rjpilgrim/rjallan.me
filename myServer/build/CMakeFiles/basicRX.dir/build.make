# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/pilgrim/myServer

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/pilgrim/myServer/build

# Include any dependencies generated for this target.
include CMakeFiles/basicRX.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/basicRX.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/basicRX.dir/flags.make

CMakeFiles/basicRX.dir/basicRX.cpp.o: CMakeFiles/basicRX.dir/flags.make
CMakeFiles/basicRX.dir/basicRX.cpp.o: ../basicRX.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/pilgrim/myServer/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/basicRX.dir/basicRX.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/basicRX.dir/basicRX.cpp.o -c /home/pilgrim/myServer/basicRX.cpp

CMakeFiles/basicRX.dir/basicRX.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/basicRX.dir/basicRX.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/pilgrim/myServer/basicRX.cpp > CMakeFiles/basicRX.dir/basicRX.cpp.i

CMakeFiles/basicRX.dir/basicRX.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/basicRX.dir/basicRX.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/pilgrim/myServer/basicRX.cpp -o CMakeFiles/basicRX.dir/basicRX.cpp.s

# Object files for target basicRX
basicRX_OBJECTS = \
"CMakeFiles/basicRX.dir/basicRX.cpp.o"

# External object files for target basicRX
basicRX_EXTERNAL_OBJECTS =

bin/basicRX: CMakeFiles/basicRX.dir/basicRX.cpp.o
bin/basicRX: CMakeFiles/basicRX.dir/build.make
bin/basicRX: /usr/local/lib/libLimeSuite.so
bin/basicRX: CMakeFiles/basicRX.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/pilgrim/myServer/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable bin/basicRX"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/basicRX.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/basicRX.dir/build: bin/basicRX

.PHONY : CMakeFiles/basicRX.dir/build

CMakeFiles/basicRX.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/basicRX.dir/cmake_clean.cmake
.PHONY : CMakeFiles/basicRX.dir/clean

CMakeFiles/basicRX.dir/depend:
	cd /home/pilgrim/myServer/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/pilgrim/myServer /home/pilgrim/myServer /home/pilgrim/myServer/build /home/pilgrim/myServer/build /home/pilgrim/myServer/build/CMakeFiles/basicRX.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/basicRX.dir/depend

