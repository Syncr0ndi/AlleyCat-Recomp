cmake_minimum_required(VERSION 3.20)
project(alleycat_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Privacy: never embed the build machine's folders in binaries.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}/=" "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
endif()

# The player's own CAT.EXE. The recompiled code is generated from it into the build folder
# (derived from the game: never committed, never shipped).
set(CAT_EXE "" CACHE FILEPATH "Path to the original CAT.EXE (Alley Cat, 1984)")
if(NOT EXISTS "${CAT_EXE}")
  message(FATAL_ERROR "Set -DCAT_EXE=<path to your CAT.EXE>")
endif()
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(GEN_DIR ${CMAKE_BINARY_DIR}/gen)
set(GEN_TOOLS ${CMAKE_SOURCE_DIR}/tools/catrecomp.py ${CMAKE_SOURCE_DIR}/tools/catmap.py ${CMAKE_SOURCE_DIR}/tools/v30dec.py)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${GEN_TOOLS} ${CAT_EXE})
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/catrecomp.py ${CAT_EXE} ${GEN_DIR}
                RESULT_VARIABLE gen_rc)
if(NOT gen_rc EQUAL 0)
  message(FATAL_ERROR "Code generation failed")
endif()
file(GLOB CAT_GEN_SRC ${GEN_DIR}/*.c)

add_library(cpu86 STATIC src/cpu/v30.c src/cpu/v30_interp.c)
target_include_directories(cpu86 PUBLIC src/cpu)

add_library(pcmachine STATIC src/pc/pc.c src/pc/font8x8.c)
target_include_directories(pcmachine PUBLIC src/pc)
target_link_libraries(pcmachine PUBLIC cpu86)

add_library(catgen STATIC ${CAT_GEN_SRC})
target_include_directories(catgen PUBLIC ${GEN_DIR})
target_link_libraries(catgen PUBLIC cpu86)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(catgen PRIVATE -Wno-unused-label -Wno-unused-variable)
  target_compile_options(pcmachine PRIVATE -Wall -Wextra)
endif()

# Headless test runner: scripted keys, PNG screenshots, WAV audio.
add_executable(cat_test src/app/test_main.c src/app/png_write.c src/app/game_file.c)
target_link_libraries(cat_test PRIVATE pcmachine catgen)

option(CAT_BUILD_APP "Build the SDL2 game" ON)
if(CAT_BUILD_APP)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  include(FetchContent)
  FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
    URL_HASH SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4)
  FetchContent_MakeAvailable(SDL2)
  add_executable(alleycat WIN32 src/app/main.c src/app/game_file.c src/app/png_write.c)
  target_link_libraries(alleycat PRIVATE pcmachine catgen SDL2::SDL2main SDL2::SDL2-static)
  if(WIN32)
    target_link_libraries(alleycat PRIVATE comdlg32)
  endif()
endif()

if(MINGW)
  foreach(t cat_test alleycat)
    if(TARGET ${t})
      target_link_options(${t} PRIVATE -static)
    endif()
  endforeach()
endif()
