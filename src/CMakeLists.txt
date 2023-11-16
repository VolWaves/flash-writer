cmake_minimum_required(VERSION 3.16)

add_definitions(-DG_LOG_ENABLED=1 -DEVT_LOG_ENABLED=1)
add_subdirectory(scheduler)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/usb_func.c
	${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/main.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
)
# Link with the pico stdlib
# hardware_clocks hardware_i2c hardware_pio hardware_interp hardware_rtc hardware_uart hardware_irq hardware_resets
# pico_unique_id
# tinyusb_host tinyusb_device tinyusb_board
target_link_libraries(${PROJECT_NAME} PUBLIC
	pico_stdlib
	tinyusb_device
	scheduler
	pico_sync
	hardware_adc
	hardware_sync
)
# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
