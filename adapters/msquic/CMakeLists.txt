cmake_minimum_required(VERSION 4.5)
project(httpant-msquic VERSION 0.1.0 LANGUAGES CXX)

option(HTTPANT_MSQUIC_BUILD_TESTS "Build the MsQuic adapter tests" ${PROJECT_IS_TOP_LEVEL})
if(WIN32)
  set(QUIC_TLS_LIB schannel CACHE STRING "TLS Library to use")
else()
  set(QUIC_TLS_LIB openssl CACHE STRING "TLS Library to use")
  set(QUIC_USE_EXTERNAL_OPENSSL ON)
endif()
if(QUIC_TLS_LIB STREQUAL "schannel")
  set(CREDENTIALS schannel)
else()
  set(CREDENTIALS openssl)
endif()

## httpant core
#
# The adapter is a consumer of httpant, not part of it. It builds the core
# from a sibling checkout unless the parent already added it.

set(HTTPANT_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "httpant core checkout")
if(NOT TARGET httpant)
  set(HTTPANT_ENABLE_IO ON)
  set(HTTPANT_ENABLE_TLS OFF)
  if(HTTPANT_MSQUIC_BUILD_TESTS AND CREDENTIALS STREQUAL "openssl")
    set(HTTPANT_ENABLE_TLS ON)
  endif()
  set(HTTPANT_IO_BACKEND asio)
  add_subdirectory("${HTTPANT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/httpant" EXCLUDE_FROM_ALL)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

## msquic
#
# msquic builds from source at the release the adapter targets; the vcpkg port
# lags behind it. TLS comes from the platform on Windows (schannel) and from the
# manifest's OpenSSL, which provides the QUIC API since 3.5, elsewhere.
# Windows also needs the upstream XDP headers. msquic's install rules are skipped:
# its package would point into this build tree, and the consumer deploys the
# shared library itself.

include(FetchContent)
set(QUIC_BUILD_SHARED ON)
set(QUIC_EMBED_GIT_HASH OFF)
set(MSQUIC_SUBMODULES "")
if(WIN32)
  set(MSQUIC_SUBMODULES submodules/xdp-for-windows)
endif()
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  set(MSQUIC_CLANG_CL ON)
  set(QUIC_SOURCE_LINK OFF)
endif()
FetchContent_Declare(msquic
  GIT_REPOSITORY https://github.com/microsoft/msquic.git
  GIT_TAG v2.6.4
  GIT_SHALLOW TRUE
  GIT_SUBMODULES "${MSQUIC_SUBMODULES}"
  GIT_SUBMODULES_RECURSE FALSE
  EXCLUDE_FROM_ALL)
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_MakeAvailable(msquic)
unset(CMAKE_SKIP_INSTALL_RULES)

if(MSQUIC_CLANG_CL)
  # MsQuic's Windows build uses MSVC's warning and analysis settings.
  get_target_property(OPTIONS warnings INTERFACE_COMPILE_OPTIONS)
  list(REMOVE_ITEM OPTIONS /WX)
  set_property(TARGET warnings PROPERTY INTERFACE_COMPILE_OPTIONS "${OPTIONS}")
  # clang-cl lacks /analyze and ignores the Release /GL option.
  foreach(TARGET core msquic_platform msquic)
    get_target_property(OPTIONS ${TARGET} COMPILE_OPTIONS)
    if(OPTIONS)
      list(REMOVE_ITEM OPTIONS /analyze)
      set_property(TARGET ${TARGET} PROPERTY COMPILE_OPTIONS "${OPTIONS}")
    endif()
    target_compile_options(${TARGET} PRIVATE -Qunused-arguments)
  endforeach()
  target_compile_options(msquic_platform PRIVATE /clang:-Wno-error=incompatible-pointer-types /clang:-Wno-error=int-conversion)
endif()

add_library(httpant-msquic STATIC)
target_sources(httpant-msquic PUBLIC FILE_SET CXX_MODULES
  BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
  FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/import/msquic.ixx"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/${CREDENTIALS}.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/quic.cppm")
target_include_directories(httpant-msquic SYSTEM PRIVATE "${msquic_SOURCE_DIR}/src/inc")
target_link_libraries(httpant-msquic PUBLIC httpant::httpant httpant::io msquic)
add_library(httpant::msquic ALIAS httpant-msquic)

if(HTTPANT_MSQUIC_BUILD_TESTS)
  enable_testing()
  if(NOT TARGET httpant-io-support)
    add_library(httpant-io-support STATIC)
    target_sources(httpant-io-support PUBLIC FILE_SET CXX_MODULES
      BASE_DIRS "${HTTPANT_SOURCE_DIR}/examples" FILES "${HTTPANT_SOURCE_DIR}/examples/io.ixx")
    target_link_libraries(httpant-io-support PUBLIC httpant::httpant httpant::io)
  endif()

  add_executable(httpant-msquic-test testing/quic.cpp "testing/${CREDENTIALS}.cpp")
  target_link_libraries(httpant-msquic-test PRIVATE httpant::msquic httpant-io-support)

  if(CREDENTIALS STREQUAL "schannel")
    set(HTTPANT_MSQUIC_TEST_THUMBPRINT "" CACHE STRING "Thumbprint of a trusted localhost certificate in CurrentUser\\My")
    if(HTTPANT_MSQUIC_TEST_THUMBPRINT)
      add_test(NAME msquic COMMAND httpant-msquic-test "${HTTPANT_MSQUIC_TEST_THUMBPRINT}")
      set_tests_properties(msquic PROPERTIES TIMEOUT 60
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:msquic>")
    endif()
  else()
    if(NOT TARGET httpant-certificate)
      if(NOT TARGET httpant-tls-imports)
        message(FATAL_ERROR "MsQuic tests require HTTPANT_ENABLE_TLS for the certificate generator")
      endif()
      add_executable(httpant-certificate "${HTTPANT_SOURCE_DIR}/testing/certificate.cpp")
      target_link_libraries(httpant-certificate PRIVATE httpant-tls-imports)
    endif()
    add_test(NAME msquic-credentials COMMAND httpant-certificate "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(msquic-credentials PROPERTIES FIXTURES_SETUP msquic-credentials)
    add_test(NAME msquic COMMAND httpant-msquic-test "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(msquic PROPERTIES FIXTURES_REQUIRED msquic-credentials TIMEOUT 60)
  endif()
endif()
