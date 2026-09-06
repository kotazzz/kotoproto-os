add_executable(koto_test_hello test_hello.cpp)
target_link_libraries(koto_test_hello PRIVATE koto_core)
add_test(NAME hello_world COMMAND koto_test_hello)
