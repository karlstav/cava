# cava on windows

so it has come to this, but why not.

## building
To make it build install sdl2, fftw (not libfftw), pthreads and glew via the nuget thing.

All paths are configure relativly to the project so linking and inlcuding should work autmatically with the nugets, but might stop workiong at some point.

The Visual C++ Redistributable for Visual Studio 2012 (maybe also a newer one) was needed when I first ran it. Not sure why we need that old one.
