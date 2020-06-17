cmake_minimum_required(VERSION 3.11)

Project(Dashboard)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# List source files
file(GLOB_RECURSE KRN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.*)

add_executable(Dashboard.Kern ${KRN_SRC})
target_include_directories(Dashboard.Kern PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source/)

# Get packages
find_package(Threads REQUIRED)
target_link_libraries(Dashboard.Kern PRIVATE Threads::Threads)

find_package(unofficial-iconv CONFIG REQUIRED)
target_link_libraries(Dashboard.Kern PRIVATE unofficial::iconv::libiconv unofficial::iconv::libcharset)

find_package(GIT2 REQUIRED)
target_include_directories(Dashboard.Kern PRIVATE ${GIT2_INCLUDE_DIR})
target_link_libraries(Dashboard.Kern PRIVATE ${GIT2_LIBRARIES})

find_package(date CONFIG REQUIRED)
target_link_libraries(Dashboard.Kern PRIVATE date::date date::tz)

find_package(CURL CONFIG REQUIRED)
target_link_libraries(Dashboard.Kern PRIVATE CURL::libcurl)
