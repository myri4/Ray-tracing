#pragma once

#include <glad/glad.h>

namespace gl {

    template<typename T>
    class Buffer {
    protected:
        GLuint m_RendererID = 0;
    public:
        Buffer() = default;

        void Create(const GLsizeiptr& flags, const GLenum& size = 1, const void* data = nullptr) {
            glCreateBuffers(1, &m_RendererID);
            glNamedBufferStorage(m_RendererID, size * sizeof(T), data, flags);
        }

        inline void Destroy() { glDeleteBuffers(1, &m_RendererID); }

        void SetData(const GLsizeiptr& size, const void* data, const GLintptr& offset = 0) {
            glNamedBufferSubData(m_RendererID, offset * sizeof(T), size * sizeof(T), data);
        }

        T* Map(const GLenum& access, const uint32_t& length = 1, const uint32_t& offset = 0) {
            return (T*)glMapNamedBufferRange(m_RendererID, offset * sizeof(T), length * sizeof(T), access);
        }

        bool UnMap() {
            return glUnmapNamedBuffer(m_RendererID);
        }

        inline operator GLuint& () { return m_RendererID; }
        inline operator const GLuint& () const { return m_RendererID; }
    };

    template<GLenum target, typename T>
    struct IndexedBuffer : public Buffer<T> {
        IndexedBuffer() = default;
        IndexedBuffer(const void* data, const GLsizeiptr& size, const GLenum& flags) { this->Create(data, size, flags); }

        void BufferRange(const GLuint& index, const GLintptr& offset, const GLsizeiptr& size) { glBindBufferRange(target, index, this->m_RendererID, offset, size); }
        void BufferBase(const GLuint& index) { glBindBufferBase(target, index, this->m_RendererID); }
        void Bind() { glBindBuffer(target, this->m_RendererID); }
    };

    template<typename T>
    using UniformBuffer = IndexedBuffer<GL_UNIFORM_BUFFER, T>;
    template<typename T>
    using ShaderStorageBuffer = IndexedBuffer<GL_SHADER_STORAGE_BUFFER, T>;
}