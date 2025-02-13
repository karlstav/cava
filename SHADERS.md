custom shaders
==============

Write your own shaders for cava!

cava can use SDL/OpenGL to render custom glsl shaders.

The shader files are loaded from the folder `$HOME/.config/cava/shaders`

under [output] set `method` to 'sdl_glsl'

use the config options `vertex_shader` and `fragment_shader` to select file.

look in the `bar_spectrum.frag` shader for how the shaders interact with cava.

the custom shaders will use some of the same config parameters as the other output modes, like number of bars.

feel free to commit your own shaders (or improvements to the sdl_glsl output mode) and create pull request.

To add a shader to the cava repo do the following:

1. Copy or create your shader in the `$HOME/.config/cava/shaders` folder and load it via the config file.
2. When done writing the shader copy it to the `output/shaders` folder in the cava source code
3. In `config.c`:
    1. Increment `NUMBER_OF_SHADERS` define
    2. Add it to the `INCTXT`, `IDR` lists as well as the `default_shader_data` and the `default_shader_name` arrays at the top of the `config.c`. The orders of the arrays is important! There are two `default_shader_data` arrays one for windows and one for linux.
4. Add it to `cava_win/cava/cava.rc`


The shader will then be compiled into the bionary and written to the `$HOME/.config/cava/shaders` folder on runtime (if it is not already there).


