# Allow to easily add flags for C and C++
include(CheckCXXCompilerFlag)
include(CheckCCompilerFlag)

function(add_c_compiler_flag)
	foreach(f ${ARGN})
		CHECK_C_COMPILER_FLAG(${f} FLAG_IS_SUPPORTED)
		if(FLAG_IS_SUPPORTED)
			string(APPEND CMAKE_C_FLAGS " ${f}")
		endif()
	endforeach()
	set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
endfunction()

function(add_cxx_compiler_flag)
	foreach(f ${ARGN})
		CHECK_CXX_COMPILER_FLAG(${f} FLAG_IS_SUPPORTED)
		if(FLAG_IS_SUPPORTED)
			string(APPEND CMAKE_CXX_FLAGS " ${f}")
		endif()
	endforeach()
	set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)
endfunction()

macro(add_compiler_flag)
	add_c_compiler_flag(${ARGN})
	add_cxx_compiler_flag(${ARGN})
endmacro()

macro(remove_compiler_flags)
	foreach(f ${ARGN})
		string(REGEX REPLACE "${f}( |^)" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
		string(REGEX REPLACE "${f}( |^)" "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
	endforeach()
endmacro()

# Note that CMake does not provide any facility to check that a linker flag is
# supported by the compiler, but most linker will just drop any unsupported flag
# (eventually with a warning).
function(add_linker_flag)
	foreach(f ${ARGN})
		string(APPEND CMAKE_EXE_LINKER_FLAGS " ${f}")
	endforeach()
	set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} PARENT_SCOPE)
endfunction()
