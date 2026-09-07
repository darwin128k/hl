get_filename_component(HL_METAHHOOK_EMBED_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(hl_embed_metahook target mh_dir)
    get_filename_component(mh_dir "${mh_dir}" ABSOLUTE)

    set(MH_CAPSTONE_INC "${mh_dir}/thirdparty/install/capstone/x86/Release/include/capstone")
    set(MH_CAPSTONE_LIB "${mh_dir}/thirdparty/install/capstone/x86/Release/lib/capstone.lib")
    set(MH_MM_INC "${mh_dir}/thirdparty/install/MemoryModulePP/x86/Release/include")
    set(MH_MM_LIB "${mh_dir}/thirdparty/install/MemoryModulePP/x86/Release/lib/MemoryModule.lib")
    if(NOT EXISTS "${MH_MM_LIB}")
        set(MH_MM_INC "${mh_dir}/thirdparty/install/MemoryModulePP/Win32/Release/include")
        set(MH_MM_LIB "${mh_dir}/thirdparty/install/MemoryModulePP/Win32/Release/lib/MemoryModule.lib")
    endif()

    if(NOT EXISTS "${MH_CAPSTONE_LIB}" OR NOT EXISTS "${MH_MM_LIB}")
        message(FATAL_ERROR
            "MetaHook deps missing. Run vellum/hl/prepare-metahook.bat first.\n"
            "Expected:\n  ${MH_CAPSTONE_LIB}\n  ${MH_MM_LIB}")
    endif()
    if(NOT EXISTS "${mh_dir}/src/metahook.cpp")
        message(FATAL_ERROR "METAHOOK_DIR=${mh_dir} is not a MetaHookSv tree")
    endif()

    set(MH_SOURCES
        "${mh_dir}/include/HLSDK/common/interface.cpp"
        "${mh_dir}/thirdparty/Detours_fork/src/detours.cpp"
        "${mh_dir}/thirdparty/Detours_fork/src/disasm.cpp"
        "${mh_dir}/thirdparty/Detours_fork/src/modules.cpp"
        "${mh_dir}/src/commandline.cpp"
        "${mh_dir}/src/GameData.cpp"
        "${mh_dir}/src/LoadDllNotification.cpp"
        "${mh_dir}/src/LoadBlob.cpp"
        "${mh_dir}/src/metahook.cpp"
        "${mh_dir}/src/registry.cpp"
        "${mh_dir}/src/sys_launcher.cpp"
        "${mh_dir}/src/Z.cpp"
        "${HL_METAHHOOK_EMBED_ROOT}/src/metahook_embed.cpp"
    )

    target_sources(${target} PRIVATE ${MH_SOURCES})
    target_include_directories(${target} PRIVATE
        "${HL_METAHHOOK_EMBED_ROOT}/src"
        "${mh_dir}/include"
        "${mh_dir}/include/Interface"
        "${mh_dir}/include/HLSDK/common"
        "${mh_dir}/include/HLSDK/cl_dll"
        "${mh_dir}/include/HLSDK/engine"
        "${mh_dir}/include/HLSDK/pm_shared"
        "${mh_dir}/include/HLSDK/public"
        "${mh_dir}/src"
        "${mh_dir}/thirdparty/Detours_fork/src"
        "${mh_dir}/thirdparty/rapidjson/include"
        "${mh_dir}/thirdparty/Chocobo1Hash/src"
        "${mh_dir}/thirdparty/MemoryModulePP/include"
        "${MH_CAPSTONE_INC}"
        "${MH_MM_INC}"
    )
    target_compile_definitions(${target} PRIVATE
        HL_METAHHOOK
        WIN32
        _WINDOWS
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(${target} PRIVATE
        /wd4005 /wd4091 /wd4311 /wd4312 /wd4819 /wd4996
    )
    set_source_files_properties(${MH_SOURCES} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
    target_link_libraries(${target} PRIVATE
        "${MH_CAPSTONE_LIB}"
        "${MH_MM_LIB}"
        ntdll
        ws2_32
    )
    message(STATUS "${target}: MetaHookSv embedded from ${mh_dir}")
endfunction()
