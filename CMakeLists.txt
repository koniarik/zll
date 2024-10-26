cmake_minimum_required(VERSION 3.19)

option(ZLL_TESTS_ENABLED "Enable tests" OFF)

project(zll)

add_library(zll INTERFACE include/zll.h)
target_include_directories(zll INTERFACE include)
target_compile_features(zll INTERFACE cxx_std_20)

if(ZLL_TESTS_ENABLED)
  include(CTest)
  if(BUILD_TESTING)

    file(GLOB TESTS test/*_test.cpp)

    add_library(doctest INTERFACE)
    target_include_directories(doctest INTERFACE deps/)

    add_executable(zll_utest ${TESTS})
    target_link_libraries(zll_utest PUBLIC zll doctest)
    target_compile_features(zll_utest INTERFACE cxx_std_20)
    add_test(NAME zll_utest COMMAND zll_utest)
  endif()
endif()
