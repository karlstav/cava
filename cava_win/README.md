# cava on windows

All dependencies should install themselves via the nuget thing:
* SDL2
* FFTW
* pthreads
* glew

include paths are configure relativly to the project so linking and inlcuding should work autmatically with the nugets, but might stop workiong at some point.

The Visual C++ Redistributable for Visual Studio 2012 (maybe also a newer one) was needed when I first ran it.
