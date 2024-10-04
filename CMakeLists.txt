cmake_minimum_required(VERSION 3.12)

project(mstack LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

find_package(spdlog)
find_package(Threads REQUIRED)
find_package(Boost COMPONENTS system)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(mstack)

add_executable(mstackbin main.cpp)
target_link_libraries(mstackbin mstack::mstack)
target_link_libraries(mstackbin Threads::Threads m)

if (TARGET Boost::system)
    target_link_libraries(mstackbin Boost::system)
else ()
    target_link_libraries(mstackbin boost_system)
endif ()
