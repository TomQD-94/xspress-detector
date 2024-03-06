#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Tries to find libxspress headers and libraries.
#
# Usage of this module as follows:
#
#  find_package(libxspress)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LIBXSPRESS_ROOT_DIR  Set this variable to the root installation of libxspress
#
# Variables defined by this module:
#
#  LIBXSPRESS_FOUND              System has libxspress libs/headers
#  LIBXSPRESS_LIBRARIES          The libxspress libraries
#  LIBXSPRESS_INCLUDE_DIRS       The location of libxspress headers

message ("\nLooking for libxspress headers and libraries")

if (LIBXSPRESS_ROOT_DIR)
    message (STATUS "Root dir: ${LIBXSPRESS_ROOT_DIR}")
endif ()

find_package(PkgConfig)
IF (PkgConfig_FOUND)
    message("Using pkgconfig")
    pkg_check_modules(PC_LIBXSPRESS libxspress)
ENDIF(PkgConfig_FOUND)

set(LIBXSPRESS_DEFINITIONS ${PC_LIBXSPRESS_CFLAGS_OTHER})

find_library(
    LIBXSPRESS_LIBRARY
    NAMES
        xspress3
    PATHS
        ${LIBXSPRESS_ROOT_DIR}/lib
        ${PC_LIBXSPRESS_LIBDIR}
        ${PC_LIBXSPRESS_LIBRARY_DIRS}
)

find_library(
    IMGMOD_LIBRARY
    NAMES
        img_mod
    PATHS
        ${LIBXSPRESS_ROOT_DIR}/lib
        ${PC_LIBXSPRESS_LIBDIR}
        ${PC_LIBXSPRESS_LIBRARY_DIRS}
)

find_path(
    LIBXSPRESS_INCLUDE_DIR
    NAMES
        xspress3.h
    PATHS
        ${LIBXSPRESS_ROOT_DIR}/include
        ${PC_LIBXSPRESS_INCLUDEDIR}
        ${PC_LIBXSPRESS_INCLUDE_DIRS}
)

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LIBXSPRESS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(
    LIBXSPRESS
    DEFAULT_MSG
    LIBXSPRESS_LIBRARY
    IMGMOD_LIBRARY
    LIBXSPRESS_INCLUDE_DIR
)

mark_as_advanced(LIBXSPRESS_INCLUDE_DIR LIBXSPRESS_LIBRARY IMGMOD_LIBRARY)

if (LIBXSPRESS_FOUND)
    set(LIBXSPRESS_INCLUDE_DIRS ${LIBXSPRESS_INCLUDE_DIR})
    set(LIBXSPRESS_LIBRARIES ${LIBXSPRESS_LIBRARY} ${IMGMOD_LIBRARY})

    get_filename_component(LIBXSPRESS_LIBRARY_DIR ${LIBXSPRESS_LIBRARY} PATH)
    get_filename_component(LIBXSPRESS_LIBRARY_NAME ${LIBXSPRESS_LIBRARY} NAME_WE)

    mark_as_advanced(LIBXSPRESS_LIBRARY_DIR LIBXSPRESS_LIBRARY_NAME)

    message(STATUS "Include directories: ${LIBXSPRESS_INCLUDE_DIRS}")
    message(STATUS "Libraries: ${LIBXSPRESS_LIBRARIES}")
endif()
