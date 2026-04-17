idf_component_register(
    SRCS max17055.c
    INCLUDE_DIRS .
    REQUIRES i2cdev log esp_idf_lib_helpers
)

set(ESP_IDF_LIB_CMAKE ${CMAKE_CURRENT_LIST_DIR}/common/cmake/esp-idf-lib.cmake)
if(EXISTS ${ESP_IDF_LIB_CMAKE})
    include(${ESP_IDF_LIB_CMAKE})
else()
    message(WARNING "${ESP_IDF_LIB_CMAKE} not found")
endif()
