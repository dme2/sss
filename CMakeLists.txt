cmake_minimum_required(VERSION 3.10)
project(SSS VERSION 0.1 DESCRIPTION "A simple sound system" LANGUAGES CXX)

add_executable(sss src/main.cc)

set_target_properties(sss PROPERTIES
  CXX_STANDARD 20 
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

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
