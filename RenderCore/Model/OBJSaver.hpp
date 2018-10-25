#pragma once
#include "RenderCoreRely.h"
#include "ModelMesh.h"
#pragma warning(disable : 4996)
#include "fmt/time.h"
#pragma warning(default : 4996)



namespace rayr::detail
{
using namespace common::file;

class OBJSaver
{
private:
    static constexpr std::byte BOM_UTF16LE[2] = { std::byte(0xff), std::byte(0xfe) };
    const Wrapper<_ModelMesh> Mesh;
    fs::path PathObj;
    fs::path PathMtl;
public:
    OBJSaver(const fs::path& fpath, const Wrapper<_ModelMesh>& mesh) : Mesh(mesh)
    {
        PathObj = fpath;
        PathObj.replace_extension(".obj");
        PathMtl = fpath;
        PathMtl.replace_extension(".mtl");
    }
    void WriteMtl() const
    {
        BufferedFileWriter mtlFile(FileObject::OpenThrow(PathMtl, OpenFlag::CreatNewBinary), 65536);
        mtlFile.Write(BOM_UTF16LE);
        const auto header = fmt::format(u"#XZiar Dizz Renderer MTL Exporter\r\n#Created at {:%Y-%m-%d %H:%M:%S}\r\n\r\n", SimpleTimer::getCurLocalTime());
        mtlFile.Write(header.size() * sizeof(char16_t), header.data());
    }
    void WriteObj() const
    {
        BufferedFileWriter objFile(FileObject::OpenThrow(PathMtl, OpenFlag::CreatNewBinary), 65536);
        objFile.Write(BOM_UTF16LE);
        const auto header = fmt::format(u"#XZiar Dizz Renderer OBJ Exporter\r\n#Created at {:%Y-%m-%d %H:%M:%S}\r\n\r\n", SimpleTimer::getCurLocalTime());
        objFile.Write(header.size() * sizeof(char16_t), header.data());
    }
};


}
