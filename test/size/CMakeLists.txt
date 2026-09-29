# Code size probes: object files built with arm-none-eabi-g++ at -Os for
# Cortex-M4 and measured by report.py, plus a warning check of every function.
# `make size` at the repository root builds and reports them.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(ZLL_SIZE_TOOL arm-none-eabi-size REQUIRED)

set(ZLL_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set(ZLL_TARGET -mcpu=cortex-m4 -mthumb -fno-exceptions -fno-rtti
               -fno-threadsafe-statics)

set(ZLL_PROBES "")
function(zll_probe name source)
  add_library(${name} OBJECT ${source})
  target_include_directories(${name} PRIVATE ${ZLL_INCLUDE})
  target_compile_options(${name} PRIVATE -Os ${ZLL_TARGET} -ffunction-sections
                                         -fdata-sections)
  target_compile_definitions(${name} PRIVATE ZLL_PROBE_NODES=16 ${ARGN})
  set(ZLL_PROBES
      "${ZLL_PROBES}    \"${name}\": \"$<TARGET_OBJECTS:${name}>\",\n"
      PARENT_SCOPE)
endfunction()

foreach(sites 1 2 4 8)
  zll_probe(ops_${sites} probe_ops.cpp ZLL_PROBE_SITES=${sites})
endforeach()
zll_probe(algos probe_algos.cpp)
zll_probe(heap probe_heap.cpp)

# The warning flags of the CI presets, at every optimization level
set(ZLL_WARNINGS
    -Wall
    -Wextra
    -Werror
    -Wfatal-errors
    -Wno-comment
    -Wpedantic
    -Wcast-qual
    -Wshadow
    -Wnon-virtual-dtor
    -Wunused
    -Wcast-align
    -Wdouble-promotion
    -Wfloat-equal
    -Wsign-conversion
    -Wconversion)
foreach(opt O0 Os O2 O3)
  add_library(coverage_${opt} OBJECT coverage.cpp)
  target_include_directories(coverage_${opt} PRIVATE ${ZLL_INCLUDE})
  target_compile_options(coverage_${opt} PRIVATE -${opt} ${ZLL_TARGET}
                                                 ${ZLL_WARNINGS})
  target_compile_definitions(coverage_${opt} PRIVATE ZLL_DEFAULT_ASSERT)
endforeach()

string(REGEX REPLACE ",\n$" "\n" ZLL_PROBES "${ZLL_PROBES}")
file(
  GENERATE
  OUTPUT ${CMAKE_BINARY_DIR}/probes.json
  CONTENT
    "{\n  \"compiler\": \"${CMAKE_CXX_COMPILER_VERSION}\",\n  \"size_tool\": \"${ZLL_SIZE_TOOL}\",\n  \"probes\": {\n${ZLL_PROBES}  }\n}\n"
)
