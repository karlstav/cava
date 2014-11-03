C.A.V.A.
=========
Console-based Audio Visualizer for Alsa

![spectrum](http://stavestrand.no/cava6.gif "spectrum")

http://youtu.be/vA4RyatP064

by [Karl Stavestrand](mailto:karl@stavestrand.no )


Updates
----------
9/22/2014 - updated support for fifo (mpd).

What it is
----------
C.A.V.A is a bar spectrum analyzer for audio using Alsa for input. The frequency range is limited to 50-12000Hz. I know that the human ear can hear from 20 up to 20,000 Hz (and probably "sense" even higher frequencies). But the frequencies between 50 - 12000Hz seems to me to be the sounds that are most distinguishable. (I do believe telephones used to be limited to as low as 8kHz.)

This is my first actual "published" work and I am not a professional programmer so the source code is probably, by all conventions, a complete mess.

This program is not intendent to be used for scientific purposes.

Please excuse all the typos as I am both dyslectic and foreign. 

Any tips/comments will be received with much appreciation.


Build Requirements
------------------
* alsa dev files: (http://alsa-project.org/)
* FFTW: (http://www.fftw.org/)

Debian/Raspbian users can get this with:
apt-get install libfftw3-dev libasound2-dev



How to get started
-------------

```
make
```




Capturing audio straight from output
-------------

If you want to capture audio straight fom the output (not just mic or line-in), you must first create an alsa loopback interface and then output the audio simultaneously to both the loopback and your normal interface.

Here is how to create a loopback interface:

- Copy the file "alsa-aloop.conf" to your  "/etc/modprobe.d/" directory. You might have to change the index=1 to match your sound setup. Look at "aplay -l" to se what index is available.
- Add the line "snd-aloop" to /etc/modules
- Run "sudo modprobe snd_aloop"

Hopefully your "aplay -l" should now contain a couple of "Loopback" interfaces. 

Now playing the audio through your Loopback interface will make it possible to capture by it cava, but there will be no sound in your speakers :(

Not to worry! There are (at least) two ways of sending the audio output to the loopback and your actual audio interface at the same time:

- pulseaudio (easy):

In the file "/etc/pulse/default.pa" add the line  "load-module module-combine-sink" (in pulseaudio versions < 2.0 the module is only called "module-combine"). Then restart pulseaudio. For some reason i had to turn of realtime scheduling in order for this to work on raspberry pi, set "realtime-scheduling = no" in /etc/pulse/daemon.conf.


Pulseaudio setup can also be done in paprefs (debain: sudo apt-get install paprefs && paprefs&), the far right tab "Simultaneous Output" and check the box.

After that there should be an extra Output in your sound options called "Simultaneous output to..." Note that when using this method if you turn down the volume on the Simultaneous output, this will effect the visualizer. So to avoid this you have to select the actual output then turn down the volume then select the Simultaneous output again.

- alsa (hard):

Look at the inculded example file: /etc/asound.cnf. I was able to make this work on my laptop an Asus ux31 running elemetary os. But had no luck with the alsa method on my Rasberry PI with an USB DAC runnig rasbian. The pulseaudio method however works perfectly on my PI.

Read more about the alsa method [here](http://stackoverflow.com/questions/12984089/capture-playback-on-play-only-sound-card-with-alsa)


Cava defaults to "hw:1,1" if your loopback interface is not on that index, or you want to capture audio from somewhere else, simply cahnge this with the -d option.


Capturing audio from fifo output (mpd)
-------------
add this lines in mpd:

```
audio_output {
    type                    "fifo"
    name                    "my_fifo"
    path                    "/tmp/mpd.fifo"
    format                  "44100:16:2"
}
```

run cava with:
```
 ~ $ ./cava -i fifo
```

The path of the fifo can be specified with -p.

I had some trouble with sync (the visualizer was before the sound) reducing the alsa buffer in the mpd fixed it:

```
audio_output {
        type            "alsa"
        name            "My ALSA"
        buffer_time     "50000" #  (50ms)  default is 500000 microseconds (0.5 seconds)
}
```


Running via ssh:
--------------------
To run via ssh to external monitor: 
```
 ~# ./cava [options] > /dev/console
```
(must be root to redirect to console, simple sudo is not enouh, run "sudo su" first)



A note on fonts:
--------------------
Since the graphics are simply based on characters the preformance is dependent on the font in the terminal.

- In ttys:

If you run this in the conosle (tty1-7) the program will change the font to the included "cava.psf" (actually "unifont") this is because that font looks good with this program and beause i had to change the font slightly.

In console fonts it seems that only 256 unicode charachters are suported, probably since theese are bitmap fonts. I could not find a font that had unicode charachters 2581-2587, these are the 1/8 - 7/8 blocks used on the top of each bar to increase resolution.  

So in cava.psf the charachters 1-7 is actually replaced by unicode charachters 2581-2587. Not to worry, when cava is excited the font is suposed to change. However if you notice that 1-7 is replaced by partial blocks just change the font with "setfont".

Another note on fonts, "setfont" is suposed to return the defualt font, but this usally isn't set and I haven't found another way to get the current font. So what cava does is setting the font to "Lat2-Fixed16" on exit (ctrl+c), this should be included in all mayor distros. I think it reverts to your default at reboot.
 
- In terminal emulators:

In terminal emulator like xTerm, the font is chosen in the software and cannot be changed by an apliaction. So find your terminal settings and try out diferent fonts, some look pretty crap with the block charachters :( . "FreeMono Bold" and "WenQuanYi Micro Hei Mono" is what I have found to look pretty good, But it still looks much better in the tty. I found out that it looks pretty good in the standard gnome terminal.

!!warning!! cava also turns of the cursor, it's suposed to turn it back on on exit (ctrl+c), but in case it terminates unexpectedly, run: "setterm -cursor on" to get the cursor back.



A note on latency:
--------------------
If you experince an issue with latency (sound before image) in terminal emulator, try to increase the font size. This will reduce the number of charachters that has to be printed out.

If there is a huge buffer in your audio device, you might experience that cava is actually faster then the audio you heare. This will reduce the experience of the visualization. To fix this you can try to increase the buffer settings in the audio playing software.

Usage
--------------------
```
Usage : ./cava [options]

Options:
	-b 1..(console columns/2-1) or 200	 number of bars in the spectrum (default 25 + fills up the console), program wil auto adjust to maxsize if input is to high)

	-i 'input method'			 method used for listnening to audio, supports 'alsa' and 'fifo'

	-d 'alsa device'			 name of alsa capture device (default 'hw:1,1')

	-p 'fifo path'				 path to fifo (default '/tmp/mpd.fifo')

	-c color				 suported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)

	-C backround color			 supported colors: same as above (default: no change) 

	-s sensitivity %			 sensitivity in percent, 0 means no respons 100 is normal 50 half 200 double and so forth

	-f framerate 				 max frames per second to be drawn, if you are experiencing high CPU usage, try redcing this (default: 60)
```
exit with ctrl+c.

If cava quits unexpectedly or is force killed echo must be turned on manualy with "stty -echo".
