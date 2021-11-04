#version 410

layout (location = 0) in vec3 vertex;
layout (location = 2) in vec2 texcoords;

uniform mat4 vertex_model_to_world;
uniform mat4 normal_model_to_world;
uniform mat4 vertex_world_to_clip;
uniform float elapsed_time_s;

out VS_OUT {
	vec3 vertex;
	vec3 normal;
	vec2 normal_coord0;
	vec2 normal_coord1;
	vec2 normal_coord2;
    vec2 texcoords;
	mat3 tbn;
} vs_out;

float a_t(in float x, in float z, in float t, in float A, in vec3 D, in float f, in float p, in float k){
	return sin((D.x*x + D.z*z)*f + t*p)*0.5 + 0.5;
}

float G_dx(in float x, in float z, in float t, in float A, in vec3 D, in float f, in float p, in float k){
	return 0.5 * k * f * A * pow(a_t(x, z, t, A, D, f, p, k), k - 1) *
		cos((D.x*x + D.z*z)*f + t*p)*D.x;
}

float G_dz(in float x, in float z, in float t, in float A, in vec3 D, in float f, in float p, in float k){
	return 0.5 * k * f * A * pow(a_t(x, z, t, A, D, f, p, k), k - 1) *
		cos((D.x*x + D.z*z)*f + t*p)*D.z;
}

float G(in float x, in float z, in float t, in float A, in vec3 D, in float f, in float p, in float k){
	return A * pow(a_t(x, z, t, A, D, f, p, k), k);
}

float H_dx(in float x, in float z, in float t){
	float A_1 = 0.5;
	vec3 D_1 = vec3(-1.0, 0.0, 0.0);
	float f_1 = 0.2;
	float p_1 = 0.5;
	float k_1 = 2.0;
	float y_1 = G_dx(x, z, t, A_1, D_1, f_1, p_1, k_1);	
	
	float A_2 = 0.3;
	vec3 D_2 = vec3(-0.7, 0.0, 0.7);
	float f_2 = 0.4;
	float p_2 = 1.3;
	float k_2 = 2.0;
	float y_2 = G_dx(x, z, t, A_2, D_2, f_2, p_2, k_2);

	return y_1 + y_2;
}

float H_dz(in float x, in float z, in float t){
	float A_1 = 0.5;
	vec3 D_1 = vec3(-1.0, 0.0, 0.0);
	float f_1 = 0.2;
	float p_1 = 0.5;
	float k_1 = 2.0;
	float y_1 = G_dz(x, z, t, A_1, D_1, f_1, p_1, k_1);	
	
	float A_2 = 0.3;
	vec3 D_2 = vec3(-0.7, 0.0, 0.7);
	float f_2 = 0.4;
	float p_2 = 1.3;
	float k_2 = 2.0;
	float y_2 = G_dz(x, z, t, A_2, D_2, f_2, p_2, k_2);

	return y_1 + y_2;
}

float H(in float x, in float z, in float t){
	float A_1 = 0.5;
	vec3 D_1 = vec3(-1.0, 0.0, 0.0);
	float f_1 = 0.2;
	float p_1 = 0.5;
	float k_1 = 2.0;
	float y_1 = G(x, z, t, A_1, D_1, f_1, p_1, k_1);	
	
	float A_2 = 0.3;
	vec3 D_2 = vec3(-0.7, 0.0, 0.7);
	float f_2 = 0.4;
	float p_2 = 1.3;
	float k_2 = 2.0;
	float y_2 = G(x, z, t, A_2, D_2, f_2, p_2, k_2);

	return y_1 + y_2;
}

void main()
{
	vec3 new_vertex = vertex;	
	new_vertex.y += H(new_vertex.x, new_vertex.z, elapsed_time_s);
	
	vs_out.vertex = vec3(vertex_model_to_world * vec4(new_vertex, 1.0));
	vs_out.normal = vec3(-H_dx(new_vertex.x, new_vertex.z, elapsed_time_s), 
		1, 
		-H_dz(new_vertex.x, new_vertex.z, elapsed_time_s));
	vs_out.texcoords = texcoords;

    vec2 texture_scale = vec2(8,4);
    float normal_time = mod(elapsed_time_s, 100.0);
    vec2 normal_speed = vec2(-0.05,0);
    vec2 normal_coord0 = texcoords*texture_scale + normal_time*normal_speed;
    vec2 normal_coord1 = texcoords*texture_scale*2 + normal_time*normal_speed*4;
    vec2 normal_coord2 = texcoords*texture_scale*4 + normal_time*normal_speed*8;
	vs_out.normal_coord0 = normal_coord0;
	vs_out.normal_coord1 = normal_coord1;
	vs_out.normal_coord2 = normal_coord2;

	vs_out.tbn = mat3(vec3(1, H_dx(new_vertex.x, new_vertex.y, elapsed_time_s), 0),
		vec3(0, H_dz(new_vertex.x, new_vertex.y, elapsed_time_s), 1),
		vec3(-H_dx(new_vertex.x, new_vertex.y, elapsed_time_s), 1, -H_dz(new_vertex.x, new_vertex.y, elapsed_time_s)));

	gl_Position = vertex_world_to_clip * vertex_model_to_world * vec4(new_vertex, 1.0);
}