cmake_minimum_required(VERSION 3.28)

project(comettextel
    VERSION 1.0.0
    DESCRIPTION "CometTextel — cross-platform GSM SMS library (serial + PDU)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(COMETTEXTEL_BUILD_SHARED "Build the shared library (DLL / .so / .dylib)" ON)
option(COMETTEXTEL_BUILD_STATIC "Build the static library" ON)
option(COMETTEXTEL_BUILD_EXAMPLES "Build example programs" ON)
option(COMETTEXTEL_BUILD_DOCS "Build Doxygen documentation" OFF)

if(NOT COMETTEXTEL_BUILD_SHARED AND NOT COMETTEXTEL_BUILD_STATIC)
    message(FATAL_ERROR "At least one of COMETTEXTEL_BUILD_SHARED or COMETTEXTEL_BUILD_STATIC must be ON")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)

set(COMETTEXTEL_PUBLIC_HEADERS
    include/comettextel/export.hpp
    include/comettextel/types.hpp
    include/comettextel/serial.hpp
    include/comettextel/pdu.hpp
    include/comettextel/modem.hpp
    include/comettextel/comettextel.hpp
)

set(COMETTEXTEL_COMMON_SOURCES
    src/types.cpp
    src/pdu.cpp
    src/modem.cpp
)

if(WIN32)
    list(APPEND COMETTEXTEL_COMMON_SOURCES src/serial/serial_win32.cpp)
else()
    list(APPEND COMETTEXTEL_COMMON_SOURCES src/serial/serial_posix.cpp)
endif()

function(comettextel_configure_lib target_name)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(${target_name} PUBLIC cxx_std_23)
    comettextel_apply_compiler_options(${target_name})

    if(WIN32)
        target_compile_definitions(${target_name} PUBLIC COMETTEXTEL_PLATFORM_WINDOWS=1)
        target_link_libraries(${target_name} PUBLIC kernel32)
    else()
        target_compile_definitions(${target_name} PUBLIC COMETTEXTEL_PLATFORM_POSIX=1)
    endif()
endfunction()

set(COMETTEXTEL_INSTALL_TARGETS)

if(COMETTEXTEL_BUILD_SHARED)
    add_library(comettextel_shared SHARED ${COMETTEXTEL_COMMON_SOURCES} ${COMETTEXTEL_PUBLIC_HEADERS})
    add_library(comettextel::shared ALIAS comettextel_shared)
    comettextel_configure_lib(comettextel_shared)
    target_compile_definitions(comettextel_shared
        PUBLIC COMETTEXTEL_SHARED
        PRIVATE COMETTEXTEL_BUILDING
    )
    set_target_properties(comettextel_shared PROPERTIES
        OUTPUT_NAME comettextel
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    list(APPEND COMETTEXTEL_INSTALL_TARGETS comettextel_shared)
endif()

if(COMETTEXTEL_BUILD_STATIC)
    add_library(comettextel_static STATIC ${COMETTEXTEL_COMMON_SOURCES} ${COMETTEXTEL_PUBLIC_HEADERS})
    add_library(comettextel::static ALIAS comettextel_static)
    comettextel_configure_lib(comettextel_static)
    target_compile_definitions(comettextel_static PUBLIC COMETTEXTEL_STATIC)
    set_target_properties(comettextel_static PROPERTIES
        OUTPUT_NAME comettextel
        VERSION ${PROJECT_VERSION}
    )
    if(WIN32 AND COMETTEXTEL_BUILD_SHARED)
        set_target_properties(comettextel_static PROPERTIES OUTPUT_NAME comettextel_static)
    endif()
    list(APPEND COMETTEXTEL_INSTALL_TARGETS comettextel_static)
endif()

if(COMETTEXTEL_BUILD_SHARED)
    add_library(comettextel::comettextel ALIAS comettextel_shared)
elseif(COMETTEXTEL_BUILD_STATIC)
    add_library(comettextel::comettextel ALIAS comettextel_static)
endif()

if(COMETTEXTEL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(COMETTEXTEL_BUILD_DOCS)
    include(Doxygen)
endif()

include(GNUInstallDirs)
install(TARGETS ${COMETTEXTEL_INSTALL_TARGETS}
    EXPORT comettextelTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/comettextel DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT comettextelTargets
    NAMESPACE comettextel::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/comettextel
)