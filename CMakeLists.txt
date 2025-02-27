# This is only for the cavacore lib, see CAVACORE.md for details
# to build cava don't use this, use the automake stuff

cmake_minimum_required(VERSION 3.13.0)
project(cavacore)
if (ANDROID)
	add_library(cavacore SHARED cavacore.c)
	if (NOT DEFINED FFTW_DIR)
		message(FATAL_ERROR "FFTW_DIR not set, required by android, see cavandroid/README.md")
	endif()
	if (NOT EXISTS ${FFTW_DIR})
		message(FATAL_ERROR "given FFTW_DIR: ${FFTW_DIR} does not exist")
	endif()
	if (NOT EXISTS "${FFTW_DIR}/jni/fftw3/api/")
		message(FATAL_ERROR "given fftw include dir: ${FFTW_DIR}/jni/fftw3/api/ does not exist")
	endif()
	if (NOT EXISTS "${FFTW_DIR}/obj/local/${CMAKE_ANDROID_ARCH_ABI}/")
		message(FATAL_ERROR "given lib dir: ${FFTW_DIR}/obj/local/${CMAKE_ANDROID_ARCH_ABI}/ does not exist, did you build it? See cavandroid/README.md")
	endif()
	target_include_directories(cavacore PRIVATE "${FFTW_DIR}/jni/fftw3/api/")
	target_link_directories(cavacore PRIVATE "${FFTW_DIR}/obj/local/${CMAKE_ANDROID_ARCH_ABI}/")
	target_link_libraries(cavacore fftw3)
elseif(WIN32)
	find_package(FFTW3)
	find_package(GLEW)
	find_package(SDL2)
	find_package(Threads)

	add_executable(cava
		cava.c
		cavacore.c
		config.c
		input/common.c
		input/winscap.c
		output/sdl_cava.c
		output/sdl_glsl.c
		output/terminal_noncurses.c
		output/raw.c
		output/noritake.c
		cava_win/cava/cava.rc
	)

	target_compile_definitions(cava PRIVATE SDL SDL_GSL NDEBUG)
	target_include_directories(cava PRIVATE ${GLEW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	target_link_libraries(cava PRIVATE pthread shlwapi fftw3 GLEW::GLEW SDL2::SDL2)
else()
	add_library(cavacore STATIC cavacore.c)
endif()
