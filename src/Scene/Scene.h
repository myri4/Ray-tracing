#pragma once
#include <wc/pch.hpp>
#include <wc/Model/Model.hpp>
#include <gl/Texture3D.h>
#include "Chunk.h"

#define NUM_LIGHTS 16

namespace wc {
	// @TODO: add method for adding meshes, transforms to them etc
	struct SceneData
	{
		glm::vec3 lower_left_corner = glm::vec3(0.f);
		alignas(16) glm::vec3 horizontal = glm::vec3(0.f);
		alignas(16) glm::vec3 vertical = glm::vec3(0.f);
		alignas(16) glm::vec3 cameraPos = glm::vec3(0.f);
		uint32_t numLights = 0;
		uint32_t maxBounces = 1; // @TODO: remove
	};

	struct Light {
		glm::vec3 vector = glm::vec3(0.f);
		uint32_t color = 0;
	};

	struct DrawCommand {
		uint32_t count = 0;
		uint32_t instanceCount = 1;
		uint32_t firstIndex = 0;
		uint32_t baseVertex = 0;
		uint32_t bvhID = 0;
	};	

	enum class BloomMode
	{
		Prefilter,
		Downsample,
		UpsampleFirst,
		Upsample
	};

	struct BloomBufferSettings {
		glm::vec4 Params = glm::vec4(1.f); // (x) threshold, (y) threshold - knee, (z) knee * 2, (w) 0.25 / knee
		float LOD = 0.f;
		int Mode = (int)BloomMode::Prefilter;
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
		gl::Texture bloomBuffers[3];

		// Materials
		gl::Texture u_Albedo;
		gl::Texture u_MaterialInfo;

		// Shaders
		gl::ComputeShader screenShader;
		gl::ComputeShader compositeShader;
		gl::ComputeShader bloomShader;

		// Data pointers
		Light* lighting = nullptr;
		DrawCommand* cmds = nullptr;
		AABB* bvhData = nullptr;

		// Data gpu buffers
		gl::UniformBuffer<SceneData> dataBuffer;
		gl::UniformBuffer<BloomBufferSettings> bloomUBO;
		gl::UniformBuffer<Light> lightBuffer;
		gl::ShaderStorageBuffer<Vertex> vertexBuffer;
		gl::ShaderStorageBuffer<uint32_t> indexBuffer;
		gl::ShaderStorageBuffer<DrawCommand> drawCommandBuffer; // change this to mesh buffer
		gl::ShaderStorageBuffer<AABB> bvhBuffer;

		gl::ShaderStorageBuffer<Chunk> chunkBuffer;
		//gl::Texture3D voxelData;
		gl::ShaderStorageBuffer<uint8_t> voxelData;


		Model model;
		glm::vec3 sunAngle = glm::vec3(0.f);

		Chunk* chunks = nullptr;
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
			lightBuffer.Create(bits, NUM_LIGHTS);
			bloomUBO.Create(GL_DYNAMIC_STORAGE_BIT);
			bvhBuffer.Create(bits);
			drawCommandBuffer.Create(bits);
			chunkBuffer.Create(bits, numChunks);

			bvhData = bvhBuffer.Map(bits);
			lighting = lightBuffer.Map(bits, NUM_LIGHTS);
			data = dataBuffer.Map(bits);
			cmds = drawCommandBuffer.Map(bits);
			chunks = chunkBuffer.Map(bits, numChunks);

			voxelData.Create(bits | GL_DYNAMIC_STORAGE_BIT, chunkVolume * numChunks);

			uint8_t data[chunkSize][chunkSize][chunkSize];
			memset(data, 0, sizeof(data));

			for (int i = 0; i < numChunks; i++) {
				chunks[i].pointerStart = i * chunkVolume;
			}
			for (int i = 0; i < numChunks; i++) {
				for (uint32_t z = 0; z < chunkSize; z++) {
					for (uint32_t x = 0; x < chunkSize; x++) {
						for (uint8_t y = 0; y < chunkSize; y++) {
							if (y == 0) data[x][y][z] = 1;
						}
					}
				}

				voxelData.SetData(chunkVolume, data, chunkVolume * i);
			}



			model.Load("assets/models/Murshroom.obj", bvhData[0].start, bvhData[0].end);

			vertexBuffer.Create(bits, model.vertices.size());
			indexBuffer.Create(bits, model.indices.size());

			Vertex* vertices = vertexBuffer.Map(bits, model.vertices.size());
			memcpy(vertices, model.vertices.data(), sizeof(Vertex) * model.vertices.size());
			vertexBuffer.UnMap();

			uint32_t* indices = indexBuffer.Map(bits, model.indices.size());
			memcpy(indices, model.indices.data(), sizeof(uint32_t) * model.indices.size());
			indexBuffer.UnMap();

			lightBuffer.BufferBase(1);
			vertexBuffer.BufferBase(2);
			indexBuffer.BufferBase(3);
			drawCommandBuffer.BufferBase(4);
			bvhBuffer.BufferBase(5);
			chunkBuffer.BufferBase(6);
			voxelData.BufferBase(7);

			cmds[0].count = model.indices.size();
			cmds[0].instanceCount = 1;

			gl::load("assets/test/albedo.png", u_Albedo);
			gl::load("assets/test/pbr_output.png", u_MaterialInfo);
			camera.Position.y = 1.f;

			addLight(glm::vec3(0.f, 1.f, 0.f), convertColor(glm::vec4(1.f, 1.f, 1.f, 0.f)));
			//addLight(glm::vec3(0.f, 1.f/*0.665f*/, 0.f), convertColor(glm::vec4(166.f / 255.f, 1.f, 253.f / 256.f, 1.f)));
		}

		void RenderBloom()
		{
			bloomShader.Bind();
			BloomBufferSettings settings;
			settings.Params = glm::vec4(bloomSettings.Threshold, bloomSettings.Threshold - bloomSettings.Knee, bloomSettings.Knee * 2.f, 0.25f / bloomSettings.Knee);
			bloomUBO.SetData(1, &settings);
			bloomBuffers[0].BindImage(0);
			scrTexture.Bind(1);
			bloomBuffers[2].Bind(2);
			bloomShader.Dispatch(glm::ceil(glm::vec2(bloomTexSize) / glm::vec2(m_BloomComputeWorkGroupSize)));

			settings.Mode = (int)BloomMode::Downsample;
			for (int currentMip = 1; currentMip < mips; currentMip++)
			{
				glm::vec2 dispatchSize = glm::ceil((glm::vec2)bloomBuffers[0].GetMipSize(currentMip) / glm::vec2(m_BloomComputeWorkGroupSize));

				// Ping 
				settings.LOD = float(currentMip - 1);
				bloomUBO.SetData(1, &settings);

				bloomBuffers[1].BindImage(0, currentMip);
				if (currentMip == 1) bloomBuffers[0].Bind(1);
				bloomShader.Dispatch(dispatchSize);

				// Pong 
				settings.LOD = float(currentMip);
				bloomUBO.SetData(1, &settings);

				bloomBuffers[0].BindImage(0, currentMip);
				bloomBuffers[1].Bind(1);
				bloomShader.Dispatch(dispatchSize);
			}

			// First Upsample		
			settings.LOD = float(mips - 2);
			settings.Mode = (int)BloomMode::UpsampleFirst;
			bloomUBO.SetData(1, &settings);

			bloomBuffers[2].BindImage(0, mips - 1);
			bloomBuffers[0].Bind(1);

			bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(mips - 1) / glm::vec2(m_BloomComputeWorkGroupSize)));

			settings.Mode = (int)BloomMode::Upsample;
			for (int currentMip = mips - 2; currentMip >= 0; currentMip--)
			{
				settings.LOD = float(currentMip);
				bloomUBO.SetData(1, &settings);

				bloomBuffers[2].BindImage(0, currentMip);

				bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(currentMip) / glm::vec2(m_BloomComputeWorkGroupSize)));
			}
		}

		void Update(const float& deltaTime) {
			auto windSize = window.GetSize();
			data->lower_left_corner = camera.lower_left_corner;
			data->cameraPos = camera.Position;
			data->horizontal = camera.horizontal;
			data->vertical = camera.vertical;
			sunAngle.x += 6.f * deltaTime;
			sunAngle.x = glm::mod(sunAngle.x, 360.f);
			glm::vec3 sunVector = glm::vec3(0.f);
			lighting[0].vector = -glm::vec3(glm::vec4(1.f, 0.f, 0.f, 0.f) * glm::rotate(glm::mat4(1.f), glm::radians(sunAngle.x), glm::vec3(0.f, 0.f, 1.f)));

			dataBuffer.BufferBase(0);

			scrTexture.BindImage(0);
			u_Albedo.Bind(1);
			u_MaterialInfo.Bind(2);

			glm::vec2 dispatchSize = glm::ceil((glm::vec2)windSize / glm::vec2(m_BloomComputeWorkGroupSize));
			//voxelData.Bind();
			screenShader.Bind();
			screenShader.Dispatch(dispatchSize);

			bloomUBO.BufferBase(0);
			RenderBloom();

			finalImage.BindImage(0);
			scrTexture.Bind(1); // use the color attachment texture as the texture of the quad plane	
			bloomBuffers[2].Bind(2);
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


				if (Keyboard::getKey(Keyboard::Key::P)) data->maxBounces += 1;
				if (Keyboard::getKey(Keyboard::Key::L)) data->maxBounces -= 1;

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

		uint32_t addLight(const glm::vec3& position, const uint32_t& color) {
			uint32_t light = data->numLights;
			if (data->numLights <= NUM_LIGHTS) {
				lighting[data->numLights].vector = position;
				lighting[data->numLights].color = color;
				data->numLights++;
			}
			return light;
		}

		void removeLight(const glm::vec3& position) {
			for (uint32_t i = 0u; i < data->numLights; i++)
				if (lighting[i].vector == position) {
					data->numLights--;
					lighting[i] = lighting[data->numLights];
					break;
				}
		}

		struct BloomSettings
		{
			float Threshold = 1.f;
			float Knee = 0.1f;
		}bloomSettings;

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
			scrTexture.SetName("scrTexture");
			finalImage.Create(scrProps);
			finalImage.SetName("finalImage");

			bloomTexSize = glm::ivec2(scrProps.Width, scrProps.Height) / 2;
			bloomTexSize += glm::ivec2(m_BloomComputeWorkGroupSize - bloomTexSize.x % m_BloomComputeWorkGroupSize, m_BloomComputeWorkGroupSize - bloomTexSize.y % m_BloomComputeWorkGroupSize);
			mips = scrTexture.GetMipLevelCount() - 4;
			scrProps.mips = mips;

			scrProps.SetSize(bloomTexSize);
			for (int i = 0; i < 3; i++) {
				bloomBuffers[i].Create(scrProps);
				bloomBuffers[i].SetName("bloomBuffers[" + std::to_string(i) + "]");
			}
		}

		void DestroyScreen() {
			scrTexture.Destroy();
			finalImage.Destroy();

			for (int i = 0; i < 3; i++)
				bloomBuffers[i].Destroy();
		}

		void AddMesh() {

		}
	private:


	};
}