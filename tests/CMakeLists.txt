include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(test_order_book test_order_book.cpp)
target_include_directories(test_order_book PRIVATE ../include)
target_link_libraries(test_order_book GTest::gtest)

include(GoogleTest)
gtest_discover_tests(test_order_book)
