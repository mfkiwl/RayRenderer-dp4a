#include "RenderCoreRely.h"
#include "resource.h"
#include "BasicTest.h"
#include "ImageUtil/ImageUtil.h"
#include "ImageUtil/DataConvertor.hpp"
#include <thread>

namespace rayr
{

struct Init
{
    Init()
    {
        basLog().verbose(u"BasicTest Static Init\n");
        oglUtil::init();
        oclu::oclUtil::init();
    }
};


void BasicTest::init2d(const u16string pname)
{
    prog2D.reset(u"Prog 2D");
    const string shaderSrc = pname.empty() ? getShaderFromDLL(IDR_SHADER_2D) : common::file::ReadAllText(pname);
    try
    {
        prog2D->AddExtShaders(shaderSrc);
    }
    catch (const OGLException& gle)
    {
        basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
        COMMON_THROW(BaseException, L"OpenGL compile fail");
    }
    try
    {
        prog2D->link();
    }
    catch (const OGLException& gle)
    {
        basLog().error(u"Fail to link Program:\n{}\n", gle.message);
        COMMON_THROW(BaseException, L"link Program error");
    }
    picVAO.reset(VAODrawMode::Triangles);
    screenBox.reset(BufferType::Array);
    {
        Vec3 DatVert[] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f } };
        screenBox->Write(DatVert, sizeof(DatVert));
        picVAO->SetDrawSize(0, 6);
        picVAO->Prepare().Set(screenBox, prog2D->Attr_Vert_Pos, sizeof(Vec3), 3, 0);
    }
}

void BasicTest::init3d(const u16string pname)
{
    cam.position = Vec3(0.0f, 0.0f, 4.0f);
    {
        oglProgram progBasic(u"3D Prog");
        const string shaderSrc = pname.empty() ? getShaderFromDLL(IDR_SHADER_3D) : common::file::ReadAllText(pname);
        try
        {
            progBasic->AddExtShaders(shaderSrc);
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
            COMMON_THROW(BaseException, L"OpenGL compile fail");
        }
        try
        {
            progBasic->link();
            progBasic->SetUniform("useNormalMap", false);
            progBasic->State().SetSubroutine("lighter", "onlytex").SetSubroutine("getNorm", "vertedNormal");
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"Fail to link Program:\n{}\n", gle.message);
            COMMON_THROW(BaseException, L"link Program error");
        }
        progBasic->setCamera(cam);
        Prog3Ds.insert(progBasic);
        prog3D = progBasic;
    }
    {
        oglProgram progPBR(u"3D-pbr");
        const string shaderSrc = pname.empty() ? getShaderFromDLL(IDR_SHADER_3DPBR) : 
            common::file::ReadAllText(common::str::ReplaceStr<char16_t>(pname, u".glsl", u"_pbr.glsl"));
        try
        {
            progPBR->AddExtShaders(shaderSrc);
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
            COMMON_THROW(BaseException, L"OpenGL compile fail");
        }
        try
        {
            progPBR->link();
            progPBR->SetUniform("useNormalMap", false);
            progPBR->SetUniform("useDiffuseMap", true);
            progPBR->State()
                .SetSubroutine("lighter", "onlytex")
                .SetSubroutine("getNorm", "vertedNormal")
                .SetSubroutine("getAlbedo", "materialAlbedo");
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"Fail to link Program:\n{}\n", gle.message);
            COMMON_THROW(BaseException, L"link Program error");
        }
        Prog3Ds.insert(progPBR);
    }
    {
        Wrapper<Pyramid> pyramid(1.0f);
        pyramid->Name = u"Pyramid";
        pyramid->position = { 0,0,0 };
        AddObject(pyramid);
        Wrapper<Sphere> ball(0.75f);
        ball->Name = u"Ball";
        ball->position = { 1,0,0 };
        AddObject(ball);
        Wrapper<Box> box(0.5f, 1.0f, 2.0f);
        box->Name = u"Box";
        box->position = { 0,1,0 };
        AddObject(box);
        Wrapper<Plane> ground(500.0f, 50.0f);
        ground->Name = u"Ground";
        ground->position = { 0,-2,0 };
        AddObject(ground);
    }
}

void BasicTest::initTex()
{
    picTex.reset(TextureType::Tex2D);
    picBuf.reset(BufferType::Pixel);
    tmpTex.reset(TextureType::Tex2D);
    {
        picTex->setProperty(TextureFilterVal::Nearest, TextureWrapVal::Repeat);
        Vec4 empty[128][128];
        for (int a = 0; a < 128; ++a)
        {
            for (int b = 0; b < 128; ++b)
            {
                auto& obj = empty[a][b];
                obj.x = a / 128.0f;
                obj.y = b / 128.0f;
                obj.z = (a + b) / 256.0f;
                obj.w = 1.0f;
            }
        }
        empty[0][0] = empty[0][127] = empty[127][0] = empty[127][127] = Vec4(0, 0, 1, 1);
        tmpTex->setData(TextureInnerFormat::RGBA8, TextureDataFormat::RGBAf, 128, 128, empty);
        picTex->setData(TextureInnerFormat::RGBA8, TextureDataFormat::RGBAf, 128, 128, empty);
        picBuf->Write(nullptr, 128 * 128 * 4, BufferWriteMode::DynamicDraw);
    }
    {
        uint32_t empty[4][4] = { 0 };
    }
    mskTex.reset(TextureType::Tex2D);
    {
        mskTex->setProperty(TextureFilterVal::Nearest, TextureWrapVal::Repeat);
        Vec4 empty[128][128];
        for (int a = 0; a < 128; ++a)
        {
            for (int b = 0; b < 128; ++b)
            {
                auto& obj = empty[a][b];
                if ((a < 64 && b < 64) || (a >= 64 && b >= 64))
                {
                    obj = Vec4(0.05, 0.05, 0.05, 1.0);
                }
                else
                {
                    obj = Vec4(0.6, 0.6, 0.6, 1.0);
                }
            }
        }
        mskTex->setData(TextureInnerFormat::RGBA8, TextureDataFormat::RGBAf, 128, 128, empty);
    }
}

void BasicTest::initUBO()
{
    uint16_t size = 0;
    for (const auto& prog : Prog3Ds)
    {
        if (auto lubo = prog->GetResource("lightBlock"))
            size = common::max(size, lubo->size);
    }
    lightUBO.reset(size);
    lightLim = (uint8_t)(size / sizeof(LightData));
}

void BasicTest::prepareLight()
{
    vector<uint8_t> data(lightUBO->Size());
    size_t pos = 0;
    for (const auto& lgt : lights)
    {
        if (!lgt->isOn) continue;
        memcpy_s(&data[pos], lightUBO->Size() - pos, &(*lgt), sizeof(LightData));
        pos += sizeof(LightData);
        if (pos >= lightUBO->Size())
            break;
    }
    prog3D->SetUniform("lightCount", (uint32_t)lights.size());
    lightUBO->Write(data, BufferWriteMode::StreamDraw);
}

void BasicTest::fontTest(const char32_t word)
{
    try
    {
        auto fonttex = fontCreator->getTexture();
        if (word == 0x0)
        {
            const auto imgShow = fontCreator->clgraysdfs(U'��', 16);
            oglUtil::invokeSyncGL([&imgShow, &fonttex](const common::asyexe::AsyncAgent& agent) 
            {
                fonttex->setData(TextureInnerFormat::R8, imgShow);
                agent.Await(oglu::oglUtil::SyncGL());
            })->wait();
            img::WriteImage(imgShow, basepath / (u"Show.png"));
            /*SimpleTimer timer;
            for (uint32_t cnt = 0; cnt < 65536; cnt += 4096)
            {
                const auto imgShow = fontCreator->clgraysdfs((char32_t)cnt, 4096);
                img::WriteImage(imgShow, basepath / (u"Show-" + std::to_u16string(cnt) + u".png"));
                basLog().success(u"successfully processed words begin from {}\n", cnt);
            }
            timer.Stop();
            basLog().success(u"successfully processed 65536 words, cost {}ms\n", timer.ElapseMs());*/
        }
        else
        {
            fontCreator->setChar(L'G', false);
            const auto imgG = fonttex->getImage(TextureDataFormat::R8);
            img::WriteImage(imgG, basepath / u"G.png");
            fontCreator->setChar(word, false);
            const auto imgA = fonttex->getImage(TextureDataFormat::R8);
            img::WriteImage(imgA, basepath / u"A.png");
            const auto imgShow = fontCreator->clgraysdfs(U'��', 16);
            fonttex->setData(TextureInnerFormat::R8, imgShow);
            img::WriteImage(imgShow, basepath / u"Show.png");
            fonttex->setProperty(oglu::TextureFilterVal::Linear, oglu::TextureWrapVal::Clamp);
            fontViewer->bindTexture(fonttex);
            //fontViewer->prog->globalState().setSubroutine("fontRenderer", "plainFont").end();
        }
    }
    catch (BaseException& be)
    {
        basLog().error(u"Font Construct failure:\n{}\n", be.message);
        //COMMON_THROW(BaseException, L"init FontViewer failed");
    }
}

BasicTest::BasicTest(const u16string sname2d, const u16string sname3d)
{
    static Init _init;
    glContext = oglu::oglContext::CurrentContext();
    glContext->SetDepthTest(DepthTestType::LessEqual);
    //glContext->SetFaceCulling(FaceCullingType::CullCW);
    fontViewer.reset();
    fontCreator.reset(oclu::Vendor::Intel);
    basepath = u"D:\\Programs Temps\\RayRenderer";
    if (!fs::exists(basepath))
        basepath = u"C:\\Programs Temps\\RayRenderer";
    fontCreator->reloadFont(basepath / u"test.ttf");

    fontTest(/*L'��'*/);
    initTex();
    init2d(sname2d);
    init3d(sname3d);
    prog2D->State().SetTexture(fontCreator->getTexture(), "tex");
    initUBO();
    for (const auto& prog : Prog3Ds)
    {
        prog->State()
            .SetTexture(mskTex, "tex")
            .SetUBO(lightUBO, "lightBlock");
    }
    glProgs.insert(prog2D);
    glProgs.insert(Prog3Ds.cbegin(), Prog3Ds.cend());
    glProgs.insert(fontViewer->prog);
}

void BasicTest::Draw()
{
    {
        const auto changed = (ChangableUBO)IsUBOChanged.exchange(0, std::memory_order::memory_order_relaxed);
        if (HAS_FIELD(changed, ChangableUBO::Light))
            prepareLight();
    }
    if (mode)
    {
        prog3D->setCamera(cam);
        auto drawcall = prog3D->draw();
        for (const auto& d : drawables)
        {
            d->Draw(drawcall);
            drawcall.Restore();
        }
    }
    else
    {
        fontViewer->draw();
        //prog2D->draw().draw(picVAO);
    }
}

void BasicTest::Resize(const int w, const int h)
{
    cam.resize(w, h);
    prog2D->setProject(cam, w, h);
    prog3D->setProject(cam, w, h);
}

void BasicTest::ReloadFontLoader(const u16string& fname)
{
    auto clsrc = file::ReadAllText(fs::path(fname));
    fontCreator->reload(clsrc);
    fontTest(0);
}

void BasicTest::ReloadFontLoaderAsync(const u16string& fname, CallbackInvoke<bool> onFinish, std::function<void(const BaseException&)> onError)
{
    std::thread([this, onFinish, onError](const u16string name)
    {
        common::SetThreadName(u"AsyncLoader for FontTest");
        try
        {
            auto clsrc = file::ReadAllText(fs::path(name));
            fontCreator->reload(clsrc);
            fontTest(0);
            onFinish([]() { return true; });
        }
        catch (BaseException& be)
        {
            basLog().error(u"failed to reload font test\n");
            if (onError)
                onError(be);
            else
                onFinish([]() { return false; });
        }
    }, fname).detach();
}


void BasicTest::LoadModelAsync(const u16string& fname, std::function<void(Wrapper<Model>)> onFinish, std::function<void(const BaseException&)> onError)
{
    std::thread([onFinish, onError](const u16string name)
    {
        common::SetThreadName(u"AsyncLoader for Model");
        try
        {
            Wrapper<Model> mod(name, true);
            mod->Name = u"model";
            onFinish(mod);
        }
        catch (BaseException& be)
        {
            basLog().error(u"failed to load model by file {}\n", name);
            if (onError)
                onError(be);
            else
                onFinish(Wrapper<Model>());
        }
    }, fname).detach();
}

void BasicTest::LoadShaderAsync(const u16string& fname, const u16string& shdName, std::function<void(oglProgram)> onFinish, std::function<void(const BaseException&)> onError /*= nullptr*/)
{
    using common::asyexe::StackSize;
    oglUtil::invokeSyncGL([onFinish, onError, fname, shdName](const common::asyexe::AsyncAgent& agent)
    {
        oglProgram prog(shdName);
        try
        {
            prog->AddExtShaders(common::file::ReadAllText(fname));
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
            onError(gle);
        }
        try
        {
            prog->link();
            prog->SetUniform("useNormalMap", false);
            prog->SetUniform("useDiffuseMap", true);
            prog->State()
                .SetSubroutine("lighter", "onlytex")
                .SetSubroutine("getNorm", "vertedNormal")
                .SetSubroutine("getAlbedo", "materialAlbedo");
        }
        catch (const OGLException& gle)
        {
            basLog().error(u"Fail to link Program:\n{}\n", gle.message);
            onError(gle);
        }
        onFinish(prog);
    }, u"load shader " + shdName, StackSize::Huge);
}

bool BasicTest::AddObject(const Wrapper<Drawable>& drawable)
{
    for(const auto& prog : Prog3Ds)
        drawable->PrepareGL(prog);
    drawables.push_back(drawable);
    basLog().success(u"Add an Drawable [{}][{}]:  {}\n", drawables.size() - 1, drawable->GetType(), drawable->Name);
    return true;
}

bool BasicTest::AddShader(const oglProgram& prog)
{
    const auto isAdd = Prog3Ds.insert(prog).second;
    if (isAdd)
    {
        for (const auto& d : drawables)
            d->PrepareGL(prog);
    }
    return isAdd;
}

bool BasicTest::AddLight(const Wrapper<Light>& light)
{
    lights.push_back(light);
    prepareLight();
    basLog().success(u"Add a Light [{}][{}]:  {}\n", lights.size() - 1, (int32_t)light->type, light->name);
    return true;
}

void BasicTest::ChangeShader(const oglProgram& prog)
{
    if (Prog3Ds.count(prog))
    {
        prog3D = prog;
        prog3D->setCamera(cam);
        prog3D->setProject(cam, cam.width, cam.height);
    }
    else
        basLog().warning(u"change to an unknown shader [{}], ignored.\n", prog ? prog->Name : u"null");
}

void BasicTest::DelAllLight()
{
    lights.clear();
    prepareLight();
}

void BasicTest::ReportChanged(const ChangableUBO target)
{
    IsUBOChanged |= (uint32_t)target;
}


}