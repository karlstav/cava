#version 300 es
precision highp float;

in vec2 fragCoord;
out vec4 fragColor;

// bar values. defaults to left channels first (low to high), then right (high to low).
uniform vec4 bars[64];
uniform int bars_count;    // number of bars (left + right) (configurable)
uniform vec3 u_resolution; // window resolution

uniform int gradient_count;
uniform vec3 colors[9]; // gradient colors

uniform int x_on;    // frequency labels on off
uniform int x_count;    // number of digits in frequency label
uniform int x_labels[128]; // digits

vec3 normalize_C(float y,vec3 col_1, vec3 col_2, float y_min, float y_max)
{
    //create color based on fraction of this color and next color
    float yr = (y - y_min) / (y_max - y_min);
    return col_1 * (1.0 - yr) + col_2 * yr;
}

int drawNumber(int number, float x, float y) {
    //padding
    if(x < 0.1 || y < 0.1 || x > 0.9 || y > 0.9)
    return 0;

    if (x< 0.25 && y < 0.5) {
        //lower left
        if (number == 0 || number == 8  || number == 2 || number == 6)
        return 1;
    }
    if (x< 0.25 && y > 0.5) {
        //upper left
        if (number == 0 || number == 8  || number == 9 || number == 5 || number == 6 || number == 4)
        return 1;
    }
    if (x> 0.75 && y < 0.5) {
        //lower right
        if (number == 0 || number == 1 || number == 3 || number == 4 || number == 5 || number == 6 || number == 7 || number == 8 || number == 9 )
        return 1;
    }
    if (x> 0.75 && y > 0.5) {
        //upper right
        if (number == 0 || number == 1  || number == 2 || number == 3 || number == 4 || number == 7 || number == 8 || number == 9)
        return 1;
    }
    if (y < 0.22) {
        //lower center
        if (number == 0 || number == 2 || number == 3 ||number == 5 ||number == 6 || number == 8 )
        return 1;
    }
    if (y > 0.44 && y < 0.56 ) {
        //center
        if (number == 2 || number == 3 || number == 4 || number == 5 ||number == 6 || number == 8 || number == 9 )
        return 1;
    }
    if (y > 0.78) {
        //upper center
        if (number == 0 || number == 2 || number == 3 || number == 5|| number == 6 || number == 7 || number == 8 || number == 9 )
        return 1;
    }
    return 0;
}

void main()
{
    // find which bar to use based on where we are on the x axis
    float x = u_resolution.x * fragCoord.x;
    int bar = int(float(bars_count) * fragCoord.x);
    int bar_4 = bar / 4;
    int bar_xyz = int(mod(float(bar), 4.0));

    //the y coordinate and bar values are the same
    float y;
    if (bar_xyz == 0)
        y =  bars[bar_4].x;
    else if (bar_xyz == 1)
        y =  bars[bar_4].y;
    else if (bar_xyz == 2)
        y =  bars[bar_4].z;
    else if (bar_xyz == 3)
        y =  bars[bar_4].w;

    //calculate a bar size
    float bar_size = u_resolution.x / float(bars_count);
    float bar_spacey = floor(bar_size * 0.1);

    // make sure there is a thin line at bottom
    if (y * u_resolution.y < 1.0)
    {
      y = 1.0 / u_resolution.y;
    }

    //draw the bar up to current height
    if (y > fragCoord.y)
    {
        //make some space between bars basen on settings
        if (bar_spacey >= 1.0 && x > float(bar + 1) * bar_size - bar_spacey)
        {
            fragColor = vec4(colors[0],1.0);
        }
        else
        {
            if (gradient_count == 0)
            {
                fragColor = vec4(colors[1],1.0);
            }
            else
            {
                //find which color in the configured gradient we are at
                int color = int(float((gradient_count - 1)) * fragCoord.y);

                //find where on y this and next color is supposed to be
                float y_min = float(color) / float((gradient_count - 1));
                float y_max = float((color + 1)) / float((gradient_count - 1));

                //make color
               fragColor = vec4(normalize_C(fragCoord.y, colors[color + 1], colors[color + 2], y_min, y_max), 1.0);

            }
        }
    }
    else
    {
        fragColor = vec4(colors[0],1.0);
    }
    if (x_on == 1)
    {
        float quadsizeX = 1.0 / float(x_count);
        float quadsizeY = quadsizeX * (u_resolution.x / u_resolution.y);
        float offset = 0.05;

        if (fragCoord.y > offset && fragCoord.y < offset + quadsizeY)
        {
            int idx = int(float(x_count) * fragCoord.x);
            int number = x_labels[idx];
            if (drawNumber(number, (fragCoord.x - (float(idx) * quadsizeX)) / quadsizeX, (fragCoord.y - offset) / quadsizeY) == 1 && number >= 0)
                fragColor = vec4(colors[1],1.0);
            else if (number >= 0)
                fragColor = vec4(colors[0],1.0);
        }
    }
}
