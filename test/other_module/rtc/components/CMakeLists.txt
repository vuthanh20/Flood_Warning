# rtc/components/CMakeLists.txt
idf_component_register(SRCS "src/ds3231_rtc.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver)