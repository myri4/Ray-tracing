#pragma once
#include <wc/pch.hpp>
#include <wc/Model/Model.hpp>
#include <gl/Texture3D.h>

namespace wc {
	struct SceneData
	{
		glm::vec3 lower_left_corner = glm::vec3(0.f);
		alignas(16) glm::vec3 horizontal = glm::vec3(0.f);
		alignas(16) glm::vec3 vertical = glm::vec3(0.f);
		alignas(16) glm::vec3 cameraPos = glm::vec3(0.f);
		uint32_t numLights = 0;
	};

	struct DrawCommand {
		uint32_t count = 0;
		uint32_t instanceCount = 1;
		uint32_t firstIndex = 0;
		uint32_t baseVertex = 0;
		uint32_t bvhID = 0;
	};

	uint32_t convertColor(const glm::vec4& color) {
		int32_t r = color.r * 255.f;
		int32_t g = color.g * 255.f;
		int32_t b = color.b * 255.f;
		int32_t a = color.a * 255.f;
		return a << 24 | b << 16 | g << 8 | r;
	}

	glm::vec4 convertColor(const uint32_t& color) {
		const float c = 1.f / 255.f;
		glm::vec4 Color;
		Color.r = float((uint32_t)(color & uint32_t(0x000000ff))) * c;
		Color.g = float((uint32_t)(color & uint32_t(0x0000ff00)) >> 8) * c;
		Color.b = float((uint32_t)(color & uint32_t(0x00ff0000)) >> 16) * c;
		Color.a = float((uint32_t)(color & uint32_t(0xff000000)) >> 24) * c;

		return Color;
	}	

	class Scene {
		// Screen stuff
		gl::Texture scrTexture;
		gl::Texture finalImage;
		gl::Texture bloomBuffer;

		// Materials
		gl::Texture u_Albedo;
		gl::Texture u_MaterialInfo;

		// Shaders
		gl::ComputeShader screenShader;
		gl::ComputeShader compositeShader;
		gl::ComputeShader bloomShader;

		struct AABB {
			glm::vec4 start;
			glm::vec4 end;
		};

		// Data pointers
		DrawCommand* cmds = nullptr;
		AABB* bvhData = nullptr;

		// Data gpu buffers
		gl::UniformBuffer<SceneData> dataBuffer;
		gl::ShaderStorageBuffer<Vertex> vertexBuffer;
		gl::ShaderStorageBuffer<uint32_t> indexBuffer;
		gl::ShaderStorageBuffer<DrawCommand> drawCommandBuffer; // change this to mesh buffer
		gl::ShaderStorageBuffer<AABB> bvhBuffer;

		Model model;
	public:

		SceneData* data = nullptr;
		glm::ivec2 bloomTexSize = glm::ivec2(0);
		uint32_t m_BloomComputeWorkGroupSize = 4;
		uint32_t mips = 1;
		float MouseSensitivity = 5.f;

		void Create() {
			screenShader.Create("shaders/screenShader.comp");
			bloomShader.Create("shaders/bloomShader.comp");
			compositeShader.Create("shaders/composite.comp");

			CreateScreen();

			uint32_t bits = GL_MAP_PERSISTENT_BIT | GL_MAP_WRITE_BIT | GL_MAP_COHERENT_BIT;

			dataBuffer.Create(bits);
			bvhBuffer.Create(bits);
			drawCommandBuffer.Create(bits);

			bvhData = bvhBuffer.Map(bits);
			data = dataBuffer.Map(bits);
			cmds = drawCommandBuffer.Map(bits);

			model.Load("assets/models/campfire.obj", bvhData[0].start, bvhData[0].end);

			vertexBuffer.Create(bits, model.vertices.size());
			indexBuffer.Create(bits, model.indices.size());

			Vertex* vertices = vertexBuffer.Map(bits, model.vertices.size());
			memcpy(vertices, model.vertices.data(), sizeof(Vertex) * model.vertices.size());
			vertexBuffer.UnMap();

			uint32_t* indices = indexBuffer.Map(bits, model.indices.size());
			memcpy(indices, model.indices.data(), sizeof(uint32_t) * model.indices.size());
			indexBuffer.UnMap();

			dataBuffer.BufferBase(0);
			vertexBuffer.BufferBase(2);
			indexBuffer.BufferBase(3);
			drawCommandBuffer.BufferBase(4);
			bvhBuffer.BufferBase(5);

			cmds[0].count = model.indices.size();
			cmds[0].instanceCount = 1;

			gl::load("assets/test/campfire.png", u_Albedo);
			gl::load("assets/test/pbr_output.png", u_MaterialInfo);
			camera.Position.y = 1.f;
		}

		void Update(const float& deltaTime) {
			auto windSize = window.GetSize();
			data->lower_left_corner = camera.lower_left_corner;
			data->cameraPos = camera.Position;
			data->horizontal = camera.horizontal;
			data->vertical = camera.vertical;

			scrTexture.BindImage(0);
			u_Albedo.Bind(1);
			u_MaterialInfo.Bind(2);

			glm::vec2 dispatchSize = glm::ceil((glm::vec2)windSize / glm::vec2(m_BloomComputeWorkGroupSize));
			screenShader.Bind();
			screenShader.Dispatch(dispatchSize);

			finalImage.BindImage(0);
			scrTexture.Bind(1); // use the color attachment texture as the texture of the quad plane	
			bloomBuffer.Bind(2);
			compositeShader.Bind();
			compositeShader.Dispatch(dispatchSize);

			Renderer2D::DrawQuad({ 0,0 }, windSize, finalImage);
		}

		bool UpdateInput = true;
		void OnInput(const float& deltaTime) {
			if (Keyboard::getKey(Keyboard::Key::F2)) {
				camera.Position = glm::vec3(2.f, 1.3f, 0.f);
				camera.Yaw = 180.f;
				camera.Pitch = -17.f;
				UpdateInput = !UpdateInput;
			}

			if (UpdateInput) {
				auto windSize = window.GetSize();
				float yaw90 = glm::radians(camera.Yaw + 90.f);
				const float MovementSpeed = 4.f * deltaTime;

				if (Keyboard::isKeyPressed(Keyboard::Key::W)) { // Front
					camera.Position.x += camera.Front.x * MovementSpeed;
					camera.Position.z += camera.Front.z * MovementSpeed;
				}

				else if (Keyboard::isKeyPressed(Keyboard::Key::S)) { // Back
					camera.Position.x -= camera.Front.x * MovementSpeed;
					camera.Position.z -= camera.Front.z * MovementSpeed;
				}
				if (Keyboard::isKeyPressed(Keyboard::Key::A)) { // Left
					camera.Position.x -= glm::cos(yaw90) * MovementSpeed;
					camera.Position.z -= glm::sin(yaw90) * MovementSpeed;
				}
				else if (Keyboard::isKeyPressed(Keyboard::Key::D)) { // Right
					camera.Position.x += glm::cos(yaw90) * MovementSpeed;
					camera.Position.z += glm::sin(yaw90) * MovementSpeed;
				}

				if (Keyboard::isKeyPressed(Keyboard::Key::Space))
					camera.Position.y += MovementSpeed;

				else if (Keyboard::isKeyPressed(Keyboard::Key::LShift))
					camera.Position.y -= MovementSpeed;


				if (Keyboard::isKeyPressed(Keyboard::Key::C)) { camera.FOV = 10.f; MouseSensitivity = 18; }
				else
				{
					MouseSensitivity = 5.f;
					camera.FOV = 90.f;
				}

				/*Mouse*/
				uint16_t xt, yt;

				glm::ivec2 pos = Mouse::GetMousePosToWindow();

				xt = windSize.x / 2;
				yt = windSize.y / 2;

				float ms = 1.f / MouseSensitivity;

				bool invertMouse = false;
				if (invertMouse) camera.Yaw += (xt - pos.x) * ms;
				else camera.Yaw -= (xt - pos.x) * ms;

				camera.Pitch += (yt - pos.y) * ms;

				// make sure that when pitch is out of bounds, screen doesn't get flipped
				if (camera.Pitch > 89.f) camera.Pitch = 89.f;
				else if (camera.Pitch < -89.f) camera.Pitch = -89.f;

				if (camera.Yaw > 360.f) camera.Yaw = 0.f;
				else if (camera.Yaw < 0.f) camera.Yaw = 360.f;

				Mouse::SetMousePosition(xt, yt);
			}
			camera.Update();
		}

		Camera camera;

		void CreateScreen() {
			// Creating the screen framebuffer
			gl::TextureProps scrProps;
			scrProps.internalFormat = GL_RGBA32F;
			scrProps.min_filter = GL_LINEAR_MIPMAP_LINEAR;
			scrProps.mag_filter = GL_LINEAR;
			scrProps.wrap_s = GL_CLAMP_TO_EDGE;
			scrProps.wrap_t = GL_CLAMP_TO_EDGE;
			scrProps.SetSize(window.GetSize());
			scrTexture.Create(scrProps);
			finalImage.Create(scrProps);

			bloomTexSize = glm::ivec2(scrProps.Width, scrProps.Height) / 2;
			bloomTexSize += glm::ivec2(m_BloomComputeWorkGroupSize - bloomTexSize.x % m_BloomComputeWorkGroupSize, m_BloomComputeWorkGroupSize - bloomTexSize.y % m_BloomComputeWorkGroupSize);
			mips = scrTexture.GetMipLevelCount() - 4;
			scrProps.mips = mips;

			scrProps.SetSize(bloomTexSize);
			bloomBuffer.Create(scrProps);
		}

		void DestroyScreen() {
			scrTexture.Destroy();
			finalImage.Destroy();
			bloomBuffer.Destroy();
		}
	private:


	};
}