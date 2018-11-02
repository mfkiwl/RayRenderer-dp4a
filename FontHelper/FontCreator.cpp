#include "FontRely.h"
#include "FontCreator.h"
#include "freetype2/freetype2.h"
#include "OpenCLUtil/oclException.h"
#include <cmath>
#include "resource.h"
#pragma warning(disable : 4996)
#include "fmt/time.h"
#pragma warning(default : 4996)

namespace oglu
{
using namespace oclu;
using xziar::img::Image;
using xziar::img::ImageDataType;


struct FontInfo
{
    uint32_t offset;
    uint8_t w, h;
};

void FontCreator::loadCL(const string& src)
{
    oclProgram clProg(clCtx, src);
    try
    {
        oclu::CLProgConfig config;
        config.Flags.insert("-cl-fast-relaxed-math");
        config.Defines.insert_or_assign("LOC_MEM_SIZE", clCtx->Devices[0]->LocalMemSize);
        clProg->Build(config);
    }
    catch (OCLException& cle)
    {
        fntLog().error(u"Fail to build opencl Program:\n{}\n", cle.message);
        COMMON_THROW(BaseException, u"build Program error");
    }
    kerSdf = clProg->GetKernel("bmpsdf");
    kerSdfGray = clProg->GetKernel("graysdf");
    kerSdfGray4 = clProg->GetKernel("graysdf4");
}

void FontCreator::loadDownSampler(const string& src)
{
    oclProgram clProg(clCtx, src);
    try
    {
        oclu::CLProgConfig config;
        if (clCtx->vendor == Vendor::NVIDIA)
        {
            config.Flags.insert({"-cl-kernel-arg-info", "-cl-nv-verbose"});
            config.Defines.insert_or_assign("NVIDIA", std::monostate{});
        }
        config.Defines.insert_or_assign("LOC_MEM_SIZE", clCtx->Devices[0]->LocalMemSize);
        clProg->Build(config);
    }
    catch (OCLException& cle)
    {
        fntLog().error(u"Fail to build opencl Program:\n{}\n", cle.message);
        COMMON_THROW(BaseException, u"build Program error");
    }
    kerDownSamp = clProg->GetKernel("avg16");// "downsample4");
}

FontCreator::FontCreator(const oclu::oclContext ctx) : clCtx(ctx)
{
    const auto dev = ctx->GetGPUDevice();
    if (!dev)
        COMMON_THROW(BaseException, u"There may be no GPU device found in context");
    clQue.reset(clCtx, dev);

    //prepare LUT
    {
        std::array<float, 256> lut;
        //sq256lut.reset(clCtx, MemType::ReadOnly | MemType::HostWriteOnly, lut.size() * 4);
        for (uint16_t i = 0; i < 256; ++i)
            lut[i] = i * i * 65536.0f;
    }
    
    loadCL(getShaderFromDLL(IDR_SHADER_SDFTEST));
    loadDownSampler(getShaderFromDLL(IDR_SHADER_DWNSAMP));
    testTex.reset();
    testTex->SetProperty(TextureFilterVal::Nearest, TextureWrapVal::Repeat);
}

FontCreator::~FontCreator()
{

}

void FontCreator::reloadFont(const fs::path& fontpath)
{
    ft2.reset(fontpath, (uint16_t)124, (uint16_t)2);
}


void FontCreator::reload(const string& src)
{
    kerSdf.release();
    kerSdfGray.release();
    kerDownSamp.release();
    loadCL(src);
    loadDownSampler(getShaderFromDLL(IDR_SHADER_DWNSAMP));
}

void FontCreator::setChar(char32_t ch) const
{
    auto[img, width, height] = ft2->getChBitmap(ch);
    testTex->SetData(TextureInnerFormat::R8, TextureDataFormat::R8, width, height, img.GetRawPtr());
}

Image FontCreator::clgraysdfs(char32_t ch, uint32_t count) const
{
    constexpr uint32_t fontsizelim = 128, fontcountlim = 64, newfontsize = 36;
    const auto fontCount = static_cast<uint16_t>(std::ceil(std::sqrt(count)));
    vector<FontInfo> finfos;
    finfos.reserve(fontCount * fontCount);
    
    SimpleTimer timer;
    fntLog().verbose(u"raster start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
    timer.Start();
    common::AlignedBuffer buffer1(fontCount * fontCount * fontsizelim * fontsizelim * sizeof(uint8_t), 4096);
    size_t offset = 0;
    for (uint32_t a = 0; a < count; ++a)
    {
        auto[img, width, height] = ft2->getChBitmap(ch + a);
        Image tmpimg(std::move(img), width, height, ImageDataType::GRAY);
        if (width > fontsizelim || height > fontsizelim)
        {
            const auto chstr = common::strchset::to_u16string(std::u32string(1, ch + a), common::str::Charset::UTF32);
            fntLog().warning(u"ch {} has invalid size {} x {}\n", chstr, width, height);
            const auto ratio = std::max(width, height) * 4 / 128.0f;
            width = std::max(1u, uint32_t(width / ratio) * 4), height = std::max(1u, uint32_t(height / ratio) * 4);
            tmpimg.Resize(width, height);
            memcpy_s(img.GetRawPtr(), img.GetSize(), tmpimg.GetRawPtr(), tmpimg.GetSize());
        }
        if (offset % 16)
            COMMON_THROW(BaseException, u"offset wrong");
        if (width % 4)
            COMMON_THROW(BaseException, u"width wrong");
        if (height % 4)
            COMMON_THROW(BaseException, u"height wrong");
        finfos.push_back(FontInfo{ (uint32_t)offset,(uint8_t)width,(uint8_t)height });
        memcpy_s(buffer1.GetRawPtr() + offset, buffer1.GetSize() - offset, tmpimg.GetRawPtr(), tmpimg.GetSize());
        offset += tmpimg.GetSize();
    }
    timer.Stop();
    fntLog().verbose(u"raster cost {} us\n", timer.ElapseUs());

    fntLog().verbose(u"prepare start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
    timer.Start();
    oclu::oclBuffer inputBuf(clCtx, MemFlag::ReadOnly | MemFlag::HostNoAccess | MemFlag::UseHost, buffer1.GetSize(), buffer1.GetRawPtr());
    oclu::oclBuffer infoBuf(clCtx, MemFlag::ReadOnly | MemFlag::HostNoAccess | MemFlag::HostCopy, finfos.size() * sizeof(FontInfo), finfos.data());
    common::AlignedBuffer buffer2(fontCount * fontCount * fontsizelim / 4 * fontsizelim / 4 * sizeof(uint8_t), 4096);
    oclu::oclBuffer outputBuf(clCtx, MemFlag::WriteOnly | MemFlag::HostReadOnly | MemFlag::UseHost, buffer2.GetSize(), buffer2.GetRawPtr());
    timer.Stop();
    fntLog().verbose(u"prepare cost {} us\n", timer.ElapseUs());
    if (true)
    {
        fntLog().verbose(u"OpenCL start at {:%H:%M:%S}\n", SimpleTimer::getCurLocalTime());
        timer.Start();
        size_t localsize[] = { fontsizelim / 4 }, worksize[] = { fontsizelim / 4 * count };

        kerSdfGray4->SetArg(0, infoBuf);
        kerSdfGray4->SetArg(1, inputBuf);
        kerSdfGray4->SetArg(2, outputBuf);
        auto pms = kerSdfGray4->Run<1>(clQue, worksize, localsize, false);
        pms->Wait();
        timer.Stop();
        fntLog().verbose(u"OpenCl [sdfGray4] cost {}us ({}us by OCL)\n", timer.ElapseUs(), pms->ElapseNs() / 1000);
        outputBuf->Map(clQue, oclu::MapFlag::Read);

        fntLog().verbose(u"post-merging start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
        timer.Start();
        Image fin(ImageDataType::GRAY);
        fin.SetSize(newfontsize * fontCount, newfontsize * fontCount, byte(255));
        const uint32_t rowstep = (uint32_t)fin.RowSize();
        uint32_t fidx = 0;
        for (const auto& fi : finfos)
        {
            const uint32_t neww = fi.w / 4, newh = fi.h / 4;
            const auto offsetx = (newfontsize - neww) / 2, offsety = (newfontsize - newh) / 2;
            uint32_t startx = (fidx % fontCount) * newfontsize + offsetx, starty = (fidx / fontCount) * newfontsize + offsety;
            uint8_t * __restrict finPtr = fin.GetRawPtr<uint8_t>(starty, startx);
            const uint8_t * __restrict inPtr = buffer2.GetRawPtr<uint8_t>() + fi.offset / 16;
            for (uint32_t row = 0; row < newh; ++row)
            {
                memcpy_s(finPtr, neww, inPtr, neww);
                finPtr += rowstep, inPtr += neww;
            }
            fidx++;
        }
        timer.Stop();
        fntLog().verbose(u"post-process cost {} us\n", timer.ElapseUs());
        return fin;
    }
    else
    {
        oclu::oclBuffer middleBuf(clCtx, MemFlag::ReadWrite | MemFlag::HostReadOnly, fontCount * fontCount * fontsizelim * fontsizelim * sizeof(uint16_t));

        fntLog().verbose(u"OpenCL start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
        timer.Start();
        size_t localsize[] = { fontsizelim }, worksize[] = { fontsizelim * count };

        kerSdfGray->SetArg(0, infoBuf);
        kerSdfGray->SetArg(1, inputBuf);
        kerSdfGray->SetArg(2, middleBuf);
        kerSdfGray->Run<1>(clQue, worksize, localsize, true);
        timer.Stop();
        fntLog().verbose(u"OpenCl cost {} us\n", timer.ElapseUs());
        if (false)
        {
            fntLog().verbose(u"clDownSampler start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
            timer.Start();
            localsize[0] /= 4, worksize[0] /= 4;
            kerDownSamp->SetArg(0, infoBuf);
            kerDownSamp->SetArg(1, middleBuf);
            kerDownSamp->SetArg(2, outputBuf);
            kerDownSamp->Run<1>(clQue, worksize, localsize, true);
            timer.Stop();
            fntLog().verbose(u"OpenCl[clDownSampler] cost {} us\n", timer.ElapseUs());
            outputBuf->Map(clQue, oclu::MapFlag::Read);

            fntLog().verbose(u"post-merging start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
            timer.Start();
            Image fin(ImageDataType::GRAY);
            fin.SetSize(newfontsize * fontCount, newfontsize * fontCount, byte(255));
            const uint32_t rowstep = (uint32_t)fin.RowSize();
            uint32_t fidx = 0;
            for (const auto& fi : finfos)
            {
                const uint32_t neww = fi.w / 4, newh = fi.h / 4;
                const auto offsetx = (newfontsize - neww) / 2, offsety = (newfontsize - newh) / 2;
                uint32_t startx = (fidx % fontCount) * newfontsize + offsetx, starty = (fidx / fontCount) * newfontsize + offsety;
                uint8_t * __restrict finPtr = fin.GetRawPtr<uint8_t>(starty, startx);
                const uint8_t * __restrict inPtr = buffer2.GetRawPtr<uint8_t>() + fi.offset / 16;
                for (uint32_t row = 0; row < newh; ++row)
                {
                    memcpy_s(finPtr, neww, inPtr, neww);
                    finPtr += rowstep, inPtr += neww;
                }
                fidx++;
            }
            timer.Stop();
            fntLog().verbose(u"post-process cost {} us\n", timer.ElapseUs());
            return fin;
        }
        else
        {
            const auto midPtr = middleBuf->Map(clQue, oclu::MapFlag::Read);
            const int16_t* __restrict distsq = midPtr.AsType<const int16_t>();
            fntLog().verbose(u"post-process start at {:%H:%H:%S}\n", SimpleTimer::getCurLocalTime());
            timer.Start();
            Image fin(ImageDataType::GRAY);
            fin.SetSize(newfontsize * fontCount, newfontsize * fontCount, byte(255));
            const uint32_t rowstep = (uint32_t)fin.RowSize();
            uint8_t * __restrict const finPtr = fin.GetRawPtr<uint8_t>();
            uint32_t fidx = 0;
            for (const auto& fi : finfos)
            {
                if (false && (fi.w == 0 || fi.h == 0))
                {
                    const auto idx = ((&fi) - finfos.data()) / sizeof(FontInfo);
                    fntLog().warning(u"index {} get wrong w/h", idx);
                }
                const auto offsetx = (newfontsize - fi.w / 4) / 2, offsety = (newfontsize - fi.h / 4) / 2;
                uint32_t startx = (fidx % fontCount) * newfontsize + offsetx, starty = (fidx / fontCount) * newfontsize + offsety;
                for (uint32_t y = 0, ylim = fi.h / 4; y < ylim; y++)
                {
                    uint32_t opos = (starty + y) * rowstep + startx;
                    size_t ipos = fi.offset + (fi.w * y * 4);
                    for (uint32_t xlim = fi.w / 4; xlim--; ipos += 4, ++opos)
                    {
                        const uint64_t data64[4] = { *(uint64_t*)&distsq[ipos],*(uint64_t*)&distsq[ipos + fi.w] ,*(uint64_t*)&distsq[ipos + 2 * fi.w] ,*(uint64_t*)&distsq[ipos + 3 * fi.w] };
                        const int16_t * __restrict const data16 = (int16_t*)data64;
                        const auto insideCount = (data16[5] < 0 ? 1 : 0) + (data16[6] < 0 ? 1 : 0) + (data16[9] < 0 ? 1 : 0) + (data16[10] < 0 ? 1 : 0);
                        int32_t dists[8];
                        dists[0] = data16[5] * 3 - data16[0], dists[1] = data16[6] * 3 - data16[3], dists[2] = data16[9] * 3 - data16[12], dists[3] = data16[10] * 3 - data16[15];
                        dists[4] = ((data16[5] + data16[9]) * 3 - (data16[4] + data16[8])) / 2, dists[5] = ((data16[6] + data16[10]) * 3 - (data16[7] + data16[11])) / 2;
                        dists[6] = ((data16[5] + data16[6]) * 3 - (data16[1] + data16[2])) / 2, dists[7] = ((data16[9] + data16[10]) * 3 - (data16[13] + data16[14])) / 2;
                        int32_t avg4 = (data16[5] + data16[6] + data16[9] + data16[10]) / 2;
                        int32_t negsum = 0, possum = 0, negcnt = 0, poscnt = 0;
                        for (const auto dist : dists)
                        {
                            if (dist <= 0)
                                negsum += dist, negcnt++;
                            else
                                possum += dist, poscnt++;
                        }
                        if (negcnt == 0)
                            finPtr[opos] = (uint8_t)std::clamp(possum / (poscnt * 32) + 128, 0, 255);
                        else if (poscnt == 0)
                            finPtr[opos] = (uint8_t)std::clamp(negsum * 3 / (negcnt * 64) + 128, 0, 255);
                        else
                        {
                            const bool isInside = avg4 < 0;
                            const auto distsum = isInside ? ((negsum + avg4) * 4 / (negcnt + 1)) : ((possum + avg4) * 2 / (poscnt + 1));
                            finPtr[opos] = (uint8_t)std::clamp(distsum / 64 + 128, 0, 255);
                        }
                    }
                }
                fidx++;
            }
            timer.Stop();
            fntLog().verbose(u"post-process cost {} us\n", timer.ElapseUs());
            return fin;
        }
    }
}

}