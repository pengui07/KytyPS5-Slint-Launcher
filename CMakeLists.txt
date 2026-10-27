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
	# Slint has no submodules; an explicit empty list skips the submodule
	# update step entirely (CMP0097 NEW in the FetchContent subbuild).
	GIT_SUBMODULES ""
	SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)
# Slint bug (v1.18.1, still on master): while a context menu is open, a pointer event outside it is
# passed to the menu's parent item shifted by +(that parent's window position) where it needs minus
# it, so hover and clicks on the page land one title bar height (32px) below the cursor. Patched in
# the fetched source; written only once, so later configures leave window.rs (and the Rust build) alone.
set(_kyty_window_rs "${slint_SOURCE_DIR}/internal/core/window.rs")
file(READ "${_kyty_window_rs}" _kyty_window_src)
string(REPLACE "\r\n" "\n" _kyty_window_src "${_kyty_window_src}")
set(_kyty_popup_old [=[
                Some(menubar_item) => {
                    event.translate(
                        menubar_item
                            .map_to_item_tree(Default::default(), &self.component())
                            .to_vector(),
                    );
                    menubar_item.parent_item(ParentItemTraversalMode::StopAtPopups)
                }]=])
set(_kyty_popup_new [=[
                Some(menubar_item) => {
                    let parent = menubar_item.parent_item(ParentItemTraversalMode::StopAtPopups);
                    if let Some(parent) = &parent {
                        event.translate(
                            -parent
                                .map_to_item_tree(Default::default(), &self.component())
                                .to_vector(),
                        );
                    }
                    parent
                }]=])
string(FIND "${_kyty_window_src}" "${_kyty_popup_old}" _kyty_popup_at)
if(NOT _kyty_popup_at EQUAL -1)
	string(REPLACE "${_kyty_popup_old}" "${_kyty_popup_new}" _kyty_window_src "${_kyty_window_src}")
	file(WRITE "${_kyty_window_rs}" "${_kyty_window_src}")
else()
	string(FIND "${_kyty_window_src}" "${_kyty_popup_new}" _kyty_popup_at)
	if(_kyty_popup_at EQUAL -1)
		message(FATAL_ERROR "Slint's window.rs changed: re-check the context menu offset patch in CMakeLists.txt")
	endif()
endif()
# Menus shaped like the shadPS4 launcher's, and the menu bar drawn as the window's title bar. Slint
# has no per-app menu styling, so the fluent style's menu file, its Palette (which carries the
# launcher's theme to the menus) and the shared menu bar implementation are replaced in the fetched
# source; slint-compiler embeds the styles and rebuilds
# when they change. Copied only when they differ.
set(KYTY_MENU_STYLE "${CMAKE_CURRENT_SOURCE_DIR}/slint-style/fluent-menu.slint")
set(KYTY_MENU_IMPL "${CMAKE_CURRENT_SOURCE_DIR}/slint-style/menus.slint")
set(KYTY_STYLE_BASE "${CMAKE_CURRENT_SOURCE_DIR}/slint-style/fluent-style-base.slint")
configure_file("${KYTY_MENU_STYLE}"
	"${slint_SOURCE_DIR}/internal/compiler/widgets/fluent/menu.slint" COPYONLY)
configure_file("${KYTY_MENU_IMPL}"
	"${slint_SOURCE_DIR}/internal/compiler/widgets/common/menus.slint" COPYONLY)
configure_file("${KYTY_STYLE_BASE}"
	"${slint_SOURCE_DIR}/internal/compiler/widgets/fluent/style-base.slint" COPYONLY)
# The embedded styles are not in the depfile slint-compiler writes (only app.slint's own imports are),
# so a style-only edit would rebuild nothing. Re-configure when they change, and drop the generated
# app.h so the codegen step re-runs and picks the new menu style up.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${KYTY_MENU_STYLE}" "${KYTY_MENU_IMPL}" "${KYTY_STYLE_BASE}")
file(SHA256 "${KYTY_MENU_STYLE}" _kyty_menu_style_hash)
file(SHA256 "${KYTY_MENU_IMPL}" _kyty_menu_impl_hash)
file(SHA256 "${KYTY_STYLE_BASE}" _kyty_style_base_hash)
set(_kyty_menu_hash "${_kyty_menu_style_hash}${_kyty_menu_impl_hash}${_kyty_style_base_hash}")
set(_kyty_menu_stamp "${CMAKE_CURRENT_BINARY_DIR}/menu-style.stamp")
if(EXISTS "${_kyty_menu_stamp}")
	file(READ "${_kyty_menu_stamp}" _kyty_menu_old_hash)
else()
	set(_kyty_menu_old_hash "")
endif()
if(NOT _kyty_menu_old_hash STREQUAL _kyty_menu_hash)
	file(WRITE "${_kyty_menu_stamp}" "${_kyty_menu_hash}")
	file(REMOVE "${CMAKE_CURRENT_BINARY_DIR}/app.h")
endif()
if(WIN32 AND KYTY_SLINT_STATIC_CRT)
	# Rust side of the static CRT (the C++ side is the target property further down).
	corrosion_add_target_rustflags(slint_cpp "-Ctarget-feature=+crt-static")
endif()


# ZArchive reads .zar game archives (the library lists them with their name, icon and trophies) and writes them
# (Library > Compress to ZAR). Only the reader/writer library and its zstd dependency are built, at the same
# revisions and with the same hardening patch as the emulator.
find_package(Git REQUIRED)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	zstd
	GIT_REPOSITORY https://github.com/facebook/zstd.git
	GIT_TAG f8745da6ff1ad1e7bab384bd1f9d742439278e99
	GIT_SHALLOW FALSE
	SOURCE_SUBDIR build/cmake
)
FetchContent_Declare(
	zarchive_source
	GIT_REPOSITORY https://github.com/Exzap/ZArchive.git
	GIT_TAG d2c717730092c7bf8cbb033b12fd4001b7c4d932
	GIT_SHALLOW FALSE
	PATCH_COMMAND "${CMAKE_COMMAND}" "-DGIT=${GIT_EXECUTABLE}"
		"-DPATCH=${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/patches/zarchive-reader.patch"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/patches/apply_once.cmake"
	# Not a real folder: ZArchive's own CMakeLists (the command-line tool) is not used.
	SOURCE_SUBDIR kyty-library-only
)
FetchContent_MakeAvailable(zstd zarchive_source)
add_library(zarchive STATIC
	"${zarchive_source_SOURCE_DIR}/src/zarchivereader.cpp"
	"${zarchive_source_SOURCE_DIR}/src/zarchivewriter.cpp"
	"${zarchive_source_SOURCE_DIR}/src/sha_256.c"
)
target_include_directories(zarchive SYSTEM PUBLIC "${zarchive_source_SOURCE_DIR}/include")
target_include_directories(zarchive PRIVATE "${zstd_SOURCE_DIR}/lib")
target_link_libraries(zarchive PRIVATE libzstd_static)
target_compile_features(zarchive PRIVATE cxx_std_20)
if(MSVC)
	target_compile_options(zarchive PRIVATE /W0)
else()
	target_compile_options(zarchive PRIVATE -w)
endif()
if(WIN32 AND KYTY_SLINT_STATIC_CRT)
	set_property(TARGET zarchive libzstd_static PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

# The version the self-updater compares with the latest release ("v1.2.3"; the release workflow passes the tag).
# A build without one is a development build and never offers an update.
set(KYTY_LAUNCHER_VERSION "" CACHE STRING "Release tag of this build (vX.Y.Z), empty for a development build")

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
	# Cheats: <run folder>/_Patches/<TITLEID>.json and the online collection
	src/cheats.cpp
	src/cheats.h
	src/cheats_page.cpp
	src/cheats_page.h
	src/secret_store.cpp
	src/secret_store.h
	src/qt_ini.cpp
	src/qt_ini.h
	src/qt_import.cpp
	src/qt_import.h
	src/version_manager.h
	# .zar game archives (read and write)
	src/zar.cpp
	src/zar.h
	# Library > Additional Content… (add-ons by title id, --addcont)
	src/addcont.cpp
	src/addcont.h
	# Help > Check for updates (this repo's GitHub releases)
	src/self_update.cpp
	src/self_update.h
	# Settings > General > Theme: the built-in themes and <data root>/themes/*.json
	src/themes.cpp
	src/themes.h
	$<$<BOOL:${WIN32}>:src/app.rc>
)
# Icons and images go into the exe. Slint's default for C++ only records their absolute paths at build
# time, which exist on the build machine and nowhere else, so a release build showed no menu icons.
set_property(TARGET kyty_slint_launcher PROPERTY SLINT_EMBED_RESOURCES embed-files)
slint_target_sources(kyty_slint_launcher ui/app.slint)
target_link_libraries(kyty_slint_launcher PRIVATE Slint::Slint zarchive)
target_compile_definitions(kyty_slint_launcher PRIVATE KYTY_LAUNCHER_VERSION="${KYTY_LAUNCHER_VERSION}")
target_include_directories(kyty_slint_launcher PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(WIN32)
	# Rounded corners and the 1px border on the frameless main window (main.cpp).
	target_link_libraries(kyty_slint_launcher PRIVATE dwmapi)
endif()
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
