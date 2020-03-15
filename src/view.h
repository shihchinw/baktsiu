#ifndef BAKTSIU_VIEW_H_
#define BAKTSIU_VIEW_H_

#include "common.h"

namespace baktsiu
{

//! The object handles viewport to image coordinates.
//! The origin of coordinates is at bottom-left.
class View
{
public:
    //! Return viewport coordinates with given image coordinates.
    Vec2f   getViewportCoords(const Vec2f& imgCoords) const;
    
    //! Return image coordinates from viewport coordinates.
    Vec2f   getImageCoords(const Vec2f& viewCoords, bool* isClamped = nullptr) const;

    //! Return origin offset.
    Vec2f   getLocalOffset() const;

    //! Get offset of scaled image (in pixels of viewport)
    Vec2f   getImageOffset() const;

    //! Return image scale factor.
    float   getImageScale() const;

    Vec2f   getImageScalePivot() const;

    void    setLocalOffset(const Vec2f& offset);

    //! Set original image size.
    void    setImageSize(const Vec2f& size);

    //! Set padding for viweport.
    //! @param padding (top, right, bottom, left)
    void    setViewportPadding(const Vec4f& padding);

    //! Scale image with pivot in viewport coordinates
    void    scale(float value, const Vec2f* pivot = nullptr);

    //! Translate in viewport coordinates.
    //! @param localSpace Offset is specified in local coordinates (before transformation).
    void    translate(const Vec2f& offset, bool localSpace = false);

    //! Reset image to viewport center.
    void    reset(bool fitViewport);

    //! Change viewport size.
    void    resize(const Vec2f& size);

private:
    //! When scale pivot is outside the image border, we have to
    //! restrict the position of pivot to make scaled image visible
    //! in the viewport.
    Vec2f   getConstrainedPivot(Vec2f pivot) const;

    Vec2f   getVisibleSize() const;

    void    restrictTranslation();

private:
    Vec2f   mLocalOffset = Vec2f(0.0f);
    Mat3f   mTransform = Mat3f(1.0f);   // 2D affine transformation for image.
    Vec4f   mViewPadding = Vec4f(0.0f); // (top, right, bottom, left)
    Vec2f   mViewSize = Vec2f(1.0f);
    Vec2f   mImageSize = Vec2f(1.0f);
    Vec2f   mImageScalePivot = Vec2f(0.0f);
    float   mImageScale = 1.0f;
};

} // namespace baktsiu
#endif