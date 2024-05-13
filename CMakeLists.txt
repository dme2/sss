cmake_minimum_required(VERSION 3.10)
project(SSS VERSION 0.1 DESCRIPTION "A simple sound system" LANGUAGES CXX)
set(CMAKE_MODULE_PATH ${SSS_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

add_executable(sss src/main.cc)

set_target_properties(sss PROPERTIES
  CXX_STANDARD 20 
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

option(ENABLE_ALSA "Enable ALSA backend" ON)
option(ENABLE_COREAUDIO "Enable CoreAudio backend" ON)

if(ENABLE_ALSA)
    find_package(ALSA)
    if(ALSA_FOUND)
        set(STATUS_ALSA "OK")
        set(SSS_HAVE_ALSA true)
        include_directories(${ALSA_INCLUDE_DIRS})
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
    find_package(COREAUDIO)
    if(COREAUDIO_FOUND)
        set(STATUS_COREAUDIO "OK")
        set(SOUNDIO_HAVE_COREAUDIO true)
        include_directories(${COREAUDIO_INCLUDE_DIR})

        find_path(COREFOUNDATION_INCLUDE_DIR NAMES CoreFoundation.h)
        find_library(COREFOUNDATION_LIBRARY NAMES CoreFoundation)
        include_directories(${COREFOUNDATION_INCLUDE_DIR})

        find_path(AUDIOUNIT_INCLUDE_DIR NAMES AudioUnit.h)
        find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
        include_directories(${AUDIOUNIT_INCLUDE_DIR})
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



find_library(AUDIO_UNIT_LIBRARY AudioUnit REQUIRED)
find_library(CORE_AUDIO_LIBRARY CoreAudio REQUIRED)
find_library(COREFOUNDATION_LIBRARY CoreFoundation REQUIRED)
find_library(AVFOUNDATION_LIBRARY AVFoundation REQUIRED)

# Include directories
#include_directories(include)

# Compiler options
target_compile_options(sss PRIVATE  -Wall -ggdb -g)

# Link libraries
target_link_libraries(sss PUBLIC ${AUDIO_UNIT_LIBRARY} ${CORE_AUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY}  ${AVFOUNDATION_LIBRARY})
