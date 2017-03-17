#pragma once

#include "RenderElement.h"
#include <filesystem>

namespace rayr
{

class Model;
namespace inner
{

namespace fs = std::experimental::filesystem;

class _ModelImage
{
	friend class ::rayr::Model;
	friend class _ModelData;
private:
	using Path = fs::path;
	static map<wstring, Wrapper<_ModelImage>> images;
	static Wrapper<_ModelImage> getImage(Path picPath, const Path& curPath);
	static Wrapper<_ModelImage> getImage(const wstring& pname);
	static void shrink();
	uint16_t width = 0, height = 0;
	vector<uint32_t> image;
	_ModelImage(const wstring& pfname);
public:
	_ModelImage(const uint16_t w, const uint16_t h, const uint32_t color = 0x0);
	void placeImage(const Wrapper<_ModelImage>& from, const uint16_t x, const uint16_t y);
	void resize(const uint16_t w, const uint16_t h);
	oglu::oglTexture genTexture();
};
using ModelImage = Wrapper<_ModelImage>;

class _ModelData : public NonCopyable, public AlignBase<>
{
	friend class ::rayr::Model;
private:
	using Path = std::experimental::filesystem::path;
	static map<wstring, Wrapper<_ModelData>> models;
	static Wrapper<_ModelData> getModel(const wstring& fname, bool asyncload = false);
	static void releaseModel(const wstring& fname);
	class OBJLoder
	{
	private:
		Path fpath;
		vector<uint8_t> fdata;
		uint32_t fcurpos, flen;
	public:
		char curline[256];
		const char* param[5];
		struct TextLine
		{
			uint64_t type;
			uint8_t pcount;
			operator const bool() const
			{
				return type != "EOF"_hash;
			}
		};
		OBJLoder(const Path &fpath_);
		~OBJLoder();
		TextLine readLine();
		wstring toWideString(const uint8_t idx);
		int8_t parseFloat(const uint8_t idx, float *output);
		int8_t parseInt(const uint8_t idx, int32_t *output);
	};
	struct alignas(Material) MtlStub
	{
		Material mtl;
		float scalex, offsetx, scaley, offsety;
		uint16_t width = 0, height = 0, sx = 0, sy = 0, posid = UINT16_MAX;
		ModelImage texs[2];
		ModelImage& diffuse() { return texs[0]; }
		const ModelImage& diffuse() const { return texs[0]; }
		ModelImage& normal() { return texs[1]; }
		const ModelImage& normal() const { return texs[1]; }
		bool hasImage(const ModelImage& img) const
		{
			for (const auto& tex : texs)
				if (tex == img)
					return true;
			return false;
		}
	};
public:
	Vec3 size;
private:
	using TexMergeItem = std::tuple<ModelImage, uint16_t, uint16_t>;
	vector<Point> pts;
	vector<uint32_t> indexs;
	vector<std::pair<string, uint32_t>> groups;
	ModelImage diffuse, normal;
	oglu::oglTexture texd, texn;
	oglu::oglBuffer vbo;
	oglu::oglEBO ebo;
	const wstring mfnane;
	std::tuple<ModelImage, ModelImage> mergeTex(map<string, MtlStub>& mtlmap, vector<TexMergeItem>& texposs);
	map<string, MtlStub> loadMTL(const Path& mtlfname);
	bool loadOBJ(const Path& objfname);
	_ModelData(const wstring& fname, bool asyncload = false);
public:
	void initData();
	~_ModelData();
	oglu::oglVAO getVAO() const;
};

}

using ModelData = Wrapper<inner::_ModelData>;

class alignas(16) Model : public Drawable
{
protected:
public:
	ModelData data;
	Model(const wstring& fname, bool asyncload = false);
	~Model();
	virtual void prepareGL(const oglu::oglProgram& prog, const map<string, string>& translator = map<string, string>()) override;
	virtual void draw(oglu::oglProgram& prog) const override;
};

}