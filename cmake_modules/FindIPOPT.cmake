# Get package info using pkg-config
find_package(PkgConfig)
pkg_search_module(IPOPT ipopt)

include(canonicalize_paths)
canonicalize_paths(IPOPT_LIBRARY_DIRS)

# add osx frameworks to IPOPT_LIBRARIES
if(IPOPT_LIBRARIES)
  if(APPLE)
    # turn "-framework;foo;-framework;bar;other" into "-framework foo;-framework bar;other"
    STRING(REPLACE "-framework;" "-framework " IPOPT_LDFLAGS_OTHER "${IPOPT_LDFLAGS_OTHER}")
    # take "-framework foo;-framework bar;other" and add only frameworks to IPOPT_LIBRARIES
    foreach(arg ${IPOPT_LDFLAGS_OTHER})
      if("${arg}" MATCHES "-framework .+")
        SET(IPOPT_LIBRARIES "${IPOPT_LIBRARIES};${arg}")
      endif("${arg}" MATCHES "-framework .+")
    endforeach(arg ${IPOPT_LDFLAGS_OTHER})
  endif(APPLE)
endif(IPOPT_LIBRARIES)

# Callback support
if(IPOPT_INCLUDEDIR)
  if(EXISTS ${IPOPT_INCLUDEDIR}/IpIpoptData.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/IpOrigIpoptNLP.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/IpTNLPAdapter.hpp  AND EXISTS ${IPOPT_INCLUDEDIR}/IpDenseVector.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/IpExpansionMatrix.hpp)
    set(WITH_IPOPT_CALLBACK TRUE)
  else ()
    message(STATUS "Detected an IPOPT configuration without development headers. Build will proceed, but without callback functionality. To enable it, see https://github.com/casadi/casadi/wiki/enableIpoptCallback")
  endif ()
endif(IPOPT_INCLUDEDIR)

# Find sIPOPT library and add this to the list of libraries
if(IPOPT_LIBRARIES)
  if(EXISTS ${IPOPT_INCLUDEDIR}/SensApplication.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/IpPDSearchDirCalc.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/IpIpoptAlg.hpp AND EXISTS ${IPOPT_INCLUDEDIR}/SensRegOp.hpp)
    find_library(SIPOPT_LIBRARY sipopt HINTS ${IPOPT_LIBDIR})
    if(SIPOPT_LIBRARY)
      set(IPOPT_LIBRARIES ${SIPOPT_LIBRARY} ${IPOPT_LIBRARIES})
      set(WITH_SIPOPT TRUE)
    else ()
      message(STATUS "Detected an IPOPT configuration without sIPOPT library. Build will proceed, but without sIPOPT functionality.")
    endif ()
  else ()
    message(STATUS "Detected an IPOPT configuration without sIPOPT headers. Build will proceed, but without sIPOPT functionality.")
  endif ()
endif(IPOPT_LIBRARIES)

# Set standard flags
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IPOPT DEFAULT_MSG IPOPT_LIBRARIES IPOPT_INCLUDE_DIRS)
