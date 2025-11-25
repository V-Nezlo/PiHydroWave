file(GLOB_RECURSE SOURCE_LIST CONFIGURE_DEPENDS "*.cpp")
list(FILTER SOURCE_LIST EXCLUDE REGEX "main\\.cpp$")

add_library(Sources OBJECT ${SOURCE_LIST})

target_include_directories(Sources PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(Sources PRIVATE UtilitaryRS)
target_compile_options(Sources PRIVATE ${COMMON_WARN_FLAGS})
