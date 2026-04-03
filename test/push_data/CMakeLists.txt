cmake_minimum_required(VERSION 3.16)

# Thêm dòng này để báo cho trình biên dịch biết đường dẫn tới thư viện Wi-Fi mẫu
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(push_data)