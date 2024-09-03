cmake_minimum_required(VERSION 3.10)

project(mstack LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

find_package(spdlog)
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system)
find_package(libnl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FILE(GLOB MSTACK_HEADERS include/*.hpp)

add_library(mstack INTERFACE ${MSTACK_HEADERS})
target_include_directories(mstack INTERFACE include)

if (TARGET spdlog::spdlog)
    target_link_libraries(mstack INTERFACE spdlog::spdlog)
else ()
    target_link_libraries(mstack INTERFACE spdlog)
endif ()

if (TARGET libnl::libnl)
    target_link_libraries(mstack INTERFACE libnl::libnl)
else ()
    target_link_libraries(mstack INTERFACE nl)
endif ()

add_library(mstack::mstack ALIAS mstack)

add_executable(mstackbin main.cpp)
target_link_libraries(mstackbin mstack::mstack)
target_link_libraries(mstackbin Threads::Threads m)

if (TARGET Boost::system)
    target_link_libraries(mstack INTERFACE Boost::system)
else ()
    target_link_libraries(mstack INTERFACE boost_system)
endif ()
