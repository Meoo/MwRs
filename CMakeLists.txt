
cmake_minimum_required(VERSION 3.0)

project(MwRs)


set(MWRS_BUILD_EXAMPLE OFF CACHE BOOL "Build MwRs example")


add_library(mwrs-client STATIC
  include/mwrs.h
  src/mwrs_client.cpp
  src/mwrs_messages.hpp)

include_directories(mwrs-client PUBLIC include)


add_library(mwrs-server STATIC
  include/mwrs.h
  src/mwrs_server.cpp
  src/mwrs_messages.hpp)

include_directories(mwrs-server PUBLIC include)


if(MWRS_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
