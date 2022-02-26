#pragma once
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_INTRINSICS 
#include "wc/pch.hpp"
#include <glm/matrix.hpp>
#include "GUI/Textbox.hpp"
#include "GUI/Button.hpp"
#include <wc/Model/Model.hpp>

namespace wc {

	class Application : public Engine {
	private:
		Clock deltaTimer;
		float deltaTime = 0.f;

		// FrameBuffer stuff
		gl::VertexBuffer scrQuad;
		gl::VertexArray scrQuadA;
		gl::Shader screenShader;
		gl::FrameBuffer screen;
		gl::Texture scrTexture;
		// Composite buffer
		gl::VertexBuffer compositeQuad;
		gl::VertexArray compositeQuadA;
		gl::Shader compositeShader;

		gl::Texture bloomBuffers[3];

		struct SceneData
		{
			glm::vec2 windowSize;
			alignas(16) glm::vec3 lower_left_corner;
			alignas(16) glm::vec3 horizontal;
			alignas(16) glm::vec3 vertical;
			alignas(16) glm::vec3 cameraPos;
			uint32_t numIndices = 0;
			uint32_t numLights = 0;
			uint32_t maxBounces = 1; // @TODO: remove
		}sceneData;

		glm::vec3 CalculateNormal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
			return glm::normalize(glm::cross(c - a, b - a));
		}

		gl::UniformBuffer sceneDataBuffer;
		gl::UniformBuffer vertexBuffer;
		gl::UniformBuffer indexBuffer;
		gl::UniformBuffer lights;

		gl::Texture u_Albedo;
		gl::Texture u_MaterialInfo;
		gl::Texture u_Normal;

		gl::ComputeShader bloomShader;
		gl::UniformBuffer bloomUBO;

		enum class BloomMode
		{
			Prefilter,
			Downsample,
			UpsampleFirst,
			Upsample
		};

		struct BloomUBOSettings {
			glm::vec4 Params = glm::vec4(1.f); // (x) threshold, (y) threshold - knee, (z) knee * 2, (w) 0.25 / knee
			float LOD = 0.f;
			int Mode = (int)BloomMode::Prefilter;
		};

		struct BloomSettings
		{
			float Threshold = 1.f;
			float Knee = 0.1f;
			float Intensity = 0.1f;
		}bloomSettings;

		Camera camera;
#define NUM_LIGHTS 16
		struct Light {
			uint32_t color;
			alignas(16) glm::vec3 vector;
		} lighting[NUM_LIGHTS];
		bool lightUpdate = false;

		uint32_t addLight(const glm::vec3& position, const uint32_t& color) {
			uint32_t light = sceneData.numLights;
			if (sceneData.numLights <= NUM_LIGHTS) {
				lighting[sceneData.numLights].vector = position;
				lighting[sceneData.numLights].color = color;
				sceneData.numLights++;
				lightUpdate = true;
			}
			return light;
		}

		void removeLight(const glm::vec3& position) {
			for (uint32_t i = 0u; i < NUM_LIGHTS; i++)
				if (lighting[i].vector == position) {
					sceneData.numLights--;
					lighting[i] = lighting[sceneData.numLights];
					lightUpdate = true;
					break;
				}
		}

		glm::ivec2 bloomTexSize;
		int32_t mBloomComputeWorkGroupSize = 16;
		int32_t mips = 1;
		void RenderBloom()
		{
			bloomShader.use();
			BloomUBOSettings settings;
			settings.Params = glm::vec4(bloomSettings.Threshold, bloomSettings.Threshold - bloomSettings.Knee, bloomSettings.Knee * 2.f, 0.25f / bloomSettings.Knee);
			bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			bloomBuffers[0].BindTextureImage(0, GL_WRITE_ONLY);
			scrTexture.Bind(1);
			bloomShader.Dispatch(glm::ceil(glm::vec2(bloomTexSize) / glm::vec2(mBloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			bloomBuffers[0].Bind(1);
			for (int currentMip = 1; currentMip < mips; currentMip++)
			{
				glm::vec2 mipSize = bloomBuffers[0].GetMipSize(currentMip);
				mipSize = glm::ceil(mipSize / glm::vec2(mBloomComputeWorkGroupSize));
				settings.Mode = (int)BloomMode::Downsample;
			
				// Ping 
				settings.LOD = currentMip - 1;
				bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			
				bloomBuffers[1].BindTextureImage(0, GL_WRITE_ONLY, currentMip);
				bloomShader.Dispatch(mipSize);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				// Pong 
				settings.LOD = currentMip;
				bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
				
				bloomBuffers[0].BindTextureImage(0, GL_WRITE_ONLY, currentMip);			
				bloomBuffers[1].Bind(1);			
				bloomShader.Dispatch(mipSize);
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}

			// First Upsample		
			settings.LOD = mips - 2;
			settings.Mode = (int)BloomMode::UpsampleFirst;
			bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			
			bloomBuffers[2].BindTextureImage(0, GL_WRITE_ONLY, mips - 1);
			bloomBuffers[0].Bind(1);
			
			bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(mips - 1) / glm::vec2(mBloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			
			bloomBuffers[2].Bind(2);
			settings.Mode = (int)BloomMode::Upsample;
			for (int currentMip = mips - 2; currentMip >= 0; currentMip--)
			{	
				settings.LOD = currentMip;
				bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			
				bloomBuffers[2].BindTextureImage(0, GL_WRITE_ONLY, currentMip);
			
				bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(currentMip) / glm::vec2(mBloomComputeWorkGroupSize)));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);				
			}
		}

		Font font;
		float MouseSensitivity = 5.f;
		//----------------------------------------------------------------------------------------------------------------------
		bool IsEngineOK() override {
			return window.isOpen();
		}
		//----------------------------------------------------------------------------------------------------------------------
		void OnInput() override {
			auto windSize = window.GetSize();
			if (resized) {
				Renderer2D::SetProjection(Renderer2D::Get2DProj(windSize));	

				screen.Destroy();
				scrTexture.Destroy();
				for (int i = 0; i < 3; i++) 
					bloomBuffers[i].Destroy();
				CreateScreen();
			}
			if (window.hasFocus()) {

			float yaw = glm::radians(camera.Yaw);
			float yaw90 = glm::radians(camera.Yaw + 90.f);
			const float MovementSpeed = 4.f * deltaTime;
			if (Keyboard::isKeyPressed(Keyboard::Key::W)) { // Front
				camera.Position.x += glm::cos(yaw) * MovementSpeed;
				camera.Position.z += glm::sin(yaw) * MovementSpeed;
			}

			else if (Keyboard::isKeyPressed(Keyboard::Key::S)) { // Back
				camera.Position.x -= glm::cos(yaw) * MovementSpeed;
				camera.Position.z -= glm::sin(yaw) * MovementSpeed;
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

			if (Keyboard::isKeyPressed(Keyboard::Key::J)) {
				screenShader.Destroy();
				screenShader.Create("shaderpacks/default/screenShader.glsl");
			}

			if (Keyboard::getKey(Keyboard::Key::P)) bloomSettings.Knee += 0.01f;
			if (Keyboard::getKey(Keyboard::Key::L)) bloomSettings.Knee -= 0.01f;

			if (Keyboard::isKeyPressed(Keyboard::Key::Y)) lighting[1].vector = camera.Position;
			if (Keyboard::isKeyPressed(Keyboard::Key::C)) { camera.FOV = 10.f; MouseSensitivity = 18; }
			else
			{
				MouseSensitivity = 5.f;
				camera.FOV = 90.f;
			}

			if (Keyboard::getKey(Keyboard::Key::Up)) {
				intensity += 0.25f;
				WC_INFO(intensity);
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

			camera.UpdateCameraAngles();
			Mouse::SetMousePosition(xt, yt);
			}
		}
		//----------------------------------------------------------------------------------------------------------------------

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

			screen.Create(scrProps.Width, scrProps.Height);
			screen.addTexture(scrTexture);

			bloomTexSize = glm::ivec2(scrProps.Width, scrProps.Height) / 2;
			bloomTexSize += glm::ivec2(mBloomComputeWorkGroupSize - bloomTexSize.x % mBloomComputeWorkGroupSize,
				mBloomComputeWorkGroupSize - bloomTexSize.y % mBloomComputeWorkGroupSize);
			mips = scrTexture.GetMipLevelCount() - 4;
			gl::TextureProps bloomProps;
			bloomProps.internalFormat = GL_RGBA32F;
			bloomProps.mips = mips;
			bloomProps.min_filter = GL_LINEAR_MIPMAP_LINEAR;
			bloomProps.mag_filter = GL_LINEAR;
			bloomProps.wrap_s = GL_CLAMP_TO_EDGE;
			bloomProps.wrap_t = GL_CLAMP_TO_EDGE;

			bloomProps.SetSize(bloomTexSize);
			for (int i = 0; i < 3; i++) {
				bloomBuffers[i].Create(bloomProps);
				bloomBuffers[i].GenerateMipMap();
			}
		}

		void OnCreate() override {
			window.Create("config/window.lua", "Real-time ray tracing");
			// OpenGL state
			Renderer::enableDebuging();
			// ------------
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			screenShader.Create("shaderpacks/default/screenShader.glsl");
			compositeShader.Create("shaderpacks/default/composite.glsl");
			bloomShader.Create("shaderpacks/default/bloomShader.glsl");
			{
				float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
					// positions 
					-1.0f, -1.0f,
					-1.0f,  1.0f,
					 1.0f, -1.0f,

					 1.0f, -1.0f,
					-1.0f,  1.0f,
					 1.0f,  1.0f,
				};

				scrQuad.Create(quadVertices, sizeof(quadVertices), 0);
				scrQuadA.Create();
				scrQuadA.VertexAttribPointer(0, 2, 0);
				scrQuadA.AddVertexBuffer(scrQuad, sizeof(float) * 2);
			}

			{
				float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
				// positions   // texCoords
				-1.0f, -1.0f,  0.0f, 1.0f,
				-1.0f,  1.0f,  0.0f, 0.0f,
				 1.0f, -1.0f,  1.0f, 1.0f,

				 1.0f, -1.0f,  1.0f, 1.0f,
				-1.0f,  1.0f,  0.0f, 0.0f,
				 1.0f,  1.0f,  1.0f, 0.0f,
				};

				compositeQuad.Create(quadVertices, sizeof(quadVertices), 0);
				compositeQuadA.Create();
				compositeQuadA.VertexAttribPointer(0, 2, 0);
				compositeQuadA.VertexAttribPointer(1, 2, 2 * sizeof(float));
				compositeQuadA.AddVertexBuffer(compositeQuad, sizeof(float) * 4);
			}

			font.Load("assets/font/Minecraft.ttf", 128);
			sceneDataBuffer.Create(nullptr, sizeof(SceneData), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			sceneDataBuffer.BufferBase(0);
			sceneDataBuffer.Bind();

			lights.Create(nullptr, sizeof(lighting), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			lights.BufferBase(1);
			lights.Bind();

			BloomUBOSettings st;
			st.Params = glm::vec4(bloomSettings.Threshold, bloomSettings.Threshold - bloomSettings.Knee, bloomSettings.Knee * 2.f, 0.25f / bloomSettings.Knee);
			bloomUBO.Create(&st, sizeof(BloomUBOSettings), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			bloomUBO.BufferBase(4);
			bloomUBO.Bind();

			Vertex vertices[8];
			vertices[0].position = glm::vec3(1, 0.5f, 0);
			vertices[1].position = glm::vec3(0, 0.5f, 0);
			vertices[2].position = glm::vec3(0, 0.5f, 1);
			vertices[3].position = glm::vec3(1, 0.5f, 1);

			vertices[0].texCoord = glm::vec2(0.f, 0.f);
			vertices[1].texCoord = glm::vec2(1.f, 0.f);
			vertices[2].texCoord = glm::vec2(1.f, 1.f);
			vertices[3].texCoord = glm::vec2(0.f, 1.f);

			vertices[4].position = glm::vec3(0, 1, 1);
			vertices[5].position = glm::vec3(0, 1, 0);
			vertices[6].position = glm::vec3(0, 0, 0);
			vertices[7].position = glm::vec3(0, 0, 1);

			vertices[4].texCoord = glm::vec2(0.f, 0.f);
			vertices[5].texCoord = glm::vec2(1.f, 0.f);
			vertices[6].texCoord = glm::vec2(1.f, 1.f);
			vertices[7].texCoord = glm::vec2(0.f, 1.f);

			vertexBuffer.Create(vertices, sizeof(vertices));
			vertexBuffer.BufferBase(2);
			vertexBuffer.Bind();

			ind indices[12] = {
				0, 1, 2, 2, 3, 0,
				4, 5, 6, 6, 7, 4
			};

			indexBuffer.Create(indices, sizeof(indices));
			indexBuffer.BufferBase(3);
			indexBuffer.Bind();

			sceneData.numIndices = ARRAYSIZE(indices);

			Renderer2D::Init();
			Renderer2D::SetProjection(Renderer2D::Get2DProj(window.GetSize()));

			addLight(glm::vec3(0.f, 1.f, 0.f), convertColor(glm::vec4(1.f, 1.f, 1.f, 0.f)));
			gl::load("assets/test/albedo.png", u_Albedo);
			gl::load("assets/test/pbr_output.png", u_MaterialInfo);
			gl::load("assets/test/normal.png", u_Normal);

			CreateScreen();
		}
		//----------------------------------------------------------------------------------------------------------------------
		float angle = 0.f;
		float intensity = 0.f;
		void OnUpdate() override {
			deltaTime = deltaTimer.restart();
			auto windsize = window.GetSize();

			sceneData.windowSize = window.GetSize();
			sceneData.lower_left_corner = camera.lower_left_corner;
			sceneData.cameraPos = camera.Position;
			sceneData.horizontal = camera.horizontal;
			sceneData.vertical = camera.vertical;
			angle += deltaTime * 6.f;
			angle = glm::mod(angle, 360.f);
			lighting[0].vector = -glm::vec3(glm::vec4(1.f, 0.f, 0.f, 0.f) * glm::rotate(glm::mat4(1.f), glm::radians(angle), glm::vec3(0.f, 0.f, 1.f)));
			lighting[1].color = convertColor(glm::vec4(0.f, 1.f, 1.f, intensity / 255.f));

			lights.SetData(0, sizeof(lighting), lighting);

			sceneDataBuffer.SetData(0, sizeof(sceneData), &sceneData);

			u_Albedo.Bind(0);
			u_MaterialInfo.Bind(1);
			u_Normal.Bind(2);

			screen.Bind();

			screenShader.use();
			scrQuadA.Bind();	
			Renderer::DrawArrays(6);
			
			screen.unbind();

			float scale = 0.4f;

			RenderBloom();

			compositeShader.use();
			compositeQuadA.Bind();
			scrTexture.Bind(); // use the color attachment texture as the texture of the quad plane			
			bloomBuffers[2].Bind(1);

			Renderer::DrawArrays(6);
			Renderer2D::DrawText("FPS: " + std::to_string((int)(1.f / deltaTime)) + " Frametime: " + std::to_string(deltaTime * 1000), font, { 25.f, 5.f * scale * 10.f }, scale);
			Renderer2D::DrawText("X: " + std::to_string(camera.Position.x) + " Y: " + std::to_string(camera.Position.y) + " Z: " + std::to_string(camera.Position.z), font, { 25.f, 15.f * scale * 10.f }, scale);
			Renderer2D::DrawText("Pitch: " + std::to_string(camera.Pitch) + " Yaw: " + std::to_string(camera.Yaw), font, {25.f, 25.f * scale * 10.f}, scale);
			Renderer2D::DrawText("Max bounces: " + std::to_string(bloomSettings.Knee), font, { 25.f, 35.f * scale * 10.f }, scale);
			
			Renderer2D::Flush();
			Renderer2D::FlushLines();
			window.display();
		}
		//----------------------------------------------------------------------------------------------------------------------
		void OnDelete() override {}
		//----------------------------------------------------------------------------------------------------------------------
	public:
		Application() {}
	};
}