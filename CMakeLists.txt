
cmake_minimum_required(VERSION 3.0)

project(MwRs)


set(MWRS_BUILD_EXAMPLE OFF CACHE BOOL "Build MwRs example")
set(MWRS_INSTALL OFF CACHE BOOL "Create MwRs install target")


if(MSVC)
  # Force to always compile with W4
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif()

  # Multi-threaded build
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  # Update if necessary
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()


add_library(mwrs-client STATIC
  include/mwrs.h
  src/mwrs_client.cpp
  src/mwrs_messages.hpp)

include_directories(mwrs-client PUBLIC include)

target_compile_features(mwrs-client PRIVATE cxx_std_11)
set_target_properties(mwrs-client PROPERTIES
  CXX_STANDARD 11
  OUTPUT_NAME mwrsclient
  DEBUG_POSTFIX d)


add_library(mwrs-server STATIC
  include/mwrs.h
  src/mwrs_server.cpp
  src/mwrs_messages.hpp)

include_directories(mwrs-server PUBLIC include)

target_compile_features(mwrs-server PRIVATE cxx_std_11)
set_target_properties(mwrs-server PROPERTIES
  CXX_STANDARD 11
  OUTPUT_NAME mwrsserver
  DEBUG_POSTFIX d)


if(MWRS_INSTALL)
  include(GNUInstallDirs)

  install(FILES include/mwrs.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  set(MWRS_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/mwrs")

  install(
    TARGETS mwrs-client mwrs-server EXPORT MwRsTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(EXPORT MwRsTargets
    FILE MwRsTargets.cmake
    NAMESPACE mwrs::
    DESTINATION ${MWRS_CMAKE_DIR}
  )
endif()

if(MWRS_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
