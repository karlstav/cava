#version 300 es
precision highp float;

in vec2 fragCoord;
out vec4 fragColor;

// bar values. defaults to left channels first (low to high), then right (high to low).
uniform vec4 bars[64];
uniform int bars_count;    // number of bars (left + right) (configurable)
uniform int x_on;    // frequency labels on off
uniform int x_count;    // number of digits in frequency label
uniform int x_labels[128]; // digits
uniform vec3 u_resolution; // window resolution

uniform int gradient_count;
uniform vec3 colors[9]; // gradient colors




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

    int number_count = 35;
    int numbers[35];
    int n = 0;
    for(int i=0;i<number_count;i++)
    {
        if (n > 9)
            n = 0;
        numbers[i] = n;
        n++;
    }
    numbers[5] = -1;
    numbers[6] = -1;

    float quadsizeX = 1.0 / float(x_count);
    float quadsizeY = quadsizeX * (u_resolution.x / u_resolution.y);
    float offset = 0.05;

    if (fragCoord.y > offset && fragCoord.y < offset + quadsizeY && x_on == 1)
    {
        int idx = int(float(x_count) * fragCoord.x);
        int number = x_labels[idx];
        if (drawNumber(number, (fragCoord.x - (float(idx) * quadsizeX)) / quadsizeX, (fragCoord.y - offset) / quadsizeY) == 1 && number >= 0)
            fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        else
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }

}
