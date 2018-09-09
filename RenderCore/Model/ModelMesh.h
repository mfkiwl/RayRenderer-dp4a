#pragma once

#include "../RenderCoreRely.h"
#include "ModelImage.h"
#include "../Material.h"
#include "OpenGLUtil/PointEnhance.hpp"

namespace rayr
{
using oglu::oglTex2DV;
class Model;

namespace detail
{

class alignas(b3d::Vec3)_ModelMesh : public NonCopyable, public common::AlignBase<alignof(b3d::Vec3)>, public xziar::respak::Serializable
{
    friend class ::rayr::Model;
private:
    static Wrapper<_ModelMesh> GetModel(const u16string& fname, const Wrapper<oglu::oglWorker>& asyncer = {});
    static void ReleaseModel(const u16string& fname);
public:
    b3d::Vec3 size;
private:
    vectorEx<oglu::PointEx> pts;
    vector<uint32_t> indexs;
    vector<std::pair<string, uint32_t>> groups;
    map<string, PBRMaterial> MaterialMap;
    oglu::oglVBO vbo;
    oglu::oglEBO ebo;
    oglu::oglIBO ibo;
    const u16string mfname;
    void loadOBJ(const fs::path& objfname);
    void InitDataBuffers();
    _ModelMesh(const u16string& fname, const Wrapper<oglu::oglWorker>& asyncer = {});
public:
    void PrepareVAO(oglu::detail::_oglVAO::VAOPrep& vaoPrep) const;

    virtual std::string_view SerializedType() const override { return "rayr#ModelMesh"; }
    virtual ejson::JObject Serialize(SerializeUtil& context) const override;
};

}
using ModelMesh = Wrapper<detail::_ModelMesh>;

}
