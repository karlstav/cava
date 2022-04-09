cavacore
=========

Core processing engine of cava.

# How it works

cava core works as a wrapper around [fftw](http://www.fftw.org/). While fftw does the job with the discrete Fourier transform
which transforms the audio from the time domain (series of samples) to the frequency domain (series of frequencies),
the raw output data from fftw will not look very nice when used directly in a visualization. The cavacore engine does several things
to improve the look of the visualization:


* limit bandwith

theoretically the human ear can hear frequencies up to 20kHz, but the information above 10k is hard to separate from eachother
and does not give much feedback by itself when visualizing audio. Similarly audio below 50Hz can be heard but does not directly provide 
anything to the visualizing of the audio.


* spread the output logarithmically

the human ear hears different frequencies logarithmically, so notes that are 2x and 4x higher than eachother, will be preceived
as beeing a fixed amount higher than eachother.


* limit number of buckets

fftw gives out (input samples / 2) + 1 number of ouput samples. cavacore can limit this to the desired needed "bars" in your application.


* noise reduction

the raw output of fftw is very jittery, cavacore processes the output signal in two ways to provide a smooth output:

a. an integral filter calculates a weighted average of the last values

b. fall off filter, when values is lower than last value it uses a  fall down effect instead of the new value


* real-time sensitivity adjustment

the range of an input signal can vary a lot. cavacore will keep the output signal within the desired range in real-time.


# Usage

look in cavacore.h for documentation and the cavacore_test.c application for how to use.