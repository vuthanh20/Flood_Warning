cmake_minimum_required(VERSION 3.16)

# Thêm dòng này nếu bạn dùng hàm example_connect() của ESP-IDF
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(flood_monitoring_system)