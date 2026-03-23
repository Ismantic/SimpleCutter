cmake_minimum_required(VERSION 3.14)
project(IsmaCut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CLI binary
add_executable(ismacut src/main.cc src/cut.cc src/segment.cc src/count.cc src/ustr.cc)
target_include_directories(ismacut PRIVATE src)

# Python module (only when built via scikit-build / pip install)
if(SKBUILD)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(_ismacut src/pip.cc src/cut.cc)
    target_include_directories(_ismacut PRIVATE src)
    install(TARGETS _ismacut DESTINATION ismacut)
    install(FILES python/ismacut/__init__.py DESTINATION ismacut)
endif()
