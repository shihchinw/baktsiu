#ifndef BAKTSIU_COMMON_H_
#define BAKTSIU_COMMON_H_

#include <stdint.h>

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include <glm/glm.hpp>

#pragma warning(push)
#pragma warning(disable: 4819 4566)
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#pragma warning(pop)


#define LOGI(...) spdlog::info(__VA_ARGS__);
#define LOGW(...) spdlog::warn(__VA_ARGS__);
#define LOGE(...) spdlog::error("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__));
#define LOGD(...) spdlog::debug(__VA_ARGS__);

#define CHECK_AND_RETURN_IT(status, msg) \
do { if (!status)  LOGE(msg); } while (0)

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

}  // namespace baktsiu
#endif // BAKTSIU_COMMON_H_