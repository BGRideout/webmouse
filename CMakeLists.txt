# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Define project
project(webmouse C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add the utility libraries
add_subdirectory(${PICO_SDK_PATH}/../pico-filesystem pfs)
add_subdirectory(../picolibs picolibs)

# Define the executable
add_executable(${PROJECT_NAME}
 	webmouse.cpp
 	mouse.cpp
	config.cpp
	keycode.cpp
	)

pico_set_program_name(${PROJECT_NAME} "webmouse")
pico_set_program_version(${PROJECT_NAME} "0.2")

# Set stdio interface
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 1)
set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

# Link the web server library
target_link_libraries(${PROJECT_NAME} PUBLIC bgr_webserver)

# Add the libraries to the build
target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib pico_cyw43_arch_lwip_threadsafe_background
											 pico_btstack_ble pico_btstack_cyw43
											 flash_filesystem)

pico_add_extra_outputs(${PROJECT_NAME})

# Make the BLE GATT definitions
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
pico_btstack_make_gatt_header(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/mouse_att.gatt)

# Add the static web resourcce files
set(WEB_RESOURCE_FILES
 	data/config.html
	data/config.js
	data/index.html
	data/webmouse.css
	data/webmouse.js
	data/favicon.ico)
	
web_files(FILES ${WEB_RESOURCE_FILES} WEBSOCKET)

