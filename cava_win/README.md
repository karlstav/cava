so it has come to this, but why not


1. install sdl2, fftw, pthreads and optinally glew via the nuget thing. Or just hack them in anyway you like. 
it's windows, so by the time you read this it's probably a new and even worse way to do it.

I had to add all types of link paths manually for the symbols to become defined.

cava src dir had to be added as an inlcude path

libpthread.dll had to be bopied from the nuget package dir to where cava.exe was

install Visual C++ Redistributable for Visual Studio 2012 

for sdl_glsl output install glew add SDL_GLSL definition and pray to deity of choice

