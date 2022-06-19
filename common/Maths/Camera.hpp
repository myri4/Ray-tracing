#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <Utils/Window.hpp>

namespace wc{
	struct Camera {
		// camera Attributes
		glm::vec3 Position = glm::vec3(0.f);
		glm::vec3 Front = glm::vec3(0.f, 0.f, -1.f);
		glm::vec3 Up = glm::vec3(0.f, 1.f, 0.f); // v
		glm::vec3 Right = glm::vec3(0.f); // u
		// Ray tracing attributes
		glm::vec3 lower_left_corner = glm::vec3(0.f);
		glm::vec3 horizontal = glm::vec3(0.f);
		glm::vec3 vertical = glm::vec3(0.f);
		// euler Angles
		float Yaw = 0.f;
		float Pitch = 0.f;
		// camera options
		float FOV = 90.f;

		// constructor with vectors
		Camera() {}

		void Update() {
			float yaw = glm::radians(Yaw);
			float pitch = glm::radians(Pitch);
			glm::vec3 front;
			front.x = glm::cos(yaw) * glm::cos(pitch);
			front.y = glm::sin(pitch);
			front.z = glm::sin(yaw) * glm::cos(pitch);
			Front = glm::normalize(front);

			// also re-calculate the Right and Up vector
			Right = normalize(cross(Front, glm::vec3(0.f, 1.f, 0.f)));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
			Up = normalize(cross(Right, Front));

			float theta = glm::tan(glm::radians(FOV) * 0.5f);
			float viewport_height = 2.f * theta;
			glm::vec2 windSize = window.GetSize();
			float viewport_width = (windSize.x / windSize.y) * viewport_height;

			horizontal = viewport_width * Right;
			vertical = viewport_height * Up;
			lower_left_corner = Position - horizontal * 0.5f + vertical * 0.5f + Front;
		}
	};
}