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
	prog2D.reset();
	if(pname.empty())
	{
		auto shaders = oglShader::loadFromExSrc(getShaderFromDLL(IDR_SHADER_2D));
		for (auto shader : shaders)
		{
			try
			{
				shader->compile();
				prog2D->addShader(std::move(shader));
			}
			catch (OGLException& gle)
			{
				basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
				COMMON_THROW(BaseException, L"OpenGL compile fail", std::any(shader));
			}
		}
	}
	else
	{
		try
		{
			auto shaders = oglShader::loadFromFiles(pname);
			if (shaders.size() < 2)
				COMMON_THROW(BaseException, L"No enough shader loaded from file");
			for (auto shader : shaders)
			{
				shader->compile();
				prog2D->addShader(std::move(shader));
			}
		}
		catch (OGLException& gle)
		{
			basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
			COMMON_THROW(BaseException, L"OpenGL compile fail");
		}
	}
	try
	{
		prog2D->link();
		prog2D->registerLocation({ "vertPos","","","" }, { "","","","","" });
	}
	catch (OGLException& gle)
	{
		basLog().error(u"Fail to link Program:\n{}\n", gle.message);
		COMMON_THROW(BaseException, L"link Program error");
	}
	picVAO.reset(VAODrawMode::Triangles);
	screenBox.reset(BufferType::Array);
	{
		Vec3 DatVert[] = { { -1.0f, -1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },
		{ 1.0f, 1.0f, 0.0f },{ -1.0f, 1.0f, 0.0f },{ 1.0f, -1.0f, 0.0f } };
		screenBox->write(DatVert, sizeof(DatVert));
		picVAO->setDrawSize(0, 6);
		picVAO->prepare().set(screenBox, prog2D->Attr_Vert_Pos, sizeof(Vec3), 3, 0).end();
	}
}

void BasicTest::init3d(const u16string pname)
{
	prog3D.reset();
	if (pname.empty())
	{
		auto shaders = oglShader::loadFromExSrc(getShaderFromDLL(IDR_SHADER_3D));
		for (auto shader : shaders)
		{
			try
			{
				shader->compile();
				prog3D->addShader(std::move(shader));
			}
			catch (OGLException& gle)
			{
				basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
				COMMON_THROW(BaseException, L"OpenGL compile fail", std::any(shader));
			}
		}
	}
	else
	{
		try
		{
			auto shaders = oglShader::loadFromFiles(pname);
			if (shaders.size() < 2)
				COMMON_THROW(BaseException, L"No enough shader loaded from file");
			for (auto shader : shaders)
			{
				shader->compile();
				prog3D->addShader(std::move(shader));
			}
		}
		catch (OGLException& gle)
		{
			basLog().error(u"OpenGL compile fail:\n{}\n", gle.message);
			COMMON_THROW(BaseException, L"OpenGL compile fail");
		}
	}
	try
	{
		prog3D->link();
		prog3D->registerLocation({ "vertPos","vertNorm","texPos","" }, { "matProj", "matView", "matModel", "matNormal", "matMVP" });
	}
	catch (OGLException& gle)
	{
		basLog().error(u"Fail to link Program:\n{}\n", gle.message);
		COMMON_THROW(BaseException, L"link Program error");
	}
	
	cam.position = Vec3(0.0f, 0.0f, 4.0f);
	prog3D->setCamera(cam);
	{
		Wrapper<Pyramid> pyramid(1.0f);
		pyramid->name = u"Pyramid";
		pyramid->position = { 0,0,0 };
		drawables.push_back(pyramid);
		Wrapper<Sphere> ball(0.75f);
		ball->name = u"Ball";
		ball->position = { 1,0,0 };
		drawables.push_back(ball);
		Wrapper<Box> box(0.5f, 1.0f, 2.0f);
		box->name = u"Box";
		box->position = { 0,1,0 };
		drawables.push_back(box);
		Wrapper<Plane> ground(500.0f, 50.0f);
		ground->name = u"Ground";
		ground->position = { 0,-2,0 };
		drawables.push_back(ground);
		for (auto& d : drawables)
		{
			d->prepareGL(prog3D);
		}
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
		picBuf->write(nullptr, 128 * 128 * 4, BufferWriteMode::DynamicDraw);
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
	if (auto lubo = prog3D->getResource("lightBlock"))
		lightUBO.reset((*lubo)->size);
	else
		lightUBO.reset(0);
	lightLim = (uint8_t)lightUBO->size / sizeof(LightData);
	if (auto mubo = prog3D->getResource("materialBlock"))
		materialUBO.reset((*mubo)->size);
	else
		materialUBO.reset(0);
	materialLim = (uint8_t)materialUBO->size / sizeof(Material);
	prog3D->globalState().setUBO(lightUBO, "lightBlock").setUBO(materialUBO, "mat").end();
}

void BasicTest::prepareLight()
{
	vector<uint8_t> data(lightUBO->size);
	size_t pos = 0;
	for (const auto& lgt : lights)
	{
		memmove(&data[pos], &(*lgt), sizeof(LightData));
		pos += sizeof(LightData);
		if (pos >= lightUBO->size)
			break;
	}
	lightUBO->write(data, BufferWriteMode::StreamDraw);
}

void BasicTest::fontTest(const char32_t word)
{
	try
	{
		auto fonttex = fontCreator->getTexture();
		if (word == 0x0)
		{
			const auto imgShow = fontCreator->clgraysdfs(U'��', 16);
			oglUtil::invokeSyncGL([&imgShow, &fonttex](const common::asyexe::AsyncAgent&) 
			{
				fonttex->setData(TextureInnerFormat::R8, imgShow);
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
			fontViewer->bindTexture(fonttex);
			const auto imgG = fonttex->getImage(TextureDataFormat::R8);
			img::WriteImage(imgG, basepath / u"G.png");
			fontCreator->setChar(word, false);
			const auto imgA = fonttex->getImage(TextureDataFormat::R8);
			img::WriteImage(imgA, basepath / u"A.png");
			const auto imgShow = fontCreator->clgraysdfs(U'��', 16);
			fonttex->setData(TextureInnerFormat::R8, imgShow);
			img::WriteImage(imgShow, basepath / (u"Show.png"));
			fonttex->setProperty(oglu::TextureFilterVal::Linear, oglu::TextureWrapVal::Repeat);
		}
	}
	catch (BaseException& be)
	{
		basLog().error(u"Font Construct failure:\n{}\n", be.message);
		//COMMON_THROW(BaseException, L"init FontViewer failed");
	}
}

Wrapper<Model> BasicTest::_addModel(const u16string& fname)
{
	Wrapper<Model> mod(fname);
	mod->name = u"model";
	return mod;
}

BasicTest::BasicTest(const u16string sname2d, const u16string sname3d)
{
	static Init _init;
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
	prog2D->globalState().setTexture(fontCreator->getTexture(), "tex").end();
	prog3D->globalState().setTexture(mskTex, "tex").end();
	initUBO();
}

void BasicTest::draw()
{
	if (mode)
	{
		prog3D->setCamera(cam);
		for (const auto& d : drawables)
		{
			d->draw(prog3D);
		}
	}
	else
	{
		fontViewer->draw();
		//prog2D->draw().draw(picVAO);
	}
}

void BasicTest::resize(const int w, const int h)
{
	cam.resize(w, h);
	prog2D->setProject(cam, w, h);
	prog3D->setProject(cam, w, h);
}

void BasicTest::reloadFontLoader(const u16string& fname)
{
	auto clsrc = file::ReadAllText(fs::path(fname));
	fontCreator->reload(clsrc);
	fontTest(0);
}

void BasicTest::reloadFontLoaderAsync(const u16string& fname, CallbackInvoke<bool> onFinish, std::function<void(BaseException&)> onError)
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

bool BasicTest::addModel(const u16string& fname)
{
	Wrapper<Model> mod(fname);
	mod->name = u"model";
	mod->prepareGL(prog3D);
	drawables.push_back(mod);
	return true;
}

void BasicTest::addModelAsync(const u16string& fname, CallbackInvoke<bool> onFinish, std::function<void(BaseException&)> onError)
{
	std::thread([this, onFinish, onError](const u16string name)
	{
		common::SetThreadName(u"AsyncLoader for Model");
		try
		{
			Wrapper<Model> mod(name, true);
			mod->name = u"model";
			onFinish([&, mod]()mutable
			{
				mod->prepareGL(prog3D);
				drawables.push_back(mod);
				return true;
			});
		}
		catch (BaseException& be)
		{
			basLog().error(u"failed to load model by file {}\n", name);
			if (onError)
				onError(be);
			else
				onFinish([]() { return false; });
		}
	}, fname).detach();
}

void BasicTest::addLight(const b3d::LightType type)
{
	Wrapper<Light> lgt;
	switch (type)
	{
	case LightType::Parallel:
		lgt = Wrapper<ParallelLight>(std::in_place);
		lgt->color = Vec4(2.0, 0.5, 0.5, 10.0);
		break;
	case LightType::Point:
		lgt = Wrapper<PointLight>(std::in_place);
		lgt->color = Vec4(0.5, 2.0, 0.5, 10.0);
		break;
	case LightType::Spot:
		lgt = Wrapper<SpotLight>(std::in_place);
		lgt->color = Vec4(0.5, 0.5, 2.0, 10.0);
		break;
	default:
		return;
	}
	lights.push_back(lgt);
	prepareLight();
	basLog().info(u"add Light {} type {}\n", lights.size(), (int32_t)lgt->type);
}

void BasicTest::delAllLight()
{
	lights.clear();
	prepareLight();
}

void BasicTest::moveobj(const uint16_t id, const float x, const float y, const float z)
{
	drawables[id]->position += Vec3(x, y, z);
}

void BasicTest::rotateobj(const uint16_t id, const float x, const float y, const float z)
{
	drawables[id]->rotation += Vec3(x, y, z);
}

void BasicTest::movelgt(const uint16_t id, const float x, const float y, const float z)
{
	lights[id]->position += Vec3(x, y, z);
}

void BasicTest::rotatelgt(const uint16_t id, const float x, const float y, const float z)
{
	lights[id]->direction += Vec3(x, y, z);
}

uint16_t BasicTest::objectCount() const
{
	return (uint16_t)drawables.size();
}

uint16_t BasicTest::lightCount() const
{
	return (uint16_t)lights.size();
}

void BasicTest::showObject(uint16_t objIdx) const
{
	const auto& d = drawables[objIdx];
	basLog().info(u"Drawable {}:\t {}  [{}]\n", objIdx, d->name, d->getType());
}

static uint32_t getTID()
{
	auto tid = std::this_thread::get_id();
	return *(uint32_t*)&tid;
}

void BasicTest::tryAsync(CallbackInvoke<bool> onFinish, std::function<void(BaseException&)> onError) const
{
	basLog().debug(u"begin async in pid {}\n", getTID());
	std::thread([onFinish, onError] ()
	{
		common::SetThreadName(u"AsyncThread for TryAsync");
		basLog().debug(u"async thread in pid {}\n", getTID());
		std::this_thread::sleep_for(std::chrono::seconds(10));
		basLog().debug(u"sleep finish. async thread in pid {}\n", getTID());
		try
		{
			if (false)
				COMMON_THROW(BaseException, L"ERROR in async call");
		}
		catch (BaseException& be)
		{
			onError(be);
			return;
		}
		onFinish([]() 
		{
			basLog().debug(u"async callback in pid {}\n", getTID());
			return true;
		});
	}).detach();
}

}