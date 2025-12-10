cmake_minimum_required(VERSION 3.10)
enable_language(C)

if (UNIX)
  add_definitions(-D_POSIX_C_SOURCE=200809L)
elseif (WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
endif()

include(CheckSymbolExists)
check_symbol_exists(getrusage sys/resource.h HAVE_GETRUSAGE)

file(GLOB PROJECT_INC_FILES
  "parameters.h"
  "attributes.h"
  "LGSCMisc.h"
  "LGSCCommon.h"
  "LGSCDecoder.h"
  "LGSCEncoder.h"
  "lgsc_chrono.h"
)

file(GLOB PROJECT_CPP_FILES
  "LGSCCommon.cpp"
  "LGSCDecoder.cpp"
  "LGSCEncoder.cpp"
  "lgsc_chrono.cpp"
)

source_group (inc FILES ${PROJECT_INC_FILES})
source_group (input FILES ${PROJECT_IN_FILES})
source_group (cpp FILES ${PROJECT_CPP_FILES})

include_directories(
  "${PROJECT_BINARY_DIR}/source"
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/dependencies/zlib-1.3.1"
)

add_library (lgsc STATIC
  ${PROJECT_CPP_FILES}
  ${PROJECT_INC_FILES}
  ${PROJECT_IN_FILES}
  ${VERSION_FILE}
)

target_link_libraries(lgsc PUBLIC zlibstatic)

target_include_directories(lgsc PUBLIC
  "${PROJECT_BINARY_DIR}/dependencies/zlib-1.3.1"
)

add_executable(lgsc-api-example1 LGSC-api-example1.cpp)
target_link_libraries(lgsc-api-example1 lgsc)
install (TARGETS lgsc-api-example1 DESTINATION bin)



