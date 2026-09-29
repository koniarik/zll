file(GLOB TESTS *_test.cpp)

include(FetchContent)
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.12
  FIND_PACKAGE_ARGS 2.4.12)
set(DOCTEST_NO_INSTALL
    ON
    CACHE BOOL "")
FetchContent_MakeAvailable(doctest)

add_executable(zll_utest ${TESTS})
target_link_libraries(zll_utest PUBLIC zll doctest::doctest)
target_compile_features(zll_utest INTERFACE cxx_std_20)
add_test(NAME zll_utest COMMAND zll_utest)

# GDB pretty-printer tests (three-phase: gen → run → eval)
add_executable(gdb_test gdb_bin.cpp)
target_link_libraries(gdb_test PUBLIC zll)

find_program(ZLL_GDB gdb)
if(ZLL_GDB AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(GDB_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/gdb_test.gdb)
  set(GDB_LOG ${CMAKE_CURRENT_BINARY_DIR}/gdb_test.log)

  add_test(NAME gdb_test_gen
           COMMAND gdb_test gen ${PROJECT_SOURCE_DIR}/pprinter.py ${GDB_SCRIPT}
                   ${GDB_LOG})
  set_tests_properties(gdb_test_gen PROPERTIES FIXTURES_SETUP gdb_script)

  add_test(NAME gdb_test_run COMMAND ${ZLL_GDB} $<TARGET_FILE:gdb_test> -x
                                     ${GDB_SCRIPT} --batch)
  set_tests_properties(gdb_test_run PROPERTIES FIXTURES_REQUIRED gdb_script
                                               FIXTURES_SETUP gdb_log)

  add_test(NAME gdb_test_eval COMMAND gdb_test eval ${GDB_LOG})
  set_tests_properties(gdb_test_eval PROPERTIES FIXTURES_REQUIRED gdb_log)
endif()
