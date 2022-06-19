#pragma once
//#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_INTRINSICS 
#include <wc/pch.hpp>
#include <wc/Model/Mesh.hpp>

namespace wc {

	class Application : public Engine {
	private:
		Clock deltaTimer;
		float deltaTime = 0.f;

		// FrameBuffer stuff
		gl::ComputeShader screenShader;
		gl::Texture scrTexture;
		// Composite buffer
		gl::Texture finalImage;
		gl::ComputeShader compositeShader;

		gl::Texture bloomBuffers[3];

		struct SceneData
		{
			alignas(16) glm::vec3 lower_left_corner = glm::vec3(0.f);
			alignas(16) glm::vec3 horizontal = glm::vec3(0.f);
			alignas(16) glm::vec3 vertical = glm::vec3(0.f);
			alignas(16) glm::vec3 cameraPos = glm::vec3(0.f);
			uint32_t IndexCount = 0;
			uint32_t numLights = 0;
			uint32_t maxBounces = 1; // @TODO: remove
		}sceneData;

		gl::UniformBuffer sceneDataBuffer;
		gl::ShaderStorageBuffer vertexBuffer;
		gl::ShaderStorageBuffer indexBuffer;

		gl::Texture u_Albedo;
		gl::Texture u_MaterialInfo;

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
		}bloomSettings;

		Camera camera;
#define NUM_LIGHTS 16

		gl::UniformBuffer lights;
		struct Light {
			glm::vec3 vector = glm::vec3(0.f);
			uint32_t color = 0;
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

		glm::ivec2 bloomTexSize = glm::ivec2(0);
		uint32_t m_BloomComputeWorkGroupSize = 4;
		uint32_t mips = 1;
		void RenderBloom()
		{
			bloomShader.use();
			BloomUBOSettings settings;
			settings.Params = glm::vec4(bloomSettings.Threshold, bloomSettings.Threshold - bloomSettings.Knee, bloomSettings.Knee * 2.f, 0.25f / bloomSettings.Knee);
			bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			bloomBuffers[0].BindTextureImage(0, GL_WRITE_ONLY);
			scrTexture.Bind(1);
			bloomShader.Dispatch(glm::ceil(glm::vec2(bloomTexSize) / glm::vec2(m_BloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			bloomBuffers[0].Bind(1);
			for (int currentMip = 1; currentMip < mips; currentMip++)
			{
				glm::vec2 mipSize = bloomBuffers[0].GetMipSize(currentMip);
				mipSize = glm::ceil(mipSize / glm::vec2(m_BloomComputeWorkGroupSize));
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
			
			bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(mips - 1) / glm::vec2(m_BloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			
			bloomBuffers[2].Bind(2);
			settings.Mode = (int)BloomMode::Upsample;
			for (int currentMip = mips - 2; currentMip >= 0; currentMip--)
			{	
				settings.LOD = currentMip;
				bloomUBO.SetData(0, sizeof(BloomUBOSettings), &settings);
			
				bloomBuffers[2].BindTextureImage(0, GL_WRITE_ONLY, currentMip);
			
				bloomShader.Dispatch(glm::ceil((glm::vec2)bloomBuffers[2].GetMipSize(currentMip) / glm::vec2(m_BloomComputeWorkGroupSize)));
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);				
			}
		}

		int alignment = 0;
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
				glViewport(0, 0, windSize.x, windSize.y);
				Renderer2D::m_Data.windowSize = windSize;

				scrTexture.Destroy();
				finalImage.Destroy();

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


			if (Keyboard::getKey(Keyboard::Key::P)) sceneData.maxBounces += 1;
			if (Keyboard::getKey(Keyboard::Key::L)) sceneData.maxBounces -= 1;

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

			camera.Update();
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
			finalImage.Create(scrProps);

			bloomTexSize = glm::ivec2(scrProps.Width, scrProps.Height) / 2;
			bloomTexSize += glm::ivec2(m_BloomComputeWorkGroupSize - bloomTexSize.x % m_BloomComputeWorkGroupSize, m_BloomComputeWorkGroupSize - bloomTexSize.y % m_BloomComputeWorkGroupSize);
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
			window.Create({ 1024, 1024 }, "RTX");
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) WC_ERROR("Failed to initialize GLAD");
			// OpenGL state
			Renderer::enableDebuging();
			// ------------
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			screenShader.Create("shaders/screenShader.comp");
			bloomShader.Create("shaders/bloomShader.comp");
			compositeShader.Create("shaders/composite.comp");			

			font.Load("assets/font/Minecraft.ttf", 128);
			sceneDataBuffer.Create(nullptr, sizeof(SceneData), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			sceneDataBuffer.BufferBase(0);

			lights.Create(nullptr, sizeof(lighting), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			lights.BufferBase(1);

			bloomUBO.Create(nullptr, sizeof(BloomUBOSettings), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			bloomUBO.BufferBase(4);

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

			vertexBuffer.Create(vertices, sizeof(vertices), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
			vertexBuffer.BufferBase(2);

			uint32_t indices[12] = {
				0, 1, 2, 2, 3, 0,
				4, 5, 6, 6, 7, 4
			};

			indexBuffer.Create(indices, sizeof(indices));
			indexBuffer.BufferBase(3);

			sceneData.IndexCount = ARRAYSIZE(indices);

			Renderer2D::Init();
			Renderer2D::m_Data.windowSize = window.GetSize();

			addLight(glm::vec3(0.f, 1.f, 0.f), convertColor(glm::vec4(1.f, 1.f, 1.f, 0.f)));
			gl::load("assets/test/albedo.png", u_Albedo);
			gl::load("assets/test/pbr_output.png", u_MaterialInfo);

			CreateScreen();
		}
		//----------------------------------------------------------------------------------------------------------------------
		float angle = 0.f;
		float intensity = 0.f;
		void OnUpdate() override {
			deltaTime = deltaTimer.restart();
			auto windsize = window.GetSize();

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

			scrTexture.BindTextureImage(0, GL_WRITE_ONLY);
			u_Albedo.Bind(1);
			u_MaterialInfo.Bind(2);
			
			screenShader.use();
			screenShader.Dispatch(glm::ceil((glm::vec2)windsize / glm::vec2(m_BloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			RenderBloom();
			
			finalImage.BindTextureImage(0, GL_WRITE_ONLY);
			scrTexture.Bind(1); // use the color attachment texture as the texture of the quad plane	
			bloomBuffers[2].Bind(2);
			compositeShader.use();
			compositeShader.Dispatch(glm::ceil((glm::vec2)windsize / glm::vec2(m_BloomComputeWorkGroupSize)));
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			Renderer2D::DrawQuad({ 0,0 }, windsize, finalImage);

			float scale = 0.4f;
			Renderer2D::DrawText("FPS: " + std::to_string((int)(1.f / deltaTime)), font, { 25.f, 5.f * scale * 10.f }, scale);
			Renderer2D::DrawText("X: " + std::to_string(camera.Position.x) + " Y: " + std::to_string(camera.Position.y) + " Z: " + std::to_string(camera.Position.z), font, { 25.f, 15.f * scale * 10.f }, scale);
			Renderer2D::DrawText("Pitch: " + std::to_string(camera.Pitch) + " Yaw: " + std::to_string(camera.Yaw), font, {25.f, 25.f * scale * 10.f}, scale);
			Renderer2D::DrawText("Max bounces: " + std::to_string(sceneData.maxBounces), font, { 25.f, 35.f * scale * 10.f }, scale);
			
			Renderer2D::Flush();
			window.display();
		}
		//----------------------------------------------------------------------------------------------------------------------
		void OnDelete() override {
			wc::window.Destroy();
		}
		//----------------------------------------------------------------------------------------------------------------------
	public:
		Application() {}
	};
}