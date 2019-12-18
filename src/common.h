#ifndef BAKTSIU_COMMON_H_
#define BAKTSIU_COMMON_H_

#include <stdint.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include <fmt/core.h>
#include <glm/glm.hpp>


#define CHECK_AND_RETURN_IT(status, msg) \
if (!status)  promptError(msg)

#if defined USE_NVTX
#include <nvToolsExt.h>
#pragma comment(lib, "nvToolsExt64_1.lib")
#define PushRangeMarker(label)  nvtxRangePushA(label)
#define PopRangeMarker() nvtxRangePop()

struct ScopeMarkerObject
{
    ScopeMarkerObject(const char* label)
    {
        id = PushRangeMarker(label);
    }

    ~ScopeMarkerObject() {
        PopRangeMarker();
    }

    nvtxRangeId_t id;
};
#define ScopeMarker(label) ScopeMarkerObject smo_(label);
#else
#define PushRangeMarker(label)
#define PopRangeMarker()
#define ScopeMarker
#endif


#define CHECK_STATUS_AND_RETURN_IT(status) if (!status) { return status; }

#define ENUM_CLASS_OPERATORS(Enum) \
    inline           Enum& operator|=(Enum& lhs, Enum rhs) { return lhs = (Enum)((__underlying_type(Enum))lhs | (__underlying_type(Enum))rhs); } \
    inline           Enum& operator&=(Enum& lhs, Enum rhs) { return lhs = (Enum)((__underlying_type(Enum))lhs & (__underlying_type(Enum))rhs); } \
    inline           Enum& operator^=(Enum& lhs, Enum rhs) { return lhs = (Enum)((__underlying_type(Enum))lhs ^ (__underlying_type(Enum))rhs); } \
    inline constexpr Enum  operator| (Enum  lhs, Enum rhs) { return (Enum)((__underlying_type(Enum))lhs | (__underlying_type(Enum))rhs); } \
    inline constexpr Enum  operator& (Enum  lhs, Enum rhs) { return (Enum)((__underlying_type(Enum))lhs & (__underlying_type(Enum))rhs); } \
    inline constexpr Enum  operator^ (Enum  lhs, Enum rhs) { return (Enum)((__underlying_type(Enum))lhs ^ (__underlying_type(Enum))rhs); } \
    inline constexpr bool  operator! (Enum  value)         { return !(__underlying_type(Enum))value; } \
    inline constexpr Enum  operator~ (Enum  value)         { return (Enum)~(__underlying_type(Enum))value; }


template<typename Enum>
inline Enum toggleFlags(Enum flags, Enum mask)
{
    return (Enum)((__underlying_type(Enum))flags ^ (__underlying_type(Enum))mask);
}

template<typename Enum>
inline bool hasAllFlags(Enum flags, Enum mask)
{
    return (((__underlying_type(Enum))flags) & (__underlying_type(Enum))mask) == ((__underlying_type(Enum))mask);
}

template<typename Enum>
inline bool hasAnyFlags(Enum flags, Enum mask)
{
    return (((__underlying_type(Enum))flags) & (__underlying_type(Enum))mask) != 0;
}


namespace baktsiu
{

using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Vec2i = glm::ivec2;
using Vec3i = glm::ivec3;
using Vec4i = glm::ivec4;
using Mat3f = glm::mat3x3;
using Mat4f = glm::mat4x4;

const float kPI = 3.14159265358979323846f;

inline void prompt(const std::string& msg)
{
    std::cout << "[BakTsiu] " << msg << "\n";
}

inline void promptWarning(const std::string& msg) 
{
    std::cerr << "[BakTsiu | WARNING] " << msg << "\n";
}

inline void promptError(const std::string& msg)
{
    std::cerr << "[BakTsiu | ERROR] " << msg << "\n";
}


//// Code from Instant-Meshes
//template <typename TimeT = std::chrono::milliseconds>
//class Timer
//{
//public:
//    Timer()
//    {
//        start = std::chrono::system_clock::now();
//    }
//
//    size_t value() const
//    {
//        auto now = std::chrono::system_clock::now();
//        auto duration = std::chrono::duration_cast<TimeT>(now - start);
//        return (size_t)duration.count();
//    }
//
//    size_t reset()
//    {
//        auto now = std::chrono::system_clock::now();
//        auto duration = std::chrono::duration_cast<TimeT>(now - start);
//        start = now;
//        return (size_t)duration.count();
//    }
//private:
//    std::chrono::system_clock::time_point start;
//};
//
//inline std::string timeString(double time, bool precise = false)
//{
//    if (std::isnan(time) || std::isinf(time))
//        return "inf";
//
//    std::string suffix = "ms";
//    if (time > 1000) {
//        time /= 1000; suffix = "s";
//        if (time > 60) {
//            time /= 60; suffix = "m";
//            if (time > 60) {
//                time /= 60; suffix = "h";
//                if (time > 12) {
//                    time /= 12; suffix = "d";
//                }
//            }
//        }
//    }
//
//    std::ostringstream os;
//    os << std::setprecision(precise ? 4 : 1)
//        << std::fixed << time << suffix;
//
//    return os.str();
//}

}  // namespace baktsiu
#endif // BAKTSIU_COMMON_H_