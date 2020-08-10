# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see file LICENSE.txt

cmake_minimum_required(VERSION 3.17)
cmake_policy(VERSION 3.17)
project(dmitigr_genicam)

add_library(dmitigr_genicam_daheng_gx INTERFACE)
add_library(dmitigr::genicam_daheng_gx ALIAS dmitigr_genicam_daheng_gx)
target_include_directories(dmitigr_genicam_daheng_gx INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib")
