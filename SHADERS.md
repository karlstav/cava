custom shaders
==============

Write your own shaders for cava!

cava can use SDL/OpenGL to render custom glsl shaders.

The shader files must be placed in $HOME/.config/cava/shaders

under [output] set `method` to 'sdl_glsl'

use the config options `vertex_shader` and `fragment_shader` to select file.

look in the `bar_spectrum.frag` shader for how the shaders interact with cava.

the custom shaders will use some of the same config parameters as the other output modes, like number of bars.

feel free to commit your own shaders (or improvements to the sdl_glsl output mode) and create pull request.

To add a shader to the cava repo put it under output/shaders here, and add it to the INCTXT at the top of the config.c file.


