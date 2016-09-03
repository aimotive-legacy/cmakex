#include "resource.h"

namespace cmakex {
const char* k_find_package_try_config_first_module_content = R"~~~~(#.rst:
# FindPackageTryConfigFirst
# -------------------------
#
# Helper macro to write find modules which hijack the offical find modules
# from the CMake distribution and try to find the config module first:
#
# Usage:
# ^^^^^^
#
# Create a find module for a package, e.g. ``FindZLIB.cmake`` with this
# line:
#
#   include(FindPackageTryConfigFirst)
#   find_package_try_config_first()
#
# Put the file along with this file in a directory and add the directory
# to ``CMAKE_MODULE_PATH``.
#
# When a ``CMakeLists.txt`` calls ``find_package(ZLIB)`` CMake will
# find your `FindZLIB.cmake` before the official module. This macro will
# attempt to find the ZLIB config module first. On failure, forwards the
# arguments to the original (module-mode) find_package.
#
# Note: the ``OPTIONAL_COMPONENTS`` and ``NO_POLICY_SCOPE`` parameters
# are not handled.

macro(find_package_try_config_first)
    get_filename_component(_FPTCF_find_module_filename ${CMAKE_CURRENT_LIST_FILE} NAME)
    string(REGEX MATCH "^Find(.+)[.]cmake$" _ ${_FPTCF_find_module_filename})
    set(_FPTCF_package_name "${CMAKE_MATCH_1}")

    if(NOT _FPTCF_package_name)
        message(FATAL_ERROR "FindPackageTryConfigFirst must be included from a find module, it was called from ${_FPTCF_find_module_filename}")
    endif()

    string(TOUPPER "${_FPTCF_package_name}" _FPTCF_package_name_upper)

    set(_FPTCF_ARGV "${_FPTCF_package_name}")

    list(APPEND _FPTCF_ARGV ${${_FPTCF_package_name}_FIND_VERSION})

    if(${_FPTCF_package_name}_FIND_VERSION_EXACT)
        list(APPEND _FPTCF_ARGV "EXACT")
    endif()

    if(${_FPTCF_package_name}_FIND_COMPONENTS)
        list(APPEND _FPTCF_ARGV COMPONENTS ${${_FPTCF_package_name}_FIND_COMPONENTS})
    endif()

    if(FPTCF_VERBOSE)
        message(STATUS "find_package(${_FPTCF_ARGV} NO_MODULE QUIET)")
    endif()

    find_package(${_FPTCF_ARGV} NO_MODULE QUIET)

    if(FPTCF_VERBOSE)
        cmake_print_variables(${_FPTCF_package_name}_DIR ${_FPTCF_package_name}_FOUND)
    endif()

    set(_FPTCF_REPEAT_FAILED_CONFIG_IF_NOT_QUIET_OR_REQUIRED 0)
    if(${_FPTCF_package_name}_DIR)
        if(FPTCF_VERBOSE)
            message(STATUS "[FPTCF] ${_FPTCF_package_name} config module has been found.")
            message(STATUS "[FPTFC] ${_FPTCF_package_name}_DIR: ${${_FPTCF_package_name}_DIR}")
        endif()
        # The config module has been found ...
        if(NOT ${_FPTCF_package_name}_FOUND AND
            NOT ${_FPTCF_package_name_upper}_FOUND
        )
            # ... but the package is considered not found
            # In this case we don't fall back to the find module (since the config
            # module has indeed been found). Instead, if the original find_package
            # call was not QUIET then repeat the call without QUIET and with optional
            # REQUIRED to display the required error messages.
            if(FPTCF_VERBOSE)
                message(STATUS "[FPTCF] But the package has been considered NOT FOUND.")
            endif()
            set(_FPTCF_REPEAT_FAILED_CONFIG_IF_NOT_QUIET_OR_REQUIRED 1)
        else()
            if(FPTCF_VERBOSE)
                message(STATUS "[FPTCF] And the package has also been found.")
            endif()
        endif()
    else()
        # Config module has not been found
        # Fall back to the official find-module if exists
        if(FPTCF_VERBOSE)
            message(STATUS "[FPTCF] ${_FPTCF_package_name} config module has not been found.")
            message(STATUS "[FPTCF] Falling-back to find module.")
        endif()
        if(FPTCF_CMAKE_ROOT_FOR_TESTING)
            set(FPTCF_CMAKE_ROOT "${FPTCF_CMAKE_ROOT_FOR_TESTING}")
        else()
            set(FPTCF_CMAKE_ROOT "${CMAKE_ROOT}")
        endif()
        if(EXISTS "${FPTCF_CMAKE_ROOT}/Modules/${_FPTCF_find_module_filename}")
            include("${FPTCF_CMAKE_ROOT}/Modules/${_FPTCF_find_module_filename}")
        else()
            message(WARNING
                "By not providing \"${_FPTCF_package_name}.cmake\" in CMAKE_MODULE_PATH this project"
                " has asked CMake to find a package configuration file provided by"
                " \"${_FPTCF_package_name}\", but CMake did not find one.")
            set(_FPTCF_REPEAT_FAILED_CONFIG_IF_NOT_QUIET_OR_REQUIRED 1)
        endif()
    endif()
    if(_FPTCF_REPEAT_FAILED_CONFIG_IF_NOT_QUIET_OR_REQUIRED)
        if(NOT ${_FPTCF_package_name}_FIND_QUIETLY OR ${_FPTCF_package_name}_FIND_REQUIRED)
            if(${_FPTCF_package_name}_FIND_REQUIRED)
                list(APPEND _FPTCF_ARGV REQUIRED)
            endif()
            find_package(${_FPTCF_ARGV} NO_MODULE)
        endif()
    endif()
endmacro()
)~~~~";

const char* k_deps_script_wrapper_cmakelists_body = R"~~~~(
include(CMakeParseArguments)

# Extend the official include command URL support.
# Also supports including relative paths in remote scripts, that is
# if the included scripts includes another script in turn on a relative path
# then the relative path will be evaluated relative to the URL of the script
# and not to the local, downloaded temporary copy of the script.
macro(include __CMAKEX_INCL_ARG0)
    cmake_parse_arguments(__CMAKEX_INCL_ARG
        "OPTIONAL;NO_POLICY_SCOPE" "RESULT_VARIABLE" "" ${ARGN})

    if(NOT "${__CMAKEX_INCL_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "include called with invalid argument: ${__CMAKEX_INCL_ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(__CMAKEX_INCL_EXTRA_ARG "")
    if(__CMAKEX_INCL_ARG_NO_POLICY_SCOPE)
        list(APPEND __CMAKEX_INCL_EXTRA_ARG "NO_POLICY_SCOPE")
    endif()
    if(__CMAKEX_INCL_ARG_OPTIONAL)
        list(APPEND __CMAKEX_INCL_EXTRA_ARG "OPTIONAL")
    endif()

    set(__CMAKEX_INCL_PATH "${__CMAKEX_INCL_ARG0}") # make it a real variable
    # if there's a parent URL and it's a relative path, we may construct a new
    # URL out of the two
    if(__CMAKEX_INCL_PARENT_URL_STACK AND NOT IS_ABSOLUTE "${__CMAKEX_INCL_PATH}")
        # if it's only name without extension, then it can be a module and
        # we need to fall-back to official include()
        get_filename_component(__CMAKEX_INCL_NAME_WE "${__CMAKEX_INCL_PATH}"
            NAME_WE)
        set(__CMAKEX_INCL_ITS_A_MODULE 0)
        if("${__CMAKEX_INCL_PATH}" STREQUAL "${__CMAKEX_INCL_NAME_WE}")
            # check this file as a module on the possible paths
            foreach(_d ${CMAKE_MODULE_PATH} ${CMAKE_ROOT}/Modules)
                if(EXISTS "${_d}/${__CMAKEX_INCL_PATH}.cmake")
                    set(__CMAKEX_INCL_ITS_A_MODULE 1)
                    break()
                endif()
            endforeach()
        endif()
        if(NOT __CMAKEX_INCL_ITS_A_MODULE)
            # construct a new path
            list(GET __CMAKEX_INCL_PARENT_URL_STACK -1 _d)
            set(__CMAKEX_INCL_PATH "${_d}/${__CMAKEX_INCL_PATH}")
        endif()
    endif()
    if(NOT "${__CMAKEX_INCL_PATH}" MATCHES "^https?://")
        _include("${__CMAKEX_INCL_PATH}" ${ARGN})
    else()
        # split input path to dir and name component
        # can't use get_filename_component because it contracts "://" to ":/"
        get_filename_component(__CMAKEX_INCL_NAME "${__CMAKEX_INCL_PATH}" NAME)
        string(LENGTH "${__CMAKEX_INCL_PATH}" __CMAKEX_INCL_PATH_LENGTH)
        string(LENGTH "${__CMAKEX_INCL_NAME}" __CMAKEX_INCL_NAME_LENGTH)
        math(EXPR __CMAKEX_INCL_DIR_LENGTH
            "${__CMAKEX_INCL_PATH_LENGTH} - ${__CMAKEX_INCL_NAME_LENGTH}")
        string(SUBSTRING "${__CMAKEX_INCL_PATH}" 0 "${__CMAKEX_INCL_DIR_LENGTH}"
            __CMAKEX_INCL_DIR)

        # construct a descriptive temporary filename to download to
        string(MAKE_C_IDENTIFIER "${__CMAKEX_INCL_DIR}" __CMAKEX_INCL_DIR_CID)
        set(__CMAKEX_INCL_TEMP_FILE
            "${CMAKE_CURRENT_BINARY_DIR}/tmp-download-${__CMAKEX_INCL_DIR_CID}_${__CMAKEX_INCL_NAME}")

        # download, handle error
        message(STATUS "include remote file: ${__CMAKEX_INCL_PATH}")
        file(DOWNLOAD "${__CMAKEX_INCL_PATH}" "${__CMAKEX_INCL_TEMP_FILE}"
            STATUS __CMAKEX_INCL_RESULT SHOW_PROGRESS)
        list(GET __CMAKEX_INCL_RESULT 0 __CMAKEX_INCL_CODE)
        if(__CMAKEX_INCL_CODE)
            if(__CMAKEX_INCL_ARG_RESULT_VARIABLE)
                set("${__CMAKEX_INCL_ARG_RESULT_VARIABLE}" "NOTFOUND")
            endif()
            if(NOT __CMAKEX_INCL_ARG_OPTIONAL)
                message(FATAL_ERROR "include could not download url: ${__CMAKEX_INCL_PATH}, reason: ${__CMAKEX_INCL_RESULT}.")
            endif()
        else()
            # Maintain two stacks. They need to be stacks because the same variables
            # will be updated by recursively called include() commands.
            # One stack for parent urls, other for temp files to be able to remove
            # them after the include.
            list(APPEND __CMAKEX_INCL_PARENT_URL_STACK "${__CMAKEX_INCL_DIR}")
            list(APPEND __CMAKEX_INCL_TEMP_FILE_STACK "${__CMAKEX_INCL_TEMP_FILE}")
            if(__CMAKEX_INCL_ARG_RESULT_VARIABLE)
                set("${__CMAKEX_INCL_ARG_RESULT_VARIABLE}" "${__CMAKEX_INCL_PATH}")
            endif()
            _include("${__CMAKEX_INCL_TEMP_FILE}" ${__CMAKEX_INCL_EXTRA_ARG}
                RESULT_VARIABLE _d)
            if(NOT _d AND __CMAKEX_INCL_ARG_RESULT_VARIABLE)
                set("${__CMAKEX_INCL_ARG_RESULT_VARIABLE}" "NOTFOUND")
            endif()
            list(REMOVE_AT __CMAKEX_INCL_PARENT_URL_STACK -1)
            list(GET __CMAKEX_INCL_TEMP_FILE_STACK -1 _d)
            file(REMOVE "${_d}")
            list(REMOVE_AT __CMAKEX_INCL_TEMP_FILE_STACK -1)
        endif()
    endif()
endmacro()

function(add_pkg NAME)
  # test list compatibility
  set(s ${NAME})
  list(LENGTH s l)
  if (NOT l EQUAL 1)
    message(FATAL_ERROR "\"${NAME}\" is an invalid name for a package")
  endif()
  set(line "${NAME}")
  foreach(x IN LISTS ARGN)
    set(line "${line}\t${x}")
  endforeach()
  file(APPEND "${__CMAKEX_ADD_PKG_OUT}" "${line}\n")
endfunction()

function(def_pkg NAME)
  # test list compatibility
  set(s ${NAME})
  list(LENGTH s l)
  if (NOT l EQUAL 1)
    message(FATAL_ERROR "\"${NAME}\" is an invalid name for a package")
  endif()
  set(line "${NAME}\tDEFINE_ONLY")
  foreach(x IN LISTS ARGN)
    set(line "${line}\t${x}")
  endforeach()
  file(APPEND "${__CMAKEX_ADD_PKG_OUT}" "${line}\n")
endfunction()

# include deps script within a function to protect local variables
function(include_deps_script path)
  if(NOT IS_ABSOLUTE "${path}")
    set(path "${CMAKE_CURRENT_LIST_DIR}/${path}")
  endif()
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "Dependency script not found: \"${path}\".")
  endif()
  include("${path}")
endfunction()

if(DEFINED command)
  message(STATUS "Dependency script wrapper command: ${command}")
  list(GET command 0 verb)

  if(verb STREQUAL "run")
    list(LENGTH command l)
    if(NOT l EQUAL 3)
      message(FATAL_ERROR "Internal error, invalid command")
    endif()
    list(GET command 1 path)
    list(GET command 2 out)
    if(NOT EXISTS "${out}" OR IS_DIRECTORY "${out}")
      message(FATAL_ERROR "Internal error, the output file \"${out}\" is not an existing file.")
    endif()
    set(__CMAKEX_ADD_PKG_OUT "${out}")
    include_deps_script("${path}")
  endif()
else()
  message(STATUS "No command specified.")
endif()
)~~~~";
}
