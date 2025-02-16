#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

class Shader
{
public:
    GLuint ID;

    Shader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath = "")
    {
        std::string vertexCode, fragmentCode, geometryCode;

        try
        {
            // Read shader files
            vertexCode = readFile(vertexPath);
            fragmentCode = readFile(fragmentPath);
            if (!geometryPath.empty())
                geometryCode = readFile(geometryPath);

        }
        catch (const std::ifstream::failure& e)
        {
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
            return;
        }

        GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexCode);
        GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentCode);

        // Create shader program and link shaders
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);

        // If a geometry shader is provided, compile and attach it
        if (!geometryPath.empty())
        {
            GLuint geometry = compileShader(GL_GEOMETRY_SHADER, geometryCode);
            glAttachShader(ID, geometry);
            glDeleteShader(geometry);  // We can delete the geometry shader after linking
        }

        // Link the program and check for errors
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // Clean up shaders once they're linked
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    // Use the shader program
    void Use() const
    {
        glUseProgram(ID);
    }

    // Setters for uniforms
    void SetBool(const std::string& name, bool value) const { setUniform(name, static_cast<int>(value)); }
    void SetInt(const std::string& name, int value) const { setUniform(name, value); }
    void SetFloat(const std::string& name, float value) const { setUniform(name, value); }
    void SetVec2(const std::string& name, const glm::vec2& value) const { setUniform(name, value); }
    void SetVec3(const std::string& name, const glm::vec3& value) const { setUniform(name, value); }
    void SetVec4(const std::string& name, const glm::vec4& value) const { setUniform(name, value); }
    void SetMat2(const std::string& name, const glm::mat2& mat) const { setUniform(name, mat); }
    void SetMat3(const std::string& name, const glm::mat3& mat) const { setUniform(name, mat); }
    void SetMat4(const std::string& name, const glm::mat4& mat) const { setUniform(name, mat); }
    void SetMat4v(const std::string& name, std::vector<glm::mat4>& matrices) const { setUniform(name, matrices); }

private:
    mutable std::unordered_map<std::string, GLint> uniformLocations;

    // Function to read file into a string
    std::string readFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
            throw std::ifstream::failure("Failed to open file: " + path);

        std::stringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    // Function to compile a shader
    GLuint compileShader(GLenum type, const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* code = source.c_str();
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
        checkCompileErrors(shader, type == GL_VERTEX_SHADER ? "VERTEX" : type == GL_FRAGMENT_SHADER ? "FRAGMENT" : "GEOMETRY");
        return shader;
    }

    // Function to set uniforms (with caching of locations)
    template<typename T>
    void setUniform(const std::string& name, const T& value) const
    {
        GLint location = getUniformLocation(name);
        setUniformImpl(location, value);
    }

    // Get uniform location and cache it
    GLint getUniformLocation(const std::string& name) const
    {
        if (uniformLocations.find(name) != uniformLocations.end())
            return uniformLocations[name];

        GLint location = glGetUniformLocation(ID, name.c_str());
        uniformLocations[name] = location;
        return location;
    }

    // Template specialization for uniform setting based on type
    void setUniformImpl(GLint location, bool value) const { glUniform1i(location, value); }
    void setUniformImpl(GLint location, int value) const { glUniform1i(location, value); }
    void setUniformImpl(GLint location, float value) const { glUniform1f(location, value); }
    void setUniformImpl(GLint location, const glm::vec2& value) const { glUniform2fv(location, 1, glm::value_ptr(value)); }
    void setUniformImpl(GLint location, const glm::vec3& value) const { glUniform3fv(location, 1, glm::value_ptr(value)); }
    void setUniformImpl(GLint location, const glm::vec4& value) const { glUniform4fv(location, 1, glm::value_ptr(value)); }
    void setUniformImpl(GLint location, const glm::mat2& mat) const { glUniformMatrix2fv(location, 1, GL_FALSE, glm::value_ptr(mat)); }
    void setUniformImpl(GLint location, const glm::mat3& mat) const { glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(mat)); }
    void setUniformImpl(GLint location, const glm::mat4& mat) const { glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat)); }
    void setUniformImpl(GLint location, const std::vector<glm::mat4>& matrices) const { glUniformMatrix4fv(location, (GLsizei)matrices.size(), GL_FALSE, glm::value_ptr(matrices[0])); }

    // Utility function to check compile/link errors
    void checkCompileErrors(GLuint shader, const std::string& type) const
    {
        GLint success;
        GLchar infoLog[1024];

        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
                std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
            }
        }
    }
};