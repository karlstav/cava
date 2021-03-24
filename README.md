C.A.V.A. [![Build Status](https://github.com/karlstav/cava/workflows/build/badge.svg)](https://github.com/karlstav/cava/actions)
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA

also supports audio input from Pulseaudio, fifo (mpd), sndio, squeezelite and portaudio.

Now also works on macOS!

by [Karl Stavestrand](mailto:karl@stavestrand.no)

![spectrum](https://github.com/karlstav/cava/blob/master/example_files/cava.gif "spectrum")

[Demo video](https://youtu.be/9PSp8VA6yjU)

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [What it is](#what-it-is)
- [Installing](#installing)
  - [From Source](#from-source)
    - [Installing Build Requirements](#installing-build-requirements)
    - [Building](#building)
    - [Installing](#installing-1)
    - [Uninstalling](#uninstalling)
  - [Some distro specific pre-made binaries/recipes](#some-distro-specific-pre-made-binariesrecipes)
    - [openSUSE](#opensuse)
    - [Fedora](#fedora)
    - [Arch](#arch)
    - [Ubuntu/Debian](#ubuntudebian)
- [Capturing audio](#capturing-audio)
  - [Pulseaudio monitor source (Easy, default if supported)](#pulseaudio-monitor-source-easy-default-if-supported)
  - [ALSA-loopback device (Tricky)](#alsa-loopback-device-tricky)
  - [mpd's fifo output](#mpds-fifo-output)
  - [sndio](#sndio)
  - [squeezelite](#squeezelite)
  - [macOS](#macos)
  - [Windows - winscap - WSL](#Windows---winscap---WSL)
- [Running via ssh](#running-via-ssh)
  - [Raw Output](#raw-output)
- [Font notes](#font-notes)
  - [In ttys](#in-ttys)
  - [In terminal emulators](#in-terminal-emulators)
- [Latency notes](#latency-notes)
- [Usage](#usage)
  - [Controls](#controls)
- [Configuration](#configuration)
- [Contribution](#contribution)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->


What it is
----------

C.A.V.A. is a bar spectrum audio visualizer for the Linux terminal using ALSA, pulseaudio or fifo buffer for input.

This program is not intended for scientific use. It's written to look responsive and aesthetic when used to visualize music. 


Installing
------------------

### From Source

#### Installing Build Requirements

Required components:
* [FFTW](http://www.fftw.org/)
* libtool
* automake
* build-essentials
* [iniparser](https://github.com/ndevilla/iniparser)


Recomended components:
* [ncursesw dev files](http://www.gnu.org/software/ncurses/) (bundled in ncurses in arch)
* [ALSA dev files](http://alsa-project.org/), or
* [Pulseaudio dev files](http://freedesktop.org/software/pulseaudio/doxygen/), or
* Portaudio, or
* Sndio

Only FFTW and the other build tools are actually required for CAVA to compile, but this will only give you the ability to read from fifo files. To more easly grab audio from your system pulseaudio, alsa, sndio or portaudio dev files are recommended (depending on what audio system you are using). Not sure how to get the pulseaudio dev files for other distros than debian/ubuntu or if they are bundled in pulseaudio. 


For better a better visual experience ncurses is also recomended.

Iniparser is also required, but if it is not already installed, a bundled version will be used.

All the requirements can be installed easily in all major distros:

Debian Buster or higher/Ubuntu 18.04 or higher :

    apt install libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev libtool automake libiniparser-dev
    export CPPFLAGS=-I/usr/include/iniparser


older Debian/Ubuntu:

    apt install libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev libtool automake


ArchLinux:

    pacman -S base-devel fftw ncurses alsa-lib iniparser pulseaudio


openSUSE:

    zypper install alsa-devel ncurses-devel fftw3-devel libpulse-devel libtool


Fedora:

    dnf install alsa-lib-devel ncurses-devel fftw3-devel pulseaudio-libs-devel libtool

    
macOS:

First install homebrew if you have't already:

    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

Then install prerequisites:

    brew install fftw ncurses libtool automake portaudio
    
Then fix macOS not finding libtool and ncursesw:

    export LIBTOOL=`which glibtool`
    export LIBTOOLIZE=`which glibtoolize`
    ln -s `which glibtoolize` /usr/local/bin/libtoolize
    ln -s /usr/lib/libncurses.dylib /usr/local/lib/libncursesw.dylib

Tested on macOS High Sierra.

For M1 Mac I was able to build all prerequisites from source. It might work with homebrew rosetta emulation, but what's the fun in that.

* build and install automake, autoconf and libtool following the instructions [here](https://superuser.com/questions/383580/how-to-install-autoconf-automake-and-related-tools-on-mac-os-x-from-source).
* build and install fftw from the gz archive [here](http://www.fftw.org/download.html)
* download ncurses source and configure with these options:
```
./configure --prefix=/usr/local \
  --without-cxx --without-cxx-binding --without-ada --without-progs --with-curses-h \
  --with-shared --without-debug  \
  --enable-widec --enable-const --enable-ext-colors --enable-sigwinch --enable-wgetch-events \
```
* just clone portaudio repo, build and install.
* install [Backround Music](https://github.com/kyleneideck/BackgroundMusic) following option 1 in "Installing from Source Code". (requires xcode)
* then build cava normally and follow the instructions in "capturing audio"




#### Building
 First of all clone this repo and cd in to it, then run:
 
    ./autogen.sh
    ./configure
    make

If you have a recommended component installed, but do not wish to use it (perhaps if building a binary on one machine to be used on another), then the corresponding feature can be disabled during configuration (see configure --help for details).


    
#### Installing 

Install `cava` to default `/usr/local`:

    make install

Or you can change `PREFIX`, for example:

   ./configure --prefix=PREFIX

#### Uninstalling

    make uninstall


### Some distro specific pre-made binaries/recipes    
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

    apt install cava
    
##### Older Ubuntu

Harshal Sheth has added CAVA to his PPA, it can be installed with:

    add-apt-repository ppa:hsheth2/ppa
    apt update
    apt install cava
    

All distro specific instalation sources might be out of date.


Capturing audio
---------------

### Pulseaudio monitor source (Easy, default if supported)

Just make sure you have installed pulseaudio dev files and that cava has been built with pulseaudio support (it should be automatically if the dev files are found).

If you're lucky all you have to do is to run cava.
 
If nothing happens you might have to use a different source than the default. The default might also be your microphone. Look at the [config](#configuration) file for help. 


### ALSA-loopback device (Tricky)

Set

    method = alsa

in the [config](#configuration) file.

ALSA can be difficult because there is no native way to grab audio from an output. If you want to capture audio straight fom the output (not just mic or line-in), you must create an ALSA loopback interface, then output the audio simultaneously to both the loopback and your normal interface.

To create a loopback interface simply run:

`sudo modprobe snd_aloop`

Hopefully your `aplay -l` should now contain a loopback interface.

To make it persistent across boot add the line `snd-aloop` to "/etc/modules". To keep it from being loaded as the first soundcard add the line `options snd-aloop index=1` to "/etc/modprobe.d/alsa-base.conf", this will load it at '1'. You can replace '1' with whatever makes most sense in your audio setup.

Playing the audio through your Loopback interface makes it possible for cava to capture it, but there will be no sound in your speakers. In order to play audio on the loopback interface and your actual interface you must make use of the ALSA multi channel.

Look at the included example file `example_files/etc/asound.conf` on how to use the multi channel. I was able to make this work on my laptop (an Asus UX31 running Ubuntu), but I had no luck with the ALSA method on my Raspberry Pi (Rasbian) with an USB DAC. The PulseAudio method however works perfectly on my Pi. 

Read more about the ALSA method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa).

If you are having problems with the alsa method on Rasberry PI, try enabling `mmap` by adding the following line to `/boot/config.txt` and reboot:

```
dtoverlay=i2s-mmap
```

### mpd's fifo output

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

### sndio

sndio is the audio framework used on OpenBSD, but it's also available on
FreeBSD and Linux. So far this is only tested on FreeBSD.

To test it
```bash
# Start sndiod with a monitor sub-device
$ sndiod -dd -s default -m mon -s monitor

# Set the AUDIODEVICE environment variable to override the default
# sndio device and run cava
$ AUDIODEVICE=snd/0.monitor cava
```

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

### Windows - winscap - WSL

@quantum5 has written a handy tool called [winscap](https://github.com/quantum5/winscap) to capture audio from Windows and output it to stdout. Just follow the instructions in the readme on how to set it up with cava.


Running via ssh
---------------

To run via ssh to an external monitor, redirect output to `/dev/console`:

     ~# ./cava  <> /dev/console >&0 2>&1

exit with ctrl+z then run 'bg' to keep it running after you log out.

(You must be root to redirect to console. Simple sudo is not enough: Run `sudo su` first.)

### Raw Output

You can also use Cava's output for other programs by using raw output mode, which will write bar data to `STDOUT` that can be piped into other processes. More information on this option is documented in [the example config file](/example_files/config).

A useful starting point example script written in python that consumes raw data can be found [here](https://github.com/karlstav/cava/issues/123#issuecomment-307891020).

Font notes
----------

Since the graphics are simply based on characters, performance is dependent on the terminal font.

### In ttys

If you run this in a TTY the program will change the font to the included `cava.psf` (actually a slightly modified "unifont").

In console fonts it seems that only 256 Unicode characters are supported, probably because they are bitmap fonts. I could not find a font with Unicode characters 2581-2587 (the 1/8 - 7/8 blocks used on the top of each bar to increase resolution).

So in `cava.psf`, the characters 1-7 are actually replaced by Unicode characters 2581-2587. When cava exits, it changes the font back. If cava exits abnormally and you notice that 1-7 are replaced by partial blocks, just change the font with `setfont`.

Actually, `setfont` is supposed to return the default font, but this usually isn't set. I haven't found another way to get the current font. So cava sets the font to "Lat2-Fixed16" when interrupted. All major distros should have it. It will revert to your default font at reboot.

### In terminal emulators

In terminal emulators like `xterm`, the font settings is chosen in the software and cannot be changed by an application. So find your terminal settings and try out different fonts and settings. Also character spacing affects the look of the bar spectrum.

Performance is also different, urxvt is the best I found so far, while Gnome-terminal is quite slow.

Cava also disables the terminal cursor, and turns it back on on exit, but in case it terminates unexpectedly, run `setterm -cursor on` to get it back.

Tip: Cava will look much nicer in small font sizes. Use a second terminal emulator for cava and set the font size to 1. Warning, can cause high CPU usage and latency if the terminal window is too large!


Latency notes
-------------

If you see latency issues (sound before image) in a terminal emulator, try increasing the font size. This will reduce the number of characters that have to be shown.

If your audio device has a huge buffer, you might experience that cava is actually faster than the audio you hear. This reduces the experience of the visualization. To fix this, try decreasing the buffer settings in your audio playing software.

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

By default a configuration file is located in `$XDG_CONFIG_HOME/cava/config` or `$HOME/.config/cava/config`, but cava can also be made to use a different file with the `-p` option.

If for some reason the config file is not in the config dir, copy the [bundled configuration file](/example_files/config) to the config dir manually.

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

Contribution
------

Please read CONTRIBUTING.md before opening a pull request.

Thanks to:
* [CelestialWalrus](https://github.com/CelestialWalrus)
* [anko](https://github.com/anko)
* [livibetter](https://github.com/livibetter)

for major contributions in the early development of this project.

Also thanks to [dpayne](https://github.com/dpayne/) for figuring out how to find the pulseaudio default sink name.
