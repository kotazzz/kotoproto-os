add_executable(koto_sim
  src/main.cpp
  src/hal_sim.cpp
  src/http_server.cpp
)

target_include_directories(koto_sim PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(koto_sim PRIVATE koto_core)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/www" KOTO_WWW_PATH)
target_compile_definitions(koto_sim PRIVATE "KOTO_WWW_DIR=\"${KOTO_WWW_PATH}\"")

if(WIN32)
  target_link_libraries(koto_sim PRIVATE ws2_32)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(koto_sim PRIVATE Threads::Threads)
