#version 410

uniform sampler2D my_texture;

in VS_OUT {
	vec2 texcoord;
} fs_in;

out vec4 frag_color;

void main()
{
	frag_color = texture(my_texture, fs_in.texcoord);
}