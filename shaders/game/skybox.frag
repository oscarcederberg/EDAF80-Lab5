#version 410

layout (location = 0) in vec3 vertex;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 texcoords;

uniform samplerCube cube_map;

in VS_OUT {
    vec3 vertex;
	vec3 normal;
    vec3 texcoords;
} fs_in;

out vec4 frag_color;

void main()
{
    frag_color = texture(cube_map, fs_in.vertex);   
}