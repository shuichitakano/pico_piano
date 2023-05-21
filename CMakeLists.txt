# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
#set(PICO_SDK_PATH "~/pico/pico-sdk")

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(pico_piano C CXX ASM)

set(PICO_CXX_ENABLE_RTTI 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pico_piano 
  main.cpp
  ble_client_manager.cpp
  midi.cpp
  ble_midi.cpp
  pm_piano/string.cpp
  pm_piano/soundboard.cpp
  pm_piano/piano.cpp
  pm_piano/note.cpp
  pm_piano/note_manager.cpp
  pm_piano/hammer.cpp
  pm_piano/filter.cpp
  pm_piano/allocator.cpp
  audio/audio.cpp
)

pico_set_program_name(pico_piano "pico_piano")
pico_set_program_version(pico_piano "0.1")

pico_enable_stdio_uart(pico_piano 1)
pico_enable_stdio_usb(pico_piano 0)

# Add the standard library to the build
target_link_libraries(pico_piano
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pico_piano PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(pico_piano
        hardware_dma
        hardware_pio
        hardware_interp
        pico_multicore
        pico_btstack_ble
#        pico_btstack_classic
        pico_btstack_cyw43
#        pico_cyw43_arch_lwip_poll
        pico_cyw43_arch_lwip_threadsafe_background
#        pico_mbedtls_tls
        )

pico_generate_pio_header(pico_piano ${CMAKE_CURRENT_LIST_DIR}/simple_serialize.pio)

pico_btstack_make_gatt_header(pico_piano PRIVATE "${CMAKE_CURRENT_LIST_DIR}/bt.gatt")
pico_add_extra_outputs(pico_piano)



