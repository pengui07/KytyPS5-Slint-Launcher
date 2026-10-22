# Thin Slint launcher for NHL 27, built on its own (split out of kytyPS5 src/slint_launcher).
# Slint is fetched and built from source (needs Rust >= 1.92).

cmake_minimum_required(VERSION 3.21...4.3)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.10 CACHE STRING "Minimum policy version for bundled dependencies")
endif()

get_property(_kyty_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _kyty_multi_config AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()

project(kyty_slint_launcher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Same check as the emulator's top-level CMakeLists.
set(KYTY_CLANG_CL FALSE)
get_filename_component(_kyty_cxx_name "${CMAKE_CXX_COMPILER}" NAME_WE)
string(TOLOWER "${_kyty_cxx_name}" _kyty_cxx_name)
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND _kyty_cxx_name STREQUAL "clang-cl")
	set(KYTY_CLANG_CL TRUE)
endif()

include(FetchContent)

# On Windows the launcher is one exe with no dependency on the VC++ Redistributable: both the
# C++ code (/MT, set on the target below) and the Rust runtime (crt-static, set after Slint is
# fetched) use the static CRT. Turn KYTY_SLINT_STATIC_CRT off to link the dynamic CRT instead.
if(WIN32)
	set(_kyty_static_crt_default ON)
else()
	set(_kyty_static_crt_default OFF)
endif()
option(KYTY_SLINT_STATIC_CRT "Link the Slint launcher with the static VC++ runtime" ${_kyty_static_crt_default})
if((KYTY_CLANG_CL OR MSVC) AND NOT KYTY_SLINT_STATIC_CRT)
	foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE)
		string(REGEX REPLACE "/MT" "/MD" ${flag_var} "${${flag_var}}")
	endforeach()
endif()

# One exe, no slint_cpp.dll; the smallest feature set a desktop window needs.
set(BUILD_SHARED_LIBS OFF)
set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_SYSTEM_TRAY OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_ACCESSIBILITY OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SKIA OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_FEMTOVG OFF CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
set(SLINT_STYLE "fluent-dark" CACHE STRING "" FORCE)

FetchContent_Declare(
	Slint
	GIT_REPOSITORY https://github.com/slint-ui/slint.git
	GIT_TAG v1.18.1
	GIT_SHALLOW TRUE
	SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)
# Popup menus shaped like the shadPS4 launcher's. Slint has no per-app menu styling, so the fluent
# style's menu file is replaced in the fetched source; slint-compiler embeds the styles and
# rebuilds when it changes. Copied only when it differs.
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/slint-style/fluent-menu.slint"
	"${slint_SOURCE_DIR}/internal/compiler/widgets/fluent/menu.slint" COPYONLY)
if(WIN32 AND KYTY_SLINT_STATIC_CRT)
	# Rust side of the static CRT (the C++ side is the target property further down).
	corrosion_add_target_rustflags(slint_cpp "-Ctarget-feature=+crt-static")
endif()

if(WIN32)
	# The exe's own icon (Explorer, taskbar pin); the windows get theirs from the .slint files.
	enable_language(RC)
endif()
add_executable(kyty_slint_launcher WIN32
	src/main.cpp
	src/settings.cpp
	src/settings.h
	src/paths.cpp
	src/paths.h
	src/launch.cpp
	src/net.cpp
	src/net.h
	src/platform.cpp
	src/platform.h
	src/launch.h
	src/library.cpp
	src/library.h
	# STEP6:6f: the Controls page (bindings, capture, host_input_mapping)
	src/input_mapping.cpp
	src/input_mapping.h
	src/versions.cpp
	src/versions.h
	src/version_manager.cpp
	src/trophies.cpp
	src/secret_store.cpp
	src/secret_store.h
	src/qt_ini.cpp
	src/qt_ini.h
	src/qt_import.cpp
	src/qt_import.h
	src/version_manager.h
	$<$<BOOL:${WIN32}>:src/app.rc>
)
slint_target_sources(kyty_slint_launcher ui/app.slint)
target_link_libraries(kyty_slint_launcher PRIVATE Slint::Slint)
target_include_directories(kyty_slint_launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(NOT WIN32)
	# Downloads go through libcurl here (WinHTTP on Windows); stb_image decodes the game art.
	find_package(CURL REQUIRED)
	target_link_libraries(kyty_slint_launcher PRIVATE CURL::libcurl ${CMAKE_DL_LIBS})
	target_include_directories(kyty_slint_launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/stb")
endif()
if(APPLE)
	# The monitor size (platform.cpp).
	target_link_libraries(kyty_slint_launcher PRIVATE "-framework CoreGraphics")
endif()
if(UNIX AND NOT APPLE)
	# The static Slint runtime finds system fonts through fontconfig (libfontconfig1-dev).
	# Added to the Slint target itself so it is linked after libslint_cpp.a (static libraries are
	# resolved in order).
	find_library(KYTY_FONTCONFIG_LIB fontconfig REQUIRED)
	cmake_policy(SET CMP0079 NEW)
	target_link_libraries(Slint INTERFACE ${KYTY_FONTCONFIG_LIB})
endif()
if(WIN32)
	if(KYTY_SLINT_STATIC_CRT)
		# clang-cl defaults to the dynamic runtime (-MD); ask for the static one (-MT).
		set_property(TARGET kyty_slint_launcher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
	endif()
	target_compile_definitions(kyty_slint_launcher PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
	# imm32 and uxtheme are needed by winit when slint_cpp is static.
	target_link_libraries(kyty_slint_launcher PRIVATE user32 shell32 ole32 imm32 uxtheme winhttp windowscodecs uuid advapi32)
	if(KYTY_CLANG_CL OR MSVC)
		# WIN32 subsystem but a normal main().
		target_link_options(kyty_slint_launcher PRIVATE /ENTRY:mainCRTStartup /OPT:NOREF)
	endif()
endif()

set_target_properties(kyty_slint_launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
install(TARGETS kyty_slint_launcher DESTINATION .)
