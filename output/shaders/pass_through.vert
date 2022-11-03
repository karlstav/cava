#version 120

in vec4 fragColor;
varying out vec2 fragCoord;

void main()
{
	gl_Position = gl_Vertex;
	fragCoord = gl_Position.xy;
}