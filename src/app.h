#ifndef BAKTSIU_APP_H_
#define BAKTSIU_APP_H_

#include "common.h"
#include "image.h"
#include "shader.h"
#include "texture.h"
#include "texture_pool.h"
#include "view.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

struct  GLFWmonitor;
struct  GLFWwindow;
struct  ImFont;
struct  ImGuiIO;

namespace baktsiu
{

// Basic entity of command stack. It stores relevant data for undo instructions.
struct Action
{
    static uint8_t extractImageId(uint16_t value) {
        return (value & 0xFF00) >> 8;
    }

    static uint8_t extractLayerIndex(uint16_t value) {
        return value & 0xFF;
    }

    static uint16_t composeImageIndex(uint8_t id, uint8_t order) {
        return order| id << 8;
    }

    enum class Type : char
    {
        Unknown,
        Add,
        Remove,
        Move,
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
    std::vector<std::string>    filepathArray;
    std::vector<uint16_t>       imageIdxArray;
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

class App;

// The class of glfw window functionalities.
class Window
{
public:
    bool        initialize(int width, int height, const char* title, App* app);

    void        destroy();

    // Get DPI scale value from the monitor with largest window area coverage.
    void        getDpiScale(float* xscale, float* yscale);

    // Callback when window is moved to another monitor.
    void        onDpiScaled(float xscale, float yscale);

    // Update window size with corresponding DPI scale.
    void        adaptToMonitorDPI();

    GLFWwindow* handle();

private:
    GLFWmonitor*    mMonitor = nullptr;
    GLFWwindow*     mWindow = nullptr;

    int     mWidth = 0;
    int     mHeight = 0;
    float   mScaleX = 1.0f;
    float   mScaleY = 1.0f;
    bool    mHasToResize = false;
};

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
    static const char* kSystemErrorDlgTitle;
    static const float kSniperImageScale;

public:
    bool    initialize(const char* title, int width, int height);

    void    importImageFiles(const std::vector<std::string>& filepathArray,
                bool recordAction, std::vector<uint16_t>* imageIdxArray = nullptr);

    void    run(CompositeFlags initFlags = CompositeFlags::Top);

    void    release();

    // Callback when dropping file path list.
    void    onFileDropped(int count, const char* filepaths[]);

    // Callback when window is moved to another monitor with different DPI scale.
    void    onWindowDpiScaled(float xscale, float yscale);

private:
    void    setThemeColors();

    void    initLogger();

    // Initialize related bitmap and uv info for text.
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

    // Handle key pressed cases.
    // @note Few key pressed events related to image transformation are handled in updateImageTransform.
    void    onKeyPressed(const ImGuiIO&);

    void    updateImagePairFromPressedKeys();

    // Update image transform from user's key stroke and mouse input.
    void    updateImageTransform(const ImGuiIO&, bool useColumnView);

    void    updateImageSplitterPos(ImGuiIO&);

    // Dispatch compute kernels for image statistics.
    void    computeImageStatistics(const RenderTexture&, float valueScale);

    // Reset image transform to viewport center.
    void    resetImageTransform(const Vec2f& imgSize, bool fitWindow = false);

    // Synchronize local offset of column views.
    void    syncSideBySideView(const ImGuiIO& io);

    // Return pixel coordinates and color from mouse position.
    bool    getPixelCoordsAndColor(Vec2f viewportCoords, Vec2f& outCoords, Vec4f& outColor) const;

    void    appendAction(Action&& action);

    void    undoAction();

    Image*  getTopImage();

    void    removeTopImage(bool recordAction);

    void    clearImages(bool recordAction);

    void    processTextureUploadTasks();

    // Open compare session.
    void    openSession(const std::string& filepath);

    // Save compare session with file extension .bts
    void    saveSession(const std::string& filepath);

    void    gradingTexImage(Image& image, int renderTexIdx);

    // Return the width of property window at right hand side.
    float   getPropWindowWidth() const;

    int     getPixelMarkerFlags() const;

    bool    inCompareMode() const;

    bool    inSideBySideMode() const;

    bool    shouldShowSplitter() const;

    void    toggleSplitView();

    void    toggleSideBySideView();

private:
    using ImageUPtr = std::unique_ptr<Image>;

    std::vector<ImageUPtr>      mImageList;
    std::deque<Action>          mActionStack;
    Action                      mCurAction;
    TexturePool                 mTexturePool;

    Window          mWindow;
    ImFont*         mSmallFont = nullptr;
    ImFont*         mSmallIconFont = nullptr;
    GLuint          mFontTexture = 0;

    std::vector<Vec4f> mCharUvRanges;   // UV bbox of each digit in font texture.
    std::vector<Vec4f> mCharUvXforms;   // UV offset of each digit in font texture.

    RenderTexture   mRenderTextures[2];   // The intermediate output for input image.
    int             mTopImageRenderTexIdx = 0;

    Shader          mGradingShader;
    Shader          mPresentShader;
    Shader          mStatisticsShader;
    GLuint          mTexHistogram;
    Sampler         mPointSampler;
    std::string     mSystemErrorMsg;

    std::array<int, 768> mHistogram;

    CompositeFlags      mCompositeFlags = CompositeFlags::Top;
    PixelMarkerFlags    mPixelMarkerFlags = PixelMarkerFlags::Default;

    // Image transformation
    View        mView;
    View        mColumnViews[2];    // Views for side by side mode.
    float       mDpiScale = 1.0f;
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
    bool        mBlendWithImageAlpha = true;
    bool        mShowImageNameOverlay = true;
    bool        mEnablePixelBorder = true;
    bool        mShowPixelMarker = false;
    bool        mSupportComputeShader = false;
    bool        mUpdateImageSelection = false;
};

}  // namespace baktsiu
#endif // BAKTSIU_APP_H_