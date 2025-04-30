#version 330 core

in vec3 vert_colour;
in vec2 vert_tex_coords;

out vec4 frag_colour;

uniform sampler2D u_texture0;
uniform sampler2D u_texture1;

void main()
{
    frag_colour = mix(texture(u_texture0, vert_tex_coords),
                      texture(u_texture1, vert_tex_coords), 0.2);
}
