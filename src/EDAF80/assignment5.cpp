#include "assignment5.hpp"
#include "interpolation.hpp"
#include "parametric_shapes.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/helpers.hpp"
#include "core/node.hpp"
#include "core/ShaderProgramManager.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <tinyfiledialogs.h>

#include <clocale>
#include <stdexcept>

using std::array;

edaf80::Assignment5::Assignment5(WindowManager& windowManager) :
	mCamera(0.5f * glm::half_pi<float>(),
	        static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
	        0.01f, 1000.0f),
	inputHandler(), mWindowManager(windowManager), window(nullptr)
{
	WindowManager::WindowDatum window_datum{ inputHandler, mCamera, config::resolution_x, config::resolution_y, 0, 0, 0, 0};

	window = mWindowManager.CreateGLFWWindow("EDAF80: Assignment 5", window_datum, config::msaa_rate);
	if (window == nullptr) {
		throw std::runtime_error("Failed to get a window: aborting!");
	}

	bonobo::init();
}

edaf80::Assignment5::~Assignment5()
{
	bonobo::deinit();
}

void
edaf80::Assignment5::run()
{
	//
	// Variables
	//
	auto pi = glm::pi<float>();
	auto player_height = 1.0f;
	auto ground_y = -25;
	auto player_position = glm::vec3(0, ground_y + player_height, 0);
	auto camera_displacement = glm::vec3(0.0f, 5.0f, 5.0f);
	auto camera_position = player_position + camera_displacement;
	auto camera_rotation = -0.2*pi;
	auto skybox_position = camera_position;

	auto delay_counter = 0.0f;
	auto delay_time = 0.1f;
	auto update_body_segments = false;

	const size_t body_segments = 8;
	const auto segment_displacement = 0.35;
	array<glm::vec3,body_segments> new_segment_positions;
	array<glm::vec3,body_segments> current_segment_positions;

	//
	// Set up the camera
	//
	mCamera.mWorld.SetTranslate(camera_position);
	mCamera.mWorld.SetRotateX(camera_rotation);
	mCamera.mMouseSensitivity = 0.003f;
	mCamera.mMovementSpeed = 3.0f; // 3 m/s => 10.8 km/h

	//
	// Create the shader programs
	//
	ShaderProgramManager program_manager;
	GLuint shader_fallback = 0u;
	program_manager.CreateAndRegisterProgram("Fallback",
	                                         { { ShaderType::vertex, "common/fallback.vert" },
	                                           { ShaderType::fragment, "common/fallback.frag" } },
	                                         shader_fallback);
	if (shader_fallback == 0u) {
		LogError("Failed to load fallback shader");
		return;
	}

	GLuint shader_skybox = 0u;
	program_manager.CreateAndRegisterProgram("Skybox",
	                                         { { ShaderType::vertex, "game/skybox.vert" },
	                                           { ShaderType::fragment, "game/skybox.frag" } },
	                                         shader_skybox);
	if (shader_skybox == 0u) {
		LogError("Failed to load skybox shader");
		return;
	}

	GLuint shader_water = 0u;
	program_manager.CreateAndRegisterProgram("Water",
	                                         { { ShaderType::vertex, "EDAF80/water.vert" },
	                                           { ShaderType::fragment, "EDAF80/water.frag" } },
	                                         shader_water);
	if (shader_water == 0u) {
		LogError("Failed to load water shader");
		return;
	}

	GLuint shader_ground = 0u;
	program_manager.CreateAndRegisterProgram("Ground",
	                                         { { ShaderType::vertex, "game/ground.vert" },
	                                           { ShaderType::fragment, "game/ground.frag" } },
	                                         shader_ground);
	if (shader_ground == 0u) {
		LogError("Failed to load ground shader");
		return;
	}
	
	GLuint shader_phong = 0u;
	program_manager.CreateAndRegisterProgram("Phong",
	                                         { { ShaderType::vertex, "EDAF80/phong.vert" },
	                                           { ShaderType::fragment, "EDAF80/phong.frag" } },
	                                         shader_phong);
	if (shader_phong == 0u)
		LogError("Failed to load phong shader");

	//
	// Set uniforms
	//
	float elapsed_time_s = 0.0f;
	auto light_position = glm::vec3(-8.0f, -15.0f, 2.0f);
	auto const uniforms_skybox = [&light_position,&camera_position,&elapsed_time_s](GLuint program){
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position)),
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position)),
		glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), elapsed_time_s);
	};

	bool use_normal_mapping = true;
	auto ambient = glm::vec3(0.1f, 0.3f, 0.2f);
	auto diffuse = glm::vec3(0.4f, 0.6f, 0.3f);
	auto specular = glm::vec3(0.3f, 1.0f, 0.5f);
	auto shininess = 10.0f;
	auto const uniforms_phong = [&use_normal_mapping,&light_position,&camera_position,&ambient,&diffuse,&specular,&shininess](GLuint program){
		glUniform1i(glGetUniformLocation(program, "use_normal_mapping"), use_normal_mapping ? 1 : 0);
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
		glUniform3fv(glGetUniformLocation(program, "ambient"), 1, glm::value_ptr(ambient));
		glUniform3fv(glGetUniformLocation(program, "diffuse"), 1, glm::value_ptr(diffuse));
		glUniform3fv(glGetUniformLocation(program, "specular"), 1, glm::value_ptr(specular));
		glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
	};

	auto const uniforms_water = [&light_position,&camera_position,&elapsed_time_s](GLuint program){
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position)),
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position)),
		glUniform1f(glGetUniformLocation(program, "elapsed_time_s"), elapsed_time_s);
	};

	//
	// Load your geometry
	//
	auto const shape_skybox = parametric_shapes::createSphere(100.0f, 100u, 100u);
	auto const shape_ground = parametric_shapes::createRandomQuad(250, 250, 1500, 1500, 0.075);
	auto const shape_player = parametric_shapes::createSphere(player_height / 2, 10, 10);
	auto const shape_point = parametric_shapes::createSphere(0.2f,20,20);

	//
	// Textures
	//
	std::string cubemap_texture_path = config::resources_path("cubemaps/NissiBeach2/");
    auto map_cube_skybox = bonobo::loadTextureCubeMap( cubemap_texture_path + "posx.jpg",
                                                        cubemap_texture_path + "negx.jpg",
                                                        cubemap_texture_path + "posy.jpg",
                                                    	cubemap_texture_path + "negy.jpg",
                                                        cubemap_texture_path + "posz.jpg",
                                                        cubemap_texture_path + "negz.jpg" );
	auto map_normal_water = bonobo::loadTexture2D(config::resources_path("textures/waves.png"));
	auto texture_ground = bonobo::loadTexture2D(config::resources_path("textures/leather_red_02_coll1_2k.jpg"));
	auto map_specular_ground = bonobo::loadTexture2D(config::resources_path("textures/leather_red_02_rough_2k.jpg"));
	auto map_normal_ground = bonobo::loadTexture2D(config::resources_path("textures/leather_red_02_nor_2k.jpg"));

	//
	// Nodes
	//
	Node skybox;
	skybox.set_geometry(shape_skybox);
	skybox.set_program(&shader_skybox, uniforms_skybox);
	skybox.add_texture("cube_map", map_cube_skybox, GL_TEXTURE_CUBE_MAP);
	skybox.get_transform().SetTranslate(skybox_position);
	
	Node point;
	point.set_geometry(shape_point);
	point.get_transform().SetTranslate(player_position + glm::vec3(0.0f,0.0f,-5.0f));
	point.set_program(&shader_phong, uniforms_phong);
	point.add_texture("sphere_texture", texture_ground, GL_TEXTURE_2D);
	point.add_texture("skybox_texture", map_cube_skybox, GL_TEXTURE_CUBE_MAP);
	point.add_texture("specular_map", map_specular_ground, GL_TEXTURE_2D);
	point.add_texture("normal_map", map_normal_ground, GL_TEXTURE_2D);
	
	Node ground;
	ground.set_geometry(shape_ground);
	ground.get_transform().SetTranslate(glm::vec3(-50.0f, ground_y, 0.0f));
	ground.set_program(&shader_ground, uniforms_phong);
	ground.add_texture("my_texture", texture_ground, GL_TEXTURE_2D);
	ground.add_texture("skybox_texture", map_cube_skybox, GL_TEXTURE_CUBE_MAP);
	ground.add_texture("specular_map", map_specular_ground, GL_TEXTURE_2D);
	ground.add_texture("normal_map", map_normal_ground, GL_TEXTURE_2D);

	Node player;
	player.set_geometry(shape_player);
	player.get_transform().SetTranslate(player_position);
	player.set_program(&shader_phong, uniforms_phong);
	player.add_texture("sphere_texture", texture_ground, GL_TEXTURE_2D);
	player.add_texture("skybox_texture", map_cube_skybox, GL_TEXTURE_CUBE_MAP);
	player.add_texture("specular_map", map_specular_ground, GL_TEXTURE_2D);
	player.add_texture("normal_map", map_normal_ground, GL_TEXTURE_2D);
	
	array<Node, body_segments> body;
	for (size_t i = 0; i < body.size(); i++)
	{
		body[i].set_geometry(shape_player);
		body[i].get_transform().SetTranslate(player_position + glm::vec3(0.0f,0.0f,segment_displacement*static_cast<float>(i+1)));
		body[i].set_program(&shader_phong, uniforms_phong);
		body[i].add_texture("sphere_texture", texture_ground, GL_TEXTURE_2D);
		body[i].add_texture("skybox_texture", map_cube_skybox, GL_TEXTURE_CUBE_MAP);
		body[i].add_texture("specular_map", map_specular_ground, GL_TEXTURE_2D);
		body[i].add_texture("normal_map", map_normal_ground, GL_TEXTURE_2D);
		player.add_child(&body[i]);
		
		current_segment_positions[i] = body[i].get_transform().GetTranslation();
		if(i < body.size()){
			new_segment_positions[i] = body[i].get_transform().GetTranslation();
		}
	}	

	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);

	auto lastTime = std::chrono::high_resolution_clock::now();
	auto cull_mode = bonobo::cull_mode_t::disabled;
	auto polygon_mode = bonobo::polygon_mode_t::fill;
	bool show_logs = true;
	bool show_gui = true;
	bool shader_reload_failed = false;
	bool show_basis = false;
	float basis_thickness_scale = 1.0f;
	float basis_length_scale = 1.0f;

	changeCullMode(cull_mode);
	
	while (!glfwWindowShouldClose(window)) {
		auto const nowTime = std::chrono::high_resolution_clock::now();
		auto const deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - lastTime);
		lastTime = nowTime;
		elapsed_time_s += std::chrono::duration<float>(deltaTimeUs).count();
		delay_counter += std::chrono::duration<float>(deltaTimeUs).count();
		if(delay_counter > delay_time){
			delay_counter = 0.0f;
			update_body_segments = true;
		}

		auto& io = ImGui::GetIO();
		inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);

		glfwPollEvents();
		inputHandler.Advance();
		mCamera.Update(deltaTimeUs, inputHandler);
		camera_position = mCamera.mWorld.GetTranslation();
		player_position = player.get_transform().GetTranslation();

		if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
			shader_reload_failed = !program_manager.ReloadAllPrograms();
			if (shader_reload_failed)
				tinyfd_notifyPopup("Shader Program Reload Error",
				                   "An error occurred while reloading shader programs; see the logs for details.\n"
				                   "Rendering is suspended until the issue is solved. Once fixed, just reload the shaders again.",
				                   "error");
		}
		if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
			show_logs = !show_logs;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
			show_gui = !show_gui;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F11) & JUST_RELEASED)
			mWindowManager.ToggleFullscreenStatusForWindow(window);

		int framebuffer_width, framebuffer_height;
		glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
		glViewport(0, 0, framebuffer_width, framebuffer_height);

		//
		// Handle input
		//
		auto speed = 0.05f, dx = 0.0f, dy = 0.0f;
		if (inputHandler.GetKeycodeState(GLFW_KEY_UP) & PRESSED) 
			dy += speed;
		if (inputHandler.GetKeycodeState(GLFW_KEY_DOWN) & PRESSED) 
			dy -= speed;
		if (inputHandler.GetKeycodeState(GLFW_KEY_LEFT) & PRESSED) 
			dx -= speed;
		if (inputHandler.GetKeycodeState(GLFW_KEY_RIGHT) & PRESSED) 
			dx += speed;
		
		auto x_new = player_position.x + dx, 
			 y_new = player_position.y + dy, 
			 z_new = player_position.z;

		x_new = fmin(fmax(x_new, -2.0f), 2.0f);
		y_new = fmin(fmax(y_new, -24.0f), -23.0f);
		player_position = glm::vec3(x_new, y_new, z_new);
		camera_position = player_position + camera_displacement;
		skybox_position = glm::vec3(camera_position.x, -23, camera_position.z);

		if(update_body_segments){
			update_body_segments = false;
			auto pos = player_position;
			auto body_z = body[0].get_transform().GetTranslation().z;

			new_segment_positions[0] = glm::vec3(pos.x, pos.y, body_z);
			for (size_t i = 1; i < body.size(); i++)
			{
				pos = body[i-1].get_transform().GetTranslation();
				body_z = body[i].get_transform().GetTranslation().z;
				new_segment_positions[i] = glm::vec3(pos.x, pos.y, body_z);
			}

			for (size_t i = 0; i < body.size(); i++)
			{
				current_segment_positions[i] = body[i].get_transform().GetTranslation();
			}	
		}
		
		//
		// Update positions
		//
		mCamera.mWorld.SetTranslate(camera_position);
		mCamera.mWorld.SetRotateX(camera_rotation);
		ground.get_transform().SetRotateY(-elapsed_time_s * 0.05 * pi);
		skybox.get_transform().SetTranslate(skybox_position);
		player.get_transform().SetTranslate(player_position);
		for (size_t i = 0; i < body.size(); i++)
		{
			body[i].get_transform().SetTranslate(interpolation::evalLERP(
				current_segment_positions[i],
				new_segment_positions[i],
				delay_counter / delay_time
			));
			//body[i].get_transform().SetTranslate(new_segment_positions[i]);
		}
		
		mWindowManager.NewImGuiFrame();

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		bonobo::changePolygonMode(polygon_mode);

		if (!shader_reload_failed) {
			//
			// Render all geometry
			//	
			skybox.render(mCamera.GetWorldToClipMatrix());
			ground.render(mCamera.GetWorldToClipMatrix());
			point.render(mCamera.GetWorldToClipMatrix());
			player.render(mCamera.GetWorldToClipMatrix());
			for (size_t i = 0; i < body.size(); i++)
			{
				body[i].render(mCamera.GetWorldToClipMatrix());
			}
		}


		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//
		// Todo: If you want a custom ImGUI window, you can set it up
		//       here
		//
		bool const opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
		if (opened) {
			ImGui::Checkbox("Show basis", &show_basis);
			ImGui::SliderFloat("Basis thickness scale", &basis_thickness_scale, 0.0f, 100.0f);
			ImGui::SliderFloat("Basis length scale", &basis_length_scale, 0.0f, 100.0f);
		}
		ImGui::End();

		if (show_basis)
			bonobo::renderBasis(basis_thickness_scale, basis_length_scale, mCamera.GetWorldToClipMatrix());
		if (show_logs)
			Log::View::Render();
		mWindowManager.RenderImGuiFrame(show_gui);

		glfwSwapBuffers(window);
	}
}

int main()
{
	std::setlocale(LC_ALL, "");

	Bonobo framework;

	try {
		edaf80::Assignment5 assignment5(framework.GetWindowManager());
		assignment5.run();
	} catch (std::runtime_error const& e) {
		LogError(e.what());
	}
}
