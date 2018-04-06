C.A.V.A. (M.P.) - G.
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA (**M**PD and **P**ulseaudio) - **G**raphical fork

by [Karl Stavestrand](mailto:karl@stavestrand.no) and this fork by [Nikola Pavlica](mailto:pavlica.nikola@gmail.com)

Changes in 0.6:
* Autosens of low values (dynamic range)
* Removed config parameter 'overshoot'
* Removed config parameter 'mode'
* New config parameter 'waves'
* Changed config parameter 'style' to 'channels' (was either 'mono' or 'stereo' anyway)
* Parameters 'integral' and 'gravity' are now in percentage


![spectrum](https://raw.githubusercontent.com/karlstav/cava/gh-pages/cava_gradient.gif "spectrum")

thanks to [anko](https://github.com/anko) for the gif, here is the [recipe]( http://unix.stackexchange.com/questions/113695/gif-screencastng-the-unix-way).

[Demo video](https://youtu.be/9PSp8VA6yjU)

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [What it is](#what-it-is)
- [Build requirements](#build-requirements)
- [Getting started](#getting-started)
  - [Installing manually](#installing-manually)
  - [Creating a desktop shortcut](#creating-a-desktop-shortcut)
  - [Uninstalling](#uninstalling)
  - [openSUSE](#opensuse)
  - [Fedora](#fedora)
  - [Arch](#arch)
  - [Ubuntu](#ubuntu)
  - [Windows](#windows)
- [Capturing audio](#capturing-audio)
  - [From PortAudio (easy)](#from-portaudio-easy)
  - [From Pulseaudio monitor source (Easy, default if supported)](#from-pulseaudio-monitor-source-easy-default-if-supported)
  - [From ALSA-loopback device (Tricky)](#from-alsa-loopback-device-tricky)
  - [From mpd's fifo output](#from-mpds-fifo-output)
- [Running via ssh](#running-via-ssh)
- [Font notes](#font-notes)
  - [In ttys](#in-ttys)
  - [In terminal emulators](#in-terminal-emulators)
- [Latency notes](#latency-notes)
- [Usage](#usage)
  - [Controls](#controls)
- [Configuration](#configuration)
- [Graphical modes](#graphical-modes)
  - [Output modes](#output-modes)
  - [OpenGL](#opengl)
  - [Window options](#window-options)
  - [Additional features](#additional-features)
  - [Shadow](#shadow)
- [Contribution](#contribution)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->


What it is
----------

C.A.V.A. is a bar spectrum audio visualizer for the Linux terminalusing ALSA, pulseaudio, portaudio or fifo buffer for input.

This program is not intended for scientific use. It's written to look responsive and aesthetic when used to visualize music. 


Build requirements
------------------
* [FFTW](http://www.fftw.org/)
* [ncursesw dev files](http://www.gnu.org/software/ncurses/) (bundled in ncurses in arch)
* [Xlib/X11 dev files](http://x.org/)
* [SDL2 dev files](https://libsdl.org/)
* [ALSA dev files](http://alsa-project.org/)
* [Pulseaudio dev files](http://freedesktop.org/software/pulseaudio/doxygen/)
* [Portaudio dev files](http://www.portaudio.com/)
* [GNU libtool](https://www.gnu.org/software/libtool/)

Only FFTW is actually required for CAVA to compile, but for maximum usage and performance ncurses and pulseaudio and/or alsa dev files are recommended. Not sure how to get the pulseaudio dev files for other distros than debian/ubuntu or if they are bundled in pulseaudio.

All the requirements can be installed easily in all major distros:

Debian/Raspbian:

    apt-get install libfftw3-dev libasound2-dev libncursesw5-dev libpulse-dev libtool libx11-dev libsdl2-dev libportaudio-dev 

ArchLinux:

    pacman -S base-devel fftw ncurses alsa-lib iniparser pulseaudio libx11 sdl2 portaudio

openSUSE:

    zypper install alsa-devel ncurses-devel fftw3-devel libX11-devel libSDL2-devel libtool portaudio-devel

Fedora:

    dnf install alsa-lib-devel ncurses-devel fftw3-devel xorg-x11-devel SDL2-devel pulseaudio-libs-devel libtool portaudio-devel

Cygwin dependencies (64bit ONLY):

    gcc-core w32api-headers libfftw3-devel libportaudio-devel libncurses-devel portaudio

Iniparser is also required, but if it is not already installed, a bundled version will be used.

To run the autogen script you will also need `automake`, `libtool` and `git`.


Getting started
---------------

    ./autogen.sh
    ./configure
    make

You can use the following for compilation options:

    --enable-debug          	enable debug messages and frequency table output
    --enable-legacy_iniparser	enable legacy iniparser mode necessary to build cava with iniparser < 4.0

For example, turning on debugging messages:

    ./configure --enable-debug 
    
### Installing manually

Install `cava` to default `/usr/local`:

    make install

Or you can change `PREFIX`, for example:

    ./configure --prefix=PREFIX

### Creating a desktop shortcut

In the example_files/desktop you will find an desktop entry for GNU/Linux systems and an icon file.

To create a desktop shortcut you can do the following:

	cp example_files/desktop/cava.desktop /usr/share/applications
	cp example_files/desktop/cava.png /usr/share/icons/hicolor/64x64/apps

### Uninstalling

    make uninstall

### openSUSE

Tumbleweed users have cava in their repo. They can just use:

    zypper in cava

Leap users need to add the multimedia:apps repository first:

    zypper ar -f obs://multimedia:apps/openSUSE_Leap_42.2 multimedia

If you use another version just replace *openSUSE_Leap_42.2* with *openSUSE_13.2*, adjust it to your version.

### Fedora

Cava is available in Fedora 26 and later.  You can install Cava by
running:

    dnf install cava

Unfourtunately for Fedora users the graphical branch is not availble and it must be compiled from source.

### Arch

CAVA (graphical branch) is availble in [AUR](https://aur.archlinux.org/packages/cava-gui-git/).

    pacaur -S cava-gui-git

### Ubuntu

Michael Nguyen has added CAVA to his PPA, it can be installed with:

    sudo add-apt-repository ppa:tehtotalpwnage/ppa
    sudo apt-get update
    sudo apt-get install cava
    
Unfourtunately for Ubuntu users, you will have to compile this branch since there are no active repositories. 

### VoidLinux

CAVA (graphical branch) is avaible in the Void repos:
	
	xbps-install cava-gui


### Windows

You must compile from source, and it is only available under Cygwin. No mingw or VSC support yet since it's deeply tied to POSIX.


All distro specific instalation sources might be out of date.


Capturing audio
---------------

### From portaudio (easy)

First make sure you have portaudio dev files and that cava was built with portaudio support (it should have done so automatically if it found the dev files).

Since portaudio combines the functionality of all audio systems, it should work "out of the box".

To enable just uncomment:
    
    method = portaudio

in the `[input]` section of your config.

And once CAVA is running, on pulseaudio you should change it's input to the monitor.

Or in case of Windows, you should set "Stereo Mix" as the default input device in the recording tab. In case you don't have it update/install your audio drivers or get a better sound card.

### From Pulseaudio monitor source (Easy, default if supported)

First make sure you have installed pulseaudio dev files and that cava has been built with pulseaudio support (it should be automatically if the dev files are found).

If you're lucky all you have to do is to uncomment this line in the config file under input:

    method = pulse
 
If nothing happens you might have to use a different source than the default. The default might also be your microphone. Look at the config file for help. 


### From ALSA-loopback device (Tricky)

Set

    method = alsa

in the config file.

ALSA can be difficult because there is no native way to grab audio from an output. If you want to capture audio straight fom the output (not just mic or line-in), you must create an ALSA loopback interface, then output the audio simultaneously to both the loopback and your normal interface.

To create a loopback interface simply run:

`sudo modprobe snd_aloop`

Hopefully your `aplay -l` should now contain a loopback interface.

To make it presistent across boot add the line `snd-aloop` to "/etc/modules". To keep it form beeing loaded as the first soundcard add the line `options snd-aloop index=1` to "/etc/modprobe.d/alsa-base.conf", this will load it at '1'. You can replace '1' with whatever makes most sense in your audio setup.

Playing the audio through your Loopback interface makes it possible for cava to to capture it, but there will be no sound in your speakers. In order to play audio on the loopback interface and your actual interface you must make use of the ALSA multi channel.

Look at the included example file `example_files/etc/asound.conf` on how to use the multi channel. I was able to make this work on my laptop (an Asus UX31 running Ubuntu), but I had no luck with the ALSA method on my Rasberry PI (Rasbian) with an USB DAC. The PulseAudio method however works perfectly on my PI. 

Read more about the ALSA method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa).

If you are having problems with the alsa method on Rasberry PI, try enabling `mmap` by adding the following line to `/boot/config.txt` and reboot:

```
dtoverlay=i2s-mmap
```

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

If you are running in terminal or raw modes the graphics are based on characters and thus the performance is dependent on the terminal font. Or you are running in graphical modes from which the graphics are pixel based and the performance depends on the window size you are running at.

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

But if you are running in either in the graphical modes, you can shrink the window, enable GPU acceleration or enable OpenGL (if supported).

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

| Key | Description |
| --- | ----------- |
| <kbd>up</kbd> / <kbd>down</kbd>| increase/decrease sensitivity |
| <kbd>left</kbd> / <kbd>right</kbd>| increase/decrease bar width |
| <kbd>a</kbd> / <kbd>s</kbd> | increase/decrease bar spacing |
| <kbd>f</kbd> | toggle fullscreen (only in window modes, besides win32) |
| <kbd>c</kbd> / <kbd>b</kbd>| change forground/background color |
| <kbd>r</kbd> | Reload configuration |
| <kbd>q</kbd> or <kbd>CTRL-C</kbd>| Quit C.A.V.A. |

Configuration
-------------

As of version 0.4.0 all options are done in the config file, no more command-line arguments!

By default a configuration file is located in `$XDG_CONFIG_HOME/cava/config` or `$HOME/.config/cava/config`, but cava can also be made to use a different file with the `-p` option. Look in the default config file for details on the different options.

Sending cava a SIGUSR1 signal, will force cava to reload its configuration file. Thus, it behaves as if the user pressed <kbd>r</kbd> in the terminal. One might send a SIGUSR1 signal using `pkill` or `killall`.
For example:
```
$ pkill -USR1 cava
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


Graphical modes
---------------

CAVA (this branch) can now run in grapical modes.

To achieve this you have to change `method` in the `[output]` category to one of the supported graphical modes.

### Output modes

CAVA currently supports these graphical modes:

	x - standard XFree86 window

	sdl - SDL2 window

	win - win32 window


### OpenGL

To run CAVA in OpenGL:
      
	opengl = true

WARNING: OpenGL isn't supported under SDL2.

### Window options

Toggle fullscreen:
     
	fullscreen = 1 or 0

WARNING: On Windows it isn't supported.


Toggle window border:
    
	border = 1 or 0

WARNING: On Windows the border is always off.


Change bar width/height (units are in pixels rather than characters):
    
	bar_width = (width in pixels)
    
	bar_spacing = (width in pixels)

NOTE: It's located in the ```window``` category.


Move the window on startup to a specific part of the screen:

	alignment = 'top_left', 'top', 'top_right'. 'left', 'center', 'right', 'bottom_left', 'bottom', 'bottom_right' and by default 'none'


In addition to window aligment you can adjust the window position further with:
    
	x_padding = (specify value)
    
	y_padding = (specify value)


You can enable transparent windows:
     
	transparency = 1 or 0

WARNING: SDL2 doesn't have transparency.


Force the window to be behind any window (works only under Xlib):
    
	keep_below = 1

Set window properties (Window Class):

	set_win_props = 1

This helps with removing blur and shadows from behind the window, but also removes the ability to interact with the window.

### Additional features

Setting foreground color to `default` will cause in Xlib to average out the color in the desktop. 

On Windows it will grab the accent color (aka. your theme) instead.

To enable this you just have to change:
    
    foreground = 'default'
    
Set foreground opacity:

    foreground_opacity = (from 0.0 to 1.0)

You need OpenGL and transparency support in order for it to work.

### Shadow

You can change the following options:
    
    size = (in pixels)

and
    
    color = '#aarrggbb'

You need to enable transparency for the shadows to work.


Contribution
------------

Please read CONTRIBUTING.md before opening a pull request.

Thanks to:
* [CelestialWalrus](https://github.com/CelestialWalrus)
* [anko](https://github.com/anko)
* [livibetter](https://github.com/livibetter)

for major contributions in the early development of this project.

Also thanks to [dpayne](https://github.com/dpayne/) for figuring out how to find the pulseaudio default sink name.
