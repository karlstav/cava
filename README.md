C.A.V.A. (M.P.)
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA (**M**PD and **P**ulseaudio)

by [Karl Stavestrand](mailto:karl@stavestrand.no)

![spectrum](https://cloud.githubusercontent.com/assets/5231746/7368413/7d7ca93e-ed9f-11e4-842f-61c54fc03dbe.gif "spectrum")

thanks to [anko](https://github.com/anko) for the gif, here is the [recipe]( http://unix.stackexchange.com/questions/113695/gif-screencastng-the-unix-way).

[Demo video](https://youtu.be/9PSp8VA6yjU)

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [What it is](#what-it-is)
- [Build requirements](#build-requirements)
- [Getting started](#getting-started)
  - [Installing manually](#installing-manually)
  - [Uninstalling](#uninstalling)
  - [openSUSE](#opensuse)
  - [Fedora](#fedora)
  - [Arch](#arch)
- [Capturing audio](#capturing-audio)
  - [From Pulseaudio monitor source (New, easy and experimental!)](#from-pulseaudio-monitor-source-new-easy-and-experimental)
  - [From ALSA-loopback device](#from-alsa-loopback-device)
    - [PulseAudio combined-sink (easy)](#pulseaudio-combined-sink-easy)
    - [ALSA multi channel (hard)](#alsa-multi-channel-hard)
  - [From mpd's fifo output](#from-mpds-fifo-output)
- [Running via ssh](#running-via-ssh)
- [Font notes](#font-notes)
  - [In ttys](#in-ttys)
  - [In terminal emulators](#in-terminal-emulators)
- [Latency notes](#latency-notes)
- [Usage](#usage)
  - [Controls](#controls)
- [Configuration](#configuration)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->


What it is
----------

C.A.V.A. is a bar spectrum analyzer for audio using ALSA for input. Its frequency range is limited to 50-10,000Hz. I know that the human ear can hear from 20 up to 20,000 Hz (and probably "sense" even higher frequencies), but the frequencies between 50-10,000Hz seem to me to be the most distinguishable. (I believe telephones used to be limited to as low as 8kHz.)

This program is not intended for scientific use.

This is my first published code. I am not a professional programmer so the source code is probably, by all conventions, a complete mess. Please excuse all the typos as I am both dyslexic and foreign.

Any tips or comments would be much appreciated.


Build requirements
------------------
* [FFTW](http://www.fftw.org/)
* [ncursesw dev files](http://www.gnu.org/software/ncurses/) (bundled in ncurses in arch)
* [ALSA dev files](http://alsa-project.org/)
* [Pulseaudio dev files](http://freedesktop.org/software/pulseaudio/doxygen/)

Only FFTW is actually required for CAVA to compile, but for maximum usage and preformance ncurses and pulseaudio and/or alsa dev files are recommended. Not sure how to get the pulseaudio dev files for other distros than debian/ubuntu or if they are bundled in pulseaudio.

All these can be installed easily in all major distros:

Debian/Raspbian:

    apt-get install libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev

ArchLinux:

    pacman -S base-devel fftw ncurses

openSUSE:

    zypper install alsa-devel ncurses-devel fftw3-devel

Fedora:

    dnf install alsa-lib-devel ncurses-devel fftw3-devel
   

A system-wide installation of Iniparser will also be used if it is installed, if not the bundled version will be used.

To run the autogen script you will also need `automake`, `libtool` and `git`.


Getting started
---------------

    ./autogen.sh
    ./configure
    make

You can use the following for compilation options:

    --enable-debug          enable debug messages and frequency table output

For example, turning on debugging messages:

    ./configure --enable-debug 
    
### Installing manually

Install `cava` to default `/usr/local`:

    make install

Or you can change `PREFIX`, for example:

   ./configure --prefix=PREFIX

### Uninstalling

    make uninstall

### openSUSE

On openSUSE you need to add jubalh's home repository to install cava via zypper. For example for Tumbleweed do:

    zypper ar -f http://download.opensuse.org/repositories/home:/jubalh/openSUSE_Tumbleweed/home:jubalh.repo
	zypper in cava

If you use 13.2 or 13.1 just replace *openSUSE_Tumbleweed* with *openSUSE_13.2*, adjust it to your version.

### Fedora

[RPM spec file](https://gist.github.com/sedrubal/617d40deb7452a25d004#file-cava-spec)

### Arch

Cava is in [AUR](https://aur.archlinux.org/packages/cava/).

    yaourt -S cava


All distro specific instalations sources might be out of date.


Capturing audio
---------------

### From Pulseaudio monitor source (Easy, default if supported)

First make sure you have installed pulseaudio dev files and that cava has been built with pulseaudio support (it should be automatically if the dev files are found).

If you're lucky all you have to do is to uncomment this line in the config file under input:

    method = pulse
 
If nothing happens you might have to use a different source than the default. The default might also be your microphone. Look at the config file for help. 


### From ALSA-loopback device (Tricky)

If you want to capture audio straight fom the output (not just mic or line-in), you must create an ALSA loopback interface, then output the audio simultaneously to both the loopback and your normal interface.

To create a loopback interface simply run:

`sudo modprobe snd_aloop`

Hopefully your `aplay -l` should now contain a loopback interface.

To make it presistent across boot add the line `snd-aloop` to "/etc/modules". To keep it form beeing loaded as the first soundcard add the line `options snd-aloop index=1` to "/etc/modprobe.d/alsa-base.conf", this will load it at '1'. You can replace '1' with whatever makes most senes in your audio setup.

Playing the audio through your Loopback interface makes it possible for cava to to capture it, but there will be no sound in your speakers. :(

Not to worry! There are (at least) two ways of sending the audio output to the loopback *and* your actual audio interface at the same time:

#### PulseAudio combined-sink (easy)

First create the loopback device as explained in the section above.

To `/etc/pulse/default.pa`, add the line `load-module module-combine-sink` (in PulseAudio versions <1.0, the module was only called `module-combine`). Then restart PulseAudio. For some reason, I had to turn off realtime scheduling for this to work on a Raspberry Pi (set `realtime-scheduling = no` in `/etc/pulse/daemon.conf`).

PulseAudio setup can also be done in paprefs (Debian: `sudo apt-get install paprefs && paprefs`): In the far right tab check the box "Simultaneous Output".

An extra Output should appear in your sound options called "Simultaneous output to..." Note that when using this method if you turn down the volume on the Simultaneous output, this will effect the visualizer. To avoid this, select the actual output, turn down the volume, then select the Simultaneous output again.


#### ALSA multi channel (hard)

First create the loopback device as explained in the section above.

Look at the inculded example file `example_files/etc/asound.conf`. I was able to make this work on my laptop (an Asus UX31 running Elementary OS). I had no luck with the ALSA method on my Rasberry PI (Rasbian) with an USB DAC. The PulseAudio method however works perfectly on my PI.

Read more about the ALSA method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa).


### From mpd's fifo output

Add these lines in mpd:

    audio_output {
        type                    "fifo"
        name                    "my_fifo"
        path                    "/tmp/mpd.fifo"
        format                  "44100:16:2"
    }

Uncomment and change input method to `fifo` in the config file.

The path of the fifo can be specified with the `source` parameter.

I had some trouble with sync (the visualizer was ahead of the sound). Reducing the ALSA buffer in mpd fixed it:

    audio_output {
            type            "alsa"
            name            "My ALSA"
            buffer_time     "50000"   # (50ms); default is 500000 microseconds (0.5s)
    }


Running via ssh
---------------

To run via ssh to an external monitor, redirect output to `/dev/console`:

     ~# ./cava  <> /dev/console >&0 2>&1

exit with ctrl+z then run 'bg' to keep it running after you log out.

(You must be root to redirect to console. Simple sudo is not enough: Run `sudo su` first.)



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

Speed preformance is also different, urxvt is the best I found so far, while Gnome-terminal is quite slow.

Cava also disables the terminal cursor, and turns it back on on exit, but in case it terminates unexpectedly, run `setterm -cursor on` to get it back.

Tip: Cava will look much nicer in small font sizes. Use a second terminal emulator for cava and set the font size to 1. Warning, can cause high CPU usage and latency if the terminal window is too large!


Latency notes
-------------

If you see latency issues (sound before image) in a terminal emulator, try increasing the font size. This will reduce the number of characters that have to be shown.

If your audio device has a huge buffer, you might experience that cava is actually faster than the audio you hear. This reduces the experience of the visualization. To fix this, you try decreasing the buffer settings in your audio playing software.

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

| Key | Description |
| --- | ----------- |
| <kbd>up</kbd> / <kbd>down</kbd>| increase/decrease sensitivity |
| <kbd>left</kbd> / <kbd>right</kbd>| increase/decrease bar width |
| <kbd>c</kbd> / <kbd>b</kbd>| change forground/background color |
| <kbd>r</kbd> | Reload configuration |
| <kbd>q</kbd> or <kbd>CTRL-C</kbd>| Quit C.A.V.A. |

Configuration
-------------

As of version 0.4.0 all options are done in the config file, no more command-line arguments!

By default a configuration file is located in `$XDG_CONFIG_HOME/cava/config` or `$HOME/.config/cava/config`, but cava can also be made to usa a different file with the `-p` option. Look in the default config file for details on the different options.


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

Thanks
------

Contributors:
* [Karl Stavestrand](mailto:karl@stavestrand.no) - creator
* [CelestialWalrus](https://github.com/CelestialWalrus)
* [anko](https://github.com/anko)
* [livibetter](https://github.com/livibetter)

Also thanks to [dpayne](https://github.com/dpayne/) for figuring out how to find the pulseaudio default sink name.
