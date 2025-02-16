#pragma once

#include "shader.hpp"
#include "texture_2D.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::ivec4 BoneIDs;
    glm::vec4 BoneWeights;
};

struct Texture
{
    Texture2D texture;
    std::string type;
    std::string path;
};

class Mesh
{
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices, const std::vector<Texture>& textures)
        : vertices(vertices), indices(indices), textures(textures), VAO(0), VBO(0), EBO(0)
    {
        setupBuffers();
    }

    void Draw(const Shader& shader) const
    {
        shader.Use();
        bindTextures(shader);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    void AddTexture(Texture texture)
    {
        textures.push_back(texture);
    }

    std::vector<Texture> GetTextures() const { return textures; }

    void Debug() const
    {
        std::cout << "Vertices: " << vertices.size() << ", Indices: " << indices.size() << ", Textures: " << textures.size() << std::endl;
        for (const auto& texture : textures)
            std::cout << "Texture: " << texture.path << ", type: " << texture.type << std::endl;
    }

private:
    GLuint VAO, VBO, EBO; // Vertex Array Object, Vertex Buffer Object, Element Buffer Object
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    std::vector<Texture> textures;

    void setupBuffers()
    {
        // Generate buffers and arrays
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        // Bind VAO
        glBindVertexArray(VAO);

        // Vertex Buffer Object
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        // Element Buffer Object
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        // Vertex attributes
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, BoneIDs));

        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, BoneWeights));

        // Unbind VAO to prevent accidental modifications
        glBindVertexArray(0);
    }

    void bindTextures(const Shader& shader) const
    {
        GLuint diffuseCount = 0;
        GLuint specularCount = 0;
        GLuint normalCount = 0;

        for (size_t i = 0; i < textures.size(); ++i)
        {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
            // Determine the uniform name based on the type
            std::string uniformName;
            if (textures[i].type == "texture_diffuse")
                uniformName = "texture_diffuse" + std::to_string(diffuseCount++);
            else if (textures[i].type == "texture_specular")
                uniformName = "texture_specular" + std::to_string(specularCount++);
            else if (textures[i].type == "texture_normal")
                uniformName = "texture_normal" + std::to_string(normalCount++);
            else
                uniformName = textures[i].type; // Fallback for custom types

            shader.SetInt(uniformName, static_cast<int>(i));
            textures[i].texture.Bind();
        }
    }
};