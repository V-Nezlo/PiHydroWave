add_library(Headers INTERFACE)
target_include_directories(Headers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Headers INTERFACE UtilitaryRS)
target_link_libraries(Headers INTERFACE EspNowUSBProto)
