cmake_minimum_required(VERSION 3.14)
project(Iscut LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CLI binary
add_executable(iscut src/main.cc src/cut.cc src/segment.cc src/count.cc src/ustr.cc)
target_include_directories(iscut PRIVATE src)

# Python module (only when built via scikit-build / pip install)
if(SKBUILD)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(_iscut src/pip.cc src/cut.cc)
    target_include_directories(_iscut PRIVATE src)
    install(TARGETS _iscut DESTINATION iscut)
    install(FILES python/iscut/__init__.py DESTINATION iscut)
endif()
