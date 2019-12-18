#include "colour.h"

namespace baktsiu
{

const char* getPropertyLabel(ColorPrimaryType type)
{
    switch (type) {
    case ColorPrimaryType::sRGB:
        return "Rec.709/sRGB";

    case ColorPrimaryType::DCI_P3_D65:
        return "DCI-P3 D65";

    case ColorPrimaryType::BT_2020:
        return "BT.2020";

    case ColorPrimaryType::ACES_AP0:
        return "ACES2065-1";

    case ColorPrimaryType::ACES_AP1:
        return "ACEScg, ACEScc";
        
    default:
        return "unknown";
    }
}

const char* getPropertyLabel(ColorEncodingType type)
{
    switch (type) {
    
    case ColorEncodingType::Linear:
        return "Linear";

    case ColorEncodingType::BT_709:
        return "BT.709";

    case ColorEncodingType::sRGB:
        return "sRGB";

    case ColorEncodingType::BT_2020:
        return "BT.2020";

    case ColorEncodingType::BT_2100_HLG:
        return "BT.2100 HLG";

    case ColorEncodingType::BT_2100_PQ:
        return "BT.2100 PQ";

    default:
        return "Unknown";
    }
}

}  // namespace baktsiu