#ifndef BAKTSIU_COLOUR_H_
#define BAKTSIU_COLOUR_H_
#pragma once

//
// https://github.com/colour-science/colour

namespace baktsiu
{

enum class ColorPrimaryType : char
{
    BT_709 = 0,     // BT.709 is also known as Rec.709
    sRGB = BT_709,  // sRGB's color primaries are the same as Rec.709
    DCI_P3_D65,
    BT_2020,
    ACES_AP0,
    ACES_AP1,
};

enum class WhitePoint : char
{
    D65,
    D50,
};

enum class ColorEncodingType : char
{
    Linear = 0,
    BT_2020,
    BT_2100_HLG,
    BT_2100_PQ,
    BT_709,
    sRGB
};

// Optical-Electro Transfer Functions
enum class OETF : char
{
    Linear = 0,
    BT_2020,
    BT_2100_HLG,
    BT_2100_PQ,
    BT_709,
};

// Type of Electro-Optical Transfer Functions
enum class EOTF : char
{
    Linear = 0,
    BT_1886,
    BT_2020,
    BT_2100_HLG,
    BT_2100_PQ,
    ST_2084 = BT_2100_PQ,
    sRGB,
};

const char* getPropertyLabel(ColorPrimaryType);
const char* getPropertyLabel(ColorEncodingType);

}  // namespace baktsiu
#endif