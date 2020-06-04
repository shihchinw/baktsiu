#ifndef BAKTSIU_IMAGE_H_
#define BAKTSIU_IMAGE_H_

#include "texture.h"

namespace baktsiu
{

// This is equivalient to a texture with grading parameters.
class Image
{
private:
    static uint8_t sSerialNo;

public:
    Image(TextureSPtr& tex, uint8_t id = 0);

    Texture* getTexture() const;

    std::string filename() const;

    std::string filepath() const;

    GLuint  texId() const;

    uint8_t id() const;

    //! Reload corresponding texture data.
    bool    reload();

    Vec2f   size() const;

private:
    TextureSPtr     mTexture;
    uint8_t         mId = 0;
};

}  // namespace baktsiu
#endif