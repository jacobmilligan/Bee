# CMAKE generated file: DO NOT EDIT!
# Generated by "NMake Makefiles" Generator, CMake Version 3.14

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE
NULL=nul
!ENDIF
SHELL = cmd.exe

# The CMake executable.
CMAKE_COMMAND = C:\Users\jacob\AppData\Local\JetBrains\Toolbox\apps\CLion\ch-0\191.7479.33\bin\cmake\win\bin\cmake.exe

# The command to remove a file.
RM = C:\Users\jacob\AppData\Local\JetBrains\Toolbox\apps\CLion\ch-0\191.7479.33\bin\cmake\win\bin\cmake.exe -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = C:\Dev\Bee

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = C:\Dev\Bee\Build\Debug

# Include any dependencies generated for this target.
include Source\Bee\CMakeFiles\Bee.TestMain.dir\depend.make

# Include the progress variables for this target.
include Source\Bee\CMakeFiles\Bee.TestMain.dir\progress.make

# Include the compile flags for this target's objects.
include Source\Bee\CMakeFiles\Bee.TestMain.dir\flags.make

Source\Bee\CMakeFiles\Bee.TestMain.dir\TestMain.cpp.obj: Source\Bee\CMakeFiles\Bee.TestMain.dir\flags.make
Source\Bee\CMakeFiles\Bee.TestMain.dir\TestMain.cpp.obj: ..\..\Source\Bee\TestMain.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=C:\Dev\Bee\Build\Debug\CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object Source/Bee/CMakeFiles/Bee.TestMain.dir/TestMain.cpp.obj"
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	C:\PROGRA~2\MICROS~1\2017\COMMUN~1\VC\Tools\MSVC\1416~1.270\bin\Hostx64\x64\cl.exe @<<
 /nologo /TP $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) /FoCMakeFiles\Bee.TestMain.dir\TestMain.cpp.obj /FdCMakeFiles\Bee.TestMain.dir\Bee.TestMain.pdb /FS -c C:\Dev\Bee\Source\Bee\TestMain.cpp
<<
	cd C:\Dev\Bee\Build\Debug

Source\Bee\CMakeFiles\Bee.TestMain.dir\TestMain.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/Bee.TestMain.dir/TestMain.cpp.i"
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	C:\PROGRA~2\MICROS~1\2017\COMMUN~1\VC\Tools\MSVC\1416~1.270\bin\Hostx64\x64\cl.exe > CMakeFiles\Bee.TestMain.dir\TestMain.cpp.i @<<
 /nologo /TP $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E C:\Dev\Bee\Source\Bee\TestMain.cpp
<<
	cd C:\Dev\Bee\Build\Debug

Source\Bee\CMakeFiles\Bee.TestMain.dir\TestMain.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/Bee.TestMain.dir/TestMain.cpp.s"
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	C:\PROGRA~2\MICROS~1\2017\COMMUN~1\VC\Tools\MSVC\1416~1.270\bin\Hostx64\x64\cl.exe @<<
 /nologo /TP $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) /FoNUL /FAs /FaCMakeFiles\Bee.TestMain.dir\TestMain.cpp.s /c C:\Dev\Bee\Source\Bee\TestMain.cpp
<<
	cd C:\Dev\Bee\Build\Debug

# Object files for target Bee.TestMain
Bee_TestMain_OBJECTS = \
"CMakeFiles\Bee.TestMain.dir\TestMain.cpp.obj"

# External object files for target Bee.TestMain
Bee_TestMain_EXTERNAL_OBJECTS =

Source\Bee\Bee.TestMain.lib: Source\Bee\CMakeFiles\Bee.TestMain.dir\TestMain.cpp.obj
Source\Bee\Bee.TestMain.lib: Source\Bee\CMakeFiles\Bee.TestMain.dir\build.make
Source\Bee\Bee.TestMain.lib: Source\Bee\CMakeFiles\Bee.TestMain.dir\objects1.rsp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=C:\Dev\Bee\Build\Debug\CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library Bee.TestMain.lib"
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	$(CMAKE_COMMAND) -P CMakeFiles\Bee.TestMain.dir\cmake_clean_target.cmake
	cd C:\Dev\Bee\Build\Debug
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	C:\PROGRA~2\MICROS~1\2017\COMMUN~1\VC\Tools\MSVC\1416~1.270\bin\Hostx64\x64\link.exe /lib /nologo /machine:x64 /out:Bee.TestMain.lib @CMakeFiles\Bee.TestMain.dir\objects1.rsp 
	cd C:\Dev\Bee\Build\Debug

# Rule to build all files generated by this target.
Source\Bee\CMakeFiles\Bee.TestMain.dir\build: Source\Bee\Bee.TestMain.lib

.PHONY : Source\Bee\CMakeFiles\Bee.TestMain.dir\build

Source\Bee\CMakeFiles\Bee.TestMain.dir\clean:
	cd C:\Dev\Bee\Build\Debug\Source\Bee
	$(CMAKE_COMMAND) -P CMakeFiles\Bee.TestMain.dir\cmake_clean.cmake
	cd C:\Dev\Bee\Build\Debug
.PHONY : Source\Bee\CMakeFiles\Bee.TestMain.dir\clean

Source\Bee\CMakeFiles\Bee.TestMain.dir\depend:
	$(CMAKE_COMMAND) -E cmake_depends "NMake Makefiles" C:\Dev\Bee C:\Dev\Bee\Source\Bee C:\Dev\Bee\Build\Debug C:\Dev\Bee\Build\Debug\Source\Bee C:\Dev\Bee\Build\Debug\Source\Bee\CMakeFiles\Bee.TestMain.dir\DependInfo.cmake --color=$(COLOR)
.PHONY : Source\Bee\CMakeFiles\Bee.TestMain.dir\depend

