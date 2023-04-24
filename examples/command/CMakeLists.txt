if (WHISPER_SUPPORT_SDL2)
    # command
    set(TARGET command)
    add_executable(${TARGET} command.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common common-audio whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()
