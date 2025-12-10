cmake_minimum_required(VERSION 3.10)
project (lgsc CXX)
set(CMAKE_CXX_STANDARD 14)

include( CheckCXXCompilerFlag )
CHECK_CXX_COMPILER_FLAG( "-std=c++11" COMPILER_SUPPORTS_CXX11 )
CHECK_CXX_COMPILER_FLAG( "-std=c++0x" COMPILER_SUPPORTS_CXX0X )

# Video encoder and decoder:

if (COMPILER_SUPPORTS_CXX14)
 set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14" )
elseif (COMPILER_SUPPORTS_CXX11)
 set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
elseif (COMPILER_SUPPORTS_CXX0X)
 set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )
else ()
 MESSAGE(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif ()

set(ZLIB_BUILD_EXAMPLES OFF)
add_subdirectory(dependencies/zlib-1.3.1)
add_subdirectory(source)
