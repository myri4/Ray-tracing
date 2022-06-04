#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.hpp"

#include <unordered_map>
#include <Maths/AssimpGLMHelpers.hpp>

namespace wc {

class Model{
public:
	// model data 
	std::string directory;

	// constructor, expects a filepath to a 3D model.
	Model() {}

	void Create(const std::string& path) {
		// read file via ASSIMP
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices);
		// check for errors
		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
		{
			WC_ERROR("ASSIMP: {0}", importer.GetErrorString());
			return;
		}
		// retrieve the directory path of the filepath
		directory = path.substr(0, path.find_last_of('/'));

		// process ASSIMP's root node recursively
		processNode(scene->mRootNode, *scene);
	}
private:

	void processNode(const aiNode* node, const aiScene& scene) {
		// process each mesh located at the current node		
		// the node object only contains indices to index the actual objects in the scene. 
		// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
		for (uint32_t j = 0; j < node->mNumMeshes; j++) {
			auto& mesh = scene.mMeshes[node->mMeshes[j]];
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;
			vertices.reserve(mesh->mNumVertices);
			for (uint32_t i = 0; i < mesh->mNumVertices; i++)
			{
				Vertex vertex;
				vertex.position = wc::AssimpGLMHelpers::GetGLMVec(mesh->mVertices[i]);

				if (mesh->mTextureCoords[0])
				{
					glm::vec2 vec;
					vec.x = mesh->mTextureCoords[0][i].x;
					vec.y = mesh->mTextureCoords[0][i].y;
					vertex.texCoord = vec;
				}

				vertices.emplace_back(vertex);
			}
			indices.reserve(mesh->mNumFaces);
			for (uint32_t i = 0; i < mesh->mNumFaces; i++)
			{
				aiFace& face = mesh->mFaces[i];
				for (uint32_t j = 0; j < face.mNumIndices; j++) 
					indices.emplace_back(face.mIndices[j]);				
			}
			//modelMesh.Create(vertices, indices);
		}	
		
		// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
		for (uint32_t i = 0; i < node->mNumChildren; i++)
			processNode(node->mChildren[i], scene);
	}
};
}