project(
    'cava',
    'c',
    version: '0.10.1',
    license: 'MIT',
    meson_version: '>=0.50.0',
    default_options : [
                'warning_level=3',
                'buildtype=release'
    ]
)

is_linux = host_machine.system() == 'linux'
is_mac = host_machine.system() == 'darwin'
is_freebsd = host_machine.system() == 'freebsd'

cc = meson.get_compiler('c')
conf = configuration_data()
c_args = ['-Wall',
          '-Wextra',
          '-Wno-unused-result',
          '-Wno-maybe-uninitialized',
          '-Wno-vla-parameter'
         ]

src_files = files('src/cavacore.c',
                  'src/common.c',
                  'src/config.c',
                  'src/input/common.c',
                  'src/input/fifo.c',
                  'src/input/shmem.c',
                  'src/output/common.c',
                  'src/output/terminal_noncurses.c',
                  'src/output/raw.c',
                  'src/output/noritake.c'
                  )

h_files = files('include/cava/cavacore.h',
                'include/cava/common.h',
                'include/cava/config.h',
                'include/cava/debug.h',
                'include/cava/util.h'
                )
h_I_files = files('include/cava/input/common.h')

h_O_files = files('include/cava/output/common.h',
                  'include/cava/output/terminal_noncurses.h',
                  'include/cava/output/raw.h',
                  'include/cava/output/noritake.h'
                  )

deps = []
inc_dirs = ['include']

if is_mac
   add_project_arguments('-DNORT', language: 'c')
endif

add_project_arguments('-DPACKAGE="@0@"'.format(meson.project_name()), language: 'c')
add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'c')

if not get_option('debug')
   add_project_arguments('-DNDEBUG', language: 'c')
endif

### Iniparser
iniparser_dep = cc.find_library('iniparser', has_headers: 'iniparser.h', required: false)
if not iniparser_dep.found()
   iniparser_dep = cc.find_library('iniparser4', has_headers: 'iniparser4/iniparser.h', required: false)
endif
if not iniparser_dep.found()
   error('iniparser library is required')
else
   if not cc.has_function('iniparser_load', dependencies: iniparser_dep)
      error('required iniparser_load method in iniparser library is not found')
   else
      deps += iniparser_dep
   endif
endif

### Math
m_dep = cc.find_library('m', required: false)
if not m_dep.found()
   error('math library is required')
else
   if not cc.has_function('sqrt', dependencies: m_dep)
      error('required sqrt method in math library is not found')
   else
      deps += m_dep
   endif
endif

### fftw3
fftw3_dep = dependency('fftw3', required: true, not_found_message: 'fftw3 library is required')
deps += fftw3_dep

### Threads
thread_dep = dependency('threads', required: false, not_found_message: 'building without threads suppport')
if not cc.has_header('pthread.h', dependencies: thread_dep)
   warning('pthread.h header not found. building without threads support')
else
   if not cc.has_function('pthread_create', dependencies: thread_dep)
      warning('pthread_create method not found. build without threads suppport')
   else
      deps += thread_dep
   endif
endif

### alloca
if cc.check_header('alloca.h')
   add_project_arguments('-DHAVE_ALLOCA_H', language: 'c')
endif

ncursesw_dep = dependency('ncursesw', required: get_option('output_ncurses'))
if ncursesw_dep.found()
   if cc.has_function('initscr', dependencies: ncursesw_dep)
      src_files += 'src/output/terminal_ncurses.c'
      h_O_files += 'include/cava/output/terminal_ncurses.h'
      deps += ncursesw_dep
      add_project_arguments('-DNCURSES', language: 'c')
   else
      warning('initscr method in ncursesw library is not found. building without ncursesw support')
   endif
else
   warning('ncursesw library is not found. building without ncursesw support')
endif

### alsa
alsa_dep = cc.find_library('asound', required: get_option('input_alsa'))
if alsa_dep.found()
   if cc.has_function('snd_pcm_open', dependencies: alsa_dep)
      src_files += 'src/input/alsa.c'
      deps += alsa_dep
      add_project_arguments('-DALSA', language: 'c')
   else
      warning('snd_pcm_open method in alsa library is not found. building without alsa support')
   endif
else
   warning('alsa library is not found. building without alsa support')
endif

### port
port_dep = cc.find_library('portaudio', required: get_option('input_portaudio'))
if port_dep.found()
   if cc.has_function('Pa_Initialize', dependencies: port_dep)
      src_files += 'src/input/portaudio.c'
      deps += port_dep
      add_project_arguments('-DPORTAUDIO', language: 'c')
   else
      warning('Pa_Initialize method in portaudio library is not found. building without portaudio')
   endif
else
   warning('portaudio library is not found. building without portaudio')
endif

### pulse
pulse_dep = dependency('libpulse', required: get_option('input_pulse'), not_found_message: 'libpulse is not found. building without pulse support')
pulse_simple_dep = cc.find_library('pulse-simple', required: get_option('input_pulse'))
if pulse_dep.found() and pulse_simple_dep.found()
   if cc.has_function('pa_simple_new', dependencies: [pulse_dep, pulse_simple_dep])
      src_files += 'src/input/pulse.c'
      deps += [pulse_dep, pulse_simple_dep]
      add_project_arguments('-DPULSE', language: 'c')
   else
      warning('pa_simple_method in pulse library is not found. building without pulse')
   endif
else
   warning('pulse library is not found. building without pulse')
endif

### sndio
sndio_dep = cc.find_library('sndio', required: get_option('input_sndio'))
if sndio_dep.found()
   if cc.has_function('sio_open', dependencies: sndio_dep)
      src_files += 'src/input/sndio.c'
      deps += sndio_dep
      add_project_arguments('-DSNDIO', language: 'c')
   else
      warning('sio_open method in sndio library is not found. building without sndio')
   endif
else
   warning('sndio library is not found. building without sndio')
endif

### pipewire
pipewire_dep = dependency('libpipewire-0.3', required: get_option('input_pipewire'), not_found_message: 'pipewire is not found. building without pipewire support')
if pipewire_dep.found()
   if cc.has_function('pw_stream_connect', dependencies: pipewire_dep)
      src_files += 'src/input/pipewire.c'
      deps += pipewire_dep
      add_project_arguments('-DPIPEWIRE', language: 'c')
   else
      warning('pw_stream_connect method in pipewire library is not found. building without pipewire')
   endif
endif

### sdl2
sdl2_dep = dependency('sdl2', required: get_option('output_sdl'), not_found_message: 'sdl2 library is not found. building without sdl2')
opengl_dep = dependency('gl', required: get_option('output_sdl_glsl'), not_found_message: 'OpenGL library is not found. building without OpenGL')
if sdl2_dep.found()
   if cc.has_function('SDL_Init', dependencies: sdl2_dep)
      src_files += 'src/output/sdl_cava.c'
      h_O_files += 'include/cava/output/sdl_cava.h'
      deps += sdl2_dep
      add_project_arguments('-DSDL', language: 'c')

      if opengl_dep.found()
         src_files += 'src/output/sdl_glsl.c'
         h_O_files += 'include/cava/output/sdl_glsl.h'
         deps += opengl_dep
         add_project_arguments('-DSDL_GLSL', language: 'c')
      endif
   else
      warning('SDL_Init method in sdl2 library is not found. building without sdl2')
   endif
endif

### OSS
if not get_option('input_oss').disabled()
   have_oss = false
   if cc.check_header('sys/soundcard.h')
      code = '''
#include <sys/soundcard.h>
int main(int argc, char** argv) {
    static const int fmts_sle[] = {AFMT_S16_LE, AFMT_S32_LE, AFMT_S24_LE, AFMT_S8};
    return 0;
}
'''
      if cc.links(code)
         have_oss = true
      endif
   endif

   if have_oss
      src_files += 'src/input/oss.c'
      h_I_files += 'include/cava/input/oss.h'
      add_project_arguments('-DOSS -D__BSD_VISIBLE', language: 'c')
   else
      warning('No oss dev files found. building without oss')
   endif
endif

### JACK
jack_dep = dependency('jack', required: get_option('input_jack'), not_found_message: 'jack library is not found. building without jack')
if jack_dep.found()
   src_files += 'src/input/jack.c'
   h_I_files += 'include/cava/input/jack.h'
   deps += jack_dep
   add_project_arguments('-DJACK', language: 'c')
endif

### Cava font
have_font = false
if get_option('cava_font')
   if is_freebsd
      vtfontcvt_prog = find_program('vtfontcvt', native: true)
      psf2bdf_prog = find_program('psf2bdf', native: true)

      if vtfontcvt_prog.found() and psf2bdf_prog.found()
         cava_bdf = configure_file(
            command: [
               psf2bdf, '--fontname="-gnu-cava-medium-r-normal--16-160-75-75-c-80-iso10646-1"',
               '@INPUT@ @OUTPUT@'],
            input: 'cava.psf',
            output: 'cava.bdf'
         )
         cava_fnt = configure_file(
            command: [
               vtfontcvt, '-o', '@OUTPUT@ @INPUT@'],
            input: 'cava.bdf',
            output: 'cava.fnt'
         )

         have_font = true
         font_dir = join_paths(get_option('prefix'), 'share/cava')
         font_file = 'cava.fnt'
      else
         warning('Font conversion tool missing. Building without cava font supported!')
      endif
   else
      have_font = true
      font_dir = join_paths(get_option('prefix'), 'share/consolefonts')
      font_file = 'cava.psf'
   endif
endif

if have_font
   add_project_arguments('-DFONTDIR="@0@"'.format(font_dir), language: 'c')
   add_project_arguments('-DFONTFILE="@0@"'.format(font_file), language: 'c')
   add_project_arguments('-DCAVAFONT', language: 'c')

   install_data(font_file,
      install_dir: font_dir)
endif

cava_lib = shared_library('cava',
                          sources: src_files,
                          dependencies: deps,
                          include_directories: inc_dirs,
                          c_args: c_args,
                          install: true)

cava_dep = declare_dependency(link_with: cava_lib,
                              dependencies: deps,
                              include_directories: inc_dirs)

foreach p : get_option('build_target')
    if p != 'lib'
       executable('cava',
                  [src_files, 'src/cava.c'],
                  dependencies: deps,
                  include_directories: inc_dirs,
                  c_args: c_args,
                  install: true)
       break
    endif
endforeach

install_headers(h_files, subdir: 'cava')
install_headers(h_I_files, subdir: 'cava/input')
install_headers(h_O_files, subdir: 'cava/output')

pkg = import('pkgconfig')
pkg.generate(libraries: cava_lib,
             subdirs: ['cava', 'cava/input', 'cava/output'],
             version: meson.project_version(),
             name: 'libcava',
             filebase: 'cava',
             description: 'A cava library')
