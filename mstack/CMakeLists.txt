cmake_minimum_required(VERSION 3.12)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()

FILE(GLOB MSTACK_SOURCES mstack/*.[ch]pp)

add_library(mstack STATIC ${MSTACK_SOURCES})
target_include_directories(mstack PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(mstack PUBLIC cxx_std_20)

if (TARGET spdlog::spdlog)
    target_link_libraries(mstack PUBLIC spdlog::spdlog)
else ()
    target_link_libraries(mstack PUBLIC spdlog)
endif ()

if (TARGET Boost::system)
    target_link_libraries(mstack PUBLIC Boost::system)
else ()
    target_link_libraries(mstack PUBLIC boost_system)
endif ()

if (TARGET Boost::headers)
    target_link_libraries(mstack PUBLIC Boost::headers)
endif ()

add_library(mstack::mstack ALIAS mstack)
