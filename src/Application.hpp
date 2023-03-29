#pragma once
#define GLM_FORCE_INTRINSICS 
#include "Scene/Scene.h"

namespace wc {	

	class Application : public Engine {
	private:
		Clock deltaTimer;
		float deltaTime = 0.f;
		bool renderGUI = true;

		Scene scene;
		gl::Fence fence;

		Font font;
		//----------------------------------------------------------------------------------------------------------------------
		bool IsEngineOK() override { return window.isOpen(); }
		//----------------------------------------------------------------------------------------------------------------------
		void OnInput() override {
			auto windSize = window.GetSize();
			if (resized) {
				glViewport(0, 0, windSize.x, windSize.y);
				Renderer2D::m_Data.windowSize = windSize;

				scene.DestroyScreen();
				scene.CreateScreen();
			}

			if (window.hasFocus()) {
				if (Keyboard::getKey(Keyboard::Key::F1)) renderGUI = !renderGUI;
				scene.OnInput(deltaTime);
			}			
		}

		void OnCreate() override {
			window.Create({ 1280, 720 }, "RTX");
			if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) WC_ERROR("Failed to initialize GLAD");
			// OpenGL state
			Renderer::enableDebuging();
			// ------------
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			font.Load("assets/font/Minecraft.ttf", 128);

			Renderer2D::Init();
			Renderer2D::m_Data.windowSize = window.GetSize();

			scene.Create();
		}
		//----------------------------------------------------------------------------------------------------------------------
		void OnUpdate() override {
			deltaTime = deltaTimer.restart();
			auto windsize = window.GetSize();
			fence.lock();
			scene.Update(deltaTime);
			fence.wait();
						
			if (renderGUI) {
				float scale = 0.4f;
				Renderer2D::DrawText("FPS: " + std::to_string((int)(1.f / deltaTime)) + " Frametime: " + std::to_string(deltaTime), font, {25.f, 5.f * scale * 10.f}, scale);
				Renderer2D::DrawText("X: " + std::to_string(scene.camera.Position.x) + " Y: " + std::to_string(scene.camera.Position.y) + " Z: " + std::to_string(scene.camera.Position.z), font, { 25.f, 15.f * scale * 10.f }, scale);
				Renderer2D::DrawText("Pitch: " + std::to_string(scene.camera.Pitch) + " Yaw: " + std::to_string(scene.camera.Yaw), font, { 25.f, 25.f * scale * 10.f }, scale);
			}
			
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