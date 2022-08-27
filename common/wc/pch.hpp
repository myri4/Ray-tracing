#pragma once
#undef min

#include <thread>
#include <memory>

#include <fstream>
#include <string>
#include <vector>
#include <ostream>
#include <unordered_map>
#include <sstream>
#include <array>

#define GLM_FORCE_PURE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>

//Custom libraries
#include <gl/Buffer.hpp>
#include <gl/Shaders.hpp>
#include <gl/Texture.hpp>
#include <gl/VertexArray.hpp>
#include <gl/Fence.h>

#include <Maths/Camera.hpp>

//Util
#include <Utils/Log.hpp>
#include <Utils/Time.hpp>
#include <Utils/State.hpp>
#include <Utils/Window.hpp>

// GUI
#include <GUI/Renderer2D.hpp>

// Game idea
// Space ship game where you go around planets, gather resources then go and fight people an invade their spaceships, until you invde all the galaxy
// Type: strategy, fps