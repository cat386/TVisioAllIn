find_package(Threads REQUIRED)
set(TV_MODERN_EXAMPLES themes colors256 table treeview progress json_table json_load json_parse filemanager data_demo)
foreach(app ${TV_MODERN_EXAMPLES})
    add_executable(${app} demo.cpp)
    target_compile_definitions(${app} PRIVATE TV_MODERN_DEMO="${app}")
    target_link_libraries(${app} PRIVATE ${PROJECT_NAME} Threads::Threads)
    tv_app_common(${app})
endforeach()
if(TV_BUILD_TESTS)
    add_custom_target(modern-examples-smoke ALL DEPENDS ${TV_MODERN_EXAMPLES})
    foreach(app ${TV_MODERN_EXAMPLES})
        add_custom_command(TARGET modern-examples-smoke POST_BUILD
            COMMAND $<TARGET_FILE:${app}> --smoke
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            VERBATIM)
    endforeach()
endif()
