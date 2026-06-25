cmake_minimum_required(VERSION 3.13.0)
project(cava)
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
	find_package(FFTW3 CONFIG REQUIRED)
	find_package(GLEW CONFIG REQUIRED)
	find_package(SDL2 CONFIG REQUIRED)
	if (USING_VCPKG)
		find_package(PThreads4W REQUIRED)
	else()
		find_package(Threads REQUIRED)
	endif()

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
		cava_win/cava.rc
	)

    target_compile_definitions(cava PRIVATE SDL SDL_GLSL NDEBUG GLEW_STATIC)
	target_include_directories(cava PRIVATE ${GLEW_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	if (USING_VCPKG)
		find_package(PThreads4W REQUIRED)
		target_link_libraries(cava PRIVATE PThreads4W::PThreads4W shlwapi FFTW3::fftw3 GLEW::GLEW SDL2::SDL2)
	else()
		target_link_libraries(cava PRIVATE Threads::Threads shlwapi fftw3 GLEW::GLEW SDL2::SDL2)
		find_package(Threads REQUIRED)
	endif()
else()
	message(STATUS "Building cavacore library and cava executable for Linux/Unix")

	find_package(Threads REQUIRED)
	find_package(PkgConfig REQUIRED)

	set(CURSES_NEED_NCURSESW TRUE)
	find_package(Curses)
	pkg_search_module(FFTW3 REQUIRED fftw3)

	pkg_search_module(SDL2 QUIET sdl2)
	find_package(OpenGL QUIET)

	# ---------------------------------------------------------
	# TARGET 1: cavacore lib
	# ---------------------------------------------------------
	add_library(cavacore STATIC)
	target_sources(cavacore PRIVATE cavacore.c)

	target_include_directories(cavacore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

	target_link_libraries(cavacore PRIVATE m ${FFTW3_LIBRARIES})
	target_include_directories(cavacore PRIVATE ${FFTW3_INCLUDE_DIRS})

	# ---------------------------------------------------------
	# TARGET 2: cava executable
	# ---------------------------------------------------------

	find_path(INIPARSER_INCLUDE_DIR
		NAMES iniparser.h
		PATH_SUFFIXES iniparser
	)
    find_library(INIPARSER_LIBRARY
		NAMES iniparser
	)
    if(NOT INIPARSER_INCLUDE_DIR OR NOT INIPARSER_LIBRARY)
		message(FATAL_ERROR "Could not find libiniparser! Please install libiniparser-dev.")
	endif()

	add_executable(cava
		cava.c
		config.c
		input/common.c
		input/fifo.c
		input/shmem.c
		output/terminal_noncurses.c
		output/raw.c
		output/noritake.c
	)

    target_link_libraries(cava PRIVATE cavacore Threads::Threads iniparser)
	target_include_directories(cava PRIVATE ${INIPARSER_INCLUDE_DIR})
	target_compile_definitions(cava PRIVATE
		PACKAGE=\"cava\"
		VERSION=\"1.0.0\"
		_POSIX_SOURCE
		_POSIX_C_SOURCE=200809L
	)

    # --- SDL2 & OpenGL ---
	if(SDL2_FOUND)
		message(STATUS "Found SDL2")
		target_compile_definitions(cava PRIVATE SDL)
		target_sources(cava PRIVATE output/sdl_cava.c)
		target_link_libraries(cava PRIVATE ${SDL2_LIBRARIES})
		target_include_directories(cava PRIVATE ${SDL2_INCLUDE_DIRS})

		if(OPENGL_FOUND)
			message(STATUS "Found OpenGL")
			target_compile_definitions(cava PRIVATE SDL_GLSL)
			target_sources(cava PRIVATE output/sdl_glsl.c)
			target_link_libraries(cava PRIVATE OpenGL::GL)
		endif()
	endif()

	# --- Ncurses ---
	if(CURSES_FOUND)
		target_compile_definitions(cava PRIVATE NCURSES _XOPEN_SOURCE_EXTENDED)
		target_sources(cava PRIVATE
			output/terminal_ncurses.c
			output/terminal_bcircle.c
		)
	    target_link_libraries(cava PRIVATE ncursesw)
		target_include_directories(cava PRIVATE ${CURSES_INCLUDE_DIR})
	endif()

	# --- Audio Inputs Auto-Detection ---
	set(AUDIO_INPUT OFF)

	pkg_search_module(PIPEWIRE QUIET libpipewire-0.3)
	pkg_search_module(SPA QUIET libspa-0.2)
	if (PIPEWIRE_FOUND AND SPA_FOUND)
		message(STATUS "Found PipeWire version ${PIPEWIRE_VERSION}")
		target_compile_definitions(cava PRIVATE PIPEWIRE)
		set(AUDIO_INPUT ON)
		target_sources(cava PRIVATE input/pipewire.c)
		target_link_libraries(cava PRIVATE ${PIPEWIRE_LIBRARIES} ${SPA_LIBRARIES})
		target_include_directories(cava PRIVATE ${PIPEWIRE_INCLUDE_DIRS} ${SPA_INCLUDE_DIRS})
		target_compile_options(cava PRIVATE ${PIPEWIRE_CFLAGS} ${SPA_CFLAGS})
	endif()

	pkg_search_module(ALSA QUIET alsa)
	if(ALSA_FOUND)
		message(STATUS "Found ALSA")
		target_compile_definitions(cava PRIVATE ALSA)
		set(AUDIO_INPUT ON)
		target_sources(cava PRIVATE input/alsa.c)
		target_link_libraries(cava PRIVATE ${ALSA_LIBRARIES})
		target_include_directories(cava PRIVATE ${ALSA_INCLUDE_DIRS})
	endif()

	pkg_search_module(PULSE QUIET libpulse-simple)
	if(PULSE_FOUND)
		message(STATUS "Found PulseAudio")
		target_compile_definitions(cava PRIVATE PULSE)
		set(AUDIO_INPUT ON)
		target_sources(cava PRIVATE input/pulse.c)
		target_link_libraries(cava PRIVATE ${PULSE_LIBRARIES})
		target_include_directories(cava PRIVATE ${PULSE_INCLUDE_DIRS})
	endif()

	if(AUDIO_INPUT)
		target_compile_definitions(cava PRIVATE AUDIO_INPUT)
	else()
		message(WARNING "No audio system (Pulse/ALSA/Pipewire) was found! The program will not hear music.")
	endif()
endif()
