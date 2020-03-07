#include "shader.h"

#include <iostream>
#include <fstream>

namespace
{

GLuint createShader(GLint type, const std::string& name, const std::string& contents) 
{
    if (contents.empty()) {
        return 0;
    }

    GLuint id = glCreateShader(type);
    const char* source = contents.c_str();
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);

    GLint status;
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        std::string shaderType = "unknown";
        if (type == GL_VERTEX_SHADER) {
            shaderType = "vertex";
        } else if (type == GL_FRAGMENT_SHADER) {
            shaderType = "fragment";
        } else if (type == GL_GEOMETRY_SHADER) {
            shaderType = "geometry";
        } else if (type == GL_COMPUTE_SHADER) {
            shaderType = "compute";
        }

        char buffer[512];
        glGetShaderInfoLog(id, 512, nullptr, buffer);
        std::string msg = fmt::format("Error while compiling {} shader \"{}\":\n{}", shaderType, name, buffer);
        baktsiu::promptError(msg);
        throw std::runtime_error(msg);
    }

    return id;
}

} // namespace

namespace baktsiu
{

bool Shader::init(const std::string& name,
    const std::string& vtxShaderStr,
    const std::string& fragShaderStr)
{
    mVertexShader = createShader(GL_VERTEX_SHADER, name, vtxShaderStr);
    mFragmentShader = createShader(GL_FRAGMENT_SHADER, name, fragShaderStr);

    if (!mVertexShader || !mFragmentShader) {
        return false;
    }

    mName = name;
    mProgram= glCreateProgram();

    glAttachShader(mProgram, mVertexShader);
    glAttachShader(mProgram, mFragmentShader);
    glLinkProgram(mProgram);

    GLint status;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        char buffer[512];
        glGetProgramInfoLog(mProgram, 512, nullptr, buffer);
        promptError(fmt::format("Linker error in \"{}\":\n{}", mName, buffer));
        mProgram= 0;
        throw std::runtime_error("Shader linking failed!");
    }

    if (mVaoId == 0) {
        glGenVertexArrays(1, &mVaoId);
    }

    return true;
}


bool Shader::initFromFiles(
    const std::string& name,
    const std::string& vertexFileName,
    const std::string& fragmentFileName) 
{
    auto loadStrFromFile = [](const std::string& filename) -> std::string {
        if (filename.empty()) {
            return "";
        }

        std::ifstream t(filename);
        return std::string((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    };

    return init(name, loadStrFromFile(vertexFileName), loadStrFromFile(fragmentFileName));
}

bool Shader::initCompute(const std::string& name, const std::string& compShaderCode)
{
    GLuint computeShader = createShader(GL_COMPUTE_SHADER, name, compShaderCode);

    if (!computeShader) {
        return false;
    }

    mName = name;
    mProgram = glCreateProgram();

    glAttachShader(mProgram, computeShader);
    glLinkProgram(mProgram);

    GLint status;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        char buffer[512];
        glGetProgramInfoLog(mProgram, 512, nullptr, buffer);
        promptError(fmt::format("Linker error in \"{}\":\n{}", mName, buffer));
        mProgram = 0;
        throw std::runtime_error("Shader linking failed!");
    }

    glDeleteShader(computeShader);

    return true;
}


void Shader::bind()
{
    glUseProgram(mProgram);
}

void Shader::release()
{
    glDeleteProgram(mProgram);          mProgram = 0;
    glDeleteShader(mVertexShader);      mVertexShader = 0;
    glDeleteShader(mFragmentShader);    mFragmentShader = 0;
    glDeleteVertexArrays(1, &mVaoId);   mVaoId = 0;
}

GLint Shader::uniform(const std::string& name) const {
    GLint id = glGetUniformLocation(mProgram, name.c_str());
    
    if (id == -1) {
        promptWarning(fmt::format("Can not find uniform: {}", name));
    }
    
    return id;
}

void Shader::drawTriangle()
{
    // Even though we populate vertices' positions and uv coordinates from vertex ID directly,
    // we still need a dummy VAO on Mac OS for rendering.
    glBindVertexArray(mVaoId);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Shader::compute(GLuint numGroupX, GLuint numGroupY, GLuint numGroupZ)
{
    glDispatchCompute(numGroupX, numGroupY, numGroupZ);
}

}  // namespace baktsiu