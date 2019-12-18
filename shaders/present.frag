#version 330

uniform sampler2D uImage1;
uniform sampler2D uImage2;
uniform sampler2D uFontImage;   // Font bit map for digit characters .0-9

uniform vec2    uOffset;
uniform vec2    uImageSize;
uniform vec2    uWindowSize;
uniform vec2    uCursorPos;

uniform float   uSplitPos;
uniform float   uImageScale;
uniform int     uPresentMode;

uniform ivec2   uInImageProp1;  // x: encoding type, y: color primaries type
uniform ivec2   uInImageProp2;

uniform float   uDisplayGamma;
uniform int     uOutTransformType;
uniform int     uPixelMarkerFlags;
uniform int     uSideBySide;
uniform vec4    uCharUvRanges[11];
uniform vec4    uCharUvXforms[11];
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

const mat3 AP1_2_BT709_MAT = mat3(
    1.7050515, -0.6217907, -0.0832587,
   -0.1302571,  1.1408029, -0.0105482,
   -0.0240033, -0.1289688,  1.1529717);

const mat3 AP1_2_P3D65_MAT = mat3(
    1.3792145, -0.3088633, -0.0703498,
   -0.0693355,  1.082295 , -0.0129618,
   -0.002159 , -0.0454592,  1.0476177);

const mat3 AP1_2_BT2020_MAT = mat3(
    1.0258249, -0.0200529, -0.0057714,
   -0.002235 ,  1.0045849, -0.0023520,
   -0.0050133, -0.02529  ,  1.0303028);

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

//! Apply color filter.
//! @param color Linear color in AP1 space.
vec3 colorTransform(vec3 color, int mode)
{
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
    float b = cos(90.0 * radians(value));
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
    color = mix(color, vec4(1.0, 0.0, 0.0, 1.0), isOverflow);

    bool isUnderflow = all(lessThan(color.rgb, vec3(1e-5))) && ((markerFlags & 0x8) != 0);
    color = mix(color, vec4(0.0, 0.0, 1.0, 1.0), isUnderflow);

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
    vec2 xy = mod(wh - offset, imageScale);
    return any(lessThanEqual(abs(xy), vec2(1.0))) && (imageScale > 5.0);
}

bool showPixelBorderHighlight(vec2 wh, vec2 cursor, vec2 offset, float imageScale)
{
    if (!uEnablePixelHighlight || imageScale < 12.0) {
        return false;
    }

    vec2 xy = floor((wh - offset) / imageScale);
    vec2 st = floor((cursor - offset) / imageScale);
    vec2 lower = mod(wh - offset, imageScale);
    vec2 upper = mod(wh - offset + vec2(1, 1), imageScale);

    return all(equal(xy - st, vec2(0.0))) && (any(lessThanEqual(lower, vec2(1.0))) || any(lessThanEqual(upper, vec2(1.0))));
}

vec3 drawRGBValues(vec2 wh, vec2 offset, float imageScale, vec3 linearColor, vec3 displayColor)
{
    // Draw RGB values within pixel box.
    vec2 xy = mod(wh - offset, imageScale);
    float opacity = clamp((imageScale - 32.0) / 64.0, 0.0, 1.0);
    opacity *= getRGBValueMatte(xy / imageScale, linearColor);

    float luminance = dot(displayColor.rgb, vec3(0.3, 0.59, 0.11));    // Rough estimated luminance.
    vec3 matteColor = mix(vec3(0.85), vec3(0.15), luminance > 0.5);
    displayColor = clamp(displayColor, vec3(0.0), vec3(1.0));
    return mix(displayColor, matteColor, opacity);
}

//! @param wh Pixel coordinates in window.
//! @param imageSize Original image size for display.
//! @param offset Offset in pixels
vec4 showImage(vec2 wh, vec2 offset, vec2 imageSize, vec2 cursorPos,
    in sampler2D image1, in sampler2D image2,
    ivec2 imageProp1, ivec2 imageProp2)
{
    vec4 result = vec4(0.0);

    vec2 regionMask = step(offset, wh) - step(offset + imageSize, wh);
    if (regionMask.x * regionMask.y == 0.0) {
        return result;
    }
   
    vec2 imageUV = (wh - offset) / imageSize;
    vec4 color1 = texture(image1, imageUV);
    result = color1;

    bool inDiffMode = (uPixelMarkerFlags & 0x3) != 0;
    if (inDiffMode) {
        vec4 color2 = texture(image2, imageUV);
        float squareError = getColorDistance(color1.rgb, color2.rgb);
        bool enableHeatMap = (uPixelMarkerFlags & 0x2) >> 1 != 0;
        result.rgb = mix(color1.rgb, vec3(1.0, 0.0, 1.0), clamp(squareError, 0.0, 1.0));
        result.rgb = mix(result.rgb, getHeatColor(squareError), enableHeatMap);
    } else {
        result.rgb = colorTransform(color1.rgb, uPresentMode);
    }

    vec3 linearColor = result.rgb;
    result = overlayPixelMarker(result, uPixelMarkerFlags);
    result.rgb = outputTransform(result.rgb, uOutTransformType, uDisplayGamma);
    result.rgb = mix(result.rgb, vec3(0.7), showPixelBorder(wh, offset, uImageScale));
    result.rgb = mix(result.rgb, vec3(1.0, 1.0, 0.0), showPixelBorderHighlight(wh, cursorPos, offset, uImageScale));

    if (!inDiffMode) {
        result.rgb = drawRGBValues(wh, offset, uImageScale, linearColor, result.rgb);
    }

    return result;
}


//! @param wh Pixel coordinates in [width, height].
//! @param imageSize Scaled image size for display.
//! @param offset Image offset in pixels.
vec4 renderSideBySide(vec2 wh, vec2 offset, vec2 imageSize, vec2 uv, float splitPos)
{
    vec2 cursorPos = uCursorPos;
    if (uCursorPos.x > splitPos * uWindowSize.x) {
        cursorPos.x = uCursorPos.x - splitPos * uWindowSize.x;
    }

    offset.x = round(offset.x * 0.5) + 0.5;   // Halve image x-axis translation for side by side view.
    vec4 color1 = showImage(wh, offset, imageSize, cursorPos, uImage1, uImage2, uInImageProp1, uInImageProp2);

    wh.x = round(wh.x - splitPos * uWindowSize.x);
    vec4 color2 = showImage(wh, offset, imageSize, cursorPos, uImage2, uImage1, uInImageProp2, uInImageProp1);

    vec4 result = mix(color1, color2, uv.x > splitPos);

    if (result.a == 0.0) {
        // Draw background checker.
        result.rgb = getCheckerColor(uv, uWindowSize);
        result.a = 1.0;
    }

    return result;
}

void main()
{
    oColor = vec4(0.0, 0.0, 0.0, 1.0);

    bool isSplitter = abs(vUV.x - uSplitPos) * uWindowSize.x < 1.0;
    bool showSplitter = uSplitPos != 1.0 && (uPixelMarkerFlags & 0x3) == 0; // Not in difference or heatmap view.
    
    vec2 wh = round(vUV * uWindowSize + vec2(0.5));

    if (uSideBySide == 1) {
        oColor = renderSideBySide(wh, uOffset, uImageSize, vUV, uSplitPos);
        oColor = mix(oColor, vec4(1.0), isSplitter);
        return;
    }

    vec2 regionMask = step(uOffset, wh) - step(uOffset + uImageSize, wh);
    if (regionMask.x * regionMask.y == 0.0) {
        // When outside of image region, just draw background checker.
        oColor.rgb = getCheckerColor(vUV, uWindowSize);
        oColor = mix(oColor, vec4(1.0), isSplitter && showSplitter);
        return;
    }

    vec2 imageUV = (wh - uOffset) / uImageSize;
    vec4 color1 = texture(uImage1, imageUV);
    vec4 color2 = texture(uImage2, imageUV);

    bool inDiffMode = (uPixelMarkerFlags & 0x3) != 0;
    if (inDiffMode) { // Should only active in compare mode.
        bool enableHeatMap = ((uPixelMarkerFlags & 0x2) >> 1) != 0;
        float squareError = getColorDistance(color1.rgb, color2.rgb);
        oColor = mix(color2, color1, vUV.x <= uSplitPos);
        oColor.rgb = mix(oColor.rgb, vec3(1.0, 0.0, 1.0), clamp(squareError, 0.0, 1.0));
        oColor.rgb = mix(oColor.rgb, getHeatColor(squareError), enableHeatMap);
    } else {
        color1.rgb = colorTransform(color1.rgb, uPresentMode);
        color2.rgb = colorTransform(color2.rgb, uPresentMode);
        oColor = mix(color2, color1, vUV.x <= uSplitPos);
    }

    vec3 linearColor = oColor.rgb;
    oColor = overlayPixelMarker(oColor, uPixelMarkerFlags);
    oColor.rgb = outputTransform(oColor.rgb, uOutTransformType, uDisplayGamma);
    oColor.rgb = mix(oColor.rgb, vec3(0.7), showPixelBorder(wh, uOffset, uImageScale));
    oColor.rgb = mix(oColor.rgb, vec3(1.0, 1.0, 0.0), showPixelBorderHighlight(wh, uCursorPos, uOffset, uImageScale));

    if (!inDiffMode) {
        oColor.rgb = drawRGBValues(wh, uOffset, uImageScale, linearColor, oColor.rgb);
    }

    oColor = mix(oColor, vec4(1.0), showSplitter && isSplitter);
}