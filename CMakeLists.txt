cmake_minimum_required(VERSION 3.10)
project(SSS VERSION 0.1 DESCRIPTION "A simple sound system" LANGUAGES CXX)
set(CMAKE_MODULE_PATH ${SSS_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

add_executable(sss src/main.cc)
add_executable(sss_sine src/examples/sss_sine.cc)
add_executable(sss_record src/examples/sss_record.cc)

set_target_properties(sss PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

set_target_properties(sss_sine PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

set_target_properties(sss_record PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

#set(CMAKE_CXX_FLAGS "-g -ggdb -fsanitize=address")
set(CMAKE_CXX_FLAGS "-g -ggdb")

include(CheckIncludeFileCXX)

# Set options
option(ENABLE_ALSA "Enable ALSA backend" ON)
option(ENABLE_COREAUDIO "Enable CoreAudio backend" OFF)

if(ENABLE_ALSA)
    find_package(ALSA REQUIRED)
    if(ALSA_FOUND)
        set(STATUS_ALSA "OK")
        set(SSS_HAVE_ALSA true)
        include_directories(${ALSA_INCLUDE_DIRS})
        target_compile_definitions(sss PRIVATE SSS_HAVE_ALSA=1)
        target_link_libraries(sss PRIVATE ${ALSA_LIBRARIES})
        target_link_libraries(sss_sine PRIVATE ${ALSA_LIBRARIES})
        target_link_libraries(sss_record PRIVATE ${ALSA_LIBRARIES})
    else()
        set(STATUS_ALSA "not found")
        set(SSS_HAVE_ALSA false)
        set(ALSA_LIBRARIES "")
    endif()
else()
    set(STATUS_ALSA "disabled")
    set(SSS_HAVE_ALSA false)
    set(ALSA_LIBRARIES "")
endif()

if(ENABLE_COREAUDIO)
    find_library(AUDIO_UNIT_LIBRARY NAMES AudioUnit REQUIRED)
    find_library(CORE_AUDIO_LIBRARY NAMES CoreAudio REQUIRED)
    find_library(COREFOUNDATION_LIBRARY NAMES CoreFoundation REQUIRED)
    find_library(AVFOUNDATION_LIBRARY NAMES AVFoundation REQUIRED)

    if(AUDIO_UNIT_LIBRARY AND CORE_AUDIO_LIBRARY AND COREFOUNDATION_LIBRARY AND AVFOUNDATION_LIBRARY)
        set(STATUS_COREAUDIO "OK")
        set(SOUNDIO_HAVE_COREAUDIO true)

        check_include_file_cxx(CoreAudio/CoreAudio.h SSS_HAVE_COREAUDIO)
        if (SSS_HAVE_COREAUDIO)
            target_compile_definitions(sss PRIVATE SSS_HAVE_COREAUDIO=1)
            target_link_libraries(sss PRIVATE ${CORE_AUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIO_UNIT_LIBRARY} ${AVFOUNDATION_LIBRARY})
            target_link_libraries(sss_sine PRIVATE ${CORE_AUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIO_UNIT_LIBRARY} ${AVFOUNDATION_LIBRARY})
            target_link_libraries(sss_record PRIVATE ${CORE_AUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIO_UNIT_LIBRARY} ${AVFOUNDATION_LIBRARY})
        endif()
    else()
        set(STATUS_COREAUDIO "not found")
        set(SOUNDIO_HAVE_COREAUDIO false)
        set(COREAUDIO_LIBRARY "")
        set(COREFOUNDATION_LIBRARY "")
        set(AUDIOUNIT_LIBRARY "")
    endif()
else()
    set(STATUS_COREAUDIO "disabled")
    set(SOUNDIO_HAVE_COREAUDIO false)
    set(COREAUDIO_LIBRARY "")
    set(COREFOUNDATION_LIBRARY "")
    set(AUDIOUNIT_LIBRARY "")
endif()

# Compiler options
target_compile_options(sss PRIVATE -Wall -ggdb -g)

# Configure file
configure_file(
    "${SSS_SOURCE_DIR}/src/config.h.in"
    "${SSS_SOURCE_DIR}/src/config.h"
)
