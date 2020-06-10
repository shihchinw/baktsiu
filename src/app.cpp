#define IMGUI_DEFINE_MATH_OPERATORS
#include <icons_font_awesome5.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#pragma warning(push)
#pragma warning(disable: 4819)
#include "portable_file_dialogs.h"      // File dialogs.

#include <GL/gl3w.h>    // Initialize with gl3wInit()
#include <GLFW/glfw3.h>
#pragma warning(pop)

#include <fstream>
#include <memory>
#include <sstream>

#include <fx/gltf.h>
#include <stb_image.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "app.h"
#include "colour.h"
#include "resources.h"

#ifdef EMBED_SHADERS
#include "shader_resources.h"
#define INIT_SHADER(shader, name, vtxName, fragName)\
shader.init(name, vtxName##_vert, fragName##_frag)
#else
#define STRING(s) #s
#define INIT_SHADER(shader, name, vtxName, fragName)\
shader.initFromFiles(name, "shaders/"##STRING(vtxName)##".vert", "shaders/"##STRING(fragName)##".frag")
#endif

#ifndef _MSC_VER
#define sprintf_s snprintf
#endif

namespace
{

bool endsWith(const std::string& str, const std::string& token)
{
    return str.rfind(token, str.size() - token.size()) != std::string::npos;
}

void APIENTRY glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, void *userParam)
{
    // Ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "[" << id << "] ";

    switch (source) {
    case GL_DEBUG_SOURCE_API:             std::cout << "API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Other"; break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               std::cout << ", Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << ", Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << ", Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << ", Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << ", Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << ", Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << ", Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << ", Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << ", Other"; break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << ", Severity high ]"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << ", Severity medium ]"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << ", Severity low ]"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << ", Severity notification ]"; break;
    }

    std::cout << "\n\n" << message;
    std::cout << "\n---------------\n\n";
}

// See more implementations about toggle button https://github.com/ocornut/imgui/issues/1537
bool ToggleButton(const char* label, bool* value, const ImVec2 &size = ImVec2(0, 0), bool enable = true)
{
    bool valueChanged = false;
    ImGuiContext& g = *ImGui::GetCurrentContext();

    if (*value) {
        ImGui::PushStyleColor(ImGuiCol_Button, g.Style.Colors[ImGuiCol_ButtonActive]);
        
        if (ImGui::Button(label, size)) {
            *value ^= true;
            valueChanged = true;
        }

        ImGui::PopStyleColor(1);
        return valueChanged;
    }

    if (!enable) {
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g.Style.Colors[ImGuiCol_Button]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g.Style.Colors[ImGuiCol_Button]);
    }

    if (ImGui::Button(label, size) && enable) {
        *value = true;
        valueChanged = true;
    }

    if (!enable) {
        ImGui::PopStyleColor(2);
    }

    return valueChanged;
}

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

}  // namespace anonymous

namespace ImGui 
{

static ImU32 InvertColorU32(ImU32 in)
{
    ImVec4 in4 = ColorConvertU32ToFloat4(in);
    in4.x = 1.f - in4.x;
    in4.y = 1.f - in4.y;
    in4.z = 1.f - in4.z;
    return GetColorU32(in4);
}

// Draw advanced histogram https://github.com/ocornut/imgui/issues/632
void PlotMultiHistograms(
    const char* label,
    int num_hists,
    const char** names,
    const ImColor* colors,
    int pixel_counts[],
    int norm_pixel_count,
    int values_count,
    float scale_min,
    float scale_max,
    ImVec2 graph_size)
{
    const int values_offset = 0;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    if (graph_size.x == 0.0f)
        graph_size.x = CalcItemWidth();
    if (graph_size.y == 0.0f)
        graph_size.y = label_size.y + (style.FramePadding.y * 2);

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(graph_size.x, graph_size.y));
    const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
    const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, NULL))
        return;

    RenderFrame(inner_bb.Min, inner_bb.Max, GetColorU32(ImGuiCol_FrameBg), true, 1.0f);

    int res_w = ImMin((int)graph_size.x, values_count);
    int item_count = values_count;

    // Tooltip on hover
    int v_hovered = -1;
    if (ItemHoverable(inner_bb, id)) {
        const float t = ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 1.0f);
        const int v_idx = std::min((int)(t * item_count), item_count - 1);
        IM_ASSERT(v_idx >= 0 && v_idx < values_count);

        // std::string toolTip;
        ImGui::BeginTooltip();
        const int idx0 = (v_idx + values_offset) % values_count;
        TextColored(ImColor(255, 255, 255, 255), "Level: %d", v_idx);

        for (int dataIdx = 0; dataIdx < num_hists; ++dataIdx) {
            const int v0 = pixel_counts[dataIdx * values_count + idx0];
            TextColored(ImColor(255, 255, 255, 255), "%s: %d", names[dataIdx], v0);
        }
        
        ImGui::EndTooltip();
        v_hovered = v_idx;
    }

    for (int data_idx = 0; data_idx < num_hists; ++data_idx) {
        const float t_step = 1.0f / (float)res_w;

        float v0 = pixel_counts[data_idx * values_count] / static_cast<float>(norm_pixel_count);
        float t0 = 0.0f;
        ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) / (scale_max - scale_min)));    // Point in the normalized space of our target rectangle

        const ImU32 col_base = colors[data_idx];
        const ImU32 col_hovered = InvertColorU32(colors[data_idx]);

        for (int n = 0; n < res_w; n++) {
            const float t1 = t0 + t_step;
            const int v1_idx = (int)(t0 * item_count + 0.5f);
            IM_ASSERT(v1_idx >= 0 && v1_idx < values_count);
            const float v1 = pixel_counts[data_idx * values_count + v1_idx] / static_cast<float>(norm_pixel_count);
            const ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) / (scale_max - scale_min)));

            // NB: Draw calls are merged together by the DrawList system. Still, we should render our batch are lower level to save a bit of CPU.
            ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
            ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, ImVec2(tp1.x, 1.0f));
            if (pos1.x >= pos0.x + 2.0f) {
                pos1.x -= 1.0f;
            }
            window->DrawList->AddRectFilled(pos0, pos1, v_hovered == v1_idx ? col_hovered : col_base);

            t0 = t1;
            tp0 = tp1;
        }
    }

    //RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
}

} // namespace ImGui


namespace baktsiu
{

const char* App::kImagePropWindowName = "ImagePropWindow";
const char* App::kImageRemoveDlgTitle = "Bak Tsiu##RemoveImage";
const char* App::kClearImagesDlgTitle = "Bak Tsiu##ClearImageLayers";

// Return UV BBox of given character in font texture.
inline Vec4f getCharUvRange(const stbtt_bakedchar& ch, float mapWidth)
{
    return Vec4f(ch.x0, ch.y0, ch.x1, ch.y1) / mapWidth;
}

// Return UV offset of given character in font texture.
inline Vec4f getCharUvXform(const stbtt_bakedchar& ch, float charHeight)
{
    Vec4f xform;
    xform.x = (ch.x1 - ch.x0) / ch.xadvance;
    xform.y = (ch.y1 - ch.y0) / charHeight;
    xform.z = ch.xoff / ch.xadvance;

    // The bitmap is baked upside down. For each char, the yoff is the value from 
    // image bottom to the baseline pixel. Ex. if yoff is -7, means the baseline
    // is at +7 pixels in y-axis. To compute the offset uv coordinates within
    // that char only, we calculate the pixels number from char bottom to baseline
    // which is (charHeight + ch.yoff), then divide by charHeight to compute the
    // normalized uv coordinates.
    xform.w = (charHeight + ch.yoff) / charHeight;
    return xform;
}


void    App::setThemeColors()
{
    ImGui::StyleColorsDark();
    ImVec4* colors = ImGui::GetStyle().Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.184f, 0.184f, 0.184f, 1.0f);
    colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_WindowBg];

    colors[ImGuiCol_FrameBg] = ImVec4(0.187f, 0.321f, 0.418f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.288f, 0.485f, 0.637f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.137f, 0.663f, 0.812f, 0.75f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.439f, 0.702f, 0.965f);

    colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.439f, 0.702f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.0f, 0.439f, 0.702f, 0.5f);
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_HeaderHovered] = colors[ImGuiCol_ButtonHovered];
    
    colors[ImGuiCol_TabActive] = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_ButtonHovered];

    colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 0.439f, 0.702f, 0.8f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.137f, 0.663f, 0.812f, 0.8f);
    colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_SliderGrabActive] = colors[ImGuiCol_ButtonHovered];
}


bool App::initialize(const char* title, int width, int height)
{
    glfwSetErrorCallback([](int error, const char* description) {
        std::cerr << "GLFW error " << error << ": " << description << std::endl;
    });

    if (!glfwInit()) {
        return false;
    }

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
    //glfwWindowHint(GLFW_DECORATED, false);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GL_FALSE);
#endif

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

    // Create window with graphics context
    mWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (mWindow == nullptr) {
        return false;
    }

    glfwSetWindowUserPointer(mWindow, this);
    glfwSetDropCallback(mWindow, [](GLFWwindow* window, int count, const char* paths[]) {
        App* self = static_cast<App*>(glfwGetWindowUserPointer(window));
        self->onFileDrop(count, paths);
    });

    GLFWimage images[1];
    // Icon is converted to byte array in pre-build stage. It's content is defined in resources.cpp.
    images[0].pixels = stbi_load_from_memory(title_bar_icon_png, title_bar_icon_png_size, &images[0].width, &images[0].height, nullptr, 0);
    glfwSetWindowIcon(mWindow, 1, images);
    glfwMakeContextCurrent(mWindow);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
    if (gl3wInit() != 0) {
        promptError("Failed to initialize OpenGL loader!");
        return false;
    }

    mSupportComputeShader = glfwExtensionSupported("GL_ARB_compute_shader");

#ifdef _DEBUG
    // Check output frame buffer has no gamma color encoding, since we done it in our shader of image presentation.
    GLint encoding;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
    assert(encoding == GL_LINEAR);

    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
#endif

    ScopeMarker("Initialization");

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    setThemeColors();

    ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    mToolbarHeight = 32.0f;
    mFooterHeight = 20.0f;
    Vec4f padding(mToolbarHeight, 0.0f, mFooterHeight, 0.0f);
    mView.setViewportPadding(padding);
    mColumnViews[0].setViewportPadding(padding);
    mColumnViews[1].setViewportPadding(padding);

    // Fonts are converted into data arrays in pre-built stage. It uses bin2c.cmake to convert
    // resources/fonts/*.ttf into corresponding data arrays. Here we simply add fonts from 
    // memory. But in order to keep ownership of font data, we need to set FontDataOwnedByAtlas
    // to false, otherwise ImGui would release font data and cause crash when closing app window.
    // https://github.com/ocornut/imgui/issues/220
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;
    mSmallFont = io.Fonts->AddFontFromMemoryTTF(roboto_regular_ttf, roboto_regular_ttf_size, 13.0f, &config);
    io.FontDefault = io.Fonts->AddFontFromMemoryTTF(roboto_regular_ttf, roboto_regular_ttf_size, 15.0f, &config);

    config.MergeMode = true;
    config.PixelSnapH = true;
    config.GlyphMinAdvanceX = 16.0f; // Use if you want to make the icon monospaced
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_size, config.GlyphMinAdvanceX, &config, icon_ranges);
    
    // Create another font with small icons.
    config.MergeMode = false;
    config.GlyphMinAdvanceX = 0.0f;
    mSmallIconFont = io.Fonts->AddFontFromMemoryTTF(roboto_regular_ttf, roboto_regular_ttf_size, 15.0f, &config);

    config.MergeMode = true;
    config.GlyphMinAdvanceX = 14.0f;
    io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_size, config.GlyphMinAdvanceX, &config, icon_ranges);

    // Initialize bit map texture for shader to render pixel's RGB values.
    initDigitCharData((unsigned char*)robotomono_regular_ttf);

    bool status = INIT_SHADER(mPresentShader, "present", quad, present);
    CHECK_AND_RETURN_IT(status, "Failed to initialize present shader");

    status = INIT_SHADER(mGradingShader, "color_grading", quad, color_grading);
    CHECK_AND_RETURN_IT(status, "Failed to initialize color grading shader");

    if (mSupportComputeShader) {
        glGenTextures(1, &mTexHistogram);
        glBindTexture(GL_TEXTURE_2D, mTexHistogram);
        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32I, static_cast<GLint>(mHistogram.size()), 1);
        status = mStatisticsShader.initCompute("statistics", statistics_comp);
    }

    mPointSampler.initialize(GL_NEAREST, GL_NEAREST);

    return status;
}

void App::initDigitCharData(const unsigned char* data)
{
    constexpr int bitmapWidth = 256;
    unsigned char bitmap[bitmapWidth * bitmapWidth];
    constexpr float fontHeight = 84.0f;

    // Bake bitmap from ASCII code 46 to 58: ./0123456789
    constexpr int charNumber = 12;
    constexpr int firstAsciiCodeIdx = 46;
    stbtt_bakedchar cdata[charNumber];
    int result = stbtt_BakeFontBitmap(data, 0, fontHeight, bitmap, bitmapWidth, bitmapWidth, firstAsciiCodeIdx, charNumber, cdata);
    if (result < 0) {
        promptError("Failed to initialized font!");
    }

    // Push data for digit 0-9 whose ASCII code is [48, 57]
    for (int c = 48; c < 58; c++) {
        const auto& ch = cdata[c - firstAsciiCodeIdx];
        mCharUvRanges.push_back(getCharUvRange(ch, bitmapWidth));
        mCharUvXforms.push_back(getCharUvXform(ch, fontHeight));
    }

    // Push data for decimal point '.'
    mCharUvRanges.push_back(getCharUvRange(cdata[0], bitmapWidth));
    mCharUvXforms.push_back(getCharUvXform(cdata[0], fontHeight));

    glGenTextures(1, &mFontTexture);
    glBindTexture(GL_TEXTURE_2D, mFontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, bitmapWidth, bitmapWidth, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);

    // Generate mipmap for the text texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void App::release()
{
    // Cleanup
    mImageList.clear();
    mPointSampler.release();

    mPresentShader.release();
    mGradingShader.release();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

// This is executed in main thread (which has GL context).
void App::processTextureUploadTasks()
{
    TextureList newTextureList = mTexturePool.upload();

    bool isUndo = (mCurAction.type != Action::Type::Unknown);
    
    if (isUndo && mTexturePool.hasNoPendingTasks()) {
        if (mCurAction.type == Action::Type::Remove) {
            const int curImageNum = static_cast<int>(mImageList.size());
            mTopImageIndex = std::min(mCurAction.prevTopImageIdx, curImageNum - 1);
            mCmpImageIndex = std::min(mCurAction.prevCmpImageIdx, curImageNum - 1);
            mUpdateImageSelection = false;
        }
        
        mCurAction.reset();
    }

    if (newTextureList.empty() && !mUpdateImageSelection) {
        return;
    }

    // When we finish importing images, we switch top image to the latest one.
    if (!isUndo && mTexturePool.hasNoPendingTasks()) {
        mTopImageIndex = static_cast<int>(mImageList.size()) - 1;
        resetImageTransform(getTopImage()->size());

        if (mCmpImageIndex == -1 && mTopImageIndex >= 1) {
            mCmpImageIndex = 0;
        }

        mUpdateImageSelection = false;
    }
}

void App::run(CompositeFlags initFlags)
{
    bool shouldChangeComposition = true;

    // Spawn worker threads to handle import images.
    const unsigned int workerNum = std::thread::hardware_concurrency();
    mTexturePool.initialize(workerNum);

    while (!glfwWindowShouldClose(mWindow)) {
        glfwPollEvents();

        processTextureUploadTasks();
        if (shouldChangeComposition && mImageList.size() >= 2) {
            mCompositeFlags = initFlags;
            shouldChangeComposition = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        initToolbar();
        initFooter();
        initImagePropWindow();
        
        ImGuiIO& io = ImGui::GetIO();

        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) &&
            !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
            updateImageSplitterPos(io);
        }

        onKeyPressed(io);
        Image* topImage = getTopImage();
        Vec2f imageSize = topImage ? topImage->size() : Vec2f(1.0f);

        const bool useColumnView = inSideBySideMode();
        if (useColumnView) {
            const float leftColumnWidth = io.DisplaySize.x * mViewSplitPos;
            mColumnViews[0].resize(Vec2f(leftColumnWidth, io.DisplaySize.y));
            mColumnViews[1].resize(Vec2f(io.DisplaySize.x - leftColumnWidth, io.DisplaySize.y));
            mColumnViews[0].setImageSize(imageSize);
            mColumnViews[1].setImageSize(imageSize);
        } else {
            mView.resize(io.DisplaySize);
            mView.setImageSize(imageSize);
        }

        updateImageTransform(io, useColumnView);

        const bool enableCompareView = inCompareMode();
        if (shouldShowSplitter() && mShowImageNameOverlay) {
            showImageNameOverlays();
        }

        if (enableCompareView && ((getPixelMarkerFlags() & 0x2) > 0)) {
            Vec2f heatbarPos(8.0f, io.DisplaySize.y - mFooterHeight - 8.0f - 20.0f);
            showHeatRangeOverlay(heatbarPos, 150.0f);
        }
        
        // Since GLFW doesn't support cursor of resize all, thus we use imgui to draw that cursor.
        // Caution: the cursor is hidden when using imgui's drawn cursor, when the root window is unfocused.
        io.MouseDrawCursor = (ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeAll);

        //ImGui::ShowDemoWindow();

        ImGui::Render();

        if (topImage && topImage->texId() != 0) {
            gradingTexImage(*topImage, mTopImageRenderTexIdx);
            if (mShowImagePropWindow && mSupportComputeShader) {
                float valueScale = topImage->getColorEncodingType() == ColorEncodingType::Linear ? 1.0f : 255.0f;
                computeImageStatistics(mRenderTextures[mTopImageRenderTexIdx], valueScale);
            }
        }

        if (enableCompareView && mCmpImageIndex >= 0) {
            Image* cmpImage = mImageList[mCmpImageIndex].get();
            if (cmpImage->texId() != 0) {
                gradingTexImage(*cmpImage, mTopImageRenderTexIdx ^ 1);
            }
        }

        // We have to apply framebuffer scale for hidh DPI display.
        const Vec2f viewportSize = io.DisplaySize * io.DisplayFramebufferScale;
        glViewport(0, 0, static_cast<GLsizei>(viewportSize.x), static_cast<GLsizei>(viewportSize.y));
        glClearColor(0.45f, 0.55f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        // Render image viewer display
        // TODO: Use depth culling to avoid over-drawing.
        mPresentShader.bind();
        glActiveTexture(GL_TEXTURE0);

        // We have to forcely use nearest filter to properly show numerical values within a pixel.
        const View& topView = useColumnView ? mColumnViews[0] : mView;
        const View& bottomView = useColumnView ? mColumnViews[1] : mView;
        const float imageScale = topView.getImageScale();
        const bool forceNearestFilter = imageScale > 5.0f;

        if (topImage) {
            mRenderTextures[mTopImageRenderTexIdx].bindAsInput(mUseLinearFilter && !forceNearestFilter);

            Vec2f imageSize = topImage->size();
            mPresentShader.setUniform("uImageSize", imageSize * imageScale);
            mPresentShader.setUniform("uOffset", topView.getImageOffset());
            mPresentShader.setUniform("uImage1", 0);
        } else {
            mPresentShader.setUniform("uImageSize", Vec2f(0.0f));
        }

        mPresentShader.setUniform("uEnablePixelHighlight", !mIsMovingSplitter && !mIsScalingImage);
        mPresentShader.setUniform("uCursorPos", Vec2f(io.MousePos.x, io.DisplaySize.y - io.MousePos.y) + Vec2f(0.5f));
        mPresentShader.setUniform("uSideBySide", mCompositeFlags == CompositeFlags::SideBySide);
        mPresentShader.setUniform("uPixelMarkerFlags", getPixelMarkerFlags());
        mPresentShader.setUniform("uPresentMode", mCurrentPresentMode);
        mPresentShader.setUniform("uOutTransformType", mOutTransformType);
        mPresentShader.setUniform("uWindowSize", Vec2f(io.DisplaySize));
        mPresentShader.setUniform("uImageScale", imageScale);
        mPresentShader.setUniform("uSplitPos", enableCompareView ? mViewSplitPos : 1.0f);
        mPresentShader.setUniform("uDisplayGamma", mDisplayGamma);
        mPresentShader.setUniform("uApplyToneMapping", mEnableToneMapping);
        mPresentShader.setUniform("uCharUvRanges", mCharUvRanges);
        mPresentShader.setUniform("uCharUvXforms", mCharUvXforms);
        mPresentShader.setUniform("uPixelBorderHighlightColor", mPixelBorderHighlightColor);

        if (enableCompareView && mCmpImageIndex >= 0) {
            glActiveTexture(GL_TEXTURE1);
            mRenderTextures[mTopImageRenderTexIdx ^ 1].bindAsInput(mUseLinearFilter && !forceNearestFilter);
            mPresentShader.setUniform("uImage2", 1);
            mPresentShader.setUniform("uOffsetExtra", bottomView.getImageOffset());
            mPresentShader.setUniform("uRelativeOffset", (bottomView.getLocalOffset() - topView.getLocalOffset()) * mImageScale);
        }

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, mFontTexture);
        mPresentShader.setUniform("uFontImage", 2);

        mPresentShader.drawTriangle();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(mWindow);
    }

    mTexturePool.release();
}

void    App::gradingTexImage(Image& image, int renderTexIdx)
{
    const Vec2i size = image.size();
    mRenderTextures[renderTexIdx].bindAsOutput(size, GL_RGBA16F);

    glViewport(0, 0, size.x, size.y);
    glClearColor(0.45f, 0.55f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    const int textureUnit = 0;
    glActiveTexture(GL_TEXTURE0);
    image.getTexture()->bind();
    mPointSampler.bind(textureUnit);

    mGradingShader.bind();
    mGradingShader.setUniform("uImage", textureUnit);
    mGradingShader.setUniform("uEV", mExposureValue);
    mGradingShader.setUniform("uInImageProp", Vec2i(
        static_cast<int>(image.getColorEncodingType()),
        static_cast<int>(image.getColorPrimaryType())));

    mGradingShader.drawTriangle();

    image.getTexture()->unbind();
    mPointSampler.unbind(textureUnit);
    mRenderTextures[renderTexIdx].unbind();
}

void    App::computeImageStatistics(const RenderTexture& texture, float valueScale)
{
    ScopeMarker("Compute Image Statistics");

    // Calculate image statistics.
    std::fill(mHistogram.begin(), mHistogram.end(), 0);
    glBindTexture(GL_TEXTURE_2D, mTexHistogram);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLint>(mHistogram.size()), 1, GL_RED_INTEGER, GL_INT, (void*)mHistogram.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    mStatisticsShader.bind();
    glBindImageTexture(0, texture.id(), 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
    glBindImageTexture(1, mTexHistogram, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);

    Vec2i size = texture.size();
    mStatisticsShader.setUniform("uImageSize", size);
    mStatisticsShader.setUniform("uValueScale", valueScale);
    mStatisticsShader.compute(size.x / 16, size.y / 16);

    /*glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glBindTexture(GL_TEXTURE_2D, mTexHistogram);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, (void*)mHistogram.data());*/
}

void    App::onKeyPressed(const ImGuiIO& io)
{
    if (ImGui::IsKeyPressed(0x102)) { // tab
        mShowImagePropWindow ^= true;
    } else if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(0x57)) { // Ctrl+Shift+w
        clearImages(true);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(0x5A)) { // Ctrl+z
        undoAction();
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(0x4F)) { // Ctrl+o
        showImportImageDlg();
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(0x45)) { // Ctrl+e
        showExportSessionDlg();
    } else if (io.KeyAlt && ImGui::IsKeyPressed(0x43)) { // Alt+c
        syncSideBySideView(io);
    } else if (ImGui::IsKeyPressed(0x53)) { // s
        toggleSplitView();
    } else if (ImGui::IsKeyPressed(0x43)) { // c
        toggleSideBySideView();
    } else if (ImGui::IsKeyPressed(0x51)) { // q
        mUseLinearFilter ^= true;
    } else if (ImGui::IsKeyPressed(0x57)) { // w
        mShowPixelMarker ^= true;
    } else if (ImGui::IsKeyPressed(0x122)) { // F1
        ImGui::OpenPopup("Home");
    } else if (ImGui::IsKeyPressed(0x126)) { // F5
        Image* image = getTopImage();
        if (image) image->reload();
    } else if (ImGui::IsKeyPressed(0x103) || ImGui::IsKeyPressed(0x105)) { // Backspace/Del
        if (mTopImageIndex > -1) ImGui::OpenPopup(kImageRemoveDlgTitle);
    } else {
        updateImagePairFromPressedKeys();
        // ps. There are few other key pressed cases are handled in updateImageTransform().
    }

    initHomeWindow("Home");

    if (showRemoveImageDlg(kImageRemoveDlgTitle)) {
        removeTopImage(true);
    }
}

void    App::updateImageSplitterPos(ImGuiIO& io)
{
    if (!shouldShowSplitter()) {
        return;
    }

    const bool isHoverSplitter = !ImGui::IsMouseDragging(0) && std::abs((io.MousePos.x / io.DisplaySize.x) - mViewSplitPos) < 1e-2f;
    ImGui::SetMouseCursor(isHoverSplitter || mIsMovingSplitter ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_Arrow);

    if (ImGui::IsMouseDown(0)) { // LMB pressed
        if (isHoverSplitter) {
            mIsMovingSplitter = true;
        }
    } else {
        mIsMovingSplitter = false;
    }

    if (mIsMovingSplitter) {
        mViewSplitPos = glm::clamp(io.MousePos.x / io.DisplaySize.x, 0.02f, 0.98f);
    }
}

void    App::syncSideBySideView(const ImGuiIO& io)
{
    int columnIdx = static_cast<int>(io.MousePos.x > io.DisplaySize.x * mViewSplitPos);
    mColumnViews[columnIdx].setLocalOffset(mColumnViews[columnIdx ^ 1].getLocalOffset());
}

void    App::updateImageTransform(const ImGuiIO& io, bool useColumnView)
{
    Image* topImage = getTopImage();
    if (!topImage) {
        return;
    }

    Vec2f scalePivot(-1.0f);

    mImageScale = inSideBySideMode() ? mColumnViews[0].getImageScale() : mView.getImageScale();

    bool onFocus = !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    float oldImageScale = mImageScale;
    
    static bool isInSniperMode = false;

    if (isInSniperMode && ImGui::IsKeyReleased(0x5A)) {
        mImageScale = mPrevImageScale;
        mPrevImageScale = -1.0f;
        isInSniperMode = false;
    }

    if (onFocus) {
        if (ImGui::IsMouseReleased(0) || ImGui::IsMouseReleased(1)) {
            mIsScalingImage = false;
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsMouseDown(1)) {
            // Scale the image when both left and right buttons are pressed.
            mImageScale *= (1.0f - glm::roundEven(io.MouseDelta.y) * 0.0078125f);
            mIsScalingImage = true;
        } else if (!mIsMovingSplitter && ImGui::IsMouseDown(0)) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

            Vec2f translate(io.MouseDelta.x, -io.MouseDelta.y);

            if (!useColumnView) {
                mView.translate(translate);
            } else if (io.KeyAlt) {
                static Vec2f residualTranslate = Vec2f(0.0f);
                translate += residualTranslate;
                
                Vec2f roundedTranslate = glm::round(translate / mImageScale);
                int columnIdx = static_cast<int>(io.MousePos.x > io.DisplaySize.x * mViewSplitPos);
                mColumnViews[columnIdx].translate(roundedTranslate, true);
                residualTranslate = translate - roundedTranslate * mImageScale;
            } else {
                mColumnViews[0].translate(translate);
                mColumnViews[1].translate(translate);
            }

            return;
        }
    }

    bool mouseAtRightColumn = false;
    auto fetchScalePivot = [](const ImGuiIO& io, bool useColumnView, float viewSplitPos, bool& mouseAtRightColumn) {
        Vec2f scalePivot(io.MousePos.x, io.DisplaySize.y - io.MousePos.y);
        if (useColumnView) {
            float leftColumnWidth = io.DisplaySize.x * viewSplitPos;
            if (scalePivot.x > leftColumnWidth) {
                scalePivot.x -= leftColumnWidth;
                mouseAtRightColumn = true;
            }
        }

        scalePivot = glm::round(scalePivot + Vec2f(0.5f)) - Vec2f(0.5f);
        return scalePivot;
    };

    // Zoom in/out from mouse scroll.
    if (onFocus && io.MouseWheel != 0.0f) {
        scalePivot = fetchScalePivot(io, useColumnView, mViewSplitPos, mouseAtRightColumn);

        if (mImageScale == 2) {
            if (io.MouseWheel >= 0.0f) {
                mImageScale += io.MouseWheel;
            } else {
                mImageScale /= 1 << (int)(-io.MouseWheel);
            }
        } else if (mImageScale > 2) {
            mImageScale *= io.MouseWheel > 0.0f ? 1.414f : 0.707f;
        } else {
            if (io.MouseWheel >= 0.0f) {
                mImageScale *= 1 << (int)(io.MouseWheel);
            } else {
                mImageScale /= 1 << (int)(-io.MouseWheel);
            }
        }
    }

    // Transfrom from keyboard
    if (ImGui::IsKeyPressed(0x14D) || ImGui::IsKeyPressed(0x2D)) {
        mImageScale *= 0.5f;
    } else if (ImGui::IsKeyPressed(0x14E) || ImGui::IsKeyPressed(0x3D)) {
        mImageScale *= 2.0f;
    } 
    else if (!io.KeyCtrl && ImGui::IsKeyPressed(0x5A) && mPrevImageScale < 0.0f) {
        isInSniperMode = true;
        scalePivot = fetchScalePivot(io, useColumnView, mViewSplitPos, mouseAtRightColumn);
        mPrevImageScale = mImageScale;
        mImageScale = 72.0f;
    }
    else if (io.KeyShift && ImGui::IsKeyPressed(0x046)) { // shift+f
        resetImageTransform(topImage->size(), true);
        return;
    } else if (ImGui::IsKeyPressed(0x14B) || ImGui::IsKeyPressed(0x2F) || ImGui::IsKeyPressed(0x046)) { // '/' or 'f'
        resetImageTransform(topImage->size());
        return;
    }

    mImageScale = glm::clamp(mImageScale, 0.125f, 256.0f);
    const float relativeScale = mImageScale / oldImageScale;
    if (abs(relativeScale - 1.0f) < 1e-4f) {
        return;
    }

    if (!useColumnView) {
        mView.scale(relativeScale, scalePivot.x > 0.0f ? &scalePivot : nullptr);
    } else if (scalePivot.x < 0.0f) {
        // Use previous scale pivot.
        mColumnViews[0].scale(relativeScale);
        mColumnViews[1].scale(relativeScale);
    } else {
        const int focusColumnIdx = mouseAtRightColumn ? 1 : 0;
        mColumnViews[focusColumnIdx].scale(relativeScale, &scalePivot);
        scalePivot = mColumnViews[focusColumnIdx].getImageScalePivot();

        // Retrieve the scale pivot in the other column view, we first get the
        // pixel coordinates, then use it to get corresponding viewport coordinates.
        Vec2f pixelCoords = mColumnViews[focusColumnIdx].getImageCoords(scalePivot);
        const int theOtherColumnIdx = focusColumnIdx ^ 1;
        Vec2f theOtherScalePivot = mColumnViews[theOtherColumnIdx].getViewportCoords(pixelCoords);

        // Align pivot height.
        theOtherScalePivot.y = scalePivot.y;
        mColumnViews[theOtherColumnIdx].scale(relativeScale, &theOtherScalePivot);
    }
}

void App::updateImagePairFromPressedKeys()
{
    const int imageNum = static_cast<int>(mImageList.size());
    if (imageNum < 2) {
        return;
    }

    bool isSwap = ImGui::IsKeyPressed(0x109) || ImGui::IsKeyPressed(0x58); // Up arrow or 'x'
    bool isNext = ImGui::IsKeyPressed(0x106) || ImGui::IsKeyPressed(0x44); // Right arrow or 'd'
    bool isPrev = ImGui::IsKeyPressed(0x107) || ImGui::IsKeyPressed(0x41); // Left arrow or 'a'

    const bool enableCompareView = inCompareMode();

    if (enableCompareView && isSwap) {
        std::swap(mCmpImageIndex, mTopImageIndex);
        mTopImageRenderTexIdx ^= 1;
    } else if (enableCompareView && imageNum > 2) {
        // Rotate compared image, shift further if it collides top image.
        if (isNext) {
            mCmpImageIndex = (mCmpImageIndex + 1) % imageNum;
            if (mCmpImageIndex == mTopImageIndex) {
                mCmpImageIndex = (mCmpImageIndex + 1) % imageNum;
            }
        } else if (isPrev) {
            mCmpImageIndex = (mCmpImageIndex - 1 + imageNum) % imageNum;
            if (mCmpImageIndex == mTopImageIndex) {
                mCmpImageIndex = (mCmpImageIndex - 1 + imageNum) % imageNum;
            }
        }
    } else if (!enableCompareView) {
        // Rotate top image, if it collides compared image, shift compared image.
        if (isNext) {
            mTopImageIndex = (mTopImageIndex + 1) % imageNum;
            if (mCmpImageIndex == mTopImageIndex) {
                mCmpImageIndex = (mCmpImageIndex + 1) % imageNum;
            }
        } else if (isPrev) {
            mTopImageIndex = (mTopImageIndex - 1 + imageNum) % imageNum;
            if (mCmpImageIndex == mTopImageIndex) {
                mCmpImageIndex = (mCmpImageIndex - 1 + imageNum) % imageNum;
            }
        }
    }
}

void    App::initToolbar()
{
    ImGuiContext& g = *ImGui::GetCurrentContext();

    ImGui::SetNextWindowPos(Vec2f(0.0f));
    ImGui::SetNextWindowSize(Vec2f(g.IO.DisplaySize.x, mToolbarHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2f(2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Vec2f(2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Vec2f(4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, Vec2f(0.5f, 0.75f));   // Make font icon align center.
    ImGui::PushStyleColor(ImGuiCol_Button, g.Style.Colors[ImGuiCol_MenuBarBg]);

    ImGui::Begin("toolbar", nullptr, ImGuiWindowFlags_NoDecoration);

    const Vec2f buttonSize(26.0f);
    const char* popupWindowName = "Home";

    if (ImGui::Button(ICON_FA_HOME, buttonSize)) {
        ImGui::OpenPopup(popupWindowName);
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Home Window"); }


    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT, buttonSize)) {
        showImportImageDlg();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Import Images"); }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_EXPORT, buttonSize)) {
        showExportSessionDlg();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Export Session"); }

    const float comboMenuWidth = 100.0f;
    static float centeredToolItemWidth = 506.0f;
    ImGui::SameLine((g.IO.DisplaySize.x - centeredToolItemWidth) * 0.5f);
    float centeredToolBeginPos = g.CurrentWindow->DC.CursorPos.x;
    ToggleButton(ICON_FA_FEATHER, &mUseLinearFilter, buttonSize);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Smooth image");
    }

    ImGui::SameLine();
    const bool couldCompare = mImageList.size() > 1;
    bool inSplitView = (mCompositeFlags == CompositeFlags::Split);
    if (ToggleButton(ICON_FA_I_CURSOR, &inSplitView, buttonSize, couldCompare)) {
        mCompositeFlags = inSplitView ? CompositeFlags::Split : CompositeFlags::Top;
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Split View"); }

    ImGui::SameLine();
    bool inSideBySideView = (mCompositeFlags == CompositeFlags::SideBySide);
    if (ToggleButton(ICON_FA_COLUMNS, &inSideBySideView, buttonSize, couldCompare)) {
        mCompositeFlags = inSideBySideView ? CompositeFlags::SideBySide : CompositeFlags::Top;
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Side by Side View"); }

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Vec2f(5.0f));
    ImGui::PushItemWidth(comboMenuWidth);
    const char* outTransformTypes[] = { "sRGB", "P3 D65", "BT.2020" };
    if (ImGui::BeginCombo("##OutputColorSpace", outTransformTypes[mOutTransformType])) // The second parameter is the label previewed before opening the combo.
    {
        for (int i = 0; i < IM_ARRAYSIZE(outTransformTypes); i++) {
            bool isSelected = (mOutTransformType == i);
            if (ImGui::Selectable(outTransformTypes[i], isSelected)) {
                mOutTransformType = i;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Display Color Primaries");
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    ImGui::SliderFloat("##EV", &mExposureValue, -8.0f, 8.0f, "EV: %.1f");
    if (ImGui::IsItemClicked(1)) {
        mExposureValue = 0.0f;
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##Gamma", &mDisplayGamma, 1.0f, 2.8f, "gamma: %.1f");
    if (ImGui::IsItemClicked(1)) {
        mDisplayGamma = 2.2f;
    }

    ImGui::SameLine();
    const char* presentModes[] = { "RGB", "R", "G", "B", "Luminance", "CIE L*", "CIE a*", "CIE b*" };
    if (ImGui::BeginCombo("##Channel", presentModes[mCurrentPresentMode])) // The second parameter is the label previewed before opening the combo.
    {
        for (int i = 0; i < IM_ARRAYSIZE(presentModes); i++) {
            bool isSelected = (mCurrentPresentMode == i);
            if (ImGui::Selectable(presentModes[i], isSelected)) {
                mCurrentPresentMode = i;
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();   // Set the initial focus when opening the combo (scrolling + for keyboard navigation support in the upcoming navigation branch)
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Channels");
    }

    ImGui::PopItemWidth();
    ImGui::PopStyleVar(1);

    ImGui::SameLine();
    ToggleButton(ICON_FA_FILM, &mEnableToneMapping, buttonSize, mCurrentPresentMode == 0);

    ImGui::SameLine();
    ToggleButton(ICON_FA_EXCLAMATION_TRIANGLE, &mShowPixelMarker, buttonSize);
    if (ImGui::IsItemClicked(1)) { ImGui::OpenPopup("PixelMarkMenu"); }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Pixel Warning Markers"); }

    if (ImGui::BeginPopup("PixelMarkMenu")) {
        const bool enableCompareView = inCompareMode();

        bool itemValue = hasAllFlags(mPixelMarkerFlags, PixelMarkerFlags::Difference);
        if (ImGui::MenuItem("Difference", "", &itemValue, enableCompareView)) {
            auto newState = itemValue ? PixelMarkerFlags::Difference : PixelMarkerFlags::None;
            mPixelMarkerFlags = (mPixelMarkerFlags & ~PixelMarkerFlags::DiffMask) | newState;
        }
        itemValue = hasAllFlags(mPixelMarkerFlags, PixelMarkerFlags::DiffHeatMap);
        if (ImGui::MenuItem("Diff as Heatmap", "", &itemValue, enableCompareView)) {
            auto newState = itemValue ? PixelMarkerFlags::DiffHeatMap : PixelMarkerFlags::None;
            mPixelMarkerFlags = (mPixelMarkerFlags & ~PixelMarkerFlags::DiffMask) | newState;
        }

        const bool showDiffMarker = hasAnyFlags(mPixelMarkerFlags, PixelMarkerFlags::DiffMask);
        
        itemValue = hasAllFlags(mPixelMarkerFlags, PixelMarkerFlags::Overflow);
        if (ImGui::MenuItem("Overflow", "", &itemValue, !enableCompareView || !showDiffMarker)) {
            mPixelMarkerFlags = toggleFlags(mPixelMarkerFlags, PixelMarkerFlags::Overflow);
        }

        itemValue = hasAllFlags(mPixelMarkerFlags, PixelMarkerFlags::Underflow);
        if (ImGui::MenuItem("Underflow", "", &itemValue, !enableCompareView || !showDiffMarker)) {
            mPixelMarkerFlags = toggleFlags(mPixelMarkerFlags, PixelMarkerFlags::Underflow);
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    centeredToolItemWidth = g.CurrentWindow->DC.CursorPos.x - centeredToolBeginPos;

    // Show buttons at right hand side.
    ImGui::SameLine(g.IO.DisplaySize.x - (buttonSize.x + g.Style.ItemSpacing.x) * 2.0f);
    ToggleButton(ICON_FA_COMMENT_ALT, &mShowImageNameOverlay, buttonSize);
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Image Name Tags"); }

    ImGui::SameLine(g.IO.DisplaySize.x - buttonSize.x - g.Style.ItemSpacing.x);
    ToggleButton(ICON_FA_CHART_BAR, &mShowImagePropWindow, buttonSize);
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Property Window"); }

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(7);

    initHomeWindow(popupWindowName);
    ImGui::End();
}

bool    App::initImagePropWindowHandle(float handleWidth, bool& showPropWindow)
{
    bool isHovered = false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    //const auto handleWidth = g.FontSize;
    ImGui::SetNextWindowPos(ImVec2(g.IO.DisplaySize.x - handleWidth, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(handleWidth, g.IO.DisplaySize.y));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2f(0.0f));
    ImGui::Begin("ImagePropHandle", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::PushStyleColor(ImGuiCol_Button, g.Style.Colors[ImGuiCol_MenuBarBg]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g.Style.Colors[ImGuiCol_ButtonHovered]);
    const char* label = showPropWindow ? ":\n}\n:" : ":\n{\n:";
    if (ImGui::Button(label, ImVec2(handleWidth, g.IO.DisplaySize.y))) {
        showPropWindow ^= true;
    }
    ImGui::PopStyleColor(2);

    if (ImGui::IsItemHovered()) {
        isHovered = true;
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    return isHovered;
}


// Ref: splitter https://github.com/ocornut/imgui/issues/319
void App::initImagePropWindow()
{
    bool checkPopupRect = false;

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const float handleWidth = g.FontSize;

    // Create a window as a toggle handle to open/close property window.
    if (initImagePropWindowHandle(handleWidth, mShowImagePropWindow)) {
        mPopupImagePropWindow = true;
    } else {
        checkPopupRect = true;
    }

    if (!mShowImagePropWindow && !mPopupImagePropWindow) {
        return;
    }

    // Create a property window.
    const float propWindowWidth = getPropWindowWidth();
    ImGui::SetNextWindowPos(ImVec2(g.IO.DisplaySize.x - propWindowWidth - handleWidth, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(propWindowWidth, g.IO.DisplaySize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2f(4.0f, mToolbarHeight));

    ImGui::Begin(kImagePropWindowName, nullptr, ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PopStyleVar(2);

    // Create horizontal splitter for up and down panels.
    float size1 = (g.IO.DisplaySize.y - mToolbarHeight - mFooterHeight) * mPropWindowHSplitRatio;
    float size2 = g.IO.DisplaySize.y - size1 - mToolbarHeight - mFooterHeight;
    Splitter(false, 2.5f, &size1, &size2, 60, 160, propWindowWidth);
    mPropWindowHSplitRatio = size1 / (g.IO.DisplaySize.y - mToolbarHeight - mFooterHeight);

    ImGui::BeginChild("ScrollingRegion1", ImVec2(propWindowWidth - g.Style.WindowPadding.x, size1));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    
    auto* topImage = getTopImage();
    if (ImGui::CollapsingHeader("Histogram") && topImage && mSupportComputeShader) {
        ScopeMarker("Draw Histogram");
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
        glBindTexture(GL_TEXTURE_2D, mTexHistogram);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_INT, (void*)mHistogram.data());

        ImGui::SetNextItemWidth(propWindowWidth - ImGui::GetStyle().ItemSpacing.x);
        const char* names[3] = {"R", "G", "B"};
        static ImColor colors[3] = { ImColor(255, 0, 0, 150), ImColor(0, 255, 0, 150), ImColor(0, 0, 255, 150) };
        const int binSize = 256;
        static float arr[binSize * 3];

        // Get the first 25-th largest bin counts and use it as the normalize factor.
        std::vector<int> temp;
        temp.assign(mHistogram.begin(), mHistogram.end());
        std::nth_element(temp.begin(), temp.end() - 25, temp.end());
        int normPixelCount =*(temp.end() - 25);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Vec2f(3.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(69, 69, 69, 255));
        ImGui::PushFont(mSmallFont);
        ImGui::PlotMultiHistograms("##histogram", 3, names, colors, mHistogram.data(), normPixelCount, binSize, 0.0f, 1.0f, ImVec2(0, 100));
        ImGui::PopFont();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(1);
    }

    //if (ImGui::CollapsingHeader("Scope")) {
    //}

    //if (ImGui::CollapsingHeader("Wavefront")) {
    //}

    if (ImGui::CollapsingHeader("Image Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        showImageProperties();
    }

    ImGui::PopStyleVar(1);
    ImGui::EndChild();

    const float spacerHeight = 4.0f;
    Vec2f buttonSize(20.0f);
    const float toolbarHeight = buttonSize.y + g.Style.FramePadding.y * 2.0f + g.Style.ItemSpacing.y;
    const float titleHeight = ImGui::GetFrameHeightWithSpacing() + spacerHeight;
    ImGui::Dummy(Vec2f(0.0f, spacerHeight));
    ImGui::Text("  " ICON_FA_LAYER_GROUP "  Images");

    ImGui::BeginChild("ScrollingRegion2", ImVec2(propWindowWidth - g.Style.WindowPadding.x, size2 - titleHeight - toolbarHeight), true);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, Vec2f(0.0f, 0.5f));

    const int bufSize = 64;
    const int maxFilenameLength = bufSize - 10; // Reserve 10 chars for icon and spaces
    char buf[bufSize];
    const int imageNum = static_cast<int>(mImageList.size());
    const bool enableCompareView = inCompareMode();

    static const Vec4f activeBorderColor(1.0f, 1.0f, 1.0f, 1.0f);
    static const Vec4f borderColor(1.0f, 1.0f, 1.0f, 0.5f);
    static bool isDraggingImageItem = false;
    static std::unique_ptr<Action> moveAction;

    for (int i = 0; i < imageNum; i++) {
        std::string filename = mImageList[i]->filename();
        void* addr = mImageList[i].get();
        const auto filenameLength = filename.size();
        if (filenameLength > maxFilenameLength) {
            filename = filename.substr(filenameLength - maxFilenameLength);
            snprintf(buf, bufSize, "        ...%s##%p", filename.c_str(), addr);
        } else {
            snprintf(buf, bufSize, "           %s##%p", filename.c_str(), addr);
        }

        if (ImGui::Selectable(buf, mTopImageIndex == i && !isDraggingImageItem, 0, Vec2f(propWindowWidth, 24.0f))) {
            if (isDraggingImageItem) {
                isDraggingImageItem = false;
                if (moveAction) {
                    if (moveAction->prevTopImageIdx != mTopImageIndex ||
                        moveAction->prevCmpImageIdx != mCmpImageIndex) {
                        appendAction(std::move(*moveAction));
                    }
                }

                moveAction.reset();
            } else {
                mTopImageIndex = i;
                if (mCmpImageIndex == i) {
                    mCmpImageIndex = (mCmpImageIndex + 1) % imageNum;
                }
            }
        }

        // Right click mouse to set compared imaeg directly.
        if (ImGui::IsItemClicked(1)) {
            if (mTopImageIndex == i) {
                mTopImageIndex = mCmpImageIndex;
            }
            mCmpImageIndex = i;
        }

        if (!isDraggingImageItem && ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        } else if (isDraggingImageItem) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
            isDraggingImageItem = true;
            if (!moveAction) {
                moveAction = std::make_unique<Action>();
                moveAction->type = Action::Type::Move;
                for (uint8_t i = 0; i < static_cast<uint8_t>(mImageList.size()); i++) {
                    moveAction->imageIdxArray.push_back(Action::composeImageIndex(mImageList[i]->id(), i));
                }
                moveAction->prevTopImageIdx = mTopImageIndex;
                moveAction->prevCmpImageIdx = mCmpImageIndex;
            }

            const int nextItemIdx = i + (ImGui::GetMouseDragDelta(0).y < 0.0f ? -1 : 1);
            if (nextItemIdx >= 0 && nextItemIdx < imageNum) {
                std::swap(mImageList[i], mImageList[nextItemIdx]);
                ImGui::ResetMouseDragDelta();

                if (nextItemIdx == mTopImageIndex) {
                    mTopImageIndex = i;
                    if (mCmpImageIndex == i) {
                        mCmpImageIndex = nextItemIdx;
                    }
                } else if (nextItemIdx == mCmpImageIndex) {
                    mCmpImageIndex = i;
                    if (mTopImageIndex == i) {
                        mTopImageIndex = nextItemIdx;
                    }
                } else {
                    if (mTopImageIndex == i) {
                        mTopImageIndex = nextItemIdx;
                    } else if (mCmpImageIndex == i) {
                        mCmpImageIndex = nextItemIdx;
                    }
                }
            }
        }

        const GLuint texId = mImageList[i]->texId();
        if (texId != 0) {
            ImGui::SameLine(g.Style.ItemSpacing.x);
            ImGui::Image((void*)(intptr_t)texId, Vec2f(28.0f, 22.0f), Vec2f(0.0f, 0.0f), Vec2f(1.0f, 1.0f),
                Vec4f(1.0f), mTopImageIndex == i ? activeBorderColor : borderColor);
        }

        if ((i == mCmpImageIndex || i == mTopImageIndex) && enableCompareView) {
            ImGui::SameLine(propWindowWidth - g.FontSize - g.Style.ItemSpacing.x * 2.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::Text(i == mCmpImageIndex ? ICON_FA_ANGLE_RIGHT : ICON_FA_ANGLE_LEFT);
        }
    }
    ImGui::PopStyleVar(1);
    ImGui::EndChild();

    const int buttonNum = 4;
    ImGui::PushFont(mSmallIconFont);
    ImGui::Dummy(Vec2f(propWindowWidth - (buttonSize.x + g.Style.ItemSpacing.x) * buttonNum - handleWidth, buttonSize.y));
    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, g.Style.Colors[ImGuiCol_MenuBarBg]);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FILE_IMPORT "##ImportImage", buttonSize)) {
        showImportImageDlg();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Import Images"); }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_SYNC_ALT "##ReloadImage", buttonSize) && mTopImageIndex > -1) {
        mImageList[mTopImageIndex]->reload();
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Reload Selected Image"); }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_MINUS_CIRCLE "##RemoveImage", buttonSize)) {
        if (ImGui::GetIO().KeyShift) {
            removeTopImage(true);
        } else {
            ImGui::OpenPopup(kImageRemoveDlgTitle);
        }
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Close Selected Image"); }
    if (showRemoveImageDlg(kImageRemoveDlgTitle)) { removeTopImage(true); }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH_ALT "##ClearImages", buttonSize)) {
        if (ImGui::GetIO().KeyShift) {
            clearImages(true);
        } else if (!mImageList.empty()) {
            ImGui::OpenPopup(kClearImagesDlgTitle);
        }
    }
    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Close All Images"); }
    if (showClearImagesDlg(kClearImagesDlgTitle)) { clearImages(true); }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (checkPopupRect) {
        mPopupImagePropWindow = ImGui::IsWindowHovered();
    }
    ImGui::End();
}


void App::initFooter()
{
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGui::PushFont(mSmallFont);

    //g.NextWindowData.MenuBarOffsetMinVal = ImVec2(g.Style.DisplaySafeAreaPadding.x, ImMax(g.Style.DisplaySafeAreaPadding.y - g.Style.FramePadding.y, 0.0f));
    ImGui::SetNextWindowPos(ImVec2(0.0f, g.IO.DisplaySize.y - mFooterHeight));
    ImGui::SetNextWindowSize(ImVec2(g.IO.DisplaySize.x, mFooterHeight));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, g.Style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##MainFooter", NULL, window_flags);
    ImGui::PopStyleVar(4);

    if (mImageScale > 1.0f) {
        sprintf_s(mImageScaleInfo, IM_ARRAYSIZE(mImageScaleInfo), "%.1fx", mImageScale);
    } else {
        sprintf_s(mImageScaleInfo, IM_ARRAYSIZE(mImageScaleInfo), "%.2f%%\n", mImageScale * 100.0f);
    }

    ImGui::Text("%s", mImageScaleInfo);

    if (mTopImageIndex >= 0) {
        const Vec2f& imageSize = mImageList[mTopImageIndex]->size();
        ImGui::SameLine(g.Style.FramePadding.x + g.FontSize * 4.0f);
        ImGui::Text("| %.0f x %.0f", imageSize.x, imageSize.y);

        Vec2f imageCoords;
        if (getImageCoordinates(g.IO.MousePos, imageCoords)) {
            ImGui::SameLine();
            imageCoords.y = imageSize.y - imageCoords.y - 1;
            ImGui::Text("(%.0f, %.0f)", imageCoords.x, imageCoords.y);
        }
    }

    const auto* topImage = getTopImage();
    if (topImage && !inCompareMode()) {
        const std::string& filename = topImage->filename();
        ImGui::SameLine(g.IO.DisplaySize.x - ImGui::CalcTextSize(filename.c_str()).x - g.Style.FramePadding.x);
        ImGui::Text(filename.c_str());
    }

    ImGui::End();
    ImGui::PopFont();
}

void    App::showImageProperties()
{
    auto* topImage = getTopImage();
    if (!topImage) {
        return;
    }

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 100.0f);

    ImGui::Text("Attrbute");
    ImGui::NextColumn();

    ImGui::Text("Value");
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("Color Primaries");
    ImGui::Text("Color Encoding");
    ImGui::Text("Location");
    ImGui::Text("Resolution");
    //ImGui::Text("Bit Depth");
    ImGui::NextColumn();

    static const ColorPrimaryType colorPrimaryTypes[] = {
        ColorPrimaryType::BT_709,
        ColorPrimaryType::DCI_P3_D65,
        ColorPrimaryType::BT_2020,
        ColorPrimaryType::ACES_AP1,
    };

    ImGui::Text(getPropertyLabel(topImage->getColorPrimaryType()));
    if (ImGui::BeginPopupContextItem("ColorPrimaryMenu")) {
        for (auto type : colorPrimaryTypes) {
            const char* label = getPropertyLabel(type);
            if (ImGui::Selectable(label)) {
                topImage->setColorPrimaryType(type);
            }
        }
        ImGui::EndPopup();
    }

    static const ColorEncodingType colorEncodingTypes[] = {
        ColorEncodingType::Linear,
        //ColorEncodingType::BT_2020,
        //ColorEncodingType::BT_2100_HLG,
        ColorEncodingType::BT_2100_PQ,
        ColorEncodingType::BT_709,
        ColorEncodingType::sRGB
    };

    ImGui::Text(getPropertyLabel(topImage->getColorEncodingType()));
    if (ImGui::BeginPopupContextItem("ColorEncodingMenu")) {
        for (auto type : colorEncodingTypes) {
            const char* label = getPropertyLabel(type);
            if (ImGui::Selectable(label)) {
                topImage->setColorEncodingType(type);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Text(topImage->filepath().c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(topImage->filepath().c_str());
    }


    auto imageSize = topImage->size();
    ImGui::Text("%.0fx%.0f", imageSize.x, imageSize.y);
    //ImGui::Text("8 bit");
    ImGui::NextColumn();
}

void    App::initHomeWindow(const char* name)
{
    bool open = true;
    
    ImGui::SetNextWindowContentWidth(500.0f);
    if (ImGui::BeginPopupModal(name, &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        //ImGui::Dummy(Vec2f(550.0f, ImGui::GetFrameHeight()));
        if (ImGui::BeginTabBar("ControlsBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Controls")) {
                ImGui::Columns(2, nullptr, false);

                ImGui::Text("Show Home Panel");
                ImGui::Text("Toggle Property Window");
                ImGui::Text("Toggle Split View");
                ImGui::Text("Toggle Side-by-Side Column View");
                ImGui::Text("Toggle Pixel Warning Markers");
                ImGui::Text("Toggle Linear Image Filter");
                ImGui::NextColumn();

                ImGui::Text("F1");
                ImGui::Text("Tab");
                ImGui::Text("S");
                ImGui::Text("C");
                ImGui::Text("W");
                ImGui::Text("Q");
                ImGui::NextColumn();
                ImGui::Separator();

                ImGui::Text("Import Images");
                ImGui::Text("Export Session");
                ImGui::Text("Reload Selected Image");
                ImGui::Text("Close Selected Image");
                ImGui::Text("Close All Images");
                ImGui::NextColumn();

                ImGui::Text("Ctrl+O");
                ImGui::Text("Ctrl+E");
                ImGui::Text("F5");
                ImGui::Text("Backspace/Del");
                ImGui::Text("Ctrl+Shift+W");
                ImGui::NextColumn();

                ImGui::Separator();
                ImGui::Text("Pan");
                ImGui::Text("Offset Column View");
                ImGui::Text("Zoom In/Out\n ");
                ImGui::Text("Zoom In/Out in Power-of-Two");
                ImGui::Text("Zoom to Actual Size");
                ImGui::Text("Fit to Viewport");
                ImGui::Text("Sync Column Views");
                ImGui::Text("Pixel Sniper");
                ImGui::NextColumn();

                ImGui::Text("Drag Left Mouse Button");
                ImGui::Text("Drag Left Mouse Button + Alt");
                ImGui::Text("Mouse Scroll or\nDrag Left + Right Mouse Buttons Vertically");
                ImGui::Text("+/-");
                ImGui::Text("/ or F");
                ImGui::Text("Shift+F");
                ImGui::Text("Alt+C");
                ImGui::Text("Holding Z");
                ImGui::NextColumn();

                ImGui::Separator();
                ImGui::Text("Next Compared Image");
                ImGui::Text("Previous Compared Image");
                ImGui::Text("Switch Compared Images");
                ImGui::NextColumn();

                ImGui::Text("D or Right Arrow");
                ImGui::Text("A or Left Arrow");
                ImGui::Text("X or Up Arrow");
                //ImGui::NextColumn();

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Dummy(Vec2f(ImGui::GetFrameHeight()));
        
        if (ImGui::BeginTabBar("AboutBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("About")) {
                ImGui::TextWrapped("Bak-Tsiu %s, an image viewer designed for comparing images and examining pixel differences.", VERSION);

                ImGui::Text(u8"\nCopyright© Shih-Chin Weng. https://github.com/shihchinw/baktsiu");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::EndPopup();
    }
}

bool    App::showRemoveImageDlg(const char* title)
{
    bool result = false;

    ImGui::SetNextWindowSizeConstraints(Vec2f(350.0f, 100.0f), Vec2f(400.0f, 250.0f));
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Close image %s?\n", getTopImage()->filepath().c_str());
        ImGui::Dummy(Vec2f(0.0f, ImGui::GetCurrentContext()->FontSize));
        ImGui::Separator();

        auto& style = ImGui::GetStyle();
        float buttonWidth = (ImGui::GetWindowWidth() - style.ItemSpacing.x) * 0.5f - style.FramePadding.x * 2.0f;
        if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) { ImGui::CloseCurrentPopup(); result = true; }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    return result;
}

bool    App::showClearImagesDlg(const char* title)
{
    bool result = false;
    
    ImGui::SetNextWindowSizeConstraints(Vec2f(300.0f, 80.0f), Vec2f(300.0f, 120.0f));
    if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Close all images?\n");
        ImGui::Dummy(Vec2f(0.0f, ImGui::GetCurrentContext()->FontSize));
        ImGui::Separator();
        
        auto& style = ImGui::GetStyle();
        float buttonWidth = (ImGui::GetWindowWidth() - style.ItemSpacing.x) * 0.5f - style.FramePadding.x * 2.0f;
        if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) { ImGui::CloseCurrentPopup(); result = true; }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    return result;
}

void    App::showImageNameOverlays()
{
    ImGuiIO& io = ImGui::GetIO();

    const float padding = 8.0f;

    Vec2f windowPos(padding, mToolbarHeight + padding);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##ImageOverlay1", nullptr, windowFlags)) {
        ImGui::Text(mImageList[mTopImageIndex]->filename().c_str());
    }
    ImGui::End();

    ImGuiContext& g = *ImGui::GetCurrentContext();
    const float imagePropHandleWidth = g.FontSize + g.Style.FramePadding.x * 4.0f;
    const std::string& cmpImageName = mImageList[mCmpImageIndex]->filename();
    windowPos.x = io.DisplaySize.x - padding - ImGui::CalcTextSize(cmpImageName.c_str()).x - imagePropHandleWidth;
    if (mShowImagePropWindow || mPopupImagePropWindow) {
        windowPos.x -= getPropWindowWidth();
    }

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    if (ImGui::Begin("##ImageOverlay2", nullptr, windowFlags)) {
        ImGui::Text(cmpImageName.c_str());
    }
    ImGui::End();
}

void App::showHeatRangeOverlay(const Vec2f& pos, float width)
{
    ImGuiContext& g = *ImGui::GetCurrentContext();

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration 
        | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings 
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    Vec2f barSize(width, 12.0f);

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(Vec2f(0, 20.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Vec2f(8.0f, 4.0f));
    ImGui::PushFont(mSmallFont);
    //ImGui::SetNextWindowBgAlpha(0.5f);
    if (ImGui::Begin("##ImageOverlayBar", nullptr, windowFlags)) {
        ImGui::Text("low");
        ImGui::SameLine();
        ImGui::Dummy(barSize);

        ImU32 hues[16 + 1];
        for (size_t i = 0; i <= 16; i++) {
            float r = i / 16.0f;
            float g = glm::sin(180.0f * glm::radians(r));
            float b = glm::cos(60.0f * glm::radians(r));
            hues[i] = IM_COL32(r * 255, g * 255, b * 255, 255);
        }

        Vec2f rampBarPadding = g.Style.WindowPadding + pos;
        rampBarPadding.x += ImGui::CalcTextSize("low").x + g.Style.ItemSpacing.x;

        ImDrawList* drawList = ImGui::GetCurrentWindow()->DrawList;
        for (int i = 0; i < 16; ++i) {
            const auto& startHue = hues[i];
            const auto& endHue = hues[i + 1];
            drawList->AddRectFilledMultiColor(
                Vec2f(barSize.x * i / 16.0, 0.0f) + rampBarPadding, 
                Vec2f(barSize.x * (i + 1) / 16.0, barSize.y) + rampBarPadding, 
                startHue, endHue, endHue, startHue);
        }

        ImGui::SameLine();
        ImGui::Text("high");
    }
    ImGui::End();
    ImGui::PopFont();
    ImGui::PopStyleVar(1);
}

//---------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------

void App::appendAction(Action&& action)
{
    mActionStack.push_back(action);
    if (mActionStack.size() > 16) {
        // Only keep the latest 16 actions.
        mActionStack.pop_front();
    }
}

void App::undoAction()
{
    if (mActionStack.empty()) {
        return;
    } else if (mCurAction.type != Action::Type::Unknown) {
        // Skip instruction if there is an undo action running currently.
        return;
    }

    Action action = mActionStack.back();
    mActionStack.pop_back();

    if (action.type == Action::Type::Remove) {
        mCurAction = action;
        importImageFiles(action.filepathArray, false, &action.imageIdxArray);
    } else if (action.type == Action::Type::Add) {
        const int imageCount = static_cast<int>(mImageList.size());
        for (int i = imageCount - 1; i >= 0; --i) {
            for (auto index : action.imageIdxArray) {
                uint8_t layerIndex = Action::extractLayerIndex(index);
                if (layerIndex == i) {
                    mImageList.erase(mImageList.begin() + layerIndex);
                    break;
                }
            }
        }

        mTopImageIndex = action.prevTopImageIdx;
        mCmpImageIndex = action.prevCmpImageIdx;
        mTexturePool.cleanUnusedTextures();
    } else if (action.type == Action::Type::Move) {
        std::vector<ImageUPtr> newImageList;
        newImageList.reserve(mImageList.size());

        for (auto index : action.imageIdxArray) {
            uint8_t id = Action::extractImageId(index);
            for (auto& image : mImageList) {
                if (image && image->id() == id) {
                    newImageList.push_back(std::move(image));
                    break;
                }
            }
        }

        mImageList.swap(newImageList);
        mTopImageIndex = action.prevTopImageIdx;
        mCmpImageIndex = action.prevCmpImageIdx;
    }
}

void    App::showImportImageDlg()
{
    static std::vector<std::string> filters = {
        "Supported Image Files", "*.bmp; *.exr; *.gif; *.jpg; *.hdr; *.png; *.bts" ,
        "BMP (*.BMP)", "*.bmp",
        "OpenEXR (*.EXR)", "*.exr",
        "GIF (*.GIF)", "*.gif",
        "JPEG (*.JPG, *.JPEG)", "*.jpg; *.jpeg",
        "HDR (*.HDR)", "*.hdr",
        "PNG (*.PNG)", "*.png",
        "Bak-Tsiu Session (*.BTS)", "*.bts",
    };

    std::vector<std::string> selection = pfd::open_file("Select image file(s)", "", filters, true).result();
    auto iter = selection.begin();
    while (iter != selection.end()) {
        if (endsWith(*iter, ".bts")) {
            openSession(*iter);
            iter = selection.erase(iter);
        } else {
            ++iter;
        }
    }

    if (!selection.empty()) {
        importImageFiles(selection, true);
    }
}

void    App::showExportSessionDlg()
{
    static std::vector<std::string> filters = {
        "Bak-Tsiu Session (*.BTS)", "*.bts",
    };

    std::string filepath = pfd::save_file("Export compare session", "", filters, true).result();

    if (!filepath.empty()) {
        if (!endsWith(filepath, ".bts")) {
            filepath += ".bts";
        }

        saveSession(filepath);
    }
}

// This function is executed in main thread.
void    App::importImageFiles(const std::vector<std::string>& filepathArray, 
                              bool recordAction, std::vector<uint16_t>* imageIdxArray)
{
    const size_t imageNum = filepathArray.size();

    if (imageNum == 0) {
        return;
    }

    Action action(Action::Type::Add, mTopImageIndex, mCmpImageIndex);

    for (size_t i = 0; i < imageNum; ++i) {
        std::string path = filepathArray[i];
        std::replace(path.begin(), path.end(), '\\', '/');

        if (!Texture::isSupported(path)) {
            promptWarning(fmt::format("Unsupported image type for \"{}\"", path));
            continue;
        }
        
        uint8_t insertIdx = static_cast<uint8_t>(mImageList.size());
        uint8_t id = 0;

        if (imageIdxArray) {
            uint16_t value = (*imageIdxArray)[i];
            id = Action::extractImageId(value);
            insertIdx = Action::extractLayerIndex(value);

            if (insertIdx <= mTopImageIndex) {
                ++mTopImageIndex;
            }

            if (insertIdx <= mCmpImageIndex) {
                ++mCmpImageIndex;
            }
        }

        auto& newTexture = mTexturePool.acquireTexture(path);
        auto& newImage = std::make_unique<Image>(newTexture, id);
        mImageList.insert(mImageList.begin() + insertIdx, std::move(newImage));

        const ImageType imageType = Texture::getImageType(path);
        if (imageType == ImageType::HDR || imageType == ImageType::OPENEXR) {
            newImage->setColorEncodingType(ColorEncodingType::Linear);
        }

        action.filepathArray.push_back(path);
        action.imageIdxArray.push_back(Action::composeImageIndex(id, insertIdx));
        mUpdateImageSelection = true;
    }
    
    if (recordAction) {
        appendAction(std::move(action));
    }
}

Image* App::getTopImage()
{
    return mTopImageIndex >= 0 ? mImageList[mTopImageIndex].get() : nullptr;
}

void    App::removeTopImage(bool recordAction)
{
    ImageUPtr image = std::move(mImageList[mTopImageIndex]);
    mImageList.erase(mImageList.begin() + mTopImageIndex);

    if (recordAction) {
        Action action(Action::Type::Remove, mTopImageIndex, mCmpImageIndex);
        action.filepathArray.push_back(image->filepath());
        action.imageIdxArray.push_back(Action::composeImageIndex(image->id(), mTopImageIndex));
        appendAction(std::move(action));
    }

    image.reset();
    mTexturePool.cleanUnusedTextures();
    
    const int imageNum = static_cast<int>(mImageList.size());
    if (imageNum < 2) {
        mCmpImageIndex = -1;
        mCompositeFlags = CompositeFlags::Top;
    }
    
    if (imageNum == 0) {
        mTopImageIndex = -1;
    } else {
        mTopImageIndex = std::min(mTopImageIndex, imageNum - 1);
    }
}

void    App::clearImages(bool recordAction)
{
    if (recordAction) {
        Action action(Action::Type::Remove, mTopImageIndex, mCmpImageIndex);

        for (int index = 0; index < mImageList.size(); ++index) {
            action.filepathArray.push_back(mImageList[index]->filepath());
            action.imageIdxArray.push_back(Action::composeImageIndex(mImageList[index]->id(), index));
        }

        mActionStack.push_back(action);
    }

    mImageList.clear();
    mTopImageIndex = mCmpImageIndex = -1;
    mCompositeFlags = CompositeFlags::Top;
}

void    App::resetImageTransform(const Vec2f &imgSize, bool fitWindow)
{
    mImageScale = 1.0f;

    mView.reset(fitWindow);
    mColumnViews[0].reset(fitWindow);
    mColumnViews[1].reset(fitWindow);
}

bool App::getImageCoordinates(Vec2f viewportCoords, Vec2f& outImageCoords) const
{
    viewportCoords.y = ImGui::GetIO().DisplaySize.y - viewportCoords.y;

    bool isOutsideImage = false;
    if (!inSideBySideMode()) {
        outImageCoords = mView.getImageCoords(viewportCoords, &isOutsideImage);
    } else {
        auto& io = ImGui::GetIO();
        const float leftColumnWidth = glm::round(io.DisplaySize.x * mViewSplitPos);
        if (viewportCoords.x > leftColumnWidth) {
            viewportCoords.x -= leftColumnWidth;
            outImageCoords = mColumnViews[1].getImageCoords(viewportCoords, &isOutsideImage);
        } else {
            outImageCoords = mColumnViews[0].getImageCoords(viewportCoords, &isOutsideImage);
        }
    }

    outImageCoords = glm::floor(outImageCoords);

    return !isOutsideImage;
}

float App::getPropWindowWidth() const
{
    ImGuiWindow* window = ImGui::FindWindowByName(kImagePropWindowName);
    return window == nullptr ? 250.0f : window->Size.x;
}

int App::getPixelMarkerFlags() const
{
    if (!mShowPixelMarker) {
        return 0;
    }

    PixelMarkerFlags flags = mPixelMarkerFlags;
    if (inCompareMode()) {
        flags &= PixelMarkerFlags::DiffMask;  // Only keeps pixel differnce marker.
    } else {
        flags &= ~PixelMarkerFlags::DiffMask; // Clear any flags for difference modes.
    }

    return static_cast<int>(flags);
}

inline void    App::toggleSplitView()
{
    if (mCmpImageIndex != -1) {
        mCompositeFlags = toggleFlags(mCompositeFlags & ~CompositeFlags::SideBySide, CompositeFlags::Split);
    }
}

inline void    App::toggleSideBySideView()
{
    if (mCmpImageIndex != -1) {
        mCompositeFlags = toggleFlags(mCompositeFlags & ~CompositeFlags::Split, CompositeFlags::SideBySide);
    }
}

inline bool    App::inCompareMode() const
{
    return (mCompositeFlags & (CompositeFlags::Split | CompositeFlags::SideBySide)) != CompositeFlags::Top;
}

inline bool    App::inSideBySideMode() const
{
    return (mCompositeFlags & CompositeFlags::SideBySide) != CompositeFlags::Top;
}

inline bool    App::shouldShowSplitter() const
{
    return inCompareMode() && (getPixelMarkerFlags() & static_cast<int>(PixelMarkerFlags::DiffMask)) == 0;
}

void    App::onFileDrop(int count, const char* filepaths[])
{
    std::vector<std::string> filepathArray;
    filepathArray.reserve(count);

    for (int i = 0; i < count; ++i) {
        const std::string &filepath = filepaths[i];

        if (endsWith(filepath, ".bts")) {
            openSession(filepath);
        } else {
            filepathArray.push_back(filepath);
        }
    }

    importImageFiles(filepathArray, true);
}

void    App::openSession(const std::string& filepath)
{
    fx::gltf::Document sessionFile = fx::gltf::LoadFromText(filepath);
    if ("baktsiu" != sessionFile.asset.generator) {
        promptError("Input is not a valid Bak-Tsiu session file");
        return;
    }

    std::vector<std::string> filepathArray;
    filepathArray.reserve(sessionFile.images.size());
    
    for (auto &imageProp : sessionFile.images) {
        filepathArray.push_back(imageProp.uri);
    }

    importImageFiles(filepathArray, true);
}

void    App::saveSession(const std::string& filepath)
{
    fx::gltf::Document sessionFile;
    sessionFile.asset.generator = "baktsiu";

    for (auto &image : mImageList) {
        fx::gltf::Image imageProp;
        imageProp.uri = image->filepath();
        sessionFile.images.push_back(imageProp);
    }

    fx::gltf::Save(sessionFile, filepath, false);
}

}  // namespace baktsiu