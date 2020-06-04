#include "image.h"

namespace baktsiu
{

uint8_t Image::sSerialNo = 0;

Image::Image(TextureSPtr& tex, uint8_t id)
    : mTexture(tex)
{
    if (id == 0) {
        mId = ++sSerialNo;
    } else {
        mId = id;
    }
}

std::string Image::filename() const
{
    return mTexture ? mTexture->filename() : "none";
}

std::string Image::filepath() const
{
    return mTexture ? mTexture->filepath() : "none";
}

GLuint Image::texId() const 
{
    return mTexture ? mTexture->id() : 0;
}

uint8_t Image::id() const
{
    return mId;
}

Texture* Image::getTexture() const
{
    return mTexture ? mTexture.get() : nullptr;
}

bool Image::reload()
{
    return mTexture ? mTexture->reloadFile() : false;
}

Vec2f   Image::size() const
{
    return mTexture ? mTexture->size() : Vec2f(1.0f);
}

} // namespace baktsiu