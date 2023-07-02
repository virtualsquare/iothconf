cmake_minimum_required(VERSION 3.13)
project("libiothconf"
    DESCRIPTION "ioth autoconfiguration"
    HOMEPAGE_URL "https://github.com/rd235/libiothconf"
    VERSION 0.1.0
    LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb -D_FORTIFY_SOURCE=2 -O2 -pedantic -Wall -Wextra")

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckSymbolExists)

set(LIBS_REQUIRED ioth mhash stropt)
set(HEADERS_REQUIRED ioth.h mhash.h stropt.h strcase.h)

foreach(THISLIB IN LISTS LIBS_REQUIRED)
  find_library(LIB${THISLIB}_OK ${THISLIB})
  if(NOT LIB${THISLIB}_OK)
    message(FATAL_ERROR "library lib${THISLIB} not found")
  endif()
endforeach(THISLIB)

foreach(HEADER IN LISTS HEADERS_REQUIRED)
  check_include_file(${HEADER} ${HEADER}_OK)
  if(NOT ${HEADER}_OK)
    message(FATAL_ERROR "header file ${HEADER} not found")
  endif()
endforeach(HEADER)

add_definitions(-D_GNU_SOURCE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(iothconf SHARED iothconf.c iothconf_data.c iothconf_hash.c iothconf_debug.c
		iothconf_rd.c iothconf_dhcp.c iothconf_dhcpv6.c iothconf_dns.c iothconf_ip.c)
target_link_libraries(iothconf ioth mhash stropt)

set_target_properties(iothconf PROPERTIES VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

# configure_file(iothconf.pc.in iothconf.pc @ONLY)
# configure_file(config.h.in config.h)

install(TARGETS iothconf DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES iothconf.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iothconf.pc
# DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

add_subdirectory(man)
add_subdirectory(test)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${PROJECT_SOURCE_DIR}/Uninstall.cmake")


