add_executable(comettextel_send_example send_example.cpp)

if(TARGET comettextel::shared)
    target_link_libraries(comettextel_send_example PRIVATE comettextel::shared)
elseif(TARGET comettextel::static)
    target_link_libraries(comettextel_send_example PRIVATE comettextel::static)
endif()

target_compile_features(comettextel_send_example PRIVATE cxx_std_23)
