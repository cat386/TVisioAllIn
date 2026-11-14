if (TV_BUILD_AVSCOLOR AND NOT WIN32)
    # AviSynth plugin. It uses AviSynth's C plugin interface, which is
    # declared in 'termcolor.cpp' itself and resolved at runtime, so neither
    # the AviSynth headers nor the AviSynth library are needed to build it.
    set(NAME termcolor)
    add_library(${NAME} SHARED ${NAME}.cpp)
    target_compile_features(${NAME} PRIVATE cxx_std_11)
    target_link_libraries(${NAME} PRIVATE ${PROJECT_NAME} ${CMAKE_DL_LIBS})
    tv_set_output_dir(${NAME})
endif()
