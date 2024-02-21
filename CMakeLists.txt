cmake_minimum_required(VERSION 3.22.0)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(logicanalyzer VERSION 0.1.0 LANGUAGES C CXX ASM)

pico_sdk_init()


add_executable(${PROJECT_NAME} main.c)
# add_executable(${PROJECT_NAME} main.c)
add_library(pinpoller pinpoller.c)
add_library(usb usb_handler.c)
add_library(dma_handler dma_handler.c)


pico_generate_pio_header(pinpoller ${CMAKE_CURRENT_LIST_DIR}/pinpoller.pio)

pico_add_extra_outputs(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} pico_stdlib pico_multicore hardware_dma hardware_pio pinpoller usb dma_handler)
target_link_libraries(pinpoller pico_stdlib hardware_pio)
target_link_libraries(usb pico_stdlib hardware_resets hardware_irq)
target_link_libraries(dma_handler pico_stdlib hardware_dma hardware_pio)

target_compile_definitions(${PROJECT_NAME} PRIVATE PIO_USB_USE_TINYUSB)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR})


pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)


