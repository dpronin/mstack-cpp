cmake_minimum_required(VERSION 3.12)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

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

if (TARGET Boost::system)
    target_link_libraries(mstack INTERFACE Boost::system)
else ()
    target_link_libraries(mstack INTERFACE boost_system)
endif ()

add_library(mstack::mstack ALIAS mstack)
