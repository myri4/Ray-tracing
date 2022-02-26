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

struct ind {
    alignas(16) uint32_t index;

    ind() {}
    ind(const uint32_t& Index) : index(Index) {}
};

namespace wc {
class Mesh {
public:
    // constructor
    Mesh() {}

    void Create(const std::vector<Vertex>& vertices, const std::vector<ind>& indices) {
        m_VertexBuffer.Create(vertices.data(), vertices.size() * sizeof(Vertex), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        m_VertexBuffer.BufferBase(2);

        m_IndexBuffer.Create(indices.data(), indices.size() * sizeof(ind), GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_WRITE_BIT);
        m_IndexBuffer.BufferBase(3);

        indexSize = indices.size();
    }

    void Bind() {
        m_VertexBuffer.Bind();
        m_IndexBuffer.Bind();
    }

    uint32_t indexSize = 0;
private:
    // render data 
    gl::UniformBuffer m_IndexBuffer;
    gl::UniformBuffer m_VertexBuffer;
};
}