# -*- cmake -*-
# Copyright (C) Dmitry Igrishin
# For conditions of distribution and use, see file LICENSE.txt

cmake_minimum_required(VERSION 3.16)
cmake_policy(VERSION 3.16)
project(dmitigr_genicam)

add_library(dmitigr_genicam_daheng_gx INTERFACE)
target_link_libraries(dmitigr_genicam_daheng_gx INTERFACE galaxy_camera)
