cmake_minimum_required(VERSION 3.16)
project(FortranBoundsChecker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Locate LLVM-18 ───────────────────────────────────────────────────────────
set(LLVM_DIR /usr/lib/llvm-18/lib/cmake/llvm)
find_package(LLVM 18 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_VERSION}: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# ── Flang include directories ────────────────────────────────────────────────
include_directories(/usr/lib/llvm-18/include/flang)
include_directories(/usr/lib/llvm-18/include)

# ── Flang static libraries ───────────────────────────────────────────────────
set(FLANG_LIB_DIR /usr/lib/llvm-18/lib)
set(FLANG_LIBS
    ${FLANG_LIB_DIR}/libFortranParser.a
    ${FLANG_LIB_DIR}/libFortranSemantics.a
    ${FLANG_LIB_DIR}/libFortranEvaluate.a
    ${FLANG_LIB_DIR}/libFortranCommon.a
    ${FLANG_LIB_DIR}/libFortranDecimal.a
    ${FLANG_LIB_DIR}/libflangFrontend.a
    ${FLANG_LIB_DIR}/libflangFrontendTool.a
)

# ── LLVM component libraries ─────────────────────────────────────────────────
llvm_map_components_to_libnames(LLVM_LIBS
    Support Core Option
)

# ── Bounds checker executable ────────────────────────────────────────────────
add_executable(bounds-checker
    src/main.cpp
)

target_include_directories(bounds-checker PRIVATE src)

target_link_libraries(bounds-checker
    ${FLANG_LIBS}
    ${LLVM_LIBS}
    -lpthread -ldl -lm
)

# Installation
install(TARGETS bounds-checker DESTINATION bin)
