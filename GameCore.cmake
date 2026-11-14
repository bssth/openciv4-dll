# oc4_add_game_core(<target> SOURCE_DIR <dir> [EXCLUDE <file>...] [DEFINITIONS <def>...]
#                   [DEF_FILE <file>] [OUTPUT_DIR <dir>] [PUBLIC_HEADERS] [STANDARD_FOR_SCOPE])
#
# Builds a Civ4 BtS game core (the SDK's CvGameCoreDLL or a mod's derivative of it) for x64
# against Python 2.7 and the compat layer, as a DLL named CvGameCoreDLL-<arch>.dll (OC4_GAME_CORE_NAME,
# so that a mod ships it beside its 32-bit CvGameCoreDLL.dll in one Assets folder). PUBLIC_HEADERS
# exposes its include directories to dependants (the host is compiled against the vanilla
# SDK headers). DEF_FILE is a module-definition file in x64 mangling, for mods that export
# the engine's names under other functions (a mod's own .def is 32-bit). With OUTPUT_DIR the
# DLL lands there instead of the common bin folder, and oc4core.exe is copied next to it so
# that it runs with that game core. Under MSVC a for loop's variable outlives the loop, as in
# VC7.1, which mods' sources rely on; STANDARD_FOR_SCOPE is for sources that do not.
if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(ARM64|aarch64)$" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
	set(OC4_ARCH arm64)
else()
	set(OC4_ARCH x64)
endif()
set(OC4_GAME_CORE_NAME "CvGameCoreDLL-${OC4_ARCH}")

function(oc4_add_game_core target)
	cmake_parse_arguments(ARG "PUBLIC_HEADERS;STANDARD_FOR_SCOPE" "SOURCE_DIR;OUTPUT_DIR;DEF_FILE" "EXCLUDE;DEFINITIONS" ${ARGN})
	get_filename_component(src "${ARG_SOURCE_DIR}" ABSOLUTE)
	file(GLOB sources CONFIGURE_DEPENDS "${src}/*.cpp")
	foreach(f IN LISTS ARG_EXCLUDE)
		list(REMOVE_ITEM sources "${src}/${f}")
	endforeach()
	set(compat "${OC4_GAMECORE_DIR}/compat")

	add_library(${target} SHARED ${sources} "${compat}/oc4_boost_python.cpp")
	if(ARG_DEF_FILE)
		get_filename_component(def "${ARG_DEF_FILE}" ABSOLUTE)
		target_sources(${target} PRIVATE "${def}")
	endif()
	if(ARG_PUBLIC_HEADERS)
		target_include_directories(${target} PUBLIC "${src}" "${compat}")
	else()
		target_include_directories(${target} PRIVATE "${src}" "${compat}")
	endif()
	if(WIN32)
		target_link_libraries(${target} PUBLIC oc4_python27 PRIVATE winmm user32 psapi)
	else()
		target_link_libraries(${target} PUBLIC oc4_python27)
		target_include_directories(${target} PUBLIC "${compat}/posix")
		target_sources(${target} PRIVATE "${compat}/posix/oc4_posix.cpp")
		set_source_files_properties("${compat}/posix/oc4_posix.cpp" TARGET_DIRECTORY ${target} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
	endif()
	target_compile_definitions(${target}
		PUBLIC $<$<PLATFORM_ID:Windows>:WIN32> $<$<PLATFORM_ID:Windows>:_WINDOWS>
		PRIVATE _USRDLL CVGAMECOREDLL_EXPORTS _CRT_SECURE_NO_WARNINGS _CRT_NON_CONFORMING_SWPRINTFS _SCL_SECURE_NO_WARNINGS
			$<$<NOT:$<CONFIG:Debug>>:NDEBUG FINAL_RELEASE> ${ARG_DEFINITIONS})
	set_target_properties(${target} PROPERTIES CXX_STANDARD 20 CXX_SCAN_FOR_MODULES OFF FOLDER "gamecore" OUTPUT_NAME ${OC4_GAME_CORE_NAME})
	if(MSVC)
		# SDK code is VC7.1-era: lenient conformance, its own code page, quiet legacy warnings.
		target_compile_options(${target} PRIVATE /permissive /W3 /EHsc /bigobj /MP
			/source-charset:windows-1252 /execution-charset:windows-1252
			/wd4244 /wd4267 /wd4305 /wd4800 /wd4996 /wd4251 /wd4018 /wd4805 /wd4838)
		if(NOT ARG_STANDARD_FOR_SCOPE)
			target_compile_options(${target} PRIVATE /Zc:forScope-)
		endif()
	endif()
	if(NOT MSVC)
		# VC7.1-era code: Microsoft extensions, and the SDK's Windows-1252 sources as raw bytes.
		# Non-POD varargs stays an error: clang compiles it to a trap (brk), not MSVC's silent pass-through.
		# -fsigned-char: the SDK stores enums in plain char fields (CvPlot::m_eOwner and others) with -1 for
		# none, as MSVC's char is signed. ARM ELF targets (Linux on ARM64, the Switch) make plain char unsigned.
		target_compile_options(${target} PRIVATE -fms-extensions -ferror-limit=0 -Wno-everything -ffp-contract=off -Werror=non-pod-varargs -fsigned-char)
	endif()
	target_precompile_headers(${target} PRIVATE "${src}/CvGameCoreDLL.h")
	set_source_files_properties("${compat}/oc4_boost_python.cpp" TARGET_DIRECTORY ${target} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

	if(ARG_OUTPUT_DIR)
		# (a generator expression keeps multi-config generators from adding a per-config folder)
		set_target_properties(${target} PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIR}$<0:>" LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIR}$<0:>" ARCHIVE_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIR}$<0:>")
		set_property(GLOBAL APPEND PROPERTY OC4_GAME_CORE_DIRS "${ARG_OUTPUT_DIR}")
	endif()
endfunction()
