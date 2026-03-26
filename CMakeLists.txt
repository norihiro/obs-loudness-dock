cmake_minimum_required(VERSION 3.12)

project(loudness-dock VERSION 0.5.1)

set(PLUGIN_AUTHOR "Norihiro Kamae")

set(PLUGIN_URL "https://obsproject.com/forum/resources/loudness-dock.1751/")
set(ID_PREFIX "net.nagater.obs-loudness-dock.")
set(MACOS_BUNDLEID "net.nagater.obs-loudness-dock")
set(MACOS_PACKAGE_UUID "4B74E0A4-7591-425D-83E4-9A56DD6102B9")
set(MACOS_INSTALLER_UUID "DAC8DE7B-1012-4EEB-893B-595E78F3F9A5")
set(WIN_INSTALLER_UUID   "BDD16F31-90DD-425C-9F78-19DD84F44749")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

option(ENABLE_COVERAGE "Enable coverage option for GCC" OFF)
option(WITH_ASSERT_THREAD "Enable thread assertion" OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	find_package(libobs REQUIRED)
	find_package(obs-frontend-api REQUIRED)
	include(cmake/ObsPluginHelpers.cmake)
	add_library(OBS::frontend-api ALIAS OBS::obs-frontend-api)
	set(QT_NO_CREATE_VERSIONLESS_TARGETS OFF)
	find_package(Qt${QT_VERSION} COMPONENTS Widgets Core Gui)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(PC_LIBEBUR128 libebur128)
endif()

configure_file(
	src/plugin-macros.h.in
	plugin-macros.generated.h
)

set(PLUGIN_SOURCES
	src/plugin-main.c
	src/loudness.c
	src/loudness-dock.cpp
	src/meter.cpp
	src/config-dialog.cpp
	src/config-dialog-table-delegate.cpp
	src/dock-compat.cpp
)

if(NOT PC_LIBEBUR128_FOUND)
	set(PLUGIN_SOURCES ${PLUGIN_SOURCES} deps/libebur128/ebur128/ebur128.c)
endif()

add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

target_link_libraries(${PROJECT_NAME}
	OBS::libobs
	OBS::frontend-api
	Qt::Core
	Qt::Widgets
	Qt::Gui
)

if(PC_LIBEBUR128_FOUND)
	target_compile_options(${PROJECT_NAME} PRIVATE ${PC_LIBEBUR128_CFLAGS})
	target_link_libraries(${PROJECT_NAME} ${PC_LIBEBUR128_LIBRARIES})
else()
	target_include_directories(${PROJECT_NAME}
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/deps/libebur128/ebur128
		${CMAKE_CURRENT_SOURCE_DIR}/deps/libebur128/ebur128/queue
	)
endif()

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-websocket
)

option(ENABLE_PROFILE "Enable profile" OFF)
if(ENABLE_PROFILE)
	target_compile_definitions(${PROJECT_NAME} PRIVATE "-DENABLE_PROFILE")
endif()

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
)

if(OS_WINDOWS)
	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
		add_definitions("-D_USE_MATH_DEFINES") # for M_PI
	endif()

	target_link_libraries(${PROJECT_NAME} OBS::w32-pthreads)

	set(LICENSE_DESTINATION "${PROJECT_NAME}/data")
endif()

if(OS_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,defs)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--unresolved-symbols=report-all)

	set(LICENSE_DESTINATION ${CMAKE_INSTALL_DOCDIR})

	if(ENABLE_COVERAGE)
		target_compile_options(${PROJECT_NAME} PRIVATE -coverage -fprofile-update=atomic)
		target_link_options(${PROJECT_NAME} PRIVATE -coverage)
	endif()
endif()

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set(LICENSE_DESTINATION "${PROJECT_NAME}.plugin/Contents/Resources")

	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
	target_compile_options(${PROJECT_NAME} PRIVATE $<$<BOOL:${ENABLE_COVERAGE}>:-coverage>)
	target_link_options(${PROJECT_NAME} PRIVATE $<$<BOOL:${ENABLE_COVERAGE}>:--coverage>)
endif()

file(GENERATE OUTPUT .gitignore CONTENT "*\n")

setup_plugin_target(${PROJECT_NAME})

option(INSTALL_LICENSE_FILES "Install license files" ON)
if(INSTALL_LICENSE_FILES)
	install(FILES
		LICENSE
		DESTINATION ${LICENSE_DESTINATION}
	)
	if(NOT PC_LIBEBUR128_FOUND)
		install(FILES
			deps/libebur128/COPYING
			RENAME libebur128-COPYING
			DESTINATION ${LICENSE_DESTINATION}
		)
	endif()
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(
		installer/installer-Windows.iss.in
		installer-Windows.generated.iss
	)

	configure_file(
		ci/ci_includes.sh.in
		ci/ci_includes.generated.sh
	)
	configure_file(
		ci/ci_includes.cmd.in
		ci/ci_includes.generated.cmd
	)

	configure_file(
		installer/installer-macOS.pkgproj.in
		installer-macOS.generated.pkgproj
	)
	configure_file(
		installer/installer-macOS-post.sh.in
		installer-macOS-post.generated.sh
	)
endif()
