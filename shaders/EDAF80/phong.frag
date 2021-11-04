#version 410

uniform samplerCube cube_map;
uniform sampler2D sphere_texture;
uniform sampler2D specular_map;
uniform sampler2D normal_map;
uniform bool use_normal_mapping;
uniform vec3 light_position;
uniform vec3 camera_position;
uniform vec3 ambient;
uniform vec3 diffuse;
uniform vec3 specular;
uniform float shininess;

in VS_OUT {
    vec3 vertex;
	vec3 normal;
    vec2 texcoords;
	vec3 tangent;
	vec3 binormal;
} fs_in;

out vec4 frag_color;

void main()
{
    vec4 texture_rgba = texture(sphere_texture, fs_in.texcoords);
    vec3 texture_rgb = vec3(texture_rgba.x, texture_rgba.y, texture_rgba.z);
    vec4 specular_rgba = texture(specular_map, fs_in.texcoords);
    vec3 specular_rgb = vec3(specular_rgba.x, specular_rgba.y, specular_rgba.z);
    vec4 normal_rgba = texture(normal_map, fs_in.texcoords);
    vec3 normal_rgb = vec3(normal_rgba.x, normal_rgba.y, normal_rgba.z);
    
    vec3 ambient_color = ambient;

    mat3 tbn = mat3(fs_in.tangent, fs_in.binormal, fs_in.normal);
    normal_rgb = normal_rgb*2 - vec3(1,1,1);

    vec3 new_normal = fs_in.normal;
    if(use_normal_mapping){
        new_normal = tbn * normal_rgb; 
    }

    vec3 L = normalize(light_position - fs_in.vertex);
    vec3 diffuse_color = texture_rgb * 
    clamp(
        dot(normalize(new_normal), L), 
    0.0, 1.0);
    
    vec3 specular_color = specular_rgb * 
    pow(
        clamp(
            dot(
                reflect(-L, normalize(new_normal)), 
            normalize(camera_position - fs_in.vertex)), 
        0.0, 1.0),
    shininess); 

    vec3 color = ambient_color + diffuse_color + specular_color;
    frag_color = vec4(color.x, color.y, color.z, 1.0);
}