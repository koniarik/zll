cmake_minimum_required(VERSION 3.19)

option(ZLL_TESTS_ENABLED "Enable tests" OFF)
option(ZLL_SIZE_ENABLED "Build the code size probes" OFF)

project(zll LANGUAGES CXX)

add_library(zll INTERFACE include/zll.hpp)
target_include_directories(
  zll INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include>)
target_compile_features(zll INTERFACE cxx_std_20)

add_library(zll::zll ALIAS zll)

# Install configuration
install(FILES include/zll.hpp DESTINATION include/)
install(
  TARGETS zll
  EXPORT zll
  INCLUDES
  DESTINATION include/)
install(
  EXPORT zll
  FILE zll-config.cmake
  NAMESPACE zll::
  DESTINATION lib/cmake/zll/)

# Tests configuration
if(ZLL_TESTS_ENABLED)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(test)
  endif()
endif()

if(ZLL_SIZE_ENABLED)
  add_subdirectory(test/size)
endif()
