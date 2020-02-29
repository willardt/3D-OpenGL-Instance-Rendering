#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdlib>

#include <gl/gl3w.h>
#include <glfw3.h>
#include <gtc/matrix_transform.hpp>

const int WINDOW_W = 1920;
const int WINDOW_H = 1080;

const float CAMERA_WIDTH = 100.0f;
const float CAMERA_HEIGHT = 100.0f;
const float ZOOM_FACTOR = 25.0f;

const float BOUNDARY_WIDTH = 1000000000.0f;
const float BOUNDARY_HEIGHT = 1000000000.0f;

const int MAX_PARTICLES = 10000000;

const float PARTICLE_SPEED = 50.0f;

const double SPAWN_TIME = 0.0001;

int num_particles = 0;

static double time = 0.0;

GLFWwindow *window = nullptr;
GLuint program = 0;

struct ShaderInfo {
	unsigned short type;
	const char *file_path;
};

GLuint load_shaders(ShaderInfo *shaders) {
	const char *data;
	std::vector<GLuint> shader_ids;
	GLuint program_id = glCreateProgram();
	GLint result;
	GLint info_log_length;
	bool loaded = true;

	for (unsigned int i = 0; shaders[i].type != GL_NONE; ++i) {
		GLuint shader_id = glCreateShader(shaders[i].type);
		shader_ids.push_back(shader_id);

		std::string sdata;
		std::stringstream ssdata;
		std::fstream shader_data(shaders[i].file_path, std::ios::in);
		if (shader_data.is_open()) {
			ssdata << shader_data.rdbuf();
			shader_data.seekg(0, std::ios::end);
			sdata.reserve((size_t)shader_data.tellg());
			sdata = ssdata.str();
			data = sdata.c_str();
		}
		else {
			std::cout << "Couldn't open shader file -- " << shaders[i].file_path << std::endl;
			loaded = false;
		}

		if (loaded) {
			glShaderSource(shader_id, 1, &data, NULL);
			glCompileShader(shader_id);

			glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
			std::cout << "Shader Result -- " << shaders[i].file_path << " -- " << ((result == GL_TRUE) ? "Good" : "Bad") << std::endl;
			glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
			if (info_log_length != 0) {
				GLchar *info_log = new GLchar[info_log_length];
				glGetShaderInfoLog(shader_id, info_log_length, 0, info_log);
				std::cout << "-- Shader Log --" << std::endl << info_log << std::endl;
				std::cout << "-- " << shaders[i].file_path << "-- " << std::endl << data << std::endl;
				delete info_log;
			}
		}
	}

	for (auto shader_id : shader_ids) {
		glAttachShader(program_id, shader_id);
	}

	glLinkProgram(program_id);

	for (auto shader_id : shader_ids) {
		glDetachShader(program_id, shader_id);
		glDeleteShader(shader_id);
	}

	return program_id;
}

GLuint make_shaders() {
	ShaderInfo shaders[] = {
		{ GL_VERTEX_SHADER, "shader.vert"   },
		{ GL_FRAGMENT_SHADER, "shader.frag" },
		{ GL_NONE, 0 }
	};

	return load_shaders(shaders);
}

struct Camera {
	float width = CAMERA_WIDTH, height = CAMERA_HEIGHT;
	float zoom = 0.0f;
	float x = 0.0f, y = 0.0f;
	glm::mat4 projection = glm::ortho(
		-(width / 2),
		width / 2,
		-(height / 2),
		height / 2,
		0.0f,
		1000.0f
	);
	glm::mat4 view = glm::lookAt(
		glm::vec3(x, y, 1),
		glm::vec3(x, y, 0),
		glm::vec3(0, 1, 0)
	);
} camera;

void update_view() {
	camera.view = glm::lookAt(
		glm::vec3(camera.x, camera.y, 1),
		glm::vec3(camera.x, camera.y, 0),
		glm::vec3(0, 1, 0)
	);

	GLuint view = glGetUniformLocation(program, "view");
	glUniformMatrix4fv(view, 1, GL_FALSE, &camera.view[0][0]);
}

void screen_to_world_coordinates(double *xpos, double *ypos) {
	*xpos /= WINDOW_W;
	*ypos /= WINDOW_H;
	float width = camera.width;
	float height = camera.height;
	*xpos = *xpos * width - (width / 2);
	*ypos = -(*ypos * height - (height / 2));
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
	float last_w = camera.width, last_h = camera.height;
	if (yoffset == 1 &&
		camera.width - ZOOM_FACTOR > 0.0f &&
		camera.height - ZOOM_FACTOR > 0.0f) {
		camera.width -= ZOOM_FACTOR;
		camera.height -= ZOOM_FACTOR;
		++camera.zoom;
	}
	else if (yoffset == -1) {
		camera.width += ZOOM_FACTOR;
		camera.height += ZOOM_FACTOR;
		--camera.zoom;
	}

	camera.projection = glm::ortho(
		-(camera.width / 2),
		camera.width / 2,
		-(camera.height / 2),
		camera.height / 2,
		0.0f,
		1000.0f
	);

	GLuint projection = glGetUniformLocation(program, "projection");
	glUniformMatrix4fv(projection, 1, GL_FALSE, &camera.projection[0][0]);
}

bool clock() {
	static double last_time_interval = glfwGetTime();
	static double last_time = glfwGetTime();
	static int frames = 0;

	time = glfwGetTime() - last_time;
	last_time = glfwGetTime();

	++frames;
	if (glfwGetTime() - last_time_interval >= 1.0) {
		std::string title = "Particles: " + std::to_string(num_particles) +
			"     fms: " + std::to_string(1000.0 / double(frames));
		glfwSetWindowTitle(window, title.c_str());
		frames = 0;
		last_time_interval += 1.0;
		return true;
	}
	return false;
}

int main() {
	glfwInit();

	window = glfwCreateWindow(WINDOW_W, WINDOW_H, "...", NULL, NULL);
	glfwMakeContextCurrent(window);

	gl3wInit();

	glfwSetScrollCallback(window, &scroll_callback);

	static double last_x, last_y, x_vel, y_vel;
	glfwGetCursorPos(window, &last_x, &last_y);
	screen_to_world_coordinates(&last_x, &last_y);

	program = make_shaders();
	glUseProgram(program);

	update_view();

	static const GLfloat vertex_buffer_data[] = {
		-0.1f, -0.1f,
		0.1f, -0.1f,
		-0.1f, 0.1f,
		0.1f, 0.1f,
	};

	static GLfloat position_buffer_data[MAX_PARTICLES * 2];
	static int position_index = 0;

	static GLfloat color_buffer_data[MAX_PARTICLES * 4];
	static int color_index = 0;

	static GLfloat vector_data[MAX_PARTICLES * 2];
	static int vector_index = 0;

	GLuint vertex;
	glCreateVertexArrays(1, &vertex);
	glBindVertexArray(vertex);

	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

	GLuint position_buffer;
	glGenBuffers(1, &position_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * MAX_PARTICLES * 2, NULL, GL_STREAM_DRAW);

	GLuint color_buffer;
	glGenBuffers(1, &color_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * MAX_PARTICLES * 4, NULL, GL_STREAM_DRAW);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(1, 1);

	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glVertexAttribDivisor(2, 1);

	GLuint projection = glGetUniformLocation(program, "projection");
	glUniformMatrix4fv(projection, 1, GL_FALSE, &camera.projection[0][0]);

	GLfloat color[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	while (!glfwWindowShouldClose(window) && !glfwGetKey(window, GLFW_KEY_ESCAPE)) {
		glClearBufferfv(GL_COLOR, 0, color);

		//glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
		glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 2 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, num_particles * sizeof(GLfloat) * 2, position_buffer_data);

		glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
		glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, num_particles * sizeof(GLfloat) * 4, color_buffer_data);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glVertexAttribDivisor(1, 1);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glVertexAttribDivisor(2, 1);

		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, num_particles);

		glfwSwapBuffers(window);

		glfwPollEvents();

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		screen_to_world_coordinates(&xpos, &ypos);
		x_vel = xpos - last_x;
		y_vel = ypos - last_y;

		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
			camera.x += -x_vel;
			camera.y += -y_vel;
			update_view();
		}

		last_x = xpos;
		last_y = ypos;

		/*
		if ((glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) && 
			last_x != xpos && last_y != ypos &&
			num_particles != MAX_PARTICLES &&
			(xpos < BOUNDARY_WIDTH) && (xpos > -BOUNDARY_WIDTH) && (ypos < BOUNDARY_HEIGHT) && (ypos > -BOUNDARY_HEIGHT)) {
			glm::vec2 vector(x_vel, y_vel);
			vector = glm::normalize(vector);
			if (vector.x != 0 && vector.y != 0) {
				vector_data[vector_index++] = vector.x;
				vector_data[vector_index++] = vector.y;
				xpos += camera.x;
				ypos += camera.y;

				position_buffer_data[position_index++] = (GLfloat)xpos;
				position_buffer_data[position_index++] = (GLfloat)ypos;
				GLfloat color[] = { (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, 1.0f };
				color_buffer_data[color_index++] = color[0];
				color_buffer_data[color_index++] = color[1];
				color_buffer_data[color_index++] = color[2];
				color_buffer_data[color_index++] = color[3];
				++num_particles;
			}
		}
		*/

		static GLfloat vectors[16][2] = {
			{0.0f, 1.0f},
			{0.0f, -1.0f},
			{1.0f, 0.0f},
			{-1.0f, 0.0f},
			{.50f, .50f},
			{.50f, -.50f},
			{-.50f, .50f},
			{-.50f, -.50f},
			{-.25f, .75f},
			{-.75f, .25f},
			{-.75f, -.25f},
			{-.25f, -.75f},
			{.25f, .75f},
			{.75f, .25f},
			{.75f, -.25f},
			{.25f, -.75f}
		};

		glfwGetCursorPos(window, &xpos, &ypos);
		screen_to_world_coordinates(&xpos, &ypos);
		xpos += camera.x;
		ypos += camera.y;

		static double update_time = time;
		if ((glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) &&
			update_time > SPAWN_TIME &&
			num_particles != MAX_PARTICLES &&
			(xpos < BOUNDARY_WIDTH) && (xpos > -BOUNDARY_WIDTH) && (ypos < BOUNDARY_HEIGHT) && (ypos > -BOUNDARY_HEIGHT)) {
			for (int i = 0; i < 16; ++i) {
				if (num_particles < MAX_PARTICLES) {
					vector_data[vector_index++] = vectors[i][0];
					vector_data[vector_index++] = vectors[i][1];

					position_buffer_data[position_index++] = (GLfloat)xpos;
					position_buffer_data[position_index++] = (GLfloat)ypos;
					GLfloat color[] = { (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f, 1.0f };
					color_buffer_data[color_index++] = color[0];
					color_buffer_data[color_index++] = color[1];
					color_buffer_data[color_index++] = color[2];
					color_buffer_data[color_index++] = color[3];
					++num_particles;
					update_time = 0.0;
				}
			}
		}
		update_time += time;

		
		for (int i = 0; i < num_particles * 2; ++i) {
			position_buffer_data[i] += vector_data[i] * PARTICLE_SPEED * time;

			if (position_buffer_data[i] >= BOUNDARY_WIDTH || position_buffer_data[i] <= -BOUNDARY_WIDTH) {
				vector_data[i] = -vector_data[i];
				position_buffer_data[i] += vector_data[i] * PARTICLE_SPEED * time;
			}

			position_buffer_data[++i] += vector_data[i] * PARTICLE_SPEED * time;
			if (position_buffer_data[i] >= BOUNDARY_HEIGHT || position_buffer_data[i] <= -BOUNDARY_HEIGHT) {
				vector_data[i] = -vector_data[i];
				position_buffer_data[i] += vector_data[i] * PARTICLE_SPEED * time;
			}
		}
	

		clock();
	}

	return 0;
}