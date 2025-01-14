#pragma once

#include "RenderCoreRely.h"

namespace dizz
{


enum class LightType : int32_t { Parallel = 0, Point = 1, Spot = 2 };

struct RENDERCOREAPI LightData
{
    b3d::Vec4 Color = b3d::Vec4::one();
    b3d::Vec3 Position = b3d::Vec3::zero();
    b3d::Vec3 Direction = b3d::Vec3(0, -1, 0);
    b3d::Vec4 Attenuation = b3d::Vec4(0.5, 0.3, 0.0, 10.0);
    float CutoffOuter, CutoffInner;
    const LightType Type;
protected:
    LightData(const LightType type) : Type(type) {}
public:
    static constexpr size_t WriteSize = 4 * 4 * sizeof(float);
    void Move(const float x, const float y, const float z)
    {
        Position += b3d::Vec3(x, y, z);
    }
    void Move(const b3d::Vec3& offset)
    {
        Position += offset;
    }
    void Rotate(const float x, const float y, const float z)
    {
        Rotate(b3d::Vec3(x, y, z));
    }
    void Rotate(const b3d::Vec3& radius)
    {
        const auto rMat = b3d::Mat3x3::RotateMatXYZ(radius);
        Direction = rMat * Direction;
    }
    void WriteData(const common::span<std::byte> space) const;
};

#if COMMON_COMPILER_MSVC
#   pragma warning(push)
#   pragma warning(disable:4275 4251)
#endif
class RENDERCOREAPI Light : public LightData, public xziar::respak::Serializable, public common::Controllable
{
protected:
    Light(const LightType type, const std::u16string& name);
    virtual u16string_view GetControlType() const override
    {
        using namespace std::literals;
        return u"Light"sv;
    }
    void RegistControllable();
public:
    bool IsOn = true;
    std::u16string Name;
    virtual ~Light() override {}
    RESPAK_DECL_COMP_DESERIALIZE("dizz#Light")
    virtual void Serialize(xziar::respak::SerializeUtil& context, xziar::ejson::JObject& object) const override;
    virtual void Deserialize(xziar::respak::DeserializeUtil& context, const xziar::ejson::JObjectRef<true>& object) override;
};
#if COMMON_COMPILER_MSVC
#   pragma warning(pop)
#endif

class ParallelLight : public Light
{
public:
    ParallelLight() : Light(LightType::Parallel, u"ParallelLight") { Attenuation.w = 1.0f; }
};

class PointLight : public Light
{
public:
    PointLight() : Light(LightType::Point, u"PointLight") {}
};

class SpotLight : public Light
{
public:
    SpotLight() : Light(LightType::Spot, u"SpotLight") 
    {
        CutoffInner = 30.0f;
        CutoffOuter = 90.0f;
    }
};

}
