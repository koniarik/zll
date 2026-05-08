cmake_minimum_required(VERSION 3.19)

option(ZLL_TESTS_ENABLED "Enable tests" OFF)

project(zll)

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

    file(GLOB TESTS test/*_test.cpp)

    add_library(doctest INTERFACE)
    target_include_directories(doctest INTERFACE deps/)

    add_executable(zll_utest ${TESTS})
    target_link_libraries(zll_utest PUBLIC zll doctest)
    target_compile_features(zll_utest INTERFACE cxx_std_20)
    add_test(NAME zll_utest COMMAND zll_utest)

    # GDB pretty-printer tests (three-phase: gen → run → eval)
    set(GDB_SCRIPT ${CMAKE_BINARY_DIR}/gdb_test.gdb)
    set(GDB_LOG ${CMAKE_BINARY_DIR}/gdb_test.log)

    add_executable(gdb_test test/gdb_bin.cpp)
    target_link_libraries(gdb_test PUBLIC zll)

    add_test(NAME gdb_test_gen
             COMMAND gdb_test gen ${CMAKE_SOURCE_DIR}/pprinter.py ${GDB_SCRIPT}
                     ${GDB_LOG})
    set_tests_properties(gdb_test_gen PROPERTIES FIXTURES_SETUP gdb_script)

    add_test(NAME gdb_test_run COMMAND gdb ${CMAKE_BINARY_DIR}/gdb_test -x
                                       ${GDB_SCRIPT} --batch)
    set_tests_properties(gdb_test_run PROPERTIES FIXTURES_REQUIRED gdb_script
                                                 FIXTURES_SETUP gdb_log)

    add_test(NAME gdb_test_eval COMMAND gdb_test eval ${GDB_LOG})
    set_tests_properties(gdb_test_eval PROPERTIES FIXTURES_REQUIRED gdb_log)
  endif()
endif()
