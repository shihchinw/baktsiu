#version 330

uniform sampler2D uImage1;
uniform sampler2D uImage2;
uniform sampler2D uFontImage;   // Font bit map for digit characters .0-9

uniform vec2    uOffset;
uniform vec2    uOffsetExtra;
uniform vec2    uRelativeOffset;
uniform vec2    uImageSize;
uniform vec2    uWindowSize;
uniform vec2    uCursorPos;
uniform vec3    uPixelBorderHighlightColor;

uniform float   uSplitPos;
uniform float   uImageScale;
uniform int     uPresentMode;

uniform float   uDisplayGamma;
uniform int     uOutTransformType;
uniform int     uPixelMarkerFlags;
uniform int     uSideBySide;
uniform vec4    uCharUvRanges[11];
uniform vec4    uCharUvXforms[11];
uniform bool    uApplyToneMapping;
uniform bool    uBlendWithImageAlpha;
uniform bool    uEnablePixelHighlight;

in  vec2 vUV;
out vec4 oColor;

//-----------------------------------------------------------------------------
// Color Transfor Matrices
//-----------------------------------------------------------------------------
// The color space transformation defined in ACES CTL is row vector-times-matrix.
// Therefore we define mul as y * x to both match CTL for better reference.
// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/README-MATRIX.md
#define mul(x, y) (y * x)

const mat3 AP1_2_XYZ_MAT = mat3(
     0.6624541811, 0.1340042065, 0.1561876870,
     0.2722287168, 0.6740817658, 0.0536895174,
    -0.0055746495, 0.0040607335, 1.0103391003);

const mat3 XYZ_2_AP1_MAT = mat3(
     1.6410233797, -0.3248032942, -0.2364246952,
    -0.6636628587,  1.6153315917,  0.0167563477,
     0.0117218943, -0.0082844420,  0.9883948585);

const mat3 AP1_2_BT709_MAT = mat3(
    1.7050515, -0.6217907, -0.0832587,
   -0.1302571,  1.1408029, -0.0105482,
   -0.0240033, -0.1289688,  1.1529717);

const mat3 AP1_2_P3D65_MAT = mat3(
    1.3792145, -0.3088633, -0.0703498,
   -0.0693355,  1.0822950, -0.0129618,
   -0.0021590, -0.0454592,  1.0476177);

const mat3 AP1_2_BT2020_MAT = mat3(
    1.0258249, -0.0200529, -0.0057714,
   -0.002235 ,  1.0045849, -0.0023520,
   -0.0050133, -0.0252900,  1.0303028);

const mat3 AP1_2_AP0_MAT = mat3(
     0.6954522414, 0.1406786965, 0.1638690622,
     0.0447945634, 0.8596711185, 0.0955343182,
    -0.0055258826, 0.0040252103, 1.0015006723);

const mat3 AP0_2_AP1_MAT = mat3(
     1.4514393161, -0.2365107469, -0.2149285693,
    -0.0765537734,  1.1762296998, -0.0996759264,
     0.0083161484, -0.0060324498,  0.9977163014);

//-----------------------------------------------------------------------------
// Color Transform Functions
//-----------------------------------------------------------------------------

float labf(float v)
{
    const float c1 = 0.008856451679;    // pow(6.0/29.0, 3.0);
    const float c2 = 7.787037037;       // pow(29.0/6.0, 2.0)/3;
    const float c3 = 0.1379310345;      // 16.0/116.0
    return mix(c2 * v + c3, pow(v, 1.0 / 3.0), v > c1);
}

vec3 XYZtoLab(vec3 xyz)
{
    const vec3 D65WhitePoint = vec3(0.95047, 1.000, 1.08883);
    xyz /= D65WhitePoint;

    vec3 v = vec3(labf(xyz.x), labf(xyz.y), labf(xyz.z));
    return vec3((116.0 * v.y) - 16.0,
                 500.0 * (v.x - v.y),
                 200.0 * (v.y - v.z));
}

//-----------------------------------------------------------------------------
// ACES Tone Mapping ported from: https://github.com/ampas/aces-dev/tree/master/transforms/ctl
// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/lib/ACESlib.Utilities_Color.ctl
//-----------------------------------------------------------------------------
#define HALF_MIN 5.96e-08
#define HALF_MAX 65504.0
#define PI 3.14159265359

mediump float rgb_2_saturation(vec3 rgb)
{
    const mediump float TINY = HALF_MIN;
    mediump float ma = max(rgb.r, max(rgb.g, rgb.b));
    mediump float mi = min(rgb.r, min(rgb.g, rgb.b));
    return (max(ma, TINY) - max(mi, TINY)) / max(ma, 1e-2);
}

mediump float rgb_2_yc(vec3 rgb)
{
    const mediump float ycRadiusWeight = 1.75;

    // Converts RGB to a luminance proxy, here called YC
    // YC is ~ Y + K * Chroma
    // Constant YC is a cone-shaped surface in RGB space, with the tip on the
    // neutral axis, towards white.
    // YC is normalized: RGB 1 1 1 maps to YC = 1
    //
    // ycRadiusWeight defaults to 1.75, although can be overridden in function
    // call to rgb_2_yc
    // ycRadiusWeight = 1 -> YC for pure cyan, magenta, yellow == YC for neutral
    // of same value
    // ycRadiusWeight = 2 -> YC for pure red, green, blue  == YC for  neutral of
    // same value.

    mediump float r = rgb.x;
    mediump float g = rgb.y;
    mediump float b = rgb.z;
    mediump float chroma = sqrt(b * (b - g) + g * (g - r) + r * (r - b));
    return (b + g + r + ycRadiusWeight * chroma) / 3.0;
}

mediump float rgb_2_hue(vec3 rgb)
{
    // Returns a geometric hue angle in degrees (0-360) based on RGB values.
    // For neutral colors, hue is undefined and the function will return a quiet NaN value.
    mediump float hue;
    if (rgb.x == rgb.y && rgb.y == rgb.z) {
        hue = 0.0; // RGB triplets where RGB are equal have an undefined hue
    } else {
        hue = (180.0 / PI) * atan(sqrt(3.0) * (rgb.y - rgb.z), 2.0 * rgb.x - rgb.y - rgb.z);
    }

    if (hue < 0.0) hue = hue + 360.0;

    return hue;
}

mediump float center_hue(mediump float hue, mediump float centerH)
{
    mediump float hueCentered = hue - centerH;
    if (hueCentered < -180.0) hueCentered = hueCentered + 360.0;
    else if (hueCentered > 180.0) hueCentered = hueCentered - 360.0;
    return hueCentered;
}

mediump float sigmoid_shaper(mediump float x)
{
    // Sigmoid function in the range 0 to 1 spanning -2 to +2.

    mediump float t = max(1.0 - abs(x / 2.0), 0.0);
    mediump float y = 1.0 + sign(x) * (1.0 - t * t);

    return y / 2.0;
}

mediump float glow_fwd(mediump float ycIn, mediump float glowGainIn, mediump float glowMid)
{
    mediump float glowGainOut;

    if (ycIn <= 2.0 / 3.0 * glowMid) {
        glowGainOut = glowGainIn;
    } else if (ycIn >= 2.0 * glowMid) {
        glowGainOut = 0.0;
    } else {
        glowGainOut = glowGainIn * (glowMid / ycIn - 1.0 / 2.0);
    }

    return glowGainOut;
}

vec3 XYZ_2_xyY(vec3 XYZ)
{
    mediump float divisor = max(dot(XYZ, vec3(1.0)), HALF_MIN);
    return vec3(XYZ.xy / divisor, XYZ.y);
}

vec3 xyY_2_XYZ(vec3 xyY)
{
    mediump float m = xyY.z / max(xyY.y, HALF_MIN);
    vec3 XYZ = vec3(xyY.xz, (1.0 - xyY.x - xyY.y));
    XYZ.xz *= m;
    return XYZ;
}

const mediump float DIM_SURROUND_GAMMA = 0.9811;
const mediump float RRT_GLOW_GAIN = 0.05;
const mediump float RRT_GLOW_MID = 0.08;
const mediump float RRT_RED_SCALE = 0.82;
const mediump float RRT_RED_PIVOT = 0.03;
const mediump float RRT_RED_HUE = 0.0;
const mediump float RRT_RED_WIDTH = 135.0;

const mat3 RRT_SAT_MAT = mat3(
    0.9708890, 0.0269633, 0.00214758,
    0.0108892, 0.9869630, 0.00214758,
    0.0108892, 0.0269633, 0.96214800);

const mat3 ODT_SAT_MAT = mat3(
    0.949056, 0.0471857, 0.00375827,
    0.019056, 0.9771860, 0.00375827,
    0.019056, 0.0471857, 0.93375800);


vec3 darkSurround_to_dimSurround(vec3 linearCV)
{
    vec3 XYZ = mul(AP1_2_XYZ_MAT, linearCV);

    vec3 xyY = XYZ_2_xyY(XYZ);
    xyY.z = clamp(xyY.z, 0.0, HALF_MAX);
    xyY.z = pow(xyY.z, DIM_SURROUND_GAMMA);
    XYZ = xyY_2_XYZ(xyY);

    return mul(XYZ_2_AP1_MAT, XYZ);
}

//! This is a numerical fitted version.
//! @param aces Linear encoded color with AP0 color parmaries.
//! @return Linear encoded color in AP1 color space.
vec3 AcesToneMapping(vec3 aces)
{
    // --- Glow module --- //
    float saturation = rgb_2_saturation(aces);
    float ycIn = rgb_2_yc(aces);
    float s = sigmoid_shaper((saturation - 0.4) / 0.2);
    float addedGlow = 1.0 + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);
    aces *= addedGlow;

    // --- Red modifier --- //
    float hue = rgb_2_hue(aces);
    float centeredHue = center_hue(hue, RRT_RED_HUE);
    float hueWeight;
    {
        //hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);
        hueWeight = smoothstep(0.0, 1.0, 1.0 - abs(2.0 * centeredHue / RRT_RED_WIDTH));
        hueWeight *= hueWeight;
    }

    aces.r += hueWeight * saturation * (RRT_RED_PIVOT - aces.r) * (1.0 - RRT_RED_SCALE);

    // --- ACES to RGB rendering space --- //
    aces = max(vec3(0.0), aces);
    vec3 rgbPre = mul(AP0_2_AP1_MAT, aces);
    rgbPre = clamp(rgbPre, vec3(0.0), vec3(HALF_MAX));

    // --- Global desaturation --- //
    rgbPre = mul(RRT_SAT_MAT, rgbPre);

    // Apply achromic curve that represents (post RRT + pre ODT).
    // See the link below for the fitting process of curve coefficients
    // https://github.com/shihchinw/numex/blob/master/notebooks/aces_color_transform.ipynb
    const float a = 180.08877305;
    const float b = 5.82507674;
    const float c = 190.14106451;
    const float d = 56.89654471;
    const float e = 53.22517853;

    vec3 rgbPost = (rgbPre * (a * rgbPre + b)) / (rgbPre * (c * rgbPre + d) + e);

    // Apply gamma adjustment to compensate for dim surround
    vec3 linearCV = darkSurround_to_dimSurround(rgbPost);

    // Apply desaturation to compensate for luminance difference
    return mul(ODT_SAT_MAT, linearCV);
}

//! Apply color filter.
//! @param color Linear color in AP1 space.
vec3 colorTransform(vec3 color, int mode)
{
    if (uApplyToneMapping) {
        color = AcesToneMapping(mul(AP1_2_AP0_MAT, color)); // Output result is in AP1.
    }

    if (mode == 1) {
        color = color.rrr;
    } else if (mode == 2) {
        color = color.ggg;
    } else if (mode == 3) {
        color = color.bbb;
    } else if (mode == 4) {
        color = mul(AP1_2_XYZ_MAT, color).yyy;
    } else if (mode >= 5 && mode < 8) { // L*, a*, b*
        const vec3 labMin = vec3(0, -128, -128);
        const vec3 labMax = vec3(100, 128, 128);

        vec3 lab = XYZtoLab(mul(AP1_2_XYZ_MAT, color));
        color = ((lab - labMin) / (labMax - labMin));
        color = vec3(color[mode - 5]);
    }

    return color;
}

// Transform color from AP1 to target color space.
// This is ODT (Ouput Device Transform)
vec3 outputTransform(vec3 color, int type, float gamma)
{
    if (type == 0) {
        color = mul(AP1_2_BT709_MAT, color);
    } else if (type == 1) {
        color = mul(AP1_2_P3D65_MAT, color);
    } else if (type == 2) {
        color = mul(AP1_2_BT2020_MAT, color);
    }

    color = max(color, vec3(0.0));  // Color values for output device should be positive.
    return pow(color, vec3(1.0 / gamma)); // Apply display gamma
}

// Return heat mapped color by given scalar value.
vec3 getHeatColor(float value)
{
    float r = clamp(value, 0, 1.0);
    float g = sin(180.0 * radians(value));
    float b = cos(60.0 * radians(value));
    return vec3(r, g, b);
}

float getColorDistance(vec3 color1, vec3 color2)
{
    vec3 lab1 = XYZtoLab(mul(AP1_2_XYZ_MAT, color1));
    vec3 lab2 = XYZtoLab(mul(AP1_2_XYZ_MAT, color2));
    vec3 diff = lab1 - lab2;
    return dot(diff, diff);
}

vec3 getCheckerColor(vec2 uv, vec2 windowSize)
{
    vec2 bgUV = uv * vec2(1.0, windowSize.y / windowSize.x);    // Make uv isotropic
    ivec2 iwh = ivec2(round(fract(bgUV * windowSize.y * 0.08)));
    return vec3(iwh.x ^ iwh.y) * 0.18;
}

vec4 overlayPixelMarker(vec4 color, int markerFlags)
{
    bool isOverflow = all(greaterThan(color.rgb, vec3(1.0))) && ((markerFlags & 0x4) != 0);
    color = mix(color, vec4(1.0, 0.0, 0.0, 1.0), vec4(isOverflow));

    bool isUnderflow = all(lessThan(color.rgb, vec3(1e-5))) && ((markerFlags & 0x8) != 0);
    color = mix(color, vec4(0.0, 0.0, 1.0, 1.0), vec4(isUnderflow));

    return color;
}

// Return the matte of given digit.
// @param st The texture coordinates of the bounding box of one digit.
float getDigitMatte(vec2 st, int d)
{
    st.y = 1.0 - st.y;  // The char local uv is upside down, thus we need to reverse it.
    vec2 fuv = uCharUvXforms[d].xy;
    st -= uCharUvXforms[d].zw;
    st.y += 0.25;	// Shift base line

    vec2 mask = step(0.0, st) - step(fuv, st);
    if (any(equal(mask, vec2(0.0)))) {
        return 0.0;
    }

    vec2 uvFont = uCharUvRanges[d].xy + (uCharUvRanges[d].zw - uCharUvRanges[d].xy) * st / fuv;
    return texture(uFontImage, uvFont).r;
}

// Get matte for the numerical values of RGB components within a pixel.
float getRGBValueMatte(vec2 uv, vec3 color)
{
    const vec2 padding = vec2(0.16, 0.04);

	const float digitWidth = (1.0 - padding.x * 2.0) / 5;
    const float digitHeight = 0.28;
    const float rowSpace = (1.0 - padding.y * 2.0 - digitHeight * 3.0) * 0.5;

    float curHeight = padding.y;
    float yMask1 = step(curHeight, uv.y) - step(curHeight + digitHeight, uv.y);

    curHeight += digitHeight + rowSpace;
    float yMask2 = step(curHeight, uv.y) - step(curHeight + digitHeight, uv.y);

    curHeight += digitHeight + rowSpace;
    float yMask3 = step(curHeight, uv.y) - step(curHeight + digitHeight, uv.y);     
    float yMask = yMask1 + yMask2 + yMask3;

    int rowIdx = int(yMask3 * 2 + yMask2);
    float value = color[2 - rowIdx];

    int exponent = int(max(0, log(value) / 2.302585));     // max log10

    // If the value is in [1000.0, 10000.0), we print 4 digits only (hide the decimal point).
    // Thus we offset uv to half digit width.
    uv.x -= digitWidth * 0.5 * float(exponent == 3);

    float xMask = step(padding.x, uv.x) - step(1.0 - padding.x, uv.x);
    int colIdx = int(floor((uv.x - padding.x) / digitWidth));

    if (xMask * yMask == 0.0 || (exponent == 3 && colIdx == 4)) {
        return 0.0;
    }

    int digitIdx = 0;

    for (int i = 0; i <= colIdx; ++i) {
        int isFraction = int(i > exponent);
        float y = pow(10.0, exponent - i + isFraction);
        digitIdx = int(max(0, floor(value / y)));
        value -= digitIdx * y;
    }

    digitIdx = colIdx == (exponent + 1) ? 10 : digitIdx;

    // Transform uv to the bounding box of each digit.
    uv.x = uv.x - padding.x - digitWidth * colIdx;
    uv.y = uv.y - padding.y - (rowSpace + digitHeight) * rowIdx;
    return getDigitMatte(uv / vec2(digitWidth, digitHeight), digitIdx);
}

bool showPixelBorder(vec2 wh, vec2 offset, float imageScale)
{
    // vec2 xy = mod(wh - offset, imageScale);
    // return any(lessThan(xy, vec2(1.0))) && (imageScale > 5.0);

    // We have to use floor to avoid inconsistent pixel border width.
    // We can't use the expressions above, they are not exactly the same, 
    // even though I don't know why.
    vec2 xy = floor(mod(wh - offset, imageScale));
    return any(equal(xy, vec2(0.0))) && (imageScale > 5.0);
}

bool showPixelBorderHighlight(vec2 wh, vec2 cursor, vec2 offset, float imageScale)
{
    if (!uEnablePixelHighlight || imageScale < 8.0) {
        return false;
    }

    float borderWidth = 1 + floor(step(46.0, imageScale));
    vec2 xy = floor((wh - offset) / imageScale);
    vec2 st = floor((cursor - offset) / imageScale);
    vec2 lower = mod(wh - offset, imageScale);
    vec2 upper = mod(wh - offset + vec2(borderWidth), imageScale);

    return all(equal(xy - st, vec2(0.0))) && (any(lessThanEqual(lower, vec2(borderWidth))) || any(lessThanEqual(upper, vec2(borderWidth))));
}

vec3 drawRGBValues(vec2 wh, vec2 offset, float imageScale, vec3 linearColor, vec3 displayColor)
{
    // Draw RGB values within pixel box.
    vec2 xy = mod(wh - offset, imageScale);
    float opacity = clamp((imageScale - 32.0) / 48.0, 0.0, 1.0);
    opacity *= getRGBValueMatte(xy / imageScale, linearColor);

    float luminance = mul(AP1_2_XYZ_MAT, displayColor.rgb).y;
    vec3 matteColor = mix(vec3(0.85), vec3(0.15), vec3(luminance > 0.5));
    displayColor = clamp(displayColor, vec3(0.0), vec3(1.0));
    return mix(displayColor, matteColor, opacity);
}

//! @param wh Pixel coordinates in window.
//! @param offset Image position in window coordinates.
//! @param imageSize Scaled image size for display.
//! @param cursorPos Cursor position in window coordinates.
//! @param image1 Texture sampler of left image.
//! @param image2 Texture sampler of right image.
//! @param uvOffset The relative UV offset for image2.
vec4 showImage(vec2 wh, vec2 offset, vec2 imageSize, vec2 cursorPos,
    in sampler2D image1, in sampler2D image2, vec2 uvOffset, vec3 bgColor)
{
    vec4 result = vec4(0.0, 0.0, 0.0, 1.0);

    // Add one extra pixel to draw the top and right pixel border.
    vec2 regionMask = step(offset, wh) - step(offset + imageSize + 1, wh);
    if (regionMask.x * regionMask.y == 0.0) {
        result.rgb = bgColor;
        return result;
    }
   
    vec2 imageUV = (wh - offset) / imageSize;
    vec4 color1 = texture(image1, imageUV);
    result = color1;

    bool inDiffMode = (uPixelMarkerFlags & 0x3) != 0;
    bool enableHeatMap = (uPixelMarkerFlags & 0x2) >> 1 != 0;
    vec3 linearColor = color1.rgb;

    if (inDiffMode) {
        imageUV += uvOffset;
        regionMask = step(0.0, imageUV) - step(1.0 + 1e-5, imageUV);

        float squareError = 1.0;
        if (regionMask.x * regionMask.y == 1.0) {
            vec4 color2 = texture(image2, imageUV);
            squareError = getColorDistance(color1.rgb, color2.rgb);
        }

        result.rgb = mix(color1.rgb, vec3(1.0, 0.0, 1.0), clamp(squareError, 0.0, 1.0));
        result.rgb = mix(result.rgb, getHeatColor(squareError), vec3(enableHeatMap));
    } else {
        result.rgb = colorTransform(color1.rgb, uPresentMode);
    }
    
    result = overlayPixelMarker(result, uPixelMarkerFlags);
    result.rgb = drawRGBValues(wh, offset, uImageScale, linearColor, result.rgb);
    result.rgb = outputTransform(result.rgb, uOutTransformType, mix(uDisplayGamma, 1.0, enableHeatMap));
    result.rgb = mix(result.rgb, bgColor, vec3((1.0 - result.a) * int(uBlendWithImageAlpha)));
    result.rgb = mix(result.rgb, vec3(0.7), vec3(showPixelBorder(wh, offset, uImageScale)));
    result.rgb = mix(result.rgb, uPixelBorderHighlightColor, vec3(showPixelBorderHighlight(wh, cursorPos, offset, uImageScale)));

    return result;
}

// We support unsynchronized translation in column view. Such relative offset
// is used to compute the cursor's position offset in the other view.
//! @param wh Pixel coordinates in [width, height].
//! @param offset Image offset in pixels. xy: left column, zw: right column offset.
//! @param curposPos Current mouse position in window coordinates.
//! @param imageSize Scaled image size for display.
vec4 renderSideBySide(vec2 wh, vec4 offset, vec2 relativeOffset, vec2 cursorPos, vec2 imageSize, vec2 uv, float splitPos)
{
    vec2 leftCursorPos, rightCursorPos;
    float leftColumnWidth = splitPos * uWindowSize.x;

    if (cursorPos.x > leftColumnWidth) {
        rightCursorPos = vec2(cursorPos.x - leftColumnWidth, cursorPos.y);
        vec2 ij = (rightCursorPos - offset.zw + relativeOffset) / uImageScale;
        leftCursorPos = round(ij * uImageScale + offset.xy);
    } else {
        leftCursorPos = cursorPos;
        vec2 ij = (leftCursorPos - offset.xy - relativeOffset) / uImageScale;
        rightCursorPos = round(ij * uImageScale + offset.zw);
    }

    vec3 bgColor = getCheckerColor(uv, uWindowSize);

    vec2 deltaUV = round(relativeOffset) / imageSize;
    vec4 color1 = showImage(wh, offset.xy, imageSize, leftCursorPos, uImage1, uImage2, -deltaUV, bgColor);

    wh.x = round(wh.x - splitPos * uWindowSize.x + 0.5) - 0.5;
    vec4 color2 = showImage(wh, offset.zw, imageSize, rightCursorPos, uImage2, uImage1, deltaUV, bgColor);

    return mix(color1, color2, vec4(uv.x > splitPos));
}

void main()
{
    oColor = vec4(0.0, 0.0, 0.0, 1.0);

    bool isSplitter = abs(vUV.x - uSplitPos) * uWindowSize.x < 1.0;
    bool showSplitter = uSplitPos != 1.0 && (uPixelMarkerFlags & 0x3) == 0; // Not in difference or heatmap view.
    
    vec2 wh = round(vUV * uWindowSize + vec2(0.5)) - vec2(0.5);

    if (uSideBySide == 1) {
        vec4 offset = vec4(uOffset, uOffsetExtra);
        oColor = renderSideBySide(wh, offset, uRelativeOffset, uCursorPos, uImageSize, vUV, uSplitPos);
        oColor = mix(oColor, vec4(1.0), vec4(isSplitter));
        return;
    }

    // Add one extra pixel to draw the top and right pixel border.
    vec2 regionMask = step(uOffset, wh) - step(uOffset + uImageSize + 1, wh);
    if (regionMask.x * regionMask.y == 0.0) {
        // When outside of image region, just draw background checker.
        oColor.rgb = getCheckerColor(vUV, uWindowSize);
        oColor = mix(oColor, vec4(1.0), vec4(isSplitter && showSplitter));
        return;
    }

    vec2 imageUV = (wh - uOffset) / uImageSize;
    vec4 color1 = texture(uImage1, imageUV);
    vec4 color2 = texture(uImage2, imageUV);
   
    bool inDiffMode = (uPixelMarkerFlags & 0x3) != 0;
    bool enableHeatMap = ((uPixelMarkerFlags & 0x2) >> 1) != 0;

    oColor = mix(color2, color1, vec4(vUV.x <= uSplitPos));
    vec3 linearColor = oColor.rgb;

    if (inDiffMode) { // Should only active in compare mode.
        float squareError = getColorDistance(color1.rgb, color2.rgb);
        oColor.rgb = mix(oColor.rgb, vec3(1.0, 0.0, 1.0), clamp(squareError, 0.0, 1.0));
        oColor.rgb = mix(oColor.rgb, getHeatColor(squareError), vec3(enableHeatMap));
        oColor.a = 1.0;
    } else {
        color1.rgb = colorTransform(color1.rgb, uPresentMode);
        color2.rgb = colorTransform(color2.rgb, uPresentMode);
        oColor = mix(color2, color1, vec4(vUV.x <= uSplitPos));
    }
    
    oColor = overlayPixelMarker(oColor, uPixelMarkerFlags);
    
    if (!inDiffMode) {
        oColor.rgb = drawRGBValues(wh, uOffset, uImageScale, linearColor, oColor.rgb);
    }

    oColor.rgb = outputTransform(oColor.rgb, uOutTransformType, mix(uDisplayGamma, 1.0, enableHeatMap));
    oColor.rgb = mix(oColor.rgb, getCheckerColor(vUV, uWindowSize), vec3((1.0 - oColor.a) * int(uBlendWithImageAlpha)));
    oColor.rgb = mix(oColor.rgb, vec3(0.7), vec3(showPixelBorder(wh, uOffset, uImageScale)));
    oColor.rgb = mix(oColor.rgb, uPixelBorderHighlightColor, vec3(showPixelBorderHighlight(wh, uCursorPos, uOffset, uImageScale)));
    
    oColor = mix(oColor, vec4(1.0), vec4(showSplitter && isSplitter));
}