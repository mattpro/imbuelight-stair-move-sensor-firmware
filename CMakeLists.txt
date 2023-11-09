# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE 
                    src/main.c 
                    src/utils.c 
                    src/adc.c
                    src/distance_sensor.c
                    src/settings.c)
