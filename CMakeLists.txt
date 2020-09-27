
cmake_minimum_required(VERSION 3.8)

project(MwRs)


set(MWRS_BUILD_EXAMPLE OFF CACHE BOOL "Build MwRs example")
set(MWRS_INSTALL OFF CACHE BOOL "Create MwRs install target")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

# Default install directory is <project>/install
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "..." FORCE)
endif()

set(CXX_EXTENSIONS OFF)

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


set(CLIENT_SOURCE
  include/mwrs.h
  include/mwrs_client.h
  src/mwrs_client.cpp
  src/mwrs_messages.hpp)

add_library(client ${CLIENT_SOURCE})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${CLIENT_SOURCE})

include_directories(client PUBLIC include)

target_compile_features(client PRIVATE cxx_std_11)
set_target_properties(client PROPERTIES
  CXX_STANDARD 11
  OUTPUT_NAME mwrsclient
  DEBUG_POSTFIX d)


set(SERVER_SOURCE
  include/mwrs.h
  include/mwrs_server.h
  src/mwrs_server.cpp
  src/mwrs_messages.hpp)

add_library(server ${SERVER_SOURCE})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SERVER_SOURCE})

include_directories(server PUBLIC include)

target_compile_features(server PRIVATE cxx_std_11)
set_target_properties(server PROPERTIES
  CXX_STANDARD 11
  OUTPUT_NAME mwrsserver
  DEBUG_POSTFIX d)


if(BUILD_SHARED_LIBS)
  set(MWRS_SHARED ON)
  target_compile_definitions(client PRIVATE MWRS_SHARED MWRS_SHARED_BUILD)
  target_compile_definitions(server PRIVATE MWRS_SHARED MWRS_SHARED_BUILD)
  if(NOT WIN32 AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
  endif()
endif()

if(MWRS_INSTALL)
  include(GNUInstallDirs)

  set(MWRS_CMAKE_GENERATED "*/")
  configure_file(include/mwrs.h ${CMAKE_CURRENT_BINARY_DIR}/mwrs.h)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/mwrs.h
    include/mwrs_client.h
    include/mwrs_server.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  set(MWRS_CMAKE_DIR "cmake")

  install(
    TARGETS client server EXPORT MwRs
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  if(BUILD_SHARED_LIBS)
    install(
      FILES $<TARGET_PDB_FILE:client> $<TARGET_PDB_FILE:server>
      DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL
    )
  endif()

  install(EXPORT MwRs
    FILE mwrsConfig.cmake
    NAMESPACE mwrs::
    DESTINATION ${MWRS_CMAKE_DIR}
  )
endif()

if(MWRS_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
