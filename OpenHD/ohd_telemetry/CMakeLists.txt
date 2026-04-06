cmake_minimum_required(VERSION 3.16.3)
project(OHDCommon)

if (TARGET OHDCommonLib)
    message(STATUS "OHDCommonLib already built")
    return()
endif()

#----------------------------------------------------------------------------------------------------------------------
# general settings and options
#----------------------------------------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Spams build due to nlohmann::json
add_compile_options(-Wno-psabi)

add_library(OHDCommonLib STATIC) # initialized below
add_library(OHDCommonLib::OHDCommonLib ALIAS OHDCommonLib)

# We use spdlog and json pretty much everywhere in OpenHD. Build it here, then make publicly available
# 1) spdlog
#find_package(spdlog REQUIRED)
#set(SPDLOG_MASTER_PROJECT OFF)
#add_subdirectory(lib/spdlog)
# Public since we use it throughout OpenHD
#target_link_libraries(OHDCommonLib PUBLIC spdlog::spdlog)
set(SPDLOG_MASTER_PROJECT OFF)
set(SPDLOG_PROJECT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/lib/spdlog)
set(SPDLOG_SOURCES_DIRECTORY  ${SPDLOG_PROJECT_DIRECTORY}/src)
set(spdlog_sources
        #"${SPDLOG_SOURCES_DIRECTORY}/async.cpp"
        "${SPDLOG_SOURCES_DIRECTORY}/bundled_fmtlib_format.cpp"
        #"${SPDLOG_SOURCES_DIRECTORY}/color_sinks.cpp"
        #"${SPDLOG_SOURCES_DIRECTORY}/file_sinks.cpp"
        #"${SPDLOG_SOURCES_DIRECTORY}/spdlog.cpp"
        #"${SPDLOG_SOURCES_DIRECTORY}/stdout_sinks.cpp"
)
#target_sources(OHDCommonLib PRIVATE ${spdlog_sources})
#file(GLOB_RECURSE X_SPDLOG_HEADERS
#        ${SPDLOG_PROJECT_DIRECTORY}/include/*.h
#)
#target_precompile_headers(OHDCommonLib PRIVATE ${SPDLOG_PROJECT_DIRECTORY}/include)
#target_precompile_headers(OHDCommonLib PRIVATE ${X_SPDLOG_HEADERS})
target_precompile_headers(OHDCommonLib PRIVATE ${SPDLOG_PROJECT_DIRECTORY}/include/spdlog/spdlog.h)
target_include_directories(OHDCommonLib
        PUBLIC
        ${SPDLOG_PROJECT_DIRECTORY}/include)

# 2) nlohmann::json
add_subdirectory(lib/json)
# Public since we use it throughout OpenHD
target_link_libraries(OHDCommonLib PUBLIC  nlohmann_json::nlohmann_json)
# 3) geographiclib
add_subdirectory(lib/geographiclib-c-2.0)
target_link_libraries(OHDCommonLib PUBLIC geodc)

find_package(Threads REQUIRED)
target_link_libraries(OHDCommonLib PUBLIC Threads::Threads)
#----------------------------------------------------------------------------------------------------------------------
# sources
#----------------------------------------------------------------------------------------------------------------------

set(sources
    "src/openhd_util.cpp"
    "src/openhd_util_filesystem.cpp"
    "src/openhd_settings_persistent.cpp"
    "src/openhd_profile.cpp"
    "src/openhd_platform.cpp"
    "src/openhd_spdlog.cpp"
    "src/openhd_reboot_util.cpp"
    "src/openhd_config.cpp"
    "src/openhd_util_async.cpp"
    "src/openhd_external_device.cpp"
    "src/openhd_action_handler.cpp"
    "src/openhd_udp.cpp"
    "src/openhd_tcp.cpp"
    src/openhd_led.cpp
    src/openhd_buttons.cpp
    src/openhd_buttons_monitor.cpp
    src/openhd_camera_state_helper.cpp
    src/openhd_flight_mode_helper.cpp
    src/openhd_object_tracking_helper.cpp
    src/openhd_settings_imp.cpp
    src/openhd_settings_directories.cpp
    src/openhd_settings_persistent.cpp
    src/openhd_util_time.cpp
    src/openhd_bitrate.cpp
    src/openhd_geo_utils.cpp
    )
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources})

#----------------------------------------------------------------------------------------------------------------------
# target
#----------------------------------------------------------------------------------------------------------------------

target_sources(OHDCommonLib PRIVATE ${sources})

target_include_directories(OHDCommonLib
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc/>")

set_target_properties(OHDCommonLib PROPERTIES
        SOVERSION ${PROJECT_VERSION_MAJOR}
        VERSION ${PROJECT_VERSION}
        LINKER_LANGUAGE CXX)

add_executable(test_openhd_util test/test_openhd_util.cpp)
target_link_libraries(test_openhd_util OHDCommonLib)

add_executable(test_ohd_reboot test/test_ohd_reboot.cpp)
target_link_libraries(test_ohd_reboot OHDCommonLib)

add_executable(test_config test/test_config.cpp)
target_link_libraries(test_config OHDCommonLib)

add_executable(test_logging test/test_logging.cpp)
target_link_libraries(test_logging OHDCommonLib)

# this one is here but not really part of interface
add_executable(test_led test/test_led.cpp)
target_link_libraries(test_led OHDCommonLib)

add_executable(test_openhd_async test/test_openhd_async.cpp)
target_link_libraries(test_openhd_async OHDCommonLib)

add_executable(test_tcp_server test/test_tcp_server.cpp)
target_link_libraries(test_tcp_server OHDCommonLib)

#----------------------------------------------------------------------------------------------------------------------
# Tests based on google testing framework
#----------------------------------------------------------------------------------------------------------------------
option(ENABLE_GOOGLE_TESTS "Enable tests based on google testing framework" OFF)
message("Tests based on google testing framework enabled: ${ENABLE_GOOGLE_TESTS}")

if (ENABLE_GOOGLE_TESTS)
	include(FetchContent)
	FetchContent_Declare(
	  googletest
	  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
	  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	add_executable(test_history_collection test/test_history_collection.cpp)
	target_link_libraries(test_history_collection OHDCommonLib GTest::gtest_main)
endif ()
