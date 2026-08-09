cmake_minimum_required(VERSION 3.16)
project(binance_depth_recorder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(nlohmann_json REQUIRED)

set(LIB_SOURCES
    src/time_utils.cpp
    src/decimal_scaler.cpp
    src/csv_writer.cpp
    src/csv_reader.cpp
    src/app_config.cpp
    src/json_parsers.cpp
    src/order_book.cpp
    src/event_processor.cpp
    src/replay.cpp
    src/binance_ws_client.cpp
    src/logging.cpp
)

add_library(capture_lib STATIC ${LIB_SOURCES})
target_include_directories(capture_lib PUBLIC include)
target_link_libraries(capture_lib PUBLIC
    Boost::boost
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    nlohmann_json::nlohmann_json
)

add_executable(binance_capture src/main.cpp)
target_link_libraries(binance_capture PRIVATE capture_lib)

add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE capture_lib)
target_compile_definitions(unit_tests PRIVATE
    SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)

add_custom_target(tests COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
