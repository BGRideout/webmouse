# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

set(PICO_BTSTACK_PATH /home/bruce/pico/btstack)

set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
project(webmouse C CXX ASM)

add_executable(${PROJECT_NAME} webmouse.cpp
 	mouse.cpp web.cpp config.cpp
	ws.cpp txt.cpp keycode.cpp persist.cpp led.cpp
	dhcpserver/dhcpserver.c	${CMAKE_CURRENT_BINARY_DIR}/generated/web_files.cpp
	)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/dhcpserver)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/build/generated)

#add_compile_definitions(CYW43_LWIP=0)

pico_set_program_name(${PROJECT_NAME} "webmouse")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard library to the build
if (${PICO_BOARD} STREQUAL "pico_w")
	set(PLAT_LIBS pico_btstack_ble pico_btstack_cyw43 pico_cyw43_arch_lwip_threadsafe_background pico_mbedtls pico_lwip_mdns hardware_flash)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib ${PLAT_LIBS})

pico_add_extra_outputs(${PROJECT_NAME})

pico_btstack_make_gatt_header(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/mouse_att.gatt)

find_package (Python3 REQUIRED COMPONENTS Interpreter)
set(WEB_RESOURCE_FILES data/config.html data/config.js data/index.html data/webmouse.css data/webmouse.js data/websocket.js)
set(WEB_FILES_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(WEB_FILES "${WEB_FILES_DIR}/web_files.cpp")
set(WEB_FILES_TARGET "${PROJECT_NAME}_web_files")
add_custom_target(${WEB_FILES_TARGET} DEPENDS ${WEB_RESOURCE_FILES})
add_custom_command(
	OUTPUT ${WEB_FILES}
	DEPENDS ${WEB_RESOURCE_FILES}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMAND ${CMAKE_COMMAND} -E make_directory ${WEB_FILES_DIR} &&
			${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/create_web_files.py -o ${WEB_FILES} ${WEB_RESOURCE_FILES}
	VERBATIM)
add_dependencies(${PROJECT_NAME}
	${WEB_FILES_TARGET}
	)

