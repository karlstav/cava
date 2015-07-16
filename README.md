C.A.V.A. 
====================

**C**onsole-based **A**udio **V**isualizer for **A**LSA

by [Karl Stavestrand](mailto:karl@stavestrand.no)

![spectrum](https://cloud.githubusercontent.com/assets/5231746/7368413/7d7ca93e-ed9f-11e4-842f-61c54fc03dbe.gif "spectrum")

thanks to [anko](https://github.com/anko) for the gif, here is the [recipe]( http://unix.stackexchange.com/questions/113695/gif-screencastng-the-unix-way).

[Demo video](http://youtu.be/vA4RyatP064) (old)



Updates
-------

7/15/2015 - 0.3.1 - added config file

7/12/2015 - 0.3.0 - Modular source code

5/23/2015 - 0.2.0 - Switched to ncurses

4/23/2015 - Fixed terminal window resizing, added smoothing 

4/19/2015 - Added Monstercat style FFT easing (by [CelestialWalrus)](https://github.com/CelestialWalrus).

9/22/2014 - Added support for mpd FIFO input.

What it is
----------

C.A.V.A. is a bar spectrum analyzer for audio using ALSA for input. Its frequency range is limited to 50-10,000Hz. I know that the human ear can hear from 20 up to 20,000 Hz (and probably "sense" even higher frequencies), but the frequencies between 50-12,000Hz seem to me to be the most distinguishable. (I believe telephones used to be limited to as low as 8kHz.)

This program is not intended for scientific use.

This is my first published code. I am not a professional programmer so the source code is probably, by all conventions, a complete mess. Please excuse all the typos as I am both dyslexic and foreign.

Any tips or comments would be much appreciated.


Build requirements
------------------

* [ALSA dev files](http://alsa-project.org/)
* [FFTW](http://www.fftw.org/)
* [ncursesw dev files](http://www.gnu.org/software/ncurses/) (bundled in ncurses in arch)

This can be installed easily in all major distros:

Debian/Raspbian:

    apt-get install libfftw3-dev libasound2-dev libncursesw5-dev

ArchLinux:

    pacman -S base-devel fftw ncurses

openSUSE:

    zypper install alsa-devel ncurses-devel fftw3-devel


Getting started
---------------

    make

You can use the following for compilation options, value in *italic style* is the default value:

| Name | Value | Description |
| ---- | ----- | ----------- |
| `debug` | *0* or 1 | Debugging message switch |

For example, turning on debugging messages:

    make debug=1

### Installing

Install `cava` to default `/usr/local`:

    make install

Or you can change `PREFIX`, for example:

    make PREFIX=$HOME/.local install

### Uninstalling

    make uninstall

Or:

    make PREFIX=$HOME/.local uninstall


Capturing audio
---------------

### Straight from output

If you want to capture audio straight fom the output (not just mic or line-in), you must create an ALSA loopback interface, then output the audio simultaneously to both the loopback and your normal interface.

To create a loopback interface simply run:

`sudo modprobe snd_aloop`

Hopefully your `aplay -l` should now contain a loopback interface.

To make it presistent across boot add the line `snd-aloop` to "/etc/modules". To keep it form beeing loaded as the first soundcard add the line `options snd-aloop index=1` to "/etc/modprobe.d/alsa-base.conf", this will load is at '1' which is what cave uses as default. 

Playing the audio through your Loopback interface makes it possible for cava to to capture it, but there will be no sound in your speakers. :(

Not to worry! There are (at least) two ways of sending the audio output to the loopback *and* your actual audio interface at the same time:

#### PulseAudio (easy)

To `/etc/pulse/default.pa`, add the line `load-module module-combine-sink` (in PulseAudio versions <1.0, the module was only called `module-combine`). Then restart PulseAudio. For some reason, I had to turn off realtime scheduling for this to work on a Raspberry Pi (set `realtime-scheduling = no` in `/etc/pulse/daemon.conf`).

PulseAudio setup can also be done in paprefs (Debian: `sudo apt-get install paprefs && paprefs`): In the far right tab check the box "Simultaneous Output".

An extra Output should appear in your sound options called "Simultaneous output to..." Note that when using this method if you turn down the volume on the Simultaneous output, this will effect the visualizer. To avoid this, select the actual output, then turn down the volume, then select the Simultaneous output again.


#### ALSA (hard)

Look at the inculded example file `example_files/etc/asound.conf`. I was able to make this work on my laptop an Asus UX31 running Elementary OS. I had no luck with the ALSA method on my Rasberry PI with an USB DAC runnig Rasbian. The PulseAudio method however works perfectly on my PI.

Read more about the ALSA method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa).

Cava defaults to using the ALSA device `hw:1,1`. If your loopback interface is not on that index, or you want to capture audio from somewhere else, simply pass the `-d` flag with the target.


### From mpd's fifo output

Add these lines in mpd:

    audio_output {
        type                    "fifo"
        name                    "my_fifo"
        path                    "/tmp/mpd.fifo"
        format                  "44100:16:2"
    }

Run cava with `./cava -i fifo`.

The path of the fifo can be specified with `-p`.

I had some trouble with sync (the visualizer was ahead of the sound). Reducing the ALSA buffer in mpd fixed it:

    audio_output {
            type            "alsa"
            name            "My ALSA"
            buffer_time     "50000"   # (50ms); default is 500000 microseconds (0.5s)
    }


Running via ssh
---------------

To run via ssh to an external monitor, redirect output to `/dev/console`:

     ~# ./cava [options] > /dev/console

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

Cava also disables the terminal cursor, and turns it back on on exit, but in case it terminates unexpectedly, run `setterm -cursor on` to get it back.e

Tip: Cava will look much nicer in small font sizes. Use a second terminal emulator for cava and set the font size to 1. Warning, can cause high CPU usage and latency if the terminal window is too large!


Latency notes
-------------

If you see latency issues (sound before image) in a terminal emulator, try increasing the font size. This will reduce the number of characters that have to be shown.

If your audio device has a huge buffer, you might experience that cava is actually faster then the audio you hear. This reduces the experience of the visualization. To fix this, you try decreasing the buffer settings in your audio playing software.

Usage
-----

    Usage : cava [options]
    Visualize audio input in terminal. 

    Options:
        -b 1..(console columns/2-1) or 200  number of bars in the spectrum (default 25 + fills up the console), program will automatically adjust if there are too many frequency bands)
        -i 'input method'           method used for listnening to audio, supports: 'alsa' and 'fifo'
        -o 'output method'          method used for outputting processed data, only supports 'terminal'
        -d 'alsa device'            name of alsa capture device (default 'hw:1,1')
        -p 'fifo path'              path to fifo (default '/tmp/mpd.fifo')
        -c foreground color         suported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)
        -C background color         supported colors: same as above (default: no change)
        -s sensitivity              sensitivity percentage, 0% - no response, 50% - half, 100% - normal, etc...
        -f framerate                FPS limit, if you are experiencing high CPU usage, try redcing this (default: 60)
        -m mode                 set mode (normal, scientific, waves)
        -h                  print the usage
        -v                  print version

Exit with ctrl+c.

If cava quits unexpectedly or is force killed, echo must be turned on manually with `stty -echo`.

### Controls

| Key | Description |
| --- | ----------- |
| <kbd>m</kbd> | Swtich between smoothing modes |
| <kbd>up</kbd> / <kbd>down</kbd>| increase/decrease sensitivity |
| <kbd>r</kbd> | Reload configuration |
| <kbd>q</kbd> or <kbd>CTRL-C</kbd>| Quit C.A.V.A. |

Configuration
-------------

Configuration file is located in `$XDG_CONFIG_HOME/cava/config` or `$HOME/.config/cava/config`.

### Example file:

    [general]
    mode=normal
    framerate=60
    sensitivity=100
    bars=0
    
    [input]
    method=fifo
    source=/tmp/mpd.fifo
    
    [output]
    method=terminal
    
    [color]
    background=white
    foreground=blue
    
    [smoothing]
    integral=0.7
    monstercat=1
    gravity=1
    ignore=0
    
    [eq]
    ; naming of keys doesn't matter
    1=0.5
    2=0.6
    3=0.7
    4=0.3
    5=0.2

### Sections:

#### [general]

* `mode` defines smoothing mode, can be `normal`, `scientific` or `waves`. Default: `normal`.
* `framerate` is framerate (FPS). Default: `60`. Accepts only non-negative values.
* `sensitivity` is sensitivity %. Default: `100`. Accepts only non-negative values.
* `bars` defines the amount of bars. `0` sets it to auto. Default: `0`. Accepts only non-negative values.

#### [input]

* `method` may be `alsa` or `fifo`.
* `source` is the ALSA path or FIFO path.

#### [output]

* `method` may be `terminal` or `circle`. Default: `terminal`.

#### [color]

* `background` is the background color.
* `foreground` is the foreground (bars) color.

#### [smoothing]

* `integral` sets the multiplier for the integral smoothing calculations. Default: `0.7`. Another example: `0.5`. Accepts only non-negative values.
* `monstercat` disables or enables the so-called "Monstercat smoothing". Default: `1`. Accepts only `0` or `1`.
* `gravity` sets the gravity multiplier. Default: `1`. Accepts only non-negative values.
* `ignore` zeroes bars with height lower than this setting's value. Default: `0`. Accepts only non-negative values.

#### [eq]

This one is tricky. You can have as much keys as you want. More keys = more precision.

**How it works:**

1. Cava takes values from this section in the order of appearance (naming of the keys doesn't really matter) and puts them into an array.
2. Visualization is divided into `x` sections (where x is the amount of values) and all bars in each of these sections are multiplied by the corresponding value from that array.

**Examples:**

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
