#version 330 core

in vec3 vert_colour;
in vec2 vert_tex_coords;

out vec4 frag_colour;

uniform sampler2D u_texture;

void main()
{
    frag_colour = texture(u_texture, vert_tex_coords);
}
