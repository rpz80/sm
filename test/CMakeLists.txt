project("sm_test")

file(GLOB_RECURSE SRC "*.cpp" "*.h")
add_executable(${PROJECT_NAME} ${SRC})

target_link_libraries(${PROJECT_NAME} -pthread)
if (NOT MSVC)
  target_link_libraries(-pthread)
endif()
