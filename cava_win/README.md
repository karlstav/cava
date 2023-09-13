# cava on windows

All dependencies should install themselves via the NuGet thing:
* SDL2
* FFTW
* pthreads
* glew

include paths are configured relatively to the project so linking and including should work automatically with the nugets, but might stop working at some point.

The Visual C++ Redistributable for Visual Studio 2012 (maybe also a newer one) was needed when I first ran it.
