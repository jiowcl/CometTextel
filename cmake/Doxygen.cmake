# Optional Doxygen documentation generation.

find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen not found; COMETTEXTEL_BUILD_DOCS will produce no documentation")
    return()
endif()

set(DOXYGEN_PROJECT_NAME "CometTextel")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_PROJECT_BRIEF "Cross-platform GSM SMS library")
set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
set(DOXYGEN_GENERATE_HTML YES)
set(DOXYGEN_GENERATE_LATEX NO)
set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_EXTRACT_PRIVATE NO)
set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(DOXYGEN_RECURSIVE YES)
set(DOXYGEN_FILE_PATTERNS "*.hpp" "*.cpp" "*.h" "*.c" "*.md")
set(DOXYGEN_EXCLUDE_PATTERNS "*/build/*" "*/examples/*")

doxygen_add_docs(comettextel_docs
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/doc"
    COMMENT "Generate API documentation with Doxygen"
)