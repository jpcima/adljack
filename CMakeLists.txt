cmake_minimum_required(VERSION 3.6)

set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

set(WITH_MIDI_SEQUENCER OFF CACHE STRING "")
set(WITH_MUS_SUPPORT OFF CACHE STRING "")
set(WITH_XMI_SUPPORT OFF CACHE STRING "")
set(libADLMIDI_STATIC ON CACHE STRING "")
set(libADLMIDI_SHARED OFF CACHE STRING "")
add_subdirectory("thirdparty/libADLMIDI")
target_include_directories(ADLMIDI_static PUBLIC "thirdparty/libADLMIDI/include")

include(FindPkgConfig)
pkg_check_modules(JACK jack REQUIRED)

add_executable(adljack "main.cc")
target_include_directories(adljack PRIVATE "${JACK_INCLUDE_DIRS}")
link_directories("${JACK_LIBRARY_DIRS}")
target_link_libraries(adljack ADLMIDI_static "${JACK_LIBRARIES}")
install(TARGETS adljack DESTINATION "bin")
