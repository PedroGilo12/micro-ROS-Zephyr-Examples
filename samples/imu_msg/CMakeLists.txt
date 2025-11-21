# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/lib/micro-ros-zephyr-module/modules/libmicroros)


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(imu_msg)

target_sources(app PRIVATE src/main.c src/microros_thread.c src/accel_driver.c)
