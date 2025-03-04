#pragma once
#include "../RenderCoreRely.h"
#include "../TextureLoader.h"
#include "../Material.h"
#include "OBJLoader.hpp"

namespace dizz
{
using oglu::oglTex2DV;
class Model;

namespace detail
{

class MTLLoader : public common::NonCopyable
{
private:
    std::shared_ptr<TextureLoader> TexLoader;
    std::map<string, std::shared_ptr<PBRMaterial>> Materials;
    std::vector<std::tuple<std::shared_ptr<PBRMaterial>, TextureLoader::LoadResult*, TexLoadType>> DelayJobs;
    std::map<fs::path, TextureLoader::LoadResult> RealJobs;
    fs::path FallbackImgPath(fs::path picPath, const fs::path& fallbackPath)
    {
        if (fs::exists(picPath))
            return picPath;
        auto picPath2 = fallbackPath / picPath.filename();
        if (fs::exists(picPath2))
            return picPath2;
#if COMMON_OS_UNIX
        if (!picPath.has_parent_path())
        {
            if (auto picPath_ = picPath.string(); picPath_.find_first_of('\\') != std::string::npos)
            {
                std::replace(picPath_.begin(), picPath_.end(), '\\', '/');
                return FallbackImgPath(picPath_, fallbackPath);
                // auto picPath3 = fallbackPath / fs::path(picPath_).filename();
            }
        }
#endif
        dizzLog().warning(u"Cannot find img of [{}]\n", picPath.u16string());
        return {};
    }
public:
    MTLLoader(const std::shared_ptr<TextureLoader>& texLoader)
        : TexLoader(texLoader) {}
    void LoadMTL(const fs::path& mtlpath) try
    {
        using common::container::FindInMap;
        OBJLoder ldr(mtlpath);
        dizzLog().verbose(u"Parsing mtl file [{}]\n", mtlpath.u16string());
        std::vector<std::tuple<std::shared_ptr<PBRMaterial>, fs::path, TexLoadType>> preJobs;
        const fs::path fallbackPath = mtlpath.parent_path();
        std::shared_ptr<PBRMaterial> curmtl;
        OBJLoder::TextLine line;
        while ((line = ldr.ReadLine()))
        {
            switch (line.Type)
            {
            case "EMPTY"_hash:
                break;
            case "#"_hash:
                dizzLog().verbose(u"--mtl-note [{}]\n", line.ToUString());
                break;
            case "newmtl"_hash:
                {
                    const string name(line.Params[1]);
                    curmtl = std::make_shared<PBRMaterial>(common::str::to_u16string(name, ldr.chset));
                    Materials.insert_or_assign(name, curmtl);
                } break;
            case "Ka"_hash:
                //curmtl->mtl.ambient = Vec4(atof(line.Params[1].data()), atof(line.Params[2].data()), atof(line.Params[3].data()), 1.0);
                break;
            case "Kd"_hash:
                curmtl->Albedo = line.ParamsToFloat3(1);
                //curmtl->Albedo = b3d::Vec3(atof(line.Params[1].data()), atof(line.Params[2].data()), atof(line.Params[3].data()));
                break;
            case "Ks"_hash:
                //curmtl->mtl.specular = Vec4(atof(line.Params[1].data()), atof(line.Params[2].data()), atof(line.Params[3].data()), 1.0);
                break;
            case "Ke"_hash:
                //curmtl->mtl.emission = Vec4(atof(line.Params[1].data()), atof(line.Params[2].data()), atof(line.Params[3].data()), 1.0);
                break;
            case "Ns"_hash:
                //curmtl->mtl.shiness = (float)atof(line.Params[1].data());
                break;
            case "map_Ka"_hash:
                //curmtl.=loadTex(ldr.param[0], mtlpath.parent_path());
                //break;
            case "map_Kd"_hash:
                if (const auto realPath = FallbackImgPath(common::str::to_u16string(line.Rest(1), ldr.chset), fallbackPath); !realPath.empty())
                {
                    preJobs.emplace_back(curmtl, realPath, TexLoadType::Color);
                }
                break;
            case "map_bump"_hash:
                if (const auto realPath = FallbackImgPath(common::str::to_u16string(line.Rest(1), ldr.chset), fallbackPath); !realPath.empty())
                {
                    preJobs.emplace_back(curmtl, realPath, TexLoadType::Normal);
                }
            }
        }
        //assign jobs
        for (const auto&[mat, imgPath, type] : preJobs)
        {
            if (const auto it = FindInMap(RealJobs, imgPath))
            {
                DelayJobs.emplace_back(mat, it, type);
            }
            else
            {
                auto loadRes = TexLoader->GetTexureAsync(imgPath, type);
                const auto ptr = &(RealJobs.insert_or_assign(imgPath, loadRes).first->second);
                DelayJobs.emplace_back(mat, ptr, type);
            }
        }
    }
    catch (const common::file::FileException&)
    {
        dizzLog().error(u"Fail to open mtl file\t[{}]\n", mtlpath.u16string());
    }

    std::map<string, PBRMaterial> GetMaterialMap()
    {
        std::map<string, PBRMaterial> materialMap;
        //assign material
        for (const auto&[mat, pmsPtr, type] : DelayJobs)
        {
            FakeTex tex;
            if (const auto it = std::get_if<FakeTex>(pmsPtr); it)
                tex = *it;
            else
            {
                tex = (std::get<1>(*pmsPtr))->Get();
                *pmsPtr = tex; // no need to wait next time
            }
            if (!tex)
                continue;
            switch (type)
            {
            case TexLoadType::Color:
                mat->DiffuseMap = tex;
                mat->UseDiffuseMap = true;
                break;
            case TexLoadType::Normal:
                mat->NormalMap = tex;
                mat->UseNormalMap = true;
                break;
            }
        }
        for (const auto&[name, mat] : Materials)
            materialMap.insert_or_assign(name, *mat);
        DelayJobs.clear();
        RealJobs.clear();
        return materialMap;
    }
};

}
}