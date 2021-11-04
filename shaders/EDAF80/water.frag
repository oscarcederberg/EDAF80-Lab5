#version 410

uniform vec3 camera_position;
uniform samplerCube skybox_texture;
uniform sampler2D normal_map;

in VS_OUT {
    vec3 vertex;
    vec3 normal;
	vec2 normal_coord0;
	vec2 normal_coord1;
	vec2 normal_coord2;
    vec2 texcoords;
    mat3 tbn;
} fs_in;

out vec4 frag_color;

void main()
{
    bool use_normal_mapping = true;

    vec3 V = normalize(camera_position - fs_in.vertex);
    vec3 normal_rgba0 = texture(normal_map, fs_in.normal_coord0).xyz*2 - 1;
    vec3 normal_rgba1 = texture(normal_map, fs_in.normal_coord1).xyz*2 - 1;
    vec3 normal_rgba2 = texture(normal_map, fs_in.normal_coord2).xyz*2 - 1;
    vec3 normal_bump = normalize(normal_rgba0 + normal_rgba1 + normal_rgba2);

    vec3 new_normal = fs_in.normal;
    if(use_normal_mapping){
        new_normal = fs_in.tbn * normal_bump; 
    }
    new_normal = normalize(new_normal);

    vec4 color_deep = vec4(0.0, 0.0, 0.1, 1.0);
    vec4 color_shallow = vec4(0.0, 0.1, 0.2, 1.0);
    float facing = 1 - max(dot(V, new_normal), 0); 
    vec4 water_color = mix(color_deep, color_shallow, facing);

    vec3 refl = reflect(-V, new_normal);
    vec4 reflection = texture(skybox_texture, refl);

    float refr_index = 1.0/1.33;
    vec3 refr = refract(-V, new_normal, refr_index);
    vec4 refraction = texture(skybox_texture, refr);

    float R_0 = 0.02037;
    float fastFresnel = R_0 + (1 - R_0) * pow(1 - dot(V, new_normal), 5);

    frag_color = water_color + reflection * fastFresnel + refraction * (1 - fastFresnel);
    frag_color = vec4(frag_color.xyz, 0.3);
}