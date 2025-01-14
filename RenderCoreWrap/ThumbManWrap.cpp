#include "RenderCoreWrapRely.h"
#include "ThumbManWrap.h"
#include "ImageUtil.h"
#include "common/CLIAsync.hpp"

using common::container::FindInMap;
using System::Windows::Media::Imaging::BitmapImage;
namespace Dizz
{


TexHolder::TexHolder(const dizz::TexHolder& holder) : TypeId(static_cast<uint8_t>(holder.index()))
{
    switch (holder.index())
    {
    case 1: WeakRef.Construct(std::get<1>(holder)); break;
    case 2: WeakRef.Construct(std::get<2>(holder)); break;
    default: break;
    }
    name = ToStr(holder.GetName());
    const auto size = holder.GetSize();
    const auto& strBuffer = common::mlog::detail::StrFormater::ToU16Str(u"{}x{}[Mip{}][{}]",
        size.first, size.second, holder.GetMipmapCount(), xziar::img::TexFormatUtil::GetFormatName(holder.GetInnerFormat()));
    format = ToStr(strBuffer);
}

TexHolder::!TexHolder()
{
    WeakRef.Destruct();
}

TexHolder^ TexHolder::CreateTexHolder(const dizz::TexHolder& holder)
{
    switch (holder.index())
    {
    case 1: 
        if (!std::get<1>(holder))
            return nullptr;
        break;
    case 2:
        if (!std::get<2>(holder))
            return nullptr;
        break;
    default: return nullptr;
    }
    return gcnew TexHolder(holder);
}

dizz::TexHolder TexHolder::ExtractHolder()
{
    WRAPPER_NATIVE_PTR_DO(WeakRef, holder, lock());
    if (!holder) return {};
    switch (TypeId)
    {
    case 1: return oglu::oglTex2D(std::reinterpret_pointer_cast<oglu::oglTex2D_>(holder));
    case 2: return std::reinterpret_pointer_cast<dizz::detail::_FakeTex>(holder);
    default: return {};
    }
}


TextureLoader::TextureLoader(const std::shared_ptr<dizz::TextureLoader>& loader)
{
    Loader = new std::weak_ptr<dizz::TextureLoader>(loader);
}

TextureLoader::!TextureLoader()
{
    if (const auto ptr = ExchangeNullptr(Loader); ptr)
        delete ptr;
}

//static TexHolder^ ToTexHolder(IntPtr ptr)
//{
//    return TexHolder::CreateTexHolder(*reinterpret_cast<dizz::FakeTex*>(ptr.ToPointer()));
//}
static gcroot<TexHolder^> ConvTexHolder(const dizz::FakeTex& tex)
{
    return TexHolder::CreateTexHolder(tex);
}
Task<TexHolder^>^ TextureLoader::LoadTextureAsync(BitmapSource^ image, TexLoadType type)
{
    auto bmp = static_cast<BitmapImage^>(image);
    auto fpath = ToU16Str(bmp->UriSource->AbsolutePath);
    auto img = XZiar::Img::ImageUtil::Convert(image);
    auto ret = Loader->lock()->GetTexureAsync(fpath, std::move(img), static_cast<dizz::TexLoadType>(type), true);
    if (const auto it = std::get_if<dizz::FakeTex>(&ret); it)
    {
        return Task::FromResult(TexHolder::CreateTexHolder(*it));
    }
    else
    {
        return ReturnTask<ConvTexHolder>(std::move(*std::get_if<common::PromiseResult<dizz::FakeTex>>(&ret)));
        /*return AsyncWaiter::ReturnTask(std::move(*std::get_if<common::PromiseResult<dizz::FakeTex>>(&ret)),
            gcnew Func<IntPtr, TexHolder^>(ToTexHolder));*/
    }
}

Task<TexHolder^>^ TextureLoader::LoadTextureAsync(String^ fname, TexLoadType type)
{
    auto ret = Loader->lock()->GetTexureAsync(ToU16Str(fname), static_cast<dizz::TexLoadType>(type), true);
    if (const auto it = std::get_if<dizz::FakeTex>(&ret); it)
    {
        return Task::FromResult(TexHolder::CreateTexHolder(*it));
    }
    else
    {
        return ReturnTask<ConvTexHolder>(std::move(*std::get_if<common::PromiseResult<dizz::FakeTex>>(&ret)));
        /*return AsyncWaiter::ReturnTask(std::move(*std::get_if<common::PromiseResult<dizz::FakeTex>>(&ret)),
            gcnew Func<IntPtr, TexHolder^>(ToTexHolder));*/
    }
}


ThumbnailMan::ThumbnailMan(const std::shared_ptr<dizz::ThumbnailManager>& thumbMan)
    : ThumbnailMap(new std::map<xziar::img::ImageView, gcroot<BitmapSource^>, common::AlignBufLessor>())
{
    Lock = gcnew ReaderWriterLockSlim();
    ThumbMan = new std::weak_ptr<dizz::ThumbnailManager>(thumbMan);
}

ThumbnailMan::!ThumbnailMan()
{
    if (const auto ptr = ExchangeNullptr(ThumbMan); ptr)
    {
        delete ThumbnailMap;
        delete ptr;
    }
}

BitmapSource^ ThumbnailMan::GetThumbnail(const xziar::img::ImageView& img)
{
    Lock->EnterReadLock();
    try
    {
        const auto bmp = FindInMap(*ThumbnailMap, img, std::in_place);
        if (bmp.has_value())
            return bmp.value();
    }
    finally
    {
        Lock->ExitReadLock();
    }

    BitmapSource^ timg = XZiar::Img::ImageUtil::Convert(img.AsRawImage());
    if (!timg)
        return nullptr;
    timg->Freeze();
    Lock->EnterWriteLock();
    ThumbnailMap->emplace(img, gcroot<BitmapSource^>(timg));
    Lock->ExitWriteLock();
    return timg;
}

//BitmapSource^ ThumbnailMan::GetThumbnail3(IntPtr imgptr)
//{
//    auto& img = *reinterpret_cast<std::optional<xziar::img::ImageView>*>(imgptr.ToPointer());
//    if (img.has_value())
//        return GetThumbnail(img.value());
//    else
//        return nullptr;
//}

BitmapSource ^ ThumbnailMan::GetThumbnail(const dizz::TexHolder & holder)
{
    auto img = ThumbMan->lock()->GetThumbnail(holder)->Get();
    if (!img.has_value())
        return nullptr;
    return GetThumbnail(img.value());
}

static gcroot<BitmapSource^> ConvThumbnail(std::optional<xziar::img::ImageView>&& img, gcroot<ThumbnailMan^> self)
{
    if (img.has_value())
        return self->GetThumbnail(img.value());
    else
        return nullptr;
}
Task<BitmapSource^>^ ThumbnailMan::GetThumbnailAsync(TexHolder^ holder)
{
    return ReturnTask<ConvThumbnail>(ThumbMan->lock()->GetThumbnail(holder->ExtractHolder()), this);
    /*return AsyncWaiter::ReturnTask(ThumbMan->lock()->GetThumbnail(holder->ExtractHolder()),
        gcnew Func<IntPtr, BitmapSource^>(this, &ThumbnailMan::GetThumbnail3));*/
}


}

