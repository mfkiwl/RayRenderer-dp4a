#include "ImageUtilRely.h"
#include "ImageTGA.h"
#include "DataConvertor.hpp"

namespace xziar::img::tga
{


using BGR16ToRGBAMap = std::vector<uint32_t>;
using RGB16ToRGBAMap = BGR16ToRGBAMap;
static BGR16ToRGBAMap GenerateBGR16ToRGBAMap()
{
	BGR16ToRGBAMap map(1 << 16);
	constexpr uint32_t COUNT = 1 << 5, STEP = 256 / COUNT, HALF_SIZE = 1 << 15;
	constexpr uint32_t RED_STEP = STEP, GREEN_STEP = STEP << 8, BLUE_STEP = STEP << 16;
	uint32_t idx = 0;
	uint32_t color = 0;
	for (uint32_t red = COUNT, colorR = color; red--; colorR += RED_STEP)
	{
		for (uint32_t green = COUNT, colorRG = colorR; green--; colorRG += GREEN_STEP)
		{
			for (uint32_t blue = COUNT, colorRGB = colorRG; blue--; colorRGB += BLUE_STEP)
			{
				map[idx++] = colorRGB;
			}
		}
	}
	for (uint32_t count = 0; count < HALF_SIZE;)//protential 4k-alignment issue
		map[idx++] = map[count++] | 0xff000000;
	return map;
}
static const BGR16ToRGBAMap& GetBGR16ToRGBAMap()
{
	static const auto map = GenerateBGR16ToRGBAMap();
	return map;
}
static RGB16ToRGBAMap GenerateRGB16ToRGBAMap()
{
	RGB16ToRGBAMap map = GetBGR16ToRGBAMap();
	convert::BGRAsToRGBAs(reinterpret_cast<uint8_t*>(map.data()), map.size());
	return map;
}
static const BGR16ToRGBAMap& GetRGB16ToRGBAMap()
{
	static const auto map = GenerateRGB16ToRGBAMap();
	return map;
}


TgaReader::TgaReader(FileObject& file) : ImgFile(file)
{
}

bool TgaReader::Validate()
{
	using detail::TGAImgType;
	ImgFile.Rewind();
	if (!ImgFile.Read(Header))
		return false;
	switch (Header.ColorMapType)
	{
	case 1://paletted image
		if (REMOVE_MASK(Header.ImageType, { TGAImgType::RLE_MASK }) != TGAImgType::COLOR_MAP)
			return false;
		if (Header.PixelDepth != 8 && Header.PixelDepth != 16)
			return false;
		break;
	case 0://color image
		if (!MATCH_ANY(REMOVE_MASK(Header.ImageType, { TGAImgType::RLE_MASK }), { TGAImgType::COLOR, TGAImgType::GREY }))
			return false;
		if (Header.PixelDepth != 8 && Header.PixelDepth != 15 && Header.PixelDepth != 16 && Header.PixelDepth != 24 && Header.PixelDepth != 32)
			return false;
		if (Header.PixelDepth == 8)//not support
			return false;
		break;
	default:
		return false;
	}
	Width = (int16_t)convert::ParseWordLE(Header.Width);
	Height = (int16_t)convert::ParseWordLE(Header.Height);
	if (Width < 1 || Height < 1)
		return false;
	return true;
}


class TgaHelper
{
public:
	template<typename ReadFunc>
	static void ReadColorData4(const uint8_t colorDepth, const uint64_t count, Image& output, const bool isOutputRGB, ReadFunc& reader)
	{
		if (output.ElementSize != 4)
			return;
		auto& color16Map = isOutputRGB ? GetBGR16ToRGBAMap() : GetRGB16ToRGBAMap();
		switch (colorDepth)
		{
		case 15:
			{
				std::vector<uint16_t> tmp(count);
				reader.Read(count, tmp);
				uint32_t * __restrict destPtr = reinterpret_cast<uint32_t*>(output.GetRawPtr());
				for (auto bgr15 : tmp)
					*destPtr++ = color16Map[bgr15 + (1 << 15)];
			}break;
		case 16:
			{
				std::vector<uint16_t> tmp(count);
				reader.Read(count, tmp);
				uint32_t * __restrict destPtr = reinterpret_cast<uint32_t*>(output.GetRawPtr());
				for (auto bgr16 : tmp)
					*destPtr++ = color16Map[bgr16];
			}break;
		case 24://BGR
			{
				uint8_t * __restrict destPtr = output.GetRawPtr();
				uint8_t * __restrict srcPtr = destPtr + count;
				reader.Read(3 * count, srcPtr);
				if (isOutputRGB)
					convert::BGRsToRGBAs(destPtr, srcPtr, count);
				else
					convert::RGBsToRGBAs(destPtr, srcPtr, count);
			}break;
		case 32:
			{//BGRA
				reader.Read(4 * count, output.GetRawPtr());
				if (isOutputRGB)
					convert::BGRAsToRGBAs(output.GetRawPtr(), count);
			}break;
		}
	}

	template<typename ReadFunc>
	static void ReadColorData3(const uint8_t colorDepth, const uint64_t count, Image& output, const bool isOutputRGB, ReadFunc& reader)
	{
		if (output.ElementSize != 3)
			return;
		auto& color16Map = isOutputRGB ? GetBGR16ToRGBAMap() : GetRGB16ToRGBAMap();
		switch (colorDepth)
		{
		case 15:
		case 16:
			{
				std::vector<uint16_t> tmp(count);
				reader.Read(count, tmp);
				uint8_t * __restrict destPtr = output.GetRawPtr();
				for (auto bgr15 : tmp)
				{
					const uint32_t color = color16Map[bgr15 + (1 << 15)];//ignore alpha
					const uint8_t* __restrict colorPtr = reinterpret_cast<const uint8_t*>(&color);
					*destPtr++ = colorPtr[0];
					*destPtr++ = colorPtr[1];
					*destPtr++ = colorPtr[2];
				}
			}break;
		case 24://BGR
			{
				reader.Read(3 * count, output.GetRawPtr());
				if (isOutputRGB)
					convert::BGRsToRGBs(output.GetRawPtr(), count);
			}break;
		case 32://BGRA
			{
				Image tmp(ImageDataType::RGBA);
				tmp.SetSize(output.Width, 1);
				for (uint32_t row = 0; row < output.Height; ++row)
				{
					reader.Read(4 * output.Width, tmp.GetRawPtr());
					if (isOutputRGB)
						convert::BGRAsToRGBs(output.GetRawPtr(row), tmp.GetRawPtr(), count);
					else
						convert::RGBAsToRGBs(output.GetRawPtr(row), tmp.GetRawPtr(), count);
				}
			}break;
		}
	}

	template<typename MapperReader, typename IndexReader>
	static void ReadFromColorMapped(const detail::TgaHeader& header, Image& image, MapperReader& mapperReader, IndexReader& reader)
	{
		const uint64_t count = (uint64_t)image.Width * image.Height;
		const bool isOutputRGB = REMOVE_MASK(image.DataType, { ImageDataType::ALPHA_MASK, ImageDataType::FLOAT_MASK }) == ImageDataType::RGB;
		const bool needAlpha = HAS_FIELD(image.DataType, ImageDataType::ALPHA_MASK);

		const detail::ColorMapInfo mapInfo(header);
		mapperReader.Skip(mapInfo.Offset);
		Image mapper(needAlpha ? ImageDataType::RGBA : ImageDataType::RGB);
		if (needAlpha)
			ReadColorData4(mapInfo.ColorDepth, mapInfo.Size, mapper, isOutputRGB, mapperReader);
		else
			ReadColorData3(mapInfo.ColorDepth, mapInfo.Size, mapper, isOutputRGB, mapperReader);

		if (needAlpha)
		{
			const uint32_t * __restrict const mapPtr = reinterpret_cast<uint32_t*>(mapper.GetRawPtr());
			uint32_t * __restrict destPtr = reinterpret_cast<uint32_t*>(image.GetRawPtr());
			if (header.PixelDepth == 8)
			{
				std::vector<uint8_t> idxes;
				reader.Read(count, idxes);
				for (auto idx : idxes)
					*destPtr++ = mapPtr[idx];
			}
			else if (header.PixelDepth == 16)
			{
				std::vector<uint16_t> idxes;
				reader.Read(count, idxes);
				for (auto idx : idxes)
					*destPtr++ = mapPtr[idx];
			}
		}
		else
		{
			const uint8_t * __restrict const mapPtr = mapper.GetRawPtr();
			uint8_t * __restrict destPtr = image.GetRawPtr();
			if (header.PixelDepth == 8)
			{
				std::vector<uint8_t> idxes;
				reader.Read(count, idxes);
				for (auto idx : idxes)
				{
					const size_t idx3 = idx * 3;
					*destPtr++ = mapPtr[idx3 + 0];
					*destPtr++ = mapPtr[idx3 + 1];
					*destPtr++ = mapPtr[idx3 + 2];
				}
			}
			else if (header.PixelDepth == 16)
			{
				std::vector<uint16_t> idxes;
				reader.Read(count, idxes);
				for (auto idx : idxes)
				{
					const size_t idx3 = idx * 3;
					*destPtr++ = mapPtr[idx3 + 0];
					*destPtr++ = mapPtr[idx3 + 1];
					*destPtr++ = mapPtr[idx3 + 2];
				}
			}
		}
	}

	template<typename ReadFunc>
	static void ReadFromColor(const detail::TgaHeader& header, Image& image, ReadFunc& reader)
	{
		const uint64_t count = (uint64_t)image.Width * image.Height;
		const bool isOutputRGB = REMOVE_MASK(image.DataType, { ImageDataType::ALPHA_MASK, ImageDataType::FLOAT_MASK }) == ImageDataType::RGB;
		const bool needAlpha = HAS_FIELD(image.DataType, ImageDataType::ALPHA_MASK);
		if (needAlpha)
			ReadColorData4(header.PixelDepth, count, image, isOutputRGB, reader);
		else
			ReadColorData3(header.PixelDepth, count, image, isOutputRGB, reader);
	}

	template<typename Writer>
	static void WriteRLE3(const uint8_t* __restrict ptr, uint32_t len, const bool isRepeat, Writer& writer)
	{
		if (isRepeat)
		{
			uint8_t color[3];
			color[0] = ptr[2], color[1] = ptr[1], color[2] = ptr[0];
			while (len)
			{
				const auto size = std::min(128u, len);
				len -= size;
				const uint8_t flag = static_cast<uint8_t>(0x80 + (size - 1));
				writer.Write(flag);
				writer.Write(color);
			}
		}
		else
		{
			AlignedBuffer<32> buffer(3 * 128);
			while (len)
			{
				const auto size = std::min(128u, len);
				len -= size;
				const uint8_t flag = static_cast<uint8_t>(size - 1);
				writer.Write(flag);
				memcpy(buffer.GetRawPtr(), ptr, 3 * size);
				ptr += 3 * size;
				convert::BGRsToRGBs(buffer.GetRawPtr(), size);
				writer.Write(3 * size, buffer.GetRawPtr());
			}
		}
	}

	template<typename Writer>
	static void WriteRLE4(const uint8_t* __restrict ptr, uint32_t len, const bool isRepeat, Writer& writer)
	{
		if (isRepeat)
		{
			uint8_t color[4];
			color[0] = ptr[2], color[1] = ptr[1], color[2] = ptr[0], color[3] = ptr[3];
			while (len)
			{
				const auto size = std::min(128u, len);
				len -= size;
				const uint8_t flag = static_cast<uint8_t>(0x80 + (size - 1));
				writer.Write(flag);
				writer.Write(color);
			}
		}
		else
		{
			AlignedBuffer<32> buffer(4 * 128);
			while (len)
			{
				const auto size = std::min(128u, len);
				len -= size;
				const uint8_t flag = static_cast<uint8_t>(size - 1);
				writer.Write(flag);
				memcpy(buffer.GetRawPtr(), ptr, 4 * size);
				ptr += 4 * size;
				convert::BGRAsToRGBAs(buffer.GetRawPtr(), size);
				writer.Write(4 * size, buffer.GetRawPtr());
			}
		}
	}

	template<typename Writer>
	static void WriteRLEColor3(const Image& image, Writer& writer)
	{
		if (image.ElementSize != 3)
			return;
		const uint32_t colMax = image.Width * 3;//tga's limit should promise this will not overflow
		for (uint32_t row = 0; row < image.Height; ++row)
		{
			const uint8_t * __restrict data = image.GetRawPtr<uint8_t>(row);
			uint32_t last;
			uint32_t len = 0;
			bool repeat = false;
			for (uint32_t col = 0; col < colMax; col += 3, ++len)
			{
				uint32_t cur = data[col] + (data[col + 1] << 8) + (data[col + 2] << 16);
				switch (len)
				{
				case 0:
					break;
				case 1:
					repeat = (cur == last);
					break;
				default:
					if (cur == last && !repeat)//changed
					{
						WriteRLE3((const uint8_t*)&data[col - len], len - 1, false, writer);
						len = 1, repeat = true;
					}
					else if (cur != last && repeat)//changed
					{
						WriteRLE3((const uint8_t*)&data[col - len], len, true, writer);
						len = 0, repeat = false;
					}
				}
				last = cur;
			}
			if (len > 0)
				WriteRLE3((const uint8_t*)&data[image.Width - len], len, repeat, writer);
		}
	}

	template<typename Writer>
	static void WriteRLEColor4(const Image& image, Writer& writer)
	{
		if (image.ElementSize != 4)
			return;
		for (uint32_t row = 0; row < image.Height; ++row)
		{
			const uint32_t * __restrict data = image.GetRawPtr<uint32_t>(row);
			uint32_t last;
			uint32_t len = 0;
			bool repeat = false;
			for (uint32_t col = 0; col < image.Width; ++col, ++len)
			{
				switch (len)
				{
				case 0:
					break;
				case 1:
					repeat = (data[col] == last);
					break;
				default:
					if (data[col] == last && !repeat)//changed
					{
						WriteRLE4((const uint8_t*)&data[col - len], len - 1, false, writer);
						len = 1, repeat = true;
					}
					else if (data[col] != last && repeat)//changed
					{
						WriteRLE4((const uint8_t*)&data[col - len], len, true, writer);
						len = 0, repeat = false;
					}
				}
				last = data[col];
			}
			if (len > 0)
				WriteRLE4((const uint8_t*)&data[image.Width - len], len, repeat, writer);
		}
	}
};

//implementation promise each read should be at least a line
//reading from file is the bottleneck
class RLEFileDecoder
{
private:
	FileObject& ImgFile;
	const uint8_t ElementSize;
	bool Read1(size_t limit, uint8_t * __restrict output)
	{
		while (limit)
		{
			const uint8_t info = ImgFile.ReadByte();
			const uint8_t size = (info & 0x7f) + 1;
			if (size > limit)
				return false;
			limit -= size;
			if (info > 127)
			{
				const uint8_t obj = ImgFile.ReadByte();
				memset(output, obj, size);
			}
			else
			{
				if (!ImgFile.Read(size, output))
					return false;
			}
			output += size;
		}
		return true;
	}
	bool Read2(size_t limit, uint16_t * __restrict output)
	{
		while (limit)
		{
			const uint8_t info = ImgFile.ReadByte();
			const uint8_t size = (info & 0x7f) + 1;
			if (size > limit)
				return false;
			limit -= size;
			if (info > 127)
			{
				uint16_t obj;
				if (!ImgFile.Read(obj))
					return false;
				for (auto count = size; count--;)
					*output++ = obj;
			}
			else
			{
				if (!ImgFile.Read(size * 2, output))
					return false;
				output += size;
			}
		}
		return true;
	}
	bool Read3(size_t limit, uint8_t * __restrict output)
	{
		while (limit)
		{
			const uint8_t info = ImgFile.ReadByte();
			const uint8_t size = (info & 0x7f) + 1;
			if (size > limit)
				return false;
			limit -= size;
			if (info > 127)
			{
				uint8_t obj[3];
				if (ImgFile.Read(obj) != 3)
					return false;
				for (auto count = size; count--;)
				{
					*output++ = obj[0];
					*output++ = obj[1];
					*output++ = obj[2];
				}
			}
			else
			{
				if (!ImgFile.Read(size * 3, output))
					return false;
				output += size * 3;
			}
		}
		return true;
	}
	bool Read4(size_t limit, uint32_t * __restrict output)
	{
		while (limit)
		{
			const uint8_t info = ImgFile.ReadByte();
			const uint8_t size = (info & 0x7f) + 1;
			if (size > limit)
				return false;
			limit -= size;
			if (info > 127)
			{
				uint32_t obj;
				if (!ImgFile.Read(obj))
					return false;
				for (auto count = size; count--;)
					*output++ = obj;
			}
			else
			{
				if (!ImgFile.Read(size * 4, output))
					return false;
				output += size;
			}
		}
		return true;
	}
public:
	RLEFileDecoder(FileObject& file, const uint8_t elementDepth) : ImgFile(file), ElementSize(elementDepth == 15 ? 2 : (elementDepth / 8)) {}
	void Skip(const size_t offset = 0) { ImgFile.Skip(offset); }

	template<class T, typename = typename std::enable_if<std::is_class<T>::value>::type>
	size_t Read(const size_t count, T& output)
	{
		return Read(count * sizeof(T::value_type), output.data()) ? count : 0;
	}

	bool Read(const size_t len, void *ptr)
	{
		switch (ElementSize)
		{
		case 1:
			return Read1(len, (uint8_t*)ptr);
		case 2:
			return Read2(len / 2, (uint16_t*)ptr);
		case 3:
			return Read3(len / 3, (uint8_t*)ptr);
		case 4:
			return Read4(len / 4, (uint32_t*)ptr);
		default:
			return false;
		}
	}
};

Image TgaReader::Read(const ImageDataType dataType)
{
	common::SimpleTimer timer;
	timer.Start();
	if (HAS_FIELD(dataType, ImageDataType::FLOAT_MASK) || REMOVE_MASK(dataType, { ImageDataType::ALPHA_MASK }) == ImageDataType::GREY)
		//NotSuported
		return Image();
	ImgFile.Rewind(detail::TGA_HEADER_SIZE + Header.IdLength);//Next ColorMap(optional)
	Image image(dataType);
	image.SetSize(Width, Height);
	if (Header.ColorMapType)
	{
		if (HAS_FIELD(Header.ImageType, detail::TGAImgType::RLE_MASK))
		{
			RLEFileDecoder decoder(ImgFile, Header.PixelDepth);
			TgaHelper::ReadFromColorMapped(Header, image, ImgFile, decoder);
		}
		else
			TgaHelper::ReadFromColorMapped(Header, image, ImgFile, ImgFile);
	}
	else
	{
		if (HAS_FIELD(Header.ImageType, detail::TGAImgType::RLE_MASK))
		{
			RLEFileDecoder decoder(ImgFile, Header.PixelDepth);
			TgaHelper::ReadFromColor(Header, image, decoder);
		}
		else
			TgaHelper::ReadFromColor(Header, image, ImgFile);
	}
	timer.Stop();
	ImgLog().debug(L"zextga read cost {} ms\n", timer.ElapseMs());
	timer.Start();
	switch ((Header.ImageDescriptor & 0x30) >> 4)
	{
	case 0://left-down
		image.FlipVertical(); break;
	case 1://right-down
		break;
	case 2://left-up
		break;
	case 3://right-up
		break;
	}
	timer.Stop();
	ImgLog().debug(L"zextga flip cost {} ms\n", timer.ElapseMs()); 
	return image;
}

TgaWriter::TgaWriter(FileObject& file) : ImgFile(file)
{
}

void TgaWriter::Write(const Image& image)
{
	constexpr char identity[] = "Truevision TGA file created by zexTGA";
	if (image.Width > INT16_MAX || image.Height > INT16_MAX)
		return;
	if (HAS_FIELD(image.DataType, ImageDataType::FLOAT_MASK))
		return;
	if (REMOVE_MASK(image.DataType, { ImageDataType::FLOAT_MASK, ImageDataType::ALPHA_MASK }) == ImageDataType::GREY)
		return;//not support grey yet
	detail::TgaHeader header;
	
	header.IdLength = sizeof(identity);
	header.ColorMapType = 0;
	header.ImageType = detail::TGAImgType::RLE_MASK | detail::TGAImgType::COLOR;
	memset(&header.ColorMapSpec[0], 0x0, 5);//5 bytes for color map spec
	convert::WordToLE(header.OriginHorizontal, 0);
	convert::WordToLE(header.OriginVertical, 0);
	convert::WordToLE(header.Width, (uint16_t)image.Width);
	convert::WordToLE(header.Height, (uint16_t)image.Height);
	header.PixelDepth = image.ElementSize * 8;
	header.ImageDescriptor = HAS_FIELD(image.DataType, ImageDataType::ALPHA_MASK) ? 0x28 : 0x20;
	
	ImgFile.Write(header);
	ImgFile.Write(identity);
	SimpleTimer timer;
	timer.Start();
	//next: true image data
	if (HAS_FIELD(image.DataType, ImageDataType::ALPHA_MASK))
		TgaHelper::WriteRLEColor4(image, ImgFile);
	else
		TgaHelper::WriteRLEColor3(image, ImgFile);
	timer.Stop();
	ImgLog().debug(L"zextga write cost {} ms\n", timer.ElapseMs());
}


TgaSupport::TgaSupport() : ImgSupport(L"Tga") 
{
	SimpleTimer timer;
	timer.Start();
	auto& map1 = GetBGR16ToRGBAMap();
	auto& map2 = GetRGB16ToRGBAMap();
	timer.Stop();
	const auto volatile time = timer.ElapseMs();
}

}