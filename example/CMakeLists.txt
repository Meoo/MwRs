
add_executable(example-server
  example_server.cpp)

target_compile_features(example-server PRIVATE cxx_std_11)
set_property(TARGET example-server PROPERTY CXX_STANDARD 11)

target_link_libraries(example-server
  server)


add_executable(example-client
  example_client.cpp)

target_compile_features(example-client PRIVATE cxx_std_11)
set_property(TARGET example-client PROPERTY CXX_STANDARD 11)

target_link_libraries(example-client
  client)
