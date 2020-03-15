#include "view.h"

namespace baktsiu
{

Vec2f   View::getViewportCoords(const Vec2f& imgCoords) const
{
    Vec2f imageOffset = getImageOffset();
    return glm::clamp(imgCoords * mImageScale + imageOffset, Vec2f(0.0f), mViewSize);
}

Vec2f   View::getImageCoords(const Vec2f& viewCoords, bool* isClamped) const
{
    Vec2f imageOffset = getImageOffset();
    Vec2f coords = (viewCoords - imageOffset) / mImageScale;

    if (isClamped) {
        Vec2f mask = glm::step(Vec2f(0.0f), coords) - glm::step(mImageSize, coords);
        *isClamped = (mask.x * mask.y == 0.0f);
    }

    return glm::clamp(coords, Vec2f(0.0f), mImageSize);
}

Vec2f   View::getLocalOffset() const
{
    return mLocalOffset;
}

void    View::setImageSize(const Vec2f& size)
{
    mImageSize = size;
}

void    View::setLocalOffset(const Vec2f& offset)
{
    mLocalOffset = offset;
}

void View::setViewportPadding(const Vec4f& padding)
{
    mViewPadding = padding;
}

Vec2f   View::getImageOffset() const
{
    // Offset is relative to the origin at bottom left.
    Vec2f visibleSize = getVisibleSize();
    Vec3f offset((visibleSize - mImageSize) * 0.5f + mLocalOffset, 1.0);
    offset.x += mViewPadding.w;
    offset.y += mViewPadding.z;

    offset = mTransform * offset;
    return glm::round(Vec2f(offset) + Vec2f(0.5f)) - Vec2f(0.5f);  // Round to pixel center of viewport.
}

float   View::getImageScale() const
{
    return mImageScale;
}

Vec2f   View::getImageScalePivot() const
{
    return mImageScalePivot;
}

void    View::scale(float value, const Vec2f* pivot)
{
    // When the scale value is less than one, we want the image scaled near to current pivot.
    // On the contrary, when we magnify the image, we want to keep the pivot snaped to the
    // image border to make the image always visible in the view.
    if (value > 1.0f) {
        if (pivot) {
            mImageScalePivot = getConstrainedPivot(*pivot);
        } else {
            mImageScalePivot = getConstrainedPivot(mImageScalePivot);
        }
    } 

    Mat3f xform(1.0f);
    xform[0][0] = value;
    xform[1][1] = value;
    xform[2] = Vec3f(-value * mImageScalePivot.x + mImageScalePivot.x, -value * mImageScalePivot.y + mImageScalePivot.y, 1.0f);
    mTransform = xform * mTransform;
 
    mImageScale *= value;
}

void    View::translate(const Vec2f& value, bool localSpace)
{
    if (!localSpace) {
        mTransform[2][0] += value.x;
        mTransform[2][1] += value.y;
    } else {
        mLocalOffset += value;
    }
    
    restrictTranslation();
}

void    View::resize(const Vec2f& size)
{
    mViewSize = size;
    
    restrictTranslation();
}

Vec2f   View::getConstrainedPivot(Vec2f pivot) const
{
    const Vec2f imageOffset = getImageOffset();
    const Vec2f scaledImageSize = mImageSize * mImageScale;
    
    if (pivot.x < imageOffset.x) {
        pivot.x = imageOffset.x;
    } else if (pivot.x > imageOffset.x + scaledImageSize.x) {
        pivot.x = (imageOffset.x + scaledImageSize.x);
    }

    if (pivot.y < imageOffset.y) {
        pivot.y = imageOffset.y;
    } else if (pivot.y > imageOffset.y + scaledImageSize.y) {
        pivot.y = (imageOffset.y + scaledImageSize.y);
    }

    pivot = glm::round(pivot + Vec2f(0.5f)) - Vec2f(0.5f);

    // Use reflected pivot would make noncontiguous jump.
    /*if (pivot.x < imageOffset.x) {
        pivot.x = imageOffset.x * 2.0f - pivot.x;
    } else if (pivot.x > imageOffset.x + scaledImageSize.x) {
        pivot.x = (imageOffset.x + scaledImageSize.x) * 2.0f - pivot.x;
    }

    if (pivot.y < imageOffset.y) {
        pivot.y = imageOffset.y * 2.0f - pivot.y;
    } else if (pivot.y > imageOffset.y + scaledImageSize.y) {
        pivot.y = (imageOffset.y + scaledImageSize.y) * 2.0f - pivot.y;
    }*/
    return pivot;
}

void    View::restrictTranslation()
{
    const Vec2f imageOffset = getImageOffset();
    const Vec2f scaledImageSize = mImageSize * mImageScale;

    const auto safePadding = Vec2f(100.0f);

    Vec2f minOffset = safePadding - scaledImageSize;
    Vec2f maxOffset = getVisibleSize() - safePadding;
    Vec2f coffset(0.0f);

    coffset = glm::mix(coffset, minOffset - imageOffset, glm::lessThan(imageOffset, minOffset));
    coffset = glm::mix(coffset, maxOffset - imageOffset, glm::greaterThan(imageOffset, maxOffset));

    mTransform[2][0] += coffset.x;
    mTransform[2][1] += coffset.y;
}

Vec2f   View::getVisibleSize() const
{
    Vec2f visibleSize = mViewSize;
    visibleSize.x -= mViewPadding.y + mViewPadding.w;
    visibleSize.y -= mViewPadding.x + mViewPadding.z;
    return visibleSize;
}

void    View::reset(bool fitViewport)
{
    mTransform = Mat3f(1.0f);
    mImageScale = 1.0f;
    mLocalOffset = Vec2f(0.0f);
    
    Vec2f visibleSize = getVisibleSize();
    mImageScalePivot = visibleSize * 0.5f;
    mImageScalePivot.x += mViewPadding.w;
    mImageScalePivot.y += mViewPadding.z;
    mImageScalePivot = glm::round(mImageScalePivot + Vec2f(0.5f)) - Vec2f(0.5f);

    if (fitViewport) {
        mImageScale = std::min(visibleSize.x / mImageSize.x, visibleSize.y / mImageSize.y);
        Mat3f xform(1.0f);
        mTransform[0][0] = mImageScale;
        mTransform[1][1] = mImageScale;
        mTransform[2] = Vec3f(-mImageScale * mImageScalePivot.x + mImageScalePivot.x, -mImageScale * mImageScalePivot.y + mImageScalePivot.y, 1.0f);
    }
}

} // namespace baktsiu