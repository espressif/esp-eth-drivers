# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(phy_tester)

# to be able to set esp_eth verbosity to "VERBOSE"
idf_component_get_property(esp_eth_lib esp_eth COMPONENT_LIB)
target_compile_definitions(${esp_eth_lib} PUBLIC "-DLOG_LOCAL_LEVEL=ESP_LOG_VERBOSE")
