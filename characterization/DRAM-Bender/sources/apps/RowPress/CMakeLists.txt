cmake_minimum_required(VERSION 3.11)
project(SoftMC_RowPress)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -O3 -g")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DROWPRESS_DEBUG")

include(ExternalProject)
######### Build spdlog #########
ExternalProject_Add(spdlog_p
  SOURCE_DIR  ${CMAKE_SOURCE_DIR}/ext/spdlog/
  INSTALL_DIR ${CMAKE_BINARY_DIR}/ext/
  CMAKE_ARGS  -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
              -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
              -DSPDLOG_BUILD_SHARED=OFF
              -DSPDLOG_BUILD_EXAMPLE=OFF
)
add_library(spdlog STATIC IMPORTED)
set_target_properties(spdlog PROPERTIES
	IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ext/lib/${CMAKE_STATIC_LIBRARY_PREFIX}spdlog${CMAKE_STATIC_LIBRARY_SUFFIX}
)
include_directories(${CMAKE_SOURCE_DIR}/ext/spdlog/include)
################################

########## Magic Enum ##########
include_directories(${CMAKE_SOURCE_DIR}/ext/magic_enum/)
################################

find_package(Boost COMPONENTS thread program_options REQUIRED)

aux_source_directory(${CMAKE_SOURCE_DIR}/src SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/../../api SRCS)
include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/../../api
)

add_executable(SoftMC_RowPress ${SRCS}) 
add_dependencies(SoftMC_RowPress spdlog_p)
target_link_libraries(
  SoftMC_RowPress PRIVATE
  Boost::thread
	Boost::program_options
  spdlog
)