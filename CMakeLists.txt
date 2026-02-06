cmake_minimum_required(VERSION 3.16)
project(frogklan_qa LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(frogklan
  src/main.cpp
  src/md5.cpp
  src/sip.cpp
  src/net.cpp
)

if (WIN32)
  target_compile_definitions(frogklan PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
  target_link_libraries(frogklan PRIVATE ws2_32)
endif()
