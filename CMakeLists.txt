cmake_minimum_required(VERSION 3.10)
project(PiHydroWave CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Общие флаги
set(COMMON_FLAGS
    -pedantic
    -Wall
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wextra
    -Wshadow
    -Wold-style-cast
    -Woverloaded-virtual
    -Wimplicit-fallthrough
    -Wdouble-promotion
    -Wduplicated-cond
    -fms-extensions
)

find_package(PkgConfig REQUIRED)
find_package(Drogon CONFIG REQUIRED)

pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(LIBSERIALPORT REQUIRED libserialport)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

message(STATUS "JSONCPP include dirs: ${JSONCPP_INCLUDE_DIRS}")
message(STATUS "PROJECT include dirs: ${LIBUSB_INCLUDE_DIRS}")

add_subdirectory(lib/UtilitaryRS)
add_subdirectory(lib/EspNowProto)
add_subdirectory(include)
add_subdirectory(sources)

add_executable(${PROJECT_NAME} sources/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Sources
    Headers
    UtilitaryRS
    EspNowUSBProto
    ${LIBUSB_LIBRARIES}
    ${LIBSERIALPORT_LIBRARIES}
    pthread
    Drogon::Drogon
    ${SQLite3_LIBRARIES}
    ${JSONCPP_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    include
    ${LIBUSB_INCLUDE_DIRS}
    ${LIBSERIALPORT_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_FLAGS})
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
