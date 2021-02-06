#ifndef BAKTSIU_TEXTURE_H_
#define BAKTSIU_TEXTURE_H_

#include <memory>
#include <string>
#include <vector>

#include <GL/gl3w.h>

#include "common.h"
#include "colour.h"

namespace baktsiu
{

enum class ImageType : char
{
    Unknown = 0,
    JPG,
    PNG,
    BMP,
    GIF,
    HDR,
    TGA,
    OPENEXR,
    DNG
};


// Internal texture object.
class Texture
{
public:
    static bool isSupported(const std::string& extension);
    static ImageType getImageType(const std::string &filepath);

public:
    // Disable copy and assign.
    Texture() = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    ~Texture();

    // Load pixel data from file.
    bool    loadFromFile(const std::string& filepath);

    // Reload file from previous path.
    bool    reloadFile();

    // Upload content to GPU.
    bool    upload();

    // Release internal graphics resources.
    void    release();

    // Return GL texture id.
    GLuint  id() const { return mTexId; }

    Vec2f   size() const { return Vec2f(mWidth, mHeight); }

    void    bind();

    void    unbind();

    inline const std::string& filename() const { return mFileName; }

    inline const std::string& filepath() const { return mFilePath; }

private:
    std::string     mFilePath;
    std::string     mFileName;

    uint8_t*        mBuffer = nullptr;
    GLuint          mTexId = 0;
    GLenum          mImageFormat = GL_RGBA8;
    GLenum          mPixelDataType = GL_UNSIGNED_BYTE;

    int             mWidth = 0;
    int             mHeight = 0;
    int             mChannelNum = 0;
    bool            mUseLinearFilter = true;
};


using TextureSPtr = std::shared_ptr<Texture>;
using TextureList = std::vector<TextureSPtr>;


// Texture object as render target.
class RenderTexture
{
public:
    bool    bindAsOutput(const Vec2i& size, GLenum imageFormat);

    void    release();

    void    bindAsInput(bool useLinearFilter);

    void    unbind();

    // Return id of output texture.
    GLuint  id() const { return mTexId; }

    Vec2i   size() const { return mSize; }

    Vec4f   getTexelColor(const Vec2f& pos) const;

private:
    Vec2i   mSize;
    GLuint  mFboId = 0;
    GLuint  mRboId = 0;
    GLuint  mTexId = 0;
    GLenum  mImageFormat;
    bool    mUseLinearFilter = true;
};


// Wrapper of texture sampler.
class Sampler
{
public:
    bool    initialize(GLenum minFilter, GLenum magFilter);

    void    release();

    void    bind(GLuint);

    void    unbind(GLuint);

private:
    GLuint  mId = 0;
};

}  // namespace baktsiu
#endif