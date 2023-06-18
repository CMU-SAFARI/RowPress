cmake_minimum_required(VERSION 3.10)

project(Ramulator)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DRAMULATOR_DEBUG")

if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.4)
    message(FATAL_ERROR "Ramulator requires g++ 8.4+.")
endif()

include(ExternalProject)
#### Build yaml-cpp ####
ExternalProject_Add(
	yaml_cpp_p
	SOURCE_DIR  ${CMAKE_SOURCE_DIR}/ext/yaml-cpp/
	INSTALL_DIR ${CMAKE_BINARY_DIR}/ext/
	CMAKE_ARGS -DYAML_CPP_BUILD_TESTS=OFF 
               -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
               -DCMAKE_BUILD_TYPE=Release
)

add_library(yaml-cpp UNKNOWN IMPORTED)
set_target_properties(yaml-cpp PROPERTIES
	IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ext/lib/${CMAKE_STATIC_LIBRARY_PREFIX}yaml-cpp${CMAKE_STATIC_LIBRARY_SUFFIX}
)
include_directories(${CMAKE_SOURCE_DIR}/ext/yaml-cpp/include)
########

######### Build spdlog #########
ExternalProject_Add(spdlog_p
    SOURCE_DIR  ${CMAKE_SOURCE_DIR}/ext/spdlog/
    INSTALL_DIR ${CMAKE_BINARY_DIR}/ext/
    CMAKE_ARGS  -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
                -DCMAKE_BUILD_TYPE=Release
                -DSPDLOG_BUILD_SHARED=OFF
)
add_library(spdlog STATIC IMPORTED)
set_target_properties(spdlog PROPERTIES
	IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ext/lib/${CMAKE_STATIC_LIBRARY_PREFIX}spdlog${CMAKE_STATIC_LIBRARY_SUFFIX}
)
include_directories(${CMAKE_SOURCE_DIR}/ext/spdlog/include)
################################

aux_source_directory(${CMAKE_SOURCE_DIR}/src SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/Controller SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/DRAMStandard SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/Processor SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/Statistics SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/RowHammer SRCS)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Controller
    ${CMAKE_SOURCE_DIR}/src/DRAMStandard
    ${CMAKE_SOURCE_DIR}/src/Processor
    ${CMAKE_SOURCE_DIR}/src/Statistics
    ${CMAKE_SOURCE_DIR}/src/RowHammer
    )

option(RAMULATOR_POWER "Whether to enable the DRAMPower based power model in Ramulator" OFF)
if (RAMULATOR_POWER)
    message("Building Ramulator with power model. To disable it use \"-DRAMULATOR_POWER=OFF\"")
    aux_source_directory(${CMAKE_SOURCE_DIR}/src/Power SRCS)
    include_directories(${CMAKE_SOURCE_DIR}/src/Power)
    add_definitions(-DRAMULATOR_POWER)
endif()
unset(RAMULATOR_POWER CACHE)

option(GEM5_LIB "Whether to build Ramulator as a library for GEM5" OFF)
if (GEM5_LIB)
    message("Building Ramulator as a library for GEM5. To build it as a standalone simulator, use \"-DGEM5_LIB=OFF\"")
    list(REMOVE_ITEM SRCS ${CMAKE_SOURCE_DIR}/src/Main.cpp)
    add_library(ramulator STATIC ${SRCS}) 
else()
    message("Building Ramulator as a standalone simulator. To build it as a library for GEM5, use \"-DGEM5_LIB=ON\"")
    add_definitions(-DRAMULATOR)
    list(REMOVE_ITEM SRCS ${CMAKE_SOURCE_DIR}/src/Gem5Wrapper.cpp)
    add_executable(ramulator ${SRCS}) 
endif()
unset(GEM5_LIB CACHE)

add_dependencies(ramulator yaml_cpp_p spdlog_p)
target_link_libraries(ramulator yaml-cpp spdlog "-static")