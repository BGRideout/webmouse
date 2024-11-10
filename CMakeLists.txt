# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

if (${USE-HTTPS} STREQUAL "true")
	message("Building https version")
	add_definitions(-DUSE_HTTPS)
else()
	message("Building http version")
endif()

# Add executable. Default name is the project name, version 0.1
project(webmouse C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add the utility libraries
add_subdirectory(${PICO_SDK_PATH}/../pico-filesystem pfs)
add_subdirectory(../picolibs picolibs)

add_executable(${PROJECT_NAME}
 	webmouse.cpp
 	mouse.cpp
	config.cpp
	keycode.cpp
	${CMAKE_CURRENT_BINARY_DIR}/generated/web_files.cpp
	)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/build/generated)

pico_set_program_name(${PROJECT_NAME} "webmouse")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Link the utility libraries
target_link_libraries(${PROJECT_NAME} PUBLIC bgr_network bgr_util)

if (${USE-HTTPS} STREQUAL "true")
target_link_libraries(${PROJECT_NAME} PUBLIC pico_lwip_mbedtls)
endif()

# Add the libraries to the build
target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib pico_cyw43_arch_lwip_threadsafe_background
											 pico_mbedtls pico_lwip_mdns
											 pico_btstack_ble pico_btstack_cyw43
											 flash_filesystem)

pico_add_extra_outputs(${PROJECT_NAME})

pico_btstack_make_gatt_header(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/mouse_att.gatt)

set(WEB_RESOURCE_FILES
 	data/config.html
	data/config.js
	data/index.html
	data/webmouse.css
	data/webmouse.js
	data/favicon.ico)
	
web_files(DIR ${WEB_FILES_DIR} OUTPUT ${WEB_FILES} FILES ${WEB_RESOURCE_FILES} WEBSOCKET)

