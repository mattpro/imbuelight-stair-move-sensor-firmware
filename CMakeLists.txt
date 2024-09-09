# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(LIB_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(blinky)



FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE 
                    src/main.c 
                    src/utils.c 
                    src/adc.c
                    src/ir_sensor.c
                    src/settings.c
                    src/sths34pf80_reg.c
                    src/infrared_pd_manager.c
                    )


add_library(other3party_lib STATIC IMPORTED GLOBAL)

set_target_properties(other3party_lib PROPERTIES IMPORTED_LOCATION ${LIB_DIR}/InfraredPD_CM4F_wc32_ot.a)

target_link_libraries(app PUBLIC other3party_lib)