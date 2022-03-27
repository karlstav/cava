cavacore
=========

Core processing engine of cava.

# How it works

cava core works as a wrapper around [fftw](http://www.fftw.org/). While fftw does the job with the discrete Fourier transform
which transforms the audio from the time domain (series of samples) to the frequency domain (series of frequencies),
the raw output data from fftw will not look very nice when used directly in a visualization. The cavacore engine does several things
to improve the look of the visualization:

## limit bandwith
theoretically the human ear can hear frequencies up to 20kHz, but the information above 10k is hard to separate from eachother
and does not give much feedback when visualizing audio. Similaryl audio below 50Hz can be heard but does not directly provide 
anything to the visualizing of the audio.

## spread the output logarithmically
the human ear hears different frequencies logarithmically, so notes that are 2x and 4x higher than eachother, will be preceived
as beeing a fixed amount higher than eachother.

## limit number of buckets
fftw gives out (input samples / 2) + 1 number of ouput samples. cavacore can limit this to the desired needed "bars" in your application.

## smoothing filters
the raw output of fftw is very jittery, cavacore processes the output signal in two ways to provide a smooth output:

- an integral filter calculates a weighted average of the last values

- fall off filter, when values is lower than last value it uses a  fall down effect instead of the new value

## real-time sensitivity adjustment
the range of an input signal can vary a lot. cavacore will keep the output signal within the desired range in real-time.


# Usage
cavacore exposes three functions:

```
extern struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels,
                                   int max_height, int framerate);
extern void cava_execute(double *cava_in, int new_samples, double *cava_out, struct cava_plan *p);
extern void cava_destroy(struct cava_plan *p);
```

`cava_init` sets everything up, takes the following parameters:

`int number_of_bars` the desired number of output values per channel.
`unsigned int rate` the samples rate of the input signal
`int channels` number of interleaved channels in the input signal (2 for stereo signal, 1 for mono)
`double range` the target max value of the output signal for the auto sensitivity
`int framerate` the approximate number of times per second cava_execute will be ran. (used for smoothing calculations)
`int auto_sensitivity` 1 = on, 0 = off.

returns `struct cava_plan` which contains the cut off frquencies among other things and is used when calling `cava_execute`.

`cava_execute` processess the input signal in `cava_in` up until `int new_samples`, output will be in `double *cava_out`.

`cava_destroy` frees up all dynamically allocated memory.

look in cavacore.h and the cavacore_test.c application for more details.