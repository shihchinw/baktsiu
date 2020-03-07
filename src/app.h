#ifndef BAKTSIU_APP_H_
#define BAKTSIU_APP_H_

#include "common.h"
#include "shader.h"
#include "texture.h"
#include "view.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

struct  GLFWwindow;
struct  ImFont;
struct  ImGuiIO;

namespace baktsiu
{

// This is a basic entity for command stack. It stores relevant data
// to undo instructions.
struct Action
{
    enum class Type : char
    {
        Unknown,
        Add,
        Remove,
    };

    Action() 
    {
        reset();
    }

    Action(Type t, int topImageIdx, int cmpImageIdx) 
        : type(t), prevTopImageIdx(topImageIdx), prevCmpImageIdx(cmpImageIdx)
    {
    }

    void reset() 
    {
        type = Type::Unknown;
        filepathArray.clear();
        imageIdxArray.clear();
        prevTopImageIdx = prevCmpImageIdx = -1;
    }

    Type type = Type::Unknown;
    std::vector<std::string> filepathArray;
    std::vector<int>         imageIdxArray;
    int prevTopImageIdx = -1;
    int prevCmpImageIdx = -1;
};


// Flags of the composition of image viewport.
enum class CompositeFlags : char
{
    Top         = 0x0,  // Display top (the selected) image only.
    Split       = 0x1,  // Scaled the compared image and put it underneath the top image.
    SideBySide  = 0x2,  // Display top and compared images side by side.
};

ENUM_CLASS_OPERATORS(CompositeFlags);

enum class PixelMarkerFlags : char
{
    None        = 0x00,
    Difference  = 0x01, // Draw pixel differences with a ramp of magenta.
    DiffHeatMap = 0x02, // Draw pixel differences in color ramp.
    DiffMask    = 0x03,
    Overflow    = 0x04, // Draw overflow pixels in red
    Underflow   = 0x08, // Draw underflow pixels in blue
    Default     = Difference | Overflow | Underflow,
};

ENUM_CLASS_OPERATORS(PixelMarkerFlags);


// The class of viewer functionalities.
//
// It contains two display layers on viewport. Top image is the one selected in 
// image property window at right hand side. In compare mode, we could move the
// splitter to swipe top image.
class App
{
public:
    static const char* kImagePropWindowName;
    static const char* kImageRemoveDlgTitle;
    static const char* kClearImagesDlgTitle;

public:
    bool    initialize(const char* title, int width, int height);

    void    importImageFiles(const std::vector<std::string>& filepathArray, bool recordAction, std::vector<int>* imageIdxArray = nullptr);

    void    run(CompositeFlags initFlags = CompositeFlags::Top);

    void    release();

private:
    void    setThemeColors();

    //! Initialize related bitmap and uv info for text.
    void    initDigitCharData(const unsigned char* data);

    void    initToolbar();

    // Create toogle handle for image property window.
    // Return true if the handle is hovered by mouse cursor.
    bool    initImagePropWindowHandle(float handleWidth, bool& showPropWindow);

    void    initImagePropWindow();

    void    initFooter();

    void    initHomeWindow(const char* name);

    void    showImageProperties();

    // Show image name overlays viewport.
    void    showImageNameOverlays();

    void    showHeatRangeOverlay(const Vec2f& pos, float width);

    bool    showRemoveImageDlg(const char *name);

    bool    showClearImagesDlg(const char* title);

    void    showImportImageDlg();

    void    showExportSessionDlg();

    //! Handle key pressed cases.
    //! @note Few key pressed events related to image transformation are handled in updateImageTransform.
    void    onKeyPressed(const ImGuiIO&);

    void    updateImagePairFromPressedKeys();

    // Update image transform from user's key stroke and mouse input.
    void    updateImageTransform(const ImGuiIO&, bool useColumnView);

    void    updateImageSplitterPos(ImGuiIO&);

    //! Dispatch compute kernels for image statistics.
    void    computeImageStatistics(const RenderTexture&, float valueScale);

    //! Reset image transform to viewport center.
    void    resetImageTransform(const Vec2f& imgSize, bool fitWindow = false);
    
    //! Return pixel coordinates from mouse position.
    bool    getImageCoordinates(Vec2f viewportCoords, Vec2f& outImageCoords) const;

    void    appendAction(Action&& action);

    void    undoAction();

    Texture*    getTopImage();

    void    removeTopImage(bool recordAction);

    void    clearImages(bool recordAction);

    void    processImportTasks();

    void    processTextureUploadTasks();

    void    onFileDrop(int count, const char* filepaths[]);

    //! Open compare session.
    void    openSession(const std::string& filepath);

    //! Save compare session with file extension .bts
    void    saveSession(const std::string& filepath);

    void    gradingTexImage(Texture &, int renderTexIdx);

    // Return the width of property window at right hand side.
    float   getPropWindowWidth() const;

    int     getPixelMarkerFlags() const;

    bool    inCompareMode() const;

    bool    inSideBySideMode() const;

    bool    shouldShowSplitter() const;

    void    toggleSplitView();

    void    toggleSideBySideView();

private:
    // Request item to load image file into specific position of image layers in image property window.
    using LoadRequest = std::tuple<std::string, int>;
    using TextureUPtr = std::unique_ptr<Texture>;

    std::vector<TextureUPtr>    mImageList;
    std::deque<LoadRequest>     mLoadRequestQueue;
    std::deque<TextureUPtr>     mUploadTaskQueue;
    std::mutex                  mLoadMutex;
    std::mutex                  mUploadMutex;
    std::condition_variable     mConditionVar;
    std::deque<Action>          mActionStack;
    Action                      mCurAction;
    std::atomic<int>            mImportRequestNum = { 0 };

    GLFWwindow* mWindow = nullptr;
    ImFont*     mSmallFont = nullptr;
    ImFont*     mSmallIconFont = nullptr;
    GLuint      mFontTexture = 0;
    
    std::vector<Vec4f> mCharUvRanges;   // UV bbox of each digit in font texture.
    std::vector<Vec4f> mCharUvXforms;   // UV offset of each digit in font texture.

    RenderTexture   mRenderTextures[2];   // The intermediate output for input image.
    int             mTopImageRenderTexIdx = 0;

    Shader          mGradingShader;
    Shader          mPresentShader;
    Shader          mStatisticsShader;
    GLuint          mTexHistogram;
    
    std::array<int, 768> mHistogram;

    CompositeFlags      mCompositeFlags = CompositeFlags::Top;
    PixelMarkerFlags    mPixelMarkerFlags = PixelMarkerFlags::Default;

    // Image transformation
    View        mView;
    View        mColumnViews[2];    // Views for side by side mode.
    float       mImageScale = 1.0f;
    float       mPrevImageScale = -1.0f;

    int         mTopImageIndex = -1;
    int         mCmpImageIndex = -1;

    int         mCurrentPresentMode = 0;
    int         mOutTransformType = 0;
    char        mImageScaleInfo[64];

    float       mToolbarHeight;
    float       mFooterHeight;

    float       mViewSplitPos = 0.5f;   // The horizontal position of viewport splitter.
    float       mPropWindowHSplitRatio = 0.65f;

    Vec3f       mPixelBorderHighlightColor = Vec3f(0.153f, 0.980f, 0.718f);
    float       mDisplayGamma = 2.2f;
    float       mExposureValue = 0.0f;

    bool        mIsMovingSplitter = false;
    bool        mIsScalingImage = false;
    bool        mEnableToneMapping = false;
    bool        mShowPixelValues = false;
    bool        mAboutToTerminate = false;

    bool        mShowImagePropWindow = false;
    bool        mPopupImagePropWindow = false;
    bool        mUseLinearFilter = true;
    bool        mShowImageNameOverlay = true;
    bool        mShowPixelMarker = false;
    bool        mSupportComputeShader = false;
};

}  // namespace baktsiu
#endif // BAKTSIU_APP_H_