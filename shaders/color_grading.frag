#version 330

uniform sampler2D uImage;
uniform ivec2 uInImageProp;  // x: encoding type, y: color primaries type
uniform float uEV;

in  vec2 vUV;
out vec4 oColor;

//-----------------------------------------------------------------------------
// Color Transfor Matrices
//-----------------------------------------------------------------------------
// The color space transformation matrices defined in ACES CTL is row vector times matrix.
// Therefore we define mul as y * x to both match CTL for better reference.
// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/README-MATRIX.md
// 
// For more transformaion matrices, we could use Python colour module to compute the
// normalized transformation matrices. For each transformatoin, we need color primaries
// and white point. Thoese information could be found here:
// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/lib/ACESlib.Utilities_Color.ctl
#define mul(x, y) (y * x)


const mat3 XYZ_2_AP1_MAT = mat3(
     1.6410233797, -0.3248032942, -0.2364246952,
    -0.6636628587,  1.6153315917,  0.0167563477,
     0.0117218943, -0.0082844420,  0.9883948585);

const mat3 BT709_2_AP1_MAT = mat3(
    0.6130973,  0.3395229,  0.0473793,
    0.0701942,  0.9163555,  0.0134523,
    0.0206156,  0.1095698,  0.8698151);

const mat3 P3D65_2_AP1_MAT = mat3(
    0.7357978,  0.2121662,  0.0520355,
    0.0471804,  0.9380473,  0.0147744,
    0.0035637,  0.0411419,  0.9552950);

const mat3 BT2020_2_AP1_MAT = mat3(
    0.9748949,  0.0195988,  0.0055058,
    0.0021802,  0.9955371,  0.0022849,
    0.0047972,  0.024532 ,  0.9706713);


// https://github.com/ampas/aces-dev/blob/master/transforms/ctl/lib/ACESlib.Utilities_Color.ctl
const float pq_m1 = 0.1593017578125; // ( 2610.0 / 4096.0 ) / 4.0;
const float pq_m2 = 78.84375; // ( 2523.0 / 4096.0 ) * 128.0;
const float pq_c1 = 0.8359375; // 3424.0 / 4096.0 or pq_c3 - pq_c2 + 1.0;
const float pq_c2 = 18.8515625; // ( 2413.0 / 4096.0 ) * 32.0;
const float pq_c3 = 18.6875; // ( 2392.0 / 4096.0 ) * 32.0;

//! Converts from the non-linear perceptually quantized space to linear cd/m^2.
//! @param N Normalized signal in [0, 1]
//! @return Linear nits value in [0, maxPQValue]. In spec the maxPQValue is 10000.
vec3 ST2084_2_Y(vec3 N, float maxPQValue)
{
    vec3 Np = pow(N, vec3(1.0 / pq_m2));
    vec3 L = max(vec3(0.0), Np - vec3(pq_c1));
    L /= (vec3(pq_c2) - pq_c3 * Np);
    L = pow(L, vec3(1.0 / pq_m1));
    return L * maxPQValue; // returns cd/m^2
}

//! Convert linear signal in nits to PQ value.
//! @param Y Linear nits value.
//! @return PQ value in [0, 1].
vec3 Y_2_ST2084(vec3 Y, float maxPQValue)
{
    vec3 L = Y / maxPQValue;
    vec3 Lm = pow(L, vec3(pq_m1));
    vec3 N1 = vec3(pq_c1) + pq_c2 * Lm;
    vec3 N2 = vec3(1.0) + pq_c3 * Lm;
    return pow(N1 / N2, vec3(pq_m2));
}


// Decode input color value to linear signal.
// Refer to ColorEncodingType@colour.h
vec3 decode(vec3 color, int type)
{
    if (type == 5) { // sRGB
        bvec3 isSmall = lessThanEqual(color, vec3(0.04045));
        color = mix(pow((color + vec3(0.055)) / 1.055, vec3(2.4)), color / 12.92, isSmall);
    } else if (type == 4) { // BT.709
        bvec3 isSmall = lessThanEqual(color, vec3(0.081));
        color = mix(pow((color + vec3(0.099)) / 1.099, vec3(1 / 0.45)), color / 4.5, isSmall);
    } else if (type == 3) { // BT.2100 PQ == ST.2084
        color = ST2084_2_Y(color, 10000.0);
    }

    return color;
}


// Transform color coordinates to ACES AP1.
vec3 inputTransform(vec3 color, int type)
{
    if (type == 0) {    // BT.709 or sRGB
        color = mul(BT709_2_AP1_MAT, color);
    } else if (type == 1) {  // P3-D65 (Display)
        color = mul(P3D65_2_AP1_MAT, color);
    } else if (type == 2) { // BT.2020
        color = mul(BT2020_2_AP1_MAT, color);
    }

    // When type == 3, ACES AP1. It's alread in AP1, just bypass the color.
    return color;
}

void main()
{
    vec2 uv = vUV;
    uv.y = 1.0 - uv.y;  // Flip y-axis for imported image.
    oColor = texture(uImage, uv);
    oColor.rgb = decode(oColor.rgb, uInImageProp.x);
    oColor.rgb = inputTransform(oColor.rgb, uInImageProp.y);
    oColor.rgb *= pow(2.0, uEV);
}