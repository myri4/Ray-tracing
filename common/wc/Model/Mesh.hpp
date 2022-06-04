#pragma once

#include <gl/Buffer.hpp>
#include <gl/Texture.hpp>
#include <gl/VertexArray.hpp>

#include <vector>
#include <Renderer/Renderer.hpp>

#define MAX_BONE_INFLUENCE 4
#define MAX_BONE_WEIGHTS MAX_BONE_INFLUENCE * 25

struct Vertex {
    glm::vec2 texCoord = glm::vec2(0.f);
    alignas(16) glm::vec3 position = glm::vec3(0.f);
};