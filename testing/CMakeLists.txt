# Test registration, included when HTTPANT_BUILD_TESTS is on.

find_package(ut CONFIG REQUIRED)

add_library(httpant-test-support STATIC)
target_sources(httpant-test-support PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${PROJECT_SOURCE_DIR}/import"
  FILES "${PROJECT_SOURCE_DIR}/import/boost.ut.ixx")
target_include_directories(httpant-test-support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(httpant-test-support PUBLIC httpant-example-support Boost::ut)

function(add_httpant_suite NAME)
  set(TARGET httpant-${NAME}-test)
  add_executable(${TARGET} main.cpp ${ARGN})
  target_link_libraries(${TARGET} PRIVATE httpant-test-support)
  add_test(NAME httpant.${NAME} COMMAND ${TARGET})
  set_tests_properties(httpant.${NAME} PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
endfunction()

add_httpant_suite(caching caching.cpp)
add_httpant_suite(conformance conformance.cpp)
add_httpant_suite(conformance-h3 conformance_h3.cpp)
add_httpant_suite(connect connect.cpp)
add_httpant_suite(cookie cookie.cpp)
add_httpant_suite(coroutine coroutine/api.cpp)
add_httpant_suite(error error.cpp)
add_httpant_suite(execution execution/api.cpp execution/boundary.cpp execution/lifetime.cpp)
add_httpant_suite(fetch fetch.cpp)
add_httpant_suite(http1-framing http1_framing.cpp)
add_httpant_suite(message message.cpp)
add_httpant_suite(protocol protocol.cpp)
add_httpant_suite(semantics semantics.cpp)
add_httpant_suite(task task.cpp)
add_httpant_suite(tls tls.cpp)
add_httpant_suite(websocket websocket.cpp)
set_tests_properties(httpant.tls PROPERTIES FIXTURES_REQUIRED tls-credentials)

if(HTTPANT_ENABLE_IO)
  # The engine tests link httpant-io alone, never httpant-test-support: the
  # engine is protocol-independent by design and its tests keep it so.
  add_executable(httpant-io-native-test io/native.cpp)
  target_link_libraries(httpant-io-native-test PRIVATE httpant-io)
  add_test(NAME httpant.io.native COMMAND httpant-io-native-test "${HTTPANT_DATA_DIR}")
  add_executable(httpant-io-execution-test io/execution.cpp)
  target_link_libraries(httpant-io-execution-test PRIVATE httpant-io)
  add_test(NAME httpant.io.execution COMMAND httpant-io-execution-test "${HTTPANT_DATA_DIR}")
  set_tests_properties(httpant.io.native httpant.io.execution PROPERTIES TIMEOUT 60)
endif()

add_executable(httpant-certificate certificate.cpp)
target_link_libraries(httpant-certificate PRIVATE httpant-example-support)
add_test(NAME tls-credentials COMMAND "${CMAKE_COMMAND}"
  "-DDIRECTORY=${PROJECT_BINARY_DIR}" "-DGENERATOR=$<TARGET_FILE:httpant-certificate>"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/quic.cmake")
set_tests_properties(tls-credentials PROPERTIES FIXTURES_SETUP tls-credentials)
