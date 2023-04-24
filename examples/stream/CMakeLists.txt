if (WHISPER_SUPPORT_SDL2 OR WHISPER_SUPPORT_MINIAUDIO)
    # stream
    set(TARGET stream)
    add_executable(${TARGET} stream.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common common-audio whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()
