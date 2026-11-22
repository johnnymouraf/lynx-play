# CMake 4 removed FindPythonInterp, which chiaki-ng still calls. This maps it
# onto FindPython3 and sets the variables chiaki-ng and nanopb read.

if(PYTHON_EXECUTABLE)
    set(Python3_EXECUTABLE "${PYTHON_EXECUTABLE}")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)

set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}" CACHE FILEPATH "Python 3 interpreter" FORCE)
set(PYTHON_VERSION_STRING "${Python3_VERSION}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PythonInterp
    REQUIRED_VARS PYTHON_EXECUTABLE
    VERSION_VAR PYTHON_VERSION_STRING)
