#   cmake -S test/consumer -B build-consumer && cmake --build build-consumer
#   ./build-consumer/consumer_app

cmake_minimum_required(VERSION 3.22)

project(nadc_consumer LANGUAGES C)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. nadc)

add_executable(consumer_app main.c)

target_link_libraries(consumer_app PRIVATE nadc::nadc)
