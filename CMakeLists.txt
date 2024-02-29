# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

set(PICO_BTSTACK_PATH /home/bruce/pico/btstack)

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
project(webmouse C CXX ASM)

add_executable(${PROJECT_NAME} webmouse.cpp mouse.cpp web.cpp ws.cpp txt.cpp keycode.cpp config.cpp persist.cpp
	)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/build/generated)

#add_compile_definitions(CYW43_LWIP=0)

pico_set_program_name(${PROJECT_NAME} "webmouse")
pico_set_program_version(${PROJECT_NAME} "0.1")

pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 1)

# Add the standard library to the build
if (${PICO_BOARD} STREQUAL "pico_w")
	set(PLAT_LIBS pico_btstack_ble pico_btstack_cyw43 pico_cyw43_arch_lwip_threadsafe_background pico_mbedtls pico_lwip_mdns hardware_flash)
else()
	set(PLAT_LIBS hardware_i2c hardware_spi)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC pico_stdlib ${PLAT_LIBS})

pico_add_extra_outputs(${PROJECT_NAME})

pico_btstack_make_gatt_header(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR}/mouse_att.gatt)

