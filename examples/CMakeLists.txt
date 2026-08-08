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
