# x64 build of the Civ4 BtS SDK game core (CvGameCoreDLL) against Python 2.7 + pybind11.
# Part of openciv4 (add_subdirectory), or a project of its own that builds the vanilla core and
# optionally a mod's (OC4_MOD_SOURCE_DIR), see README.md.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	cmake_minimum_required(VERSION 3.28)
	project(openciv4-dll LANGUAGES C CXX)
	if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "openciv4-dll is x64-only")
	endif()
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo,Release>:ProgramDatabase>")
	if(MSVC)
		add_link_options($<$<CONFIG:Release>:/DEBUG:FULL> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
	endif()
	option(OC4_LTO "Link-time code generation for optimised builds" ON)
	if(OC4_LTO)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
	endif()
	set(OC4_GAMECORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	include(GameCore.cmake)
	set(OC4_STANDALONE ON)
endif()

set(OC4_PYTHON27_DIR "${PROJECT_SOURCE_DIR}/external/python27" CACHE PATH "Python 2.7 x64 (include/, libs/python27.lib, python27.dll)")
# Migrate the previous default in existing build trees; preserve custom overrides.
if(OC4_PYBIND11_DIR STREQUAL "${PROJECT_SOURCE_DIR}/external/pybind11-2.9.2"
	OR OC4_PYBIND11_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/external/pybind11")
	unset(OC4_PYBIND11_DIR CACHE)
endif()
set(OC4_PYBIND11_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pybind11" CACHE PATH "Patched pybind11 2.9.2 (bundled submodule)")

if(NOT EXISTS "${OC4_PYTHON27_DIR}/include/python2.7/Python.h" AND NOT EXISTS "${OC4_PYTHON27_DIR}/include/Python.h")
	message(FATAL_ERROR "Python 2.7 not found: set OC4_PYTHON27_DIR (see README)")
endif()
if(NOT EXISTS "${OC4_PYBIND11_DIR}/include/pybind11/pybind11.h")
	message(FATAL_ERROR "pybind11 not found: run git submodule update --init --recursive from the repository root, or check OC4_PYBIND11_DIR")
endif()

add_library(oc4_python27 INTERFACE)
target_include_directories(oc4_python27 SYSTEM INTERFACE "${OC4_PYTHON27_DIR}/include" "${OC4_PYTHON27_DIR}/include/python2.7" "${OC4_PYBIND11_DIR}/include")
if(WIN32)
	target_link_libraries(oc4_python27 INTERFACE "${OC4_PYTHON27_DIR}/libs/python27.lib")
else()
	target_link_libraries(oc4_python27 INTERFACE "${OC4_PYTHON27_DIR}/lib/libpython2.7${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

# The vanilla game core; its headers are what the host is compiled against.
# CvTextScreens.cpp is not part of the original project (references a missing CvTextMgr.h).
oc4_add_game_core(CvGameCoreDLL SOURCE_DIR CvGameCoreDLL EXCLUDE CvTextScreens.cpp PUBLIC_HEADERS)

# Profile-guided optimisation of the game core, driven by tools\pgo.cmd: GEN links an
# instrumented DLL that counts what AI autoplay runs through, USE links the DLL optimised
# with those counts. Only the link step differs, so switching modes relinks the DLL.
set(OC4_PGO "OFF" CACHE STRING "Profile-guided optimisation of the game core: OFF, GEN or USE")
set_property(CACHE OC4_PGO PROPERTY STRINGS OFF GEN USE)
set(OC4_PGO_PGD "${CMAKE_BINARY_DIR}/pgo/CvGameCoreDLL.pgd" CACHE FILEPATH "Profile database for OC4_PGO")
if(MSVC AND NOT OC4_PGO STREQUAL "OFF")
	if(NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE)
		message(FATAL_ERROR "OC4_PGO needs link-time code generation (OC4_LTO)")
	endif()
	get_filename_component(OC4_PGO_DIR "${OC4_PGO_PGD}" DIRECTORY)
	file(MAKE_DIRECTORY "${OC4_PGO_DIR}")
	if(OC4_PGO STREQUAL "GEN")
		target_link_options(CvGameCoreDLL PRIVATE "$<$<CONFIG:Release>:/GENPROFILE:PGD=${OC4_PGO_PGD}>")
	elseif(OC4_PGO STREQUAL "USE")
		if(NOT EXISTS "${OC4_PGO_PGD}")
			message(FATAL_ERROR "No profile at ${OC4_PGO_PGD}: run tools\\pgo.cmd")
		endif()
		target_link_options(CvGameCoreDLL PRIVATE "$<$<CONFIG:Release>:/USEPROFILE:PGD=${OC4_PGO_PGD}>")
		set_property(TARGET CvGameCoreDLL APPEND PROPERTY LINK_DEPENDS "${OC4_PGO_PGD}")
	else()
		message(FATAL_ERROR "OC4_PGO must be OFF, GEN or USE")
	endif()
endif()

# A mod's game core next to the vanilla one, into build/<dir>/mod/CvGameCoreDLL.dll.
if(OC4_STANDALONE)
	set(OC4_MOD_SOURCE_DIR "" CACHE PATH "A mod's CvGameCoreDLL sources to build for x64 too")
	set(OC4_MOD_DEFINITIONS "" CACHE STRING "Its preprocessor definitions (its Makefile's /D flags), ;-separated")
	set(OC4_MOD_DEF_FILE "" CACHE FILEPATH "Its module-definition file in x64 mangling, if it has a .def")
	set(OC4_MOD_EXCLUDE "CvTextScreens.cpp" CACHE STRING "Its .cpp files that are not part of its project, ;-separated")
	if(OC4_MOD_SOURCE_DIR)
		set(def_args)
		if(OC4_MOD_DEF_FILE)
			set(def_args DEF_FILE "${OC4_MOD_DEF_FILE}")
		endif()
		oc4_add_game_core(CvGameCoreDLL_mod SOURCE_DIR "${OC4_MOD_SOURCE_DIR}" EXCLUDE ${OC4_MOD_EXCLUDE}
			DEFINITIONS ${OC4_MOD_DEFINITIONS} ${def_args} OUTPUT_DIR "${CMAKE_BINARY_DIR}/mod")
	endif()
endif()
