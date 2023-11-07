cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(pico-gbprint)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project

add_executable(${PROJECT_NAME}
    src/main.c src/linkcable.c src/gb_printer.c src/pico_gb_printer.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pio pico_unique_id)

# Include header files
target_include_directories(${PROJECT_NAME} PRIVATE "include")

# disable usb output, enable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/linkcable.pio)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/linkcable_sm.pio)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(${PROJECT_NAME})
