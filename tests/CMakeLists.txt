add_executable(comettextel_pdu_tests pdu_roundtrip_test.cpp)

if(TARGET comettextel_static)
    target_link_libraries(comettextel_pdu_tests PRIVATE comettextel_static)
elseif(TARGET comettextel_shared)
    target_link_libraries(comettextel_pdu_tests PRIVATE comettextel_shared)
endif()

target_compile_features(comettextel_pdu_tests PRIVATE cxx_std_23)

if(COMETTEXTEL_BUILD_C_API)
    target_compile_definitions(comettextel_pdu_tests PRIVATE COMETTEXTEL_HAS_C_API=1)
endif()

add_test(NAME comettextel.pdu_roundtrip COMMAND comettextel_pdu_tests)

if(WIN32 AND TARGET comettextel_shared AND NOT TARGET comettextel_static)
    add_custom_command(TARGET comettextel_pdu_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:comettextel_shared>
            $<TARGET_FILE_DIR:comettextel_pdu_tests>
    )
endif()
