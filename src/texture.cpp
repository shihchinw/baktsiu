#include "texture.h"

#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef USE_OPENEXR
#include <ImathBox.h>
#include <ImfRgba.h>
#include <ImfRgbaFile.h>
#include <ImfTestFile.h>
#include <thread>
#endif

namespace baktsiu
{

bool Texture::isSupported(const std::string& filepath)
{
    return getImageType(filepath) != ImageType::Unknown;
}

ImageType Texture::getImageType(const std::string &filepath)
{
    ImageType type = ImageType::Unknown;

    FILE *f = stbi__fopen(filepath.c_str(), "rb");
    if (!f) return type;

    stbi__context s;
    stbi__start_file(&s, f);

    if (stbi__jpeg_test(&s)) {
        type = ImageType::JPG;
    } else if (stbi__png_test(&s)) {
        type = ImageType::PNG;
    } else if (stbi__bmp_test(&s)) {
        type = ImageType::BMP;
    } else if (stbi__gif_test(&s)) {
        type = ImageType::GIF;
    } else if (stbi__hdr_test(&s)) {
        type = ImageType::HDR;
    } else if (stbi__tga_test(&s)) {
        type = ImageType::TGA;
    }

    if (type != ImageType::Unknown) {
        fclose(f);
    }
#ifdef USE_OPENEXR
    else if (Imf::isOpenExrFile(filepath.c_str())) {
        // Check other derived type like EXR, DNG.
        type = ImageType::OPENEXR;
    }
#endif

    return type;
}

//-----------------------------------------------------------------------------

Texture::~Texture()
{
    release();
}

bool Texture::loadFromFile(const std::string& filepath)
{
    ImageType imageType = getImageType(filepath);

    uint8_t* buffer = nullptr;
    if (imageType == ImageType::HDR) {
        PushRangeMarker(__FUNCTION__);
        buffer = reinterpret_cast<uint8_t*>(stbi_loadf(filepath.c_str(), &mWidth, &mHeight, &mChannelNum, 4));
        mPixelDataType = GL_FLOAT;
        mImageFormat = GL_RGBA16F;
        PopRangeMarker();
    }
#ifdef USE_OPENEXR
    else if (imageType == ImageType::OPENEXR) {
        Imf::setGlobalThreadCount(std::thread::hardware_concurrency());

        Imf::RgbaInputFile file(filepath.c_str());
        Imath::Box2i dw = file.dataWindow();

        mWidth = dw.max.x - dw.min.x + 1;
        mHeight = dw.max.y - dw.min.y + 1;
        mChannelNum = 4;
        buffer = (uint8_t*)stbi__malloc_mad4(mWidth, mHeight, mChannelNum, sizeof(Imf::Rgba), 0);

        file.setFrameBuffer((Imf::Rgba*)buffer - dw.min.x - dw.min.y * mWidth, 1, mWidth);
        file.readPixels(dw.min.y, dw.max.y);

        mPixelDataType = GL_HALF_FLOAT;
        mImageFormat = GL_RGBA16F;
    }
#endif
    else {
        buffer = stbi_load(filepath.c_str(), &mWidth, &mHeight, &mChannelNum, 4);
        mPixelDataType = GL_UNSIGNED_BYTE;
        mImageFormat = GL_RGBA8;
    }

    if (!buffer) {
        return false;
    }

    if (mBuffer) {
        // Remove old texture data.
        stbi_image_free(mBuffer);
    }

    mBuffer = buffer;

    mFilePath = filepath;
    mFileName = filepath.substr(filepath.find_last_of("/") + 1);

    return true;
}

bool Texture::reloadFile()
{
    return loadFromFile(mFilePath) && upload();
}

bool Texture::upload()
{
    ScopeMarker(__FUNCTION__);

    if (!mBuffer) {
        return false;
    }

    if (mTexId == 0) {
        // Create a OpenGL texture identifier
        glGenTextures(1, &mTexId);
    }

    glBindTexture(GL_TEXTURE_2D, mTexId);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, mBuffer);

    glTexStorage2D(GL_TEXTURE_2D, 1, mImageFormat, mWidth, mHeight);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, mPixelDataType, mBuffer);

    stbi_image_free(mBuffer);
    mBuffer = nullptr;

    return true;
}

void Texture::release()
{
    if (mBuffer) {
        stbi_image_free(mBuffer);
        mBuffer = nullptr;
    }

    if (mTexId) {
        glDeleteTextures(1, &mTexId);
        mTexId = 0;
    }
}

void    Texture::bind()
{
    glBindTexture(GL_TEXTURE_2D, mTexId);
}

void    Texture::unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

//-----------------------------------------------------------------------------

bool    RenderTexture::bindAsOutput(const Vec2i& size, GLenum imageFormat)
{
    if (mSize == size && mImageFormat == imageFormat) {
        glBindFramebuffer(GL_FRAMEBUFFER, mFboId);
        return true;
    }

    if (mTexId == 0) {
        glGenTextures(1, &mTexId);
    }

    glBindTexture(GL_TEXTURE_2D, mTexId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (mFboId == 0) {
        glGenFramebuffers(1, &mFboId);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, mFboId);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexId, 0);

#ifdef _DEBUG
    bool isValid = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    if (!isValid) {
        return false;
    }
#endif

    mSize = size;
    mImageFormat = imageFormat;

    return true;
}

void    RenderTexture::release()
{
    glDeleteTextures(1, &mTexId);
    glDeleteFramebuffers(1, &mFboId);

    mTexId = 0;
    mFboId = 0;
}

void    RenderTexture::bindAsInput(bool useLinearFilter)
{
    glBindTexture(GL_TEXTURE_2D, mTexId);

    if (useLinearFilter == mUseLinearFilter) {
        return;
    }

    // Setup filtering parameters for display
    GLint filterType = useLinearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);
    mUseLinearFilter = useLinearFilter;
}

void    RenderTexture::unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Vec4f   RenderTexture::getTexelColor(const Vec2f& pos) const
{
    Vec4f result(0.0f);

    if (mTexId > 0) {
        glGetTextureSubImage(mTexId, 0, static_cast<GLint>(pos.x), static_cast<GLint>(pos.y),
            0, 1, 1, 1, GL_RGBA, GL_FLOAT, sizeof(result), &result[0]);
    }

    return result;
}

//-----------------------------------------------------------------------------

bool    Sampler::initialize(GLenum minFilter, GLenum magFilter)
{
    if (mId == 0) {
        glGenSamplers(1, &mId);
    }

    glSamplerParameteri(mId, GL_TEXTURE_MIN_FILTER, minFilter);
    glSamplerParameteri(mId, GL_TEXTURE_MAG_FILTER, magFilter);
    glSamplerParameteri(mId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(mId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(mId, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return mId > 0;
}

void    Sampler::release()
{
    glDeleteSamplers(1, &mId);
}

void    Sampler::bind(GLuint unit)
{
    glBindSampler(unit, mId);
}

void    Sampler::unbind(GLuint unit)
{
    glBindSampler(unit, 0);
}


}  // namespace baktsiu