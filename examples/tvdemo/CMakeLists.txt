tv_app_add_generic(tvdemo)

if (WIN32)
    target_link_libraries(tvdemo PRIVATE
        psapi # Необходимо для GetProcessMemoryInfo в gadgets.cpp.
    )
endif()
