cmake_minimum_required(VERSION 3.12)

project(obs-loudness-dock VERSION 0.2.1)

set(PLUGIN_AUTHOR "Norihiro Kamae")

# Replace `com.example.obs-plugin-template` with a unique Bundle ID for macOS releases
# (used both in the installer and when submitting the installer for notarization)
set(MACOS_BUNDLEID "net.nagater.obs-loudness-dock")
set(ID_PREFIX "net.nagater.obs-loudness-dock.")
set(MACOS_PACKAGE_UUID "4B74E0A4-7591-425D-83E4-9A56DD6102B9")
set(MACOS_INSTALLER_UUID "DAC8DE7B-1012-4EEB-893B-595E78F3F9A5")
set(PLUGIN_URL "https://obsproject.com/forum/resources/loudness-dock.1751/")

# Replace `me@contoso.com` with the maintainer email address you want to put in Linux packages
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

# TAKE NOTE: No need to edit things past this point

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
include(cmake/ObsPluginHelpers.cmake)
find_qt(VERSION ${QT_VERSION} COMPONENTS Widgets Core Gui)

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_LIBEBUR128 libebur128)
endif()

configure_file(
	src/plugin-macros.h.in
	plugin-macros.generated.h
)
configure_file(
	installer/installer-Windows.iss.in
	../installer/installer-Windows.generated.iss
)

configure_file(
	ci/ci_includes.sh.in
	../ci/ci_includes.generated.sh
)
configure_file(
	ci/ci_includes.cmd.in
	../ci/ci_includes.generated.cmd
)

set(PLUGIN_SOURCES
	src/plugin-main.c
	src/loudness.c
	src/loudness-dock.cpp
	src/dock-compat.cpp
)

if(NOT PC_LIBEBUR128_FOUND)
	set(PLUGIN_SOURCES ${PLUGIN_SOURCES} deps/libebur128/ebur128/ebur128.c)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME}
	OBS::libobs
	OBS::obs-frontend-api
	Qt::Core
	Qt::Widgets
	Qt::Gui
	Qt::GuiPrivate
)

if(PC_LIBEBUR128_FOUND)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE ${PC_LIBEBUR128_CFLAGS})
	target_link_libraries(${CMAKE_PROJECT_NAME} ${PC_LIBEBUR128_LIBRARIES})
else()
	target_include_directories(${CMAKE_PROJECT_NAME}
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/deps/libebur128/ebur128
		${CMAKE_CURRENT_SOURCE_DIR}/deps/libebur128/ebur128/queue
	)
endif()

target_include_directories(${CMAKE_PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-websocket
)

option(ENABLE_PROFILE "Enable profile" OFF)
if(ENABLE_PROFILE)
	target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE "-DENABLE_PROFILE")
endif()

target_include_directories(${CMAKE_PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

if(OS_WINDOWS)
	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
		add_definitions("-D_USE_MATH_DEFINES") # for M_PI
	endif()

	target_link_libraries(${CMAKE_PROJECT_NAME} OBS::w32-pthreads)
endif()

if(OS_LINUX)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra)
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,-z,defs)
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,--unresolved-symbols=report-all)
	if(ENABLE_COVERAGE)
		target_compile_options(${PROJECT_NAME} PRIVATE -coverage)
		target_link_options(${PROJECT_NAME} PRIVATE -coverage)
	endif()
endif()

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
endif()

setup_plugin_target(${CMAKE_PROJECT_NAME})

configure_file(installer/installer-macOS.pkgproj.in installer-macOS.generated.pkgproj)
