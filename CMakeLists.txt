# This is only for the cavacore lib, see CAVACORE.md for details
# to build cava don't use this, use the automake stuff

cmake_minimum_required(VERSION 3.13.0)
project(cavacore)
add_library(cavacore STATIC cavacore.c)
