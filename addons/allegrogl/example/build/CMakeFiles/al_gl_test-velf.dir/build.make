# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
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
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ammeir/dev/allegro_vita/addons/allegrogl/example

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build

# Utility rule file for al_gl_test-velf.

# Include any custom commands dependencies for this target.
include CMakeFiles/al_gl_test-velf.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/al_gl_test-velf.dir/progress.make

CMakeFiles/al_gl_test-velf: al_gl_test
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/ammeir/dev/allegro_vita/addons/allegrogl/example/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Converting to Sony ELF al_gl_test.velf"
	/usr/local/vitasdk/bin/vita-elf-create /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build/al_gl_test /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build/al_gl_test.velf

al_gl_test-velf: CMakeFiles/al_gl_test-velf
al_gl_test-velf: CMakeFiles/al_gl_test-velf.dir/build.make
.PHONY : al_gl_test-velf

# Rule to build all files generated by this target.
CMakeFiles/al_gl_test-velf.dir/build: al_gl_test-velf
.PHONY : CMakeFiles/al_gl_test-velf.dir/build

CMakeFiles/al_gl_test-velf.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/al_gl_test-velf.dir/cmake_clean.cmake
.PHONY : CMakeFiles/al_gl_test-velf.dir/clean

CMakeFiles/al_gl_test-velf.dir/depend:
	cd /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ammeir/dev/allegro_vita/addons/allegrogl/example /home/ammeir/dev/allegro_vita/addons/allegrogl/example /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build /home/ammeir/dev/allegro_vita/addons/allegrogl/example/build/CMakeFiles/al_gl_test-velf.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/al_gl_test-velf.dir/depend

