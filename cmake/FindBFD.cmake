# - Find BFD (bfd.h, config.h, libbfd.so, libbfd.lib)
# This module defines
#  BFD_INCLUDE_DIR, directory containing headers
#  BFD_LIBRARY_DIR, directory containing bfd libraries
#  BFD_STATIC_LIBS, path to libbfd*.a/libbfd*.lib
#  BFD_SHARED_LIB_RESOURCES, shared libraries required to use BFD, i.e. libbfd*.so
#  BFD_FOUND, whether bfd has been found

if ("${BFD_ROOT}" STREQUAL "")
  set(BFD_ROOT "$ENV{BFD_ROOT}")
  if (NOT "${BFD_ROOT}" STREQUAL "")
    string(REPLACE "\"" "" BFD_ROOT ${BFD_ROOT})
  else()
    set(BFD_ROOT "/usr")
  endif()
endif()

if ("${BFD_ROOT_SUFFIX}" STREQUAL "")
  set(BFD_ROOT_SUFFIX "$ENV{BFD_ROOT_SUFFIX}")
  if (NOT "${BFD_ROOT_SUFFIX}" STREQUAL "")
    string(REPLACE "\"" "" BFD_ROOT_SUFFIX ${BFD_ROOT_SUFFIX})
  endif()
endif()

set(BFD_SEARCH_HEADER_PATHS
  ${BFD_ROOT}/bfd
  ${BFD_ROOT}/include
  ${BFD_ROOT}/local/include
)

set(BFD_SEARCH_LIB_PATH
  ${BFD_ROOT}/lib
  ${BFD_ROOT}/local/lib
  ${BFD_ROOT}/libiberty
  ${BFD_ROOT}/zlib
)

set(UNIX_DEFAULT_INCLUDE "/usr/include")

find_path(BFD_INCLUDE_DIR_ANSIDECL
  ansidecl.h
  PATHS ${BFD_SEARCH_HEADER_PATHS} ${UNIX_DEFAULT_INCLUDE}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)
find_path(BFD_INCLUDE_DIR_BFD
  bfd.h
  PATHS ${BFD_SEARCH_HEADER_PATHS} ${UNIX_DEFAULT_INCLUDE}
  NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

include(Utils)
set(UNIX_DEFAULT_LIB "/usr/lib")


# set options for: shared
set(BFD_LIBRARY_PREFIX "lib")
set(BFD_LIBRARY_SUFFIX ".so")
set_find_library_options("${BFD_LIBRARY_PREFIX}" "${BFD_LIBRARY_SUFFIX}")

# restore initial options
restore_find_library_options()


# set options for: static
set(BFD_LIBRARY_PREFIX "lib")
set(BFD_LIBRARY_SUFFIX ".a")
set_find_library_options("${BFD_LIBRARY_PREFIX}" "${BFD_LIBRARY_SUFFIX}")

# find library
find_library(BFD_STATIC_LIB
  NAMES bfd
  PATHS ${BFD_SEARCH_LIB_PATH} ${UNIX_DEFAULT_LIB}
  PATH_SUFFIXES ${BFD_ROOT_SUFFIX}
  NO_DEFAULT_PATH
)
find_library(BFD_STATIC_LIB_IBERTY
  NAMES iberty
  PATHS ${BFD_SEARCH_LIB_PATH} ${UNIX_DEFAULT_LIB}
  PATH_SUFFIXES ${BFD_ROOT_SUFFIX}
  NO_DEFAULT_PATH
)
find_library(BFD_STATIC_LIB_Z
  NAMES z
  PATHS ${BFD_SEARCH_LIB_PATH} ${UNIX_DEFAULT_LIB}
  PATH_SUFFIXES ${BFD_ROOT_SUFFIX}
  NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()


if (BFD_INCLUDE_DIR_ANSIDECL AND BFD_INCLUDE_DIR_BFD AND BFD_STATIC_LIB AND BFD_STATIC_LIB_IBERTY AND BFD_STATIC_LIB_Z)
   message("BFD_FOUND")
  set(BFD_FOUND TRUE)
  list(APPEND BFD_INCLUDE_DIR ${BFD_INCLUDE_DIR_ANSIDECL} ${BFD_INCLUDE_DIR_BFD})
  list(APPEND BFD_STATIC_LIBS ${BFD_STATIC_LIB} ${BFD_STATIC_LIB_IBERTY} ${BFD_STATIC_LIB_Z})
  set(BFD_LIBRARY_DIR
    "${BFD_SEARCH_LIB_PATH}"
    CACHE PATH
    "Directory containing bfd libraries"
    FORCE
  )

else ()
  set(BFD_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BFD
  DEFAULT_MSG
  BFD_INCLUDE_DIR
  BFD_STATIC_LIBS
  BFD_INCLUDE_DIR_ANSIDECL
  BFD_INCLUDE_DIR_BFD
  BFD_STATIC_LIB
  BFD_STATIC_LIB_IBERTY
  BFD_STATIC_LIB_Z
)
message("BFD_INCLUDE_DIR: " ${BFD_INCLUDE_DIR})
message("BFD_LIBRARY_DIR: " ${BFD_LIBRARY_DIR})
message("BFD_STATIC_LIBS: " ${BFD_STATIC_LIBS})

mark_as_advanced(
  BFD_INCLUDE_DIR
  BFD_LIBRARY_DIR
  BFD_STATIC_LIBS
  BFD_INCLUDE_DIR_ANSIDECL
  BFD_INCLUDE_DIR_BFD
  BFD_STATIC_LIB
  BFD_STATIC_LIB_IBERTY
  BFD_STATIC_LIB_Z
)
