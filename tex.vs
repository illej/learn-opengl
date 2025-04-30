#version 330 core

layout (location=0) in vec3 pos;
layout (location=1) in vec3 colour;
layout (location=2) in vec2 coords;

out vec3 vert_colour;
out vec2 vert_tex_coords;

uniform mat4 u_xfrm;

void main()
{
    gl_Position = u_xfrm * vec4(pos, 1.0);
    vert_colour = colour;
    vert_tex_coords = coords;
}
