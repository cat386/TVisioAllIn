if (TV_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SRC "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

    add_executable(${PROJECT_NAME}-test ${TEST_SRC})

    # Тесты используют встроенную GoogleTest-совместимую среду tvtest.h,
    # внешние библиотеки не требуются.
    target_compile_features(${PROJECT_NAME}-test PRIVATE cxx_std_17)
    target_include_directories(${PROJECT_NAME}-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")

    tv_add_private_includes(${PROJECT_NAME}-test)
    tv_set_warnings(${PROJECT_NAME}-test)

    target_link_libraries(${PROJECT_NAME}-test PRIVATE
        ${PROJECT_NAME}
    )

    add_test(NAME tvision-unit COMMAND $<TARGET_FILE:${PROJECT_NAME}-test>)
    set_tests_properties(tvision-unit PROPERTIES TIMEOUT 120)

    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16.0" AND TV_OPTIMIZE_BUILD)
        target_precompile_headers(${PROJECT_NAME}-test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/test.h")
        tv_enable_unity(${PROJECT_NAME}-test)
    endif()
endif()
