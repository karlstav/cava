project('cava', 'c',
  version : '0.8.3',
  license: 'MIT',
  default_options : ['warning_level=3'])

conf = configuration_data()
compiler = meson.get_compiler('c')

c_args = []
c_link_args = []

add_project_arguments('-DPACKAGE="@0@"'.format(meson.project_name()), language: 'c')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'c')
add_project_arguments('-DFONTDIR="${datarootdir}/consolefonts"', language: 'c')

src_files = files(
    'cava.c',
    'cavacore.c',
    'config.c',
    'input/common.c',
    'input/fifo.c',
    'input/shmem.c',
    'output/terminal_noncurses.c',
    'output/raw.c',
    'output/noritake.c')

if get_option('asan').enabled()
   add_project_arguments('-fsanitize=address', language: 'c')
   c_link_args += '-fsanitize=address'
endif

if get_option('tsan').enabled()
   add_project_arguments('-fsanitize=thread', language: 'c')
   c_link_args += '-fsanitize=thread'
endif

if get_option('ubsan').enabled()
   add_project_arguments('-fsanitize=undefined', language: 'c')
   c_link_args += '-fsanitize=undefined'
endif
######################
# Checking for pthread
######################
thread_dep = dependency('threads', required: true)

if not compiler.has_function('pthread_create', dependencies: thread_dep)
   error('pthread_create method not found')
endif
########################
# Checking for iniparser
########################
iniparser_dep = compiler.find_library('iniparser', has_headers: 'iniparser.h', required: false)

if not iniparser_dep.found()
  iniparser_dep = compiler.find_library('iniparser4', has_headers: 'iniparser4/iniparser.h', required: false)
endif

if not iniparser_dep.found()
   error('iniparser library is required!')
else
   if not compiler.has_function('iniparser_load', dependencies: iniparser_dep)
      error('iniparser library is required!')
   endif
endif
####################
# Checking for fftw3
####################
fftw3_dep = dependency('fftw3', required: true, not_found_message: 'fftw library is required!')
#######################
# Checking for math lib
#######################
m_dep = compiler.find_library('m', required: true)
if not compiler.has_function('sqrt', dependencies: m_dep)
   error('sqrt method not found')
endif
#######################
# Checking for alloca.h
#######################
if compiler.check_header('alloca.h')
   add_project_arguments('-DHAVE_ALLOCA_H', language: 'c')
endif
#######################
# Checking for alsa dev
#######################
alsa_dep = compiler.find_library('asound', required: get_option('input_alsa'))
if alsa_dep.found()
   if compiler.has_function('snd_pcm_open', dependencies: alsa_dep)
      add_project_arguments('-DALSA', language: 'c')
      src_files += 'input/alsa.c'
   else
      warning('No alsa dev files found building without alsa support')
   endif
else
   warning('No alsa dev files found building without alsa support')
endif
############################
# Checking for portaudio dev
############################
port_dep = compiler.find_library('portaudio', required: get_option('input_portaudio'))
if port_dep.found()
   if compiler.has_function('Pa_Initialize', dependencies: port_dep)
      add_project_arguments('-DPORTAUDIO', language: 'c')
      src_files += 'input/portaudio.c'
   else
      warning('No portaudio dev files found building without portaudio support')
   endif
else
   warning('No portaudio dev files found building without portaudio support')
endif
########################
# Checking for pulse dev
########################
pulse_dep = dependency('libpulse', required: get_option('input_pulse'))
pulse_simple_dep = compiler.find_library('pulse-simple', required: get_option('input_pulse'))

if pulse_simple_dep.found() and pulse_dep.found()
   if compiler.has_function('pa_simple_new', dependencies: [pulse_simple_dep, pulse_dep])
      add_project_arguments('-DPULSE', language: 'c')
      src_files += 'input/pulse.c'
   else
      warning('No pulseaudio dev files found building without pulseaudio support')
   endif
else
   warning('No pulseaudio dev files found building without pulseaudio support')
endif
########################
# Checking for sndio dev
########################
sndio_dep = compiler.find_library('sndio', required: get_option('input_sndio'))
if sndio_dep.found()
   if compiler.has_function('sio_open', dependencies: sndio_dep)
      add_project_arguments('-DSNDIO', language: 'c')
      src_files += 'input/sndio.c'
   else
      warning('No sndio dev files found building without sndio support')
   endif
else
   warning('No sndio dev files found building without sndio support')
endif
#######################
# Checking for ncursesw
#######################
check_headers = [
  ['ncursesw/menu.h', 'HAVE_NCURSESW_MENU_H'],
  ['ncurses/menu.h', 'HAVE_NCURSES_MENU_H'],
  ['menu.h', 'HAVE_MENU_H'],
  ['ncursesw/curses.h', 'HAVE_NCURSESW_CURSES_H'],
  ['ncursesw.h', 'HAVE_NCURSESW_H'],
  ['ncurses/curses.h', 'HAVE_NCURSES_CURSES_H'],
  ['ncurses.h', 'HAVE_NCURSES_H'],
  ['curses.h', 'HAVE_CURSES_H'],
]

foreach h : check_headers
  if compiler.has_header(h.get(0))
    conf.set(h.get(1), 1)
  endif
endforeach

ncursesw_dep = dependency('ncursesw', required: get_option('output_ncurses'))
if ncursesw_dep.found()
   if compiler.has_function('initscr', dependencies: ncursesw_dep)
      add_project_arguments('-DNCURSES', language: 'c')
      src_files += 'output/terminal_ncurses.c'
   else
      warning('building without ncursesw support ncursesw is recommended!')
   endif
else
   warning('building without ncursesw support ncursesw is recommended!')
endif
###################
# Checking for sdl2
###################
sdl2_dep = compiler.find_library('SDL2', required: get_option('output_sdl'))
opengl_dep = dependency('GL', required: get_option('output_sdl_glsl'))
if sdl2_dep.found()
   if compiler.has_function('SDL_Init', dependencies: sdl2_dep)
      add_project_arguments('-DSDL', language: 'c')
      src_files += 'output/sdl_cava.c'
      #####################
      # Checking for opengl
      #####################
      if opengl_dep.found()
         add_project_arguments('-DSDL_GLSL', language: 'c')
         src_files += 'output/sdl_glsl.c'
      else
         warning('building without sdl_glsl (opengl) support')
      endif
   else
      warning('building without sdl support')
   endif
else
   warning('building without sdl support')
endif

executable('cava',
           src_files,
           dependencies: [
               thread_dep,
               iniparser_dep,
               m_dep,
               fftw3_dep,
               alsa_dep,
               port_dep,
               pulse_dep,
               pulse_simple_dep,
               sdl2_dep,
               sndio_dep,
               ncursesw_dep,
               opengl_dep
           ],
           include_directories: [include_directories('input'), include_directories('output')],
           install : true)
