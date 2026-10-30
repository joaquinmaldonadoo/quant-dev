add_executable(hft_tests
  test_config.cpp
  test_order_book.cpp
  test_feed.cpp
  test_features.cpp
  test_strategy_risk.cpp
  test_execution.cpp
)
target_link_libraries(hft_tests PRIVATE hft_core GTest::gtest_main)
target_compile_definitions(hft_tests PRIVATE HFT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

include(GoogleTest)
gtest_discover_tests(hft_tests)
