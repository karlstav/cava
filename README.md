This is a CAVA fork the main purpose of is to build cava as a shared library.
In the same time cava application is provided as well
====================
CAVA [![Build Status](https://github.com/karlstav/cava/workflows/build-and-test/badge.svg)](https://github.com/karlstav/cava/actions)
====================

**C**ross-platform **A**udio **V**isu**a**lizer

by [Karl Stavestrand](mailto:karl@stavestrand.no)

<a href='https://play.google.com/store/apps/details?id=com.karlstav.cava&pcampaignid=pcampaignidMKT-Other-global-all-co-prtnr-py-PartBadge-Mar2515-1'><img alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png' width="200"/></a>

![spectrum](https://github.com/karlstav/cava/blob/master/example_files/cava.gif "spectrum")

[Demo video](https://youtu.be/9PSp8VA6yjU)

- [What it is](#what-it-is)
- [Installing](#installing)
  - [From Source](#from-source)
  - [Package managers](#package-managers)
- [Capturing audio](#capturing-audio)
  - [Pulseaudio](#pulseaudio)
  - [Pipewire](#pipewire)
  - [ALSA](#alsa)
  - [MPD](#mpd)
  - [Sndio](#sndio)
  - [OSS](#oss)
  - [JACK](#jack)
  - [squeezelite](#squeezelite)
  - [macOS](#macos-1)
  - [Windows](#windows)
- [Running via ssh](#running-via-ssh)
- [Troubleshooting](#troubleshooting)
- [Usage](#usage)
  - [Controls](#controls)
- [Configuration](#configuration)
- [Using cava in other applications](#using-cava-in-other-applications)
  - [cavacore](#cavacore-library)
  - [Raw Output](#raw-output)
- [Contribution](#contribution)



What it is
----------

Cava is a bar spectrum audio visualizer for terminal (ncurses) or desktop (SDL).

works on:
* Linux
* FreeBSD
* macOS
* Windows

This program is not intended for scientific use. It's written to look responsive and aesthetic when used to visualize music. 


Installing
------------------

### From Source

#### Installing Build Requirements

Required components:
* [FFTW](http://www.fftw.org/)
* libtool
* automake
* autoconf-archive (needed for setting up OpenGL)
* pkgconf
* build-essentials
* [iniparser](https://github.com/ndevilla/iniparser)


Recomended components:

The development lib of one of these audio frameworks, depending on your distro:
* ALSA
* Pulseaudio
* Pipewire
* Portaudio
* Sndio
* JACK


Optional components:
* SDL2 dev files
* [ncursesw dev files](http://www.gnu.org/software/ncurses/) (bundled in ncurses in arch)

Only FFTW, iniparser and the build tools are actually required for CAVA to compile, but this will only give you the ability to read from fifo files. To capture audio directlty from your system pipewire, pulseaudio, alsa, sndio, jack or portaudio dev files are required (depending on what audio system you are using).

Ncurses can be used as an alternative output method if you have issues with the default one. But it is not required.

All the requirements can be installed easily in all major distros:

FreeBSD

    pkg install autoconf autoconf-archive automake fftw3 iniparser jackit libglvnd libtool pkgconf psftools sdl2 sndio

Additionally, run these commands on FreeBSD before building:

    export CFLAGS="-I/usr/local/include"
    export LDFLAGS="-L/usr/local/lib"


Debian/Ubuntu:

    sudo apt install build-essential libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev libtool automake autoconf-archive libiniparser-dev libsdl2-2.0-0 libsdl2-dev libpipewire-0.3-dev libjack-jackd2-dev pkgconf


ArchLinux:

    pacman -S base-devel fftw ncurses alsa-lib iniparser pulseaudio autoconf-archive pkgconf


openSUSE:

    zypper install alsa-devel ncurses-devel fftw3-devel libpulse-devel libtool autoconf-archive pkgconf


Fedora:

    dnf install alsa-lib-devel ncurses-devel fftw3-devel pulseaudio-libs-devel libtool autoconf-archive iniparser-devel pkgconf

    
macOS:

First install homebrew if you have't already:

    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

Then install prerequisites:

    brew install fftw ncurses libtool automake autoconf-archive pkgconf portaudio iniparser
    
The installation location for Homebrew packages is different between Intel Macs and Apple Silicon Macs.
As such, the commands will be a little bit different.
You can find out which type you have [here](https://support.apple.com/en-us/HT211814#:~:text=To%20open%20About%20This%20Mac,as%20an%20Intel%2Dbased%20Mac.)

For both machines, run these commands to fix macOS not finding libtool and ncursesw:

    export LIBTOOL=`which glibtool`
    export LIBTOOLIZE=`which glibtoolize`
    ln -s `which glibtoolize` /usr/local/bin/libtoolize

On an Intel Mac, run the following command as well:

    ln -s /usr/lib/libncurses.dylib /usr/local/lib/libncursesw.dylib

On an Apple Silicon Mac, run this command instead:

    ln -s /opt/homebrew/lib/lib/libncursesw.6.dylib /usr/local/lib/libncursesw.dylib

Note that the file name may be a little bit different depending on the versions, but the directory should be the same.

Additionally, run these commands on Apple Silicon Macs so that ./configure can find the Homebrew packages:

    export LDFLAGS="-L/opt/homebrew/lib"
    export CPPFLAGS="-I/opt/homebrew/include"

Intel Mac instructions tested on macOS Big Sur.

Apple Silicon instructions tested on macOS Ventura.


Windows:

see separate readme in `cava_win` folder.

#### Building
 First of all clone this repo and cd in to it, then run:
 
    ./autogen.sh
    ./configure
    make

If you have a recommended component installed, but do not wish to use it (perhaps if building a binary on one machine to be used on another), then the corresponding feature can be disabled during configuration (see configure --help for details).

For windows there is a VS solution file in the `cava_win` folder.
    
#### Installing 

Install `cava` to default `/usr/local`:

    make install

Or you can change `PREFIX`, for example:

   ./configure --prefix=PREFIX

#### Uninstalling

    make uninstall


### Package managers

All distro specific instalation sources might be out of date. Please check version before reporting any issues here.


#### FreeBSD

    pkg install cava

#### openSUSE

Tumbleweed users have cava in their repo. They can just use:

    zypper in cava

Leap users need to add the multimedia:apps repository first:

    zypper ar -f obs://multimedia:apps/openSUSE_Leap_42.2 multimedia

If you use another version just replace *openSUSE_Leap_42.2* with *openSUSE_13.2*, adjust it to your version.

#### Fedora

Cava is available in Fedora 26 and later.  You can install Cava by
running:

    dnf install cava

#### Arch

Cava is in [AUR](https://aur.archlinux.org/packages/cava/).

    pacaur -S cava

#### Ubuntu/Debian

##### Ubuntu 20.10 or more recent / Debian (unstable)

    sudo apt install cava
    
##### Older Ubuntu

Harshal Sheth has added CAVA to his PPA, it can be installed with:

    add-apt-repository ppa:hsheth2/ppa
    apt update
    apt install cava
    
#### macOS

cava is in homebrew.

    brew install cava



Capturing audio
---------------

### Pulseaudio

Just make sure you have installed pulseaudio dev files and that cava has been built with pulseaudio support (it should be automatically if the dev files are found).

If you're lucky all you have to do is to run cava.
 
If nothing happens you might have to use a different source than the default. The default might also be your microphone. Look at the [config](#configuration) file for help. 


### Pipewire

Set

    method = pipewire

The default source is `auto` and will most likely be your currently selected output.
If you run wireplumber you can use `wpctl` to get the `object.path` or `object.serial` of the desired device to visualize.

e.g.

    source = alsa:pcm:3:front:3:playback


### ALSA

Set

    method = alsa

in the [config](#configuration) file.

ALSA can be difficult because there is no native way to grab audio from an output. If you want to capture audio straight fom the output (not just mic or line-in), you must create an ALSA loopback interface, then output the audio simultaneously to both the loopback and your normal interface.

To create a loopback interface simply run:

`sudo modprobe snd_aloop`

Hopefully your `aplay -l` should now contain a loopback interface.

To make it persistent across boot add the line `snd-aloop` to "/etc/modules". To keep it from being loaded as the first soundcard add the line `options snd-aloop index=1` to "/etc/modprobe.d/alsa-base.conf", this will load it at '1'. You can replace '1' with whatever makes most sense in your audio setup.

Playing the audio through your Loopback interface makes it possible for cava to capture it, but there will be no sound in your speakers. In order to play audio on the loopback interface and your actual interface you must make use of the ALSA multi channel.

Look at the included example file `example_files/etc/asound.conf` on how to use the multi channel. I was able to make this work with a HDA Intel PCH sound card, but I had no luck with the an USB DAC.

Read more about the ALSA method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa).

If you are having problems with the alsa method on Rasberry PI, try enabling `mmap` by adding the following line to `/boot/config.txt` and reboot:

```
dtoverlay=i2s-mmap
```

#### dmix

@reluekiss, was able to make cava work with dmix. Check out the example config in `example_files/etc/asound_dmix.conf` and issue [534](https://github.com/karlstav/cava/issues/534).



### mpd

Add these lines in mpd:

    audio_output {
        type                    "fifo"
        name                    "my_fifo"
        path                    "/tmp/mpd.fifo"
        format                  "44100:16:2"
    }

Uncomment and change input method to `fifo` in the [config](#configuration) file.

The path of the fifo can be specified with the `source` parameter.

I had some trouble with sync (the visualizer was ahead of the sound). Reducing the ALSA buffer in mpd fixed it:

    audio_output {
            type            "alsa"
            name            "My ALSA"
            buffer_time     "50000"   # (50ms); default is 500000 microseconds (0.5s)
    }

### Sndio

Set

    method = sndio

Sndio is the audio framework used on OpenBSD, but it's also available on FreeBSD, NetBSD and Linux.
So far this is only tested on FreeBSD, but it's probably very similar on other operating systems. The
following example demonstrates how to setup CAVA for sndio on FreeBSD (please consult the [OSS](#oss)
section for a deeper explanation of the various `pcmX` sound devices and the corresponding `/dev/dspX`
audio devices in this example).
```sh
$ cat /dev/sndstat
Installed devices:
pcm0: <Realtek ALC1220 (Rear Analog)> (play/rec) default
pcm1: <Realtek ALC1220 (Front Analog Mic)> (rec)
pcm2: <USB audio> (play/rec)
No devices installed from userspace.
```
Sndio operates on device descriptors. In general for every `/dev/dspX` audio device there is a corresponding
`rsnd/X` sndio raw device descriptor. In this example there are `rsnd/0`, `rsnd/1` and `rsnd/2` (they
are not listed in `/dev`, sndio uses these descriptors to access the corresponding audio devices internally).
Sndio also handles the implicit `default` device descriptor, which acts like a symlink to the raw device
descriptor corresponding to the default audio device `/dev/dsp`. In this example it acts like a symlink
to `rsnd/0` because the default audio device `/dev/dsp` symlinks to `/dev/dsp0`. Sndio also evaluates
the environment variables `AUDIODEVICE` and `AUDIORECDEVICE`. If one of these is set (`AUDIORECDEVICE`
overrides `AUDIODEVICE` if both are set) and a sndio-aware program tries to open the `default` device
descriptor or an unspecified device descriptor, then the program will use the device descriptor specified
in the environment variable.

Now in order to visualize the mic input in CAVA, the `source` value in the configuration file must
be set to the corresponding audio descriptor:

    source = default    # default; symlink to rsnd/0 in this example; AUDIORECDEVICE and AUDIODEVICE evaluation
    source =            # unspecified device descriptor; same as default above
    source = rsnd/0     # for the pcm0 mic on the rear
    source = rsnd/1     # for the pcm1 mic on the front
    source = rsnd/2     # for the pcm2 mic on the USB headset

With `source = default` one can switch the visualization on the commandline without changing the configuration
file again:
```sh
$ AUDIODEVICE=rsnd/0 cava
$ AUDIODEVICE=rsnd/1 cava
$ AUDIODEVICE=rsnd/2 cava
```
Sndio can't record the played back audio with just the raw device descriptors, i.e. the sounds from
a music player or a browser which play on the external stereo speakers through `rsnd/0` are not visualized
in CAVA. For this to work the sndio server has to be started and a monitoring sub-device has to be
created. The following example shows how to start the server and create a monitoring sub-device `snd/0`
from `rsnd/0` and then start CAVA with `AUDIODEVICE` pointing to the new monitoring sub-device:
```sh
$ sndiod -f rsnd/0 -m play,mon
$ AUDIODEVICE=snd/0 cava
```
Switch between the speakers and the USB headset:
```sh
$ sndiod -f rsnd/2 -m play,mon -s usb -f rsnd/0 -m play,mon -s speakers
$ AUDIODEVICE=snd/usb cava
$ AUDIODEVICE=snd/speakers cava
```
Consult the manpage `sndiod(8)` for further information regarding configuration and startup of a sndio
server.

### OSS

Set

    method = oss

The audio system used on FreeBSD is the Open Sound System (OSS).
The following example demonstrates how to setup CAVA for OSS on FreeBSD:
```sh
$ cat /dev/sndstat
Installed devices:
pcm0: <Realtek ALC1220 (Rear Analog)> (play/rec) default
pcm1: <Realtek ALC1220 (Front Analog Mic)> (rec)
pcm2: <USB audio> (play/rec)
No devices installed from userspace.
```
The system has three `pcm` sound devices, `pcm0`, `pcm1` and `pcm2`. `pcm0` corresponds to the analog
output jack on the rear, in which external stereo speakers are plugged in, and the analog input jack,
in which one could plug in a microphone. Because it encapsulates both, output and input, it is marked
as `play/rec`. It is also set as the `default` sound device. `pcm1` corresponds to another analog input
jack for a mic on the front side and is marked `rec`. A USB headset with an integrated mic is plugged
in an USB port and the system has created the `pcm2` sound device with `play/rec` capabilities for
it.

In general for every `pcmX` device there is a corresponding `/dev/dspX` audio device. In this example
there are `/dev/dsp0`, `/dev/dsp1` and `/dev/dsp2` (the system creates them when needed, they are not
listed via `ls /dev` if they are currently not in use). The system also creates an implicit `/dev/dsp`,
which acts like a symlink to the `default` audio device, in this example to `/dev/dsp0`.

Now in order to visualize the mic input in CAVA, the `source` value in the configuration file must
be set to the corresponding audio device:

    source = /dev/dsp     # default; symlink to /dev/dsp0 in this example
    source = /dev/dsp0    # for the pcm0 mic on the rear
    source = /dev/dsp1    # for the pcm1 mic on the front
    source = /dev/dsp2    # for the pcm2 mic on the USB headset

OSS can't record the outgoing audio on its own, i.e. the sounds from a music player or a browser which
play on the external stereo speakers through `/dev/dsp0` are not visualized in CAVA. A solution is
to use Virtual OSS. It can create virtual audio devices from existing audio devices, in particular
it can create a loopback audio device from `/dev/dsp0` and from which the played back audio can be
fed into CAVA:
```sh
$ doas pkg install virtual_oss
$ doas virtual_oss -r44100 -b16 -c2 -s4ms -O /dev/dsp0 -R /dev/null -T /dev/sndstat -l dsp.cava

$ cat /dev/sndstat
Installed devices:
pcm0: <Realtek ALC1220 (Rear Analog)> (play/rec) default
pcm1: <Realtek ALC1220 (Front Analog Mic)> (rec)
pcm2: <USB audio> (play/rec)
Installed devices from userspace:
dsp.cava: <Virtual OSS> (play/rec)
```
It created a virtual loopback device `/dev/dsp.cava` from `/dev/dsp0`. Now the audio is visualized
in CAVA with `source = /dev/dsp.cava` in the configuration file. The playback program must have a configuration
to use the `/dev/dsp.cava` device. For programs where this is not possible, e.g. which always use `/dev/dsp`,
replace `-l dsp.cava` with `-l dsp`. Virtual OSS can be configured and started as a service on FreeBSD.

### JACK

Set

    method = jack

The JACK Audio Connection Kit (JACK) is a professional sound server API which is available on several
operating systems, e.g. FreeBSD and Linux.

CAVA is a JACK client with the base client name `cava` and adheres to the standard server start and
stop behaviour, i.e. CAVA starts a JACK server if none is already running and the environment variable
`JACK_START_SERVER` is defined, in which case the server also stops when all clients have exited. The
`source` in the CAVA configuration file specifies the name of the JACK server to which CAVA tries to
connect to. The default value is `default`, which is also the default JACK server name. The value can
be empty, in which case it implies `default`. Therefore the following three entries are equivalent:

    ; source = default
    source = default
    source =

One exception is the combination of an empty `source` entry and the environment variable `JACK_DEFAULT_SERVER`.
If the environment variable is defined, e.g. `export JACK_DEFAULT_SERVER=foo`, then the following entries
are equivalent:

    source = foo
    source =

Consult the manpage `jackd(1)` for further information regarding configuration and startup of a JACK
server.

CAVA creates terminal audio-typed (so no MIDI support) input ports. These ports can connect to output
ports of other JACK clients, e.g. connect to the output ports of a music player and CAVA will visualize
the music. Currently CAVA supports up to two input ports, i.e. it supports mono and stereo. The number
of input ports can be controlled via the `channels` option in the input section of the configuration
file:

    channels = 1    # one input port, mono
    channels = 2    # two input ports, stereo (default)

The port's short name is simply `M` for mono, and `L` and `R` for stereo. The full name of the input
port according to the base client name is `cava:M` for mono, and `cava:L` and `cava:R` for stereo.

The option `autoconnect` controls the connection strategy for CAVA's ports to other client's ports:

    autoconnect = 0    # don't connect to other ports automatically
    autoconnect = 1    # only connect to other ports during startup
    autoconnect = 2    # reconnect to new ports regularly (default)

The automatic connection strategies scan the physical terminal input-ports, i.e. the real audio device
which actually outputs the sound, and applies the same connections to CAVA's ports. In this way CAVA
visualizes the played back audio from JACK clients by default.

In order to control and manage the connection between CAVA's ports and ports of other client programs,
there are connection management programs for JACK. Some well known connection managers with a graphical
user interface are QjackCtl and Cadence. The JACK package itself often comes with CLI tools. Depending
on the operating system it could be necessary to install them separately, e.g. on FreeBSD:
```sh
$ doas pkg install jack-example-tools
```
Among the tools are the programs `jack_lsp` and `jack_connect`. These two tools are enough to list
and connect ports on the commandline. The following example demonstrates how to setup connections with
these tools:
```sh
$ jack_lsp
system:capture_1
system:capture_2
system:playback_1
system:playback_2
cava:L
moc:output0
moc:output1
cava:R
```
This listing shows all full port names that are currently available. These correspond to two external
JACK clients, `cava` and `moc`, and one internal JACK client `system`. The types and current active
connections between the ports can be listed with the `-p` and `-c` switches for `jack_lsp`. In order
to connect the ports of CAVA and MOC, `jack_connect` is used:
```sh
$ jack_connect cava:L moc:output0
$ jack_connect cava:R moc:output1
```
Now CAVA visualizes the outgoing audio from MOC.

### squeezelite
[squeezelite](https://en.wikipedia.org/wiki/Squeezelite) is one of several software clients available for the Logitech Media Server. Squeezelite can export its audio data as shared memory, which is what this input module uses.
Just adapt your [config](#configuration):
```
method = shmem
source = /squeezelite-AA:BB:CC:DD:EE:FF
```
where `AA:BB:CC:DD:EE:FF` is squeezelite's MAC address (check the LMS Web GUI (Settings>Information) if unsure).
Note: squeezelite must be started with the `-v` flag to enable visualizer support.

### macOS

Note: Cava doesn't render correctly within the default macOS terminal. In order to achieve an optimal display, install [Kitty](https://sw.kovidgoyal.net/kitty/index.html). Beware that you may run in to the issue presented in #109; however, it can be resolved with [this](https://stackoverflow.com/questions/7165108/in-os-x-lion-lang-is-not-set-to-utf-8-how-to-fix-it).

**Background Music**

Install [Background Music](https://github.com/kyleneideck/BackgroundMusic) which provides a loopback interface automatically. Once installed and running just edit your [config](#configuration) to use this interface with portaudio:

```
method = portaudio
source = "Background Music"
```

**Sound Flower**

[Soundflower](https://github.com/mattingalls/Soundflower) also works to create a loopback interface. Use Audio MIDI Setup to configure a virtual interface that outputs audio to both your speakers and the loopback interface, following [this](https://github.com/RogueAmoeba/Soundflower-Original/issues/44#issuecomment-151586106) recipe. By creating a multi-output device you lose the ability to control the volume on your keyboard. Because of this, we recommend the Background Music app which still gives you keyboard controls.

Then edit your [config](#configuration) to use this interface with portaudio:

```
method = portaudio
source = "Soundflower (2ch)"
```

### Windows

Should capture the audio from the default output device automatically.


Running via ssh
---------------

To run via ssh to an external monitor, redirect output to `/dev/console`:

     ~# ./cava  <> /dev/console >&0 2>&1

exit with ctrl+z then run 'bg' to keep it running after you log out.

(You must be root to redirect to console. Simple sudo is not enough: Run `sudo su` first.)


## Troubleshooting

### No bars in terminal

Most likley issue [#399](https://github.com/karlstav/cava/issues/399). Locale settings need to be set correctly in order for cava to work.

### Visualizer reacts to microphone instead of output

This is a known issue with pipewire. Try the workaround described [here](https://github.com/karlstav/cava/issues/422#issuecomment-994270910)

### Vertical lines in bars

This is either an issue with the font, or line spacing being enabled in the terminal emulator. Try to change font or disable line spacing.

### Low resolution

Since the graphics are simply based on characters, try decreasing the font size.

### Low frame rate

Some terminal emulators are just slow. Cava will look best in a GPU based terminal like kitty or alacritty. You can also try to increase the font size

### Font is changed in ttys after exit

If you run cava in a TTY (like ctrl+alt+F2) the program will change the font to the included `cava.psf` (actually a slightly modified "unifont").

In console fonts it seems that only 256 Unicode characters are supported, probably because they are bitmap fonts. I could not find a font with Unicode characters 2581-2587 (the 1/8 - 7/8 blocks used on the top of each bar to increase resolution).

So in `cava.psf`, the characters 1-7 are actually replaced by Unicode characters 2581-2587. When cava exits, it changes the font back. If cava exits abnormally and you notice that 1-7 are replaced by partial blocks, just change the font with `setfont`.

Actually, `setfont` is supposed to return the default font, but this usually isn't set. I haven't found another way to get the current font. So cava sets the font to "Lat2-Fixed16" when interrupted. All major distros should have it. It will revert to your default font at reboot.

### Gradient not working in Konsole

Konsole simply does not support this. #194

Usage
-----

    Usage : cava [options]
    Visualize audio input in terminal. 

    Options:
    	    -p          path to config file
    	    -v          print version



Exit with ctrl+c or q.

If cava quits unexpectedly or is force killed, echo must be turned on manually with `stty -echo`.

### Controls

NOTE: only works in ncurses output mode.

| Key | Description |
| --- | ----------- |
| <kbd>up</kbd> / <kbd>down</kbd>| increase/decrease sensitivity |
| <kbd>left</kbd> / <kbd>right</kbd>| increase/decrease bar width |
| <kbd>f</kbd> / <kbd>b</kbd>| change foreground/background color |
| <kbd>r</kbd> | Reload configuration |
| <kbd>c</kbd> | Reload colors only |
| <kbd>q</kbd> or <kbd>CTRL-C</kbd>| Quit C.A.V.A. |

Configuration
-------------

As of version 0.4.0 all options are done in the config file, no more command-line arguments!

By default a configuration file is created upon first launch in `$XDG_CONFIG_HOME/cava/config` or `$HOME/.config/cava/config`, but cava can also be made to use a different file with the `-p` option.

Sending cava a SIGUSR1 signal, will force cava to reload its configuration file. Thus, it behaves as if the user pressed <kbd>r</kbd> in the terminal. One might send a SIGUSR1 signal using `pkill` or `killall`.
For example:
```
$ pkill -USR1 cava
```

Similarly, sending cava a SIGUSR2 signal will only reload the colors from the configuration file, which is the same as pressing <kbd>c</kbd> in the terminal. This is slightly faster than reloading the entire config as the audio processing does not need to reinitialize.  
```
$ pkill -USR2 cava
```

**Examples on how the equalizer works:**

    [eq]
    1=0
    2=1
    3=0
    4=1
    5=0

![3_138](https://cloud.githubusercontent.com/assets/6376571/8670183/a54a851e-29e8-11e5-9eff-346bf6ed91e0.png)

    [eq]
    1=2
    2=2
    3=1
    4=1
    5=0.5

![3_139](https://cloud.githubusercontent.com/assets/6376571/8670181/9db0ef50-29e8-11e5-81bc-3e2bb9892da0.png)


Using cava in other applications
--------------------------------

### cavacore library

* The core processing engine in cava has been split into a separate library `cavacore`. See CAVACORE.md for details.
* cava can be built as a standalone libcava library. Practice example can be found [waybar cava module](https://github.com/Alexays/Waybar/blob/master/src/modules/cava.cpp) :

    ```
    meson setup build
    meson compile -C build
    meson install -C build
    ```

### Raw Output

You can also use Cava's output for other programs by using raw output mode, which will write bar data to `STDOUT` that can be piped into other processes. More information on this option is documented in [the example config file](/example_files/config).

A useful starting point example script written in python that consumes raw data can be found [here](https://github.com/karlstav/cava/issues/123#issuecomment-307891020).


Contribution
------

Please read CONTRIBUTING.md before opening a pull request.

Thanks to:
* [CelestialWalrus](https://github.com/CelestialWalrus)
* [anko](https://github.com/anko)
* [livibetter](https://github.com/livibetter)

for major contributions in the early development of this project.

Also thanks to [dpayne](https://github.com/dpayne/) for figuring out how to find the pulseaudio default sink name.
