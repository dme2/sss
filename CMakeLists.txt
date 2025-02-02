Cmake_minimum_required(VERSION 3.10)
project(simple_sound_system VERSION 0.1 DESCRIPTION "A simple sound system" LANGUAGES CXX)
include(CheckIncludeFileCXX)

option(ENABLE_ALSA "Enable ALSA backend" ON)
option(ENABLE_COREAUDIO "Enable CoreAudio backend" OFF)

if (ENABLE_ALSA)
  set(SSS_HAVE_ALSA true)
endif()

if (ENABLE_COREAUDIO)
  set(SSS_HAVE_COREAUDIO true)
endif()

configure_file(
    src/config.h.in
    ../src/config.h
)

add_library(sss INTERFACE)
add_executable(sss_sine src/examples/sss_sine.cc)

if(ENABLE_ALSA)
    find_package(ALSA REQUIRED)
    if(ALSA_FOUND)
        set(STATUS_ALSA "OK")
        include_directories(${ALSA_INCLUDE_DIRS})
				target_compile_definitions(sss INTERFACE SSS_HAVE_ALSA=1)
        target_link_libraries(sss_sine PRIVATE ${ALSA_LIBRARIES})
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
						target_compile_definitions(sss INTERFACE SSS_HAVE_COREAUDIO=1)
            target_link_libraries(sss_sine PRIVATE ${CORE_AUDIO_LIBRARY} ${COREFOUNDATION_LIBRARY} ${AUDIO_UNIT_LIBRARY} ${AVFOUNDATION_LIBRARY})
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



set_target_properties(
   	sss 
    PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR}
)

# set the include directories
target_include_directories(
  sss 
	INTERFACE src/ 
)

# define the C++ standard needed to compile this library and make it visible to
# dependers
target_compile_features(sss INTERFACE cxx_std_23)

target_link_libraries(sss_sine PUBLIC sss)


