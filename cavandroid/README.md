# CAVA on android

## FFTW3

To build cava on android you need to get the fftw3 library.
Luckily someone has made a repository for compiling FFTW3 on Android.
Just make sure ndk-build is in your path before running the build script.


```
git clone https://github.com/Lauszus/fftw3-android
cd fftw3-android
./build.sh

```

By default we will look for the fftw3 lib in a folder called fftw3-android besides the cava dir:

```
./
../
cava/
fftw3-android/
```

Edit the cmake argument in the app gradle file for setting it to some other place.
