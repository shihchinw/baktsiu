#ifndef BAKTSIU_SHADER_H_
#define BAKTSIU_SHADER_H_
#pragma once

#include "common.h"

#include <GL/gl3w.h>

#include <string>

namespace baktsiu
{

// Helper class for compling and linking OpenGL shader.
class Shader
{
public:
    //! Initialize graphics shader form string contents.
    bool    init(const std::string& name, 
                 const std::string& vertShderCode, 
                 const std::string& fragShaderCode);

    bool    initFromFiles(const std::string& name, 
                          const std::string& vertShderPath,
                          const std::string& fragShaderPath);

    const   std::string& name() const { return mName; }

    void    bind();

    //! Release internal graphics resources.
    void    release();

    // Return location of uniform.
    GLint   uniform(const std::string& name) const;

    /// Initialize a uniform parameter with a 4x4 matrix (float)
    template <typename T>
    void setUniform(const std::string& name, const Mat4f& mat) {
        glUniformMatrix4fv(uniform(name), 1, GL_FALSE, mat.value);
    }

    void setUniform(const std::string& name, const std::vector<Vec4f>& values)
    {
        glUniform4fv(uniform(name), static_cast<GLsizei>(values.size()), static_cast<const float*>(&values[0].x));
    }

    /// Initialize a uniform parameter with a 3x3 matrix (float)
    template <typename T>
    void setUniform(const std::string& name, const Mat3f& mat) {
        glUniformMatrix3fv(uniform(name), 1, GL_FALSE, mat.value);
    }

    /// Initialize a uniform parameter with a boolean value
    void setUniform(const std::string& name, bool value) {
        glUniform1i(uniform(name), (int)value);
    }

    /// Initialize a uniform parameter with an integer value
    void setUniform(const std::string& name, int value) {
        glUniform1i(uniform(name), value);
    }

    /// Initialize a uniform parameter with a floating point value
    void setUniform(const std::string& name, float value) {
        glUniform1f(uniform(name), value);
    }

    /// Initialize a uniform parameter with a 2D vector (int)
    void setUniform(const std::string& name, const Vec2i& v) {
        glUniform2i(uniform(name), v.x, v.y);
    }

   /// Initialize a uniform parameter with a 2D vector (float)
    void setUniform(const std::string& name, const Vec2f& v) {
        glUniform2f(uniform(name), v.x, v.y);
    }

    /// Initialize a uniform parameter with a 3D vector (int)
    void setUniform(const std::string& name, const Vec3i& v) {
        glUniform3i(uniform(name), v.x, v.y, v.z);
    }

    void setUniform(const std::string& name, const Vec3f& v) {
        glUniform3f(uniform(name), v.x, v.y, v.z);
    }

    void setUniform(const std::string& name, const Vec4i& v) {
        glUniform4i(uniform(name), v.x, v.y, v.z, v.w);
    }

    void setUniform(const std::string& name, const Vec4f& v) {
        glUniform4f(uniform(name), v.x, v.y, v.z, v.w);
    }

    /// Initialize a uniform buffer with a uniform buffer object
    //void setUniform(const std::string& name, const GLUniformBuffer& buf, bool warn = true);

    // Draw 
    void drawTriangle();

private:
    std::string     mName;
    GLuint          mVertexShader = 0u;
    GLuint          mFragmentShader = 0u;
    GLuint          mProgram = 0u;
};

}  // namespace baktsiu
#endif