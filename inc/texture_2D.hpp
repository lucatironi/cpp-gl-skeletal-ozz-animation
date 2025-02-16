#pragma once

#include "assimp/texture.h"
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <string>

struct TextureParams
{
    GLuint wrapS = GL_CLAMP_TO_EDGE;
    GLuint wrapT = GL_CLAMP_TO_EDGE;
    GLuint filterMin = GL_NEAREST_MIPMAP_LINEAR;
    GLuint filterMax = GL_NEAREST;
};

class Texture2D
{
public:
    GLuint ID;
    GLuint Width, Height;
    GLuint InternalFormat, ImageFormat;
    GLuint WrapS, WrapT;
    GLuint FilterMin, FilterMax;

    Texture2D() = default;

    Texture2D(const std::string& path, const TextureParams& params = {})
        : WrapS(params.wrapS), WrapT(params.wrapT),
          FilterMin(params.filterMin), FilterMax(params.filterMax)
    {
        glGenTextures(1, &ID);
        unsigned char* image = loadImageFromPath(path.c_str());
        if (image)
        {
            generate(image);
            stbi_image_free(image); // Free after texture generation
        }
        else
        {
            std::cerr << "ERROR::TEXTURE2D: Failed to load texture: " << path << std::endl;
        }
    }

    Texture2D(unsigned char* data, unsigned int w, unsigned int h, const TextureParams& params = {})
        : WrapS(params.wrapS), WrapT(params.wrapT),
          FilterMin(params.filterMin), FilterMax(params.filterMax)
    {
        glGenTextures(1, &ID);
        unsigned char* image = loadImageFromData(data, w, h);
        if (image)
        {
            generate(image);
            stbi_image_free(image); // Free after texture generation
        }
        else
        {
            std::cerr << "ERROR::TEXTURE2D: Failed to load texture from data" << std::endl;
        }
    }

    void Bind() const
    {
        glBindTexture(GL_TEXTURE_2D, ID);
    }

private:
    // Helper function to load the image data
    unsigned char* loadImageFromPath(const char* path)
    {
        int width, height, channels;
        unsigned char* image = stbi_load(path, &width, &height, &channels, 0);
        if (image)
            setParams(width, height, channels);
        return image;
    }

    unsigned char* loadImageFromData(unsigned char* data, unsigned int w, unsigned int h)
    {
        int size, width, height, channels;
        size = (h == 0) ? w : w * h;
        unsigned char* image = stbi_load_from_memory(data, size, &width, &height, &channels, 0);
        if (image)
            setParams(width, height, channels);
        return image;
    }

    void generate(unsigned char* data)
    {
        // Create Texture
        glBindTexture(GL_TEXTURE_2D, ID);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, ImageFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        // Set Texture wrap and filter modes
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, WrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, WrapT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FilterMin);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FilterMax);
        // Unbind texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void setParams(int width, int height, int channels)
    {
        Width = width;
        Height = height;

        if (channels == 3)
        {
            InternalFormat = GL_RGB;
            ImageFormat = GL_RGB;
        }
        else if (channels == 4)
        {
            InternalFormat = GL_RGBA;
            ImageFormat = GL_RGBA;
        }
    }
};