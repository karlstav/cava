cavacore
=========

Core processing engine of cava.

# How it works

cava core works as a wrapper around [fftw](http://www.fftw.org/). While fftw does the job with the discrete Fourier transform
which transforms the audio from the time domain (series of samples) to the frequency domain (series of frequencies),
the raw output data from fftw will not look very nice when used directly in a visualization. The cavacore engine does several things
to improve the look of the visualization:


## adjustable bandwith

Theoretically the human ear can hear frequencies up to 20kHz, but the information above 10k is hard to separate from eachother
and does not give much feedback by itself when visualizing audio. Similarly audio down to 20Hz can be heard, but the frequencies
below 50Hz does not directly provide anything to the visualizing of the audio.


## spread the output logarithmically

The human ear hears different frequencies logarithmically, so notes that are 2x and 4x higher than eachother, will be preceived
as beeing a fixed amount higher than eachother. Therefore cavacore outputs the frequencies logarithmically. Cavacore will also
group the output in the desired number of samples.


## noise reduction

the raw output of fftw is very noisy, cavacore processes the output signal in two ways to provide a smooth output:

  * an integral filter calculates a weighted average of the last values
  * fall off filter, when values is lower than last value it uses a  fall down effect instead of the new value

This feature can be adjusted.


## real-time sensitivity adjustment

The range of an input signal can vary a lot. cavacore can keep the output signal within range in real-time. This feature can be disabled.


# Building

use the root CMakeLists.txt to build it:

```
mkdir build
cd build
cmake ..
cmake --build .
```

# Usage

See cavacore.h for documentation and the cavacore_test.c application for how to use.
