#include "RenderCoreRely.h"
#include "Model.h"


namespace rayr
{
using common::container::FindInMap;
using common::asyexe::AsyncAgent;


void Model::InitMaterial()
{
    MaterialHolder = MultiMaterialHolder((uint8_t)Mesh->groups.size());
    MaterialUBO.reset(32 * MultiMaterialHolder::UnitSize);
    MaterialBuf.resize(MaterialUBO->Size());

    uint8_t i = 0;
    for (const auto& group : Mesh->groups)
    {
        if (const auto mat = FindInMap(Mesh->MaterialMap, group.first))
            MaterialHolder[i] = *mat;
        ++i;
    }

    AssignMaterial();
}

Model::Model(const u16string& fname, bool asyncload) : Drawable(this, TYPENAME), Mesh(detail::_ModelMesh::GetModel(fname, asyncload))
{
    const auto resizer = 2 / max(max(Mesh->size.x, Mesh->size.y), Mesh->size.z);
    scale = Vec3(resizer, resizer, resizer);
    if (asyncload)
    {
        auto task = oglu::oglUtil::invokeSyncGL([&](const common::asyexe::AsyncAgent&) { InitMaterial(); });
    }
    else
    {
        InitMaterial();
    }
}

Model::~Model()
{
    const auto mfname = Mesh->mfname;
    Mesh.release();
    detail::_ModelMesh::ReleaseModel(mfname);
}

void Model::PrepareGL(const oglu::oglProgram& prog, const map<string, string>& translator)
{
    oglu::oglVAO vao(oglu::VAODrawMode::Triangles);
    const GLint attrs[4] = { prog->Attr_Vert_Pos, prog->Attr_Vert_Norm, prog->Attr_Vert_Texc, prog->Attr_Vert_Tan };
    {
        auto vaoprep = std::move(vao->Prepare()
            .Set(Mesh->vbo, attrs, 0)
            .Set<uint32_t>(Drawable::GetDrawIdVBO(), prog->Attr_Draw_ID, sizeof(uint32_t), 1, 0, 1)
            .SetIndex(Mesh->ebo));
        Mesh->PrepareVAO(vaoprep);
    }
    SetVAO(prog, vao);
    vao->Test();
}

void Model::Draw(Drawcall& drawcall) const
{
    MaterialHolder.BindTexture(drawcall);
    DrawPosition(drawcall)
        .SetUBO(MaterialUBO, "materialBlock")
        .Draw(GetVAO(drawcall.GetProg()));
}

}