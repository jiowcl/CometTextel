function(comettextel_add_example target_name source_file)
    add_executable(${target_name} ${source_file})

    if(TARGET comettextel::shared)
        target_link_libraries(${target_name} PRIVATE comettextel::shared)
    elseif(TARGET comettextel::static)
        target_link_libraries(${target_name} PRIVATE comettextel::static)
    endif()

    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_features(${target_name} PRIVATE cxx_std_23)
endfunction()

comettextel_add_example(comettextel_send_example send_example.cpp)
comettextel_add_example(comettextel_list_example list_example.cpp)
comettextel_add_example(comettextel_delete_example delete_example.cpp)
comettextel_add_example(comettextel_receive_example receive_example.cpp)

if(COMETTEXTEL_BUILD_C_API)
    add_executable(comettextel_c_api_example c_api_example.c)

    if(TARGET comettextel::shared)
        target_link_libraries(comettextel_c_api_example PRIVATE comettextel::shared)
    elseif(TARGET comettextel::static)
        target_link_libraries(comettextel_c_api_example PRIVATE comettextel::static)
        set_property(TARGET comettextel_c_api_example PROPERTY LINKER_LANGUAGE CXX)
    endif()

    set_target_properties(comettextel_c_api_example PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )

    if(MSVC)
        target_compile_options(comettextel_c_api_example PRIVATE /W4 /utf-8)
        target_compile_definitions(comettextel_c_api_example PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(comettextel_c_api_example PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(WIN32 AND TARGET comettextel_shared)
        add_custom_command(TARGET comettextel_c_api_example POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:comettextel_shared>
                $<TARGET_FILE_DIR:comettextel_c_api_example>
        )
    endif()

    if(COMETTEXTEL_BUILD_TESTS)
        add_test(
            NAME comettextel.c_api_pdu_ascii
            COMMAND comettextel_c_api_example pdu 886912345678 "Hello" 886932000000
        )
        add_test(
            NAME comettextel.c_api_pdu_ucs2_zh
            COMMAND comettextel_c_api_example pdu 886912345678 "測試中文簡訊" 886932000000
        )
        set_tests_properties(
            comettextel.c_api_pdu_ascii
            comettextel.c_api_pdu_ucs2_zh
            PROPERTIES
                WORKING_DIRECTORY $<TARGET_FILE_DIR:comettextel_c_api_example>
        )
        if(WIN32 AND TARGET comettextel_shared)
            set_tests_properties(
                comettextel.c_api_pdu_ascii
                comettextel.c_api_pdu_ucs2_zh
                PROPERTIES
                    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:comettextel_shared>;$ENV{PATH}"
            )
        endif()
    endif()
endif()
