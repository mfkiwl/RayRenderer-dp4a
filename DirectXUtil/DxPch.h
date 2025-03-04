#pragma once
#include "DxRely.h"
#include "SystemCommon/StringConvert.h"
#include "SystemCommon/FileEx.h"
#include "SystemCommon/ThreadEx.h"
#include "SystemCommon/StackTrace.h"
#include "SystemCommon/CopyEx.h"
#include "SystemCommon/MiniLogger.h"
#include "SystemCommon/Exceptions.h"
#include "ImageUtil/ImageUtil.h"
#include "ImageUtil/TexFormat.h"
#include "common/Linq2.hpp"
#include "common/StringEx.hpp"
#include "common/StringLinq.hpp"

#include <algorithm>

#include <boost/range/adaptor/reversed.hpp>


#define COM_NO_WINDOWS_H
#define NOMINMAX 1
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_6.h>
#include "d3dx12.h"

//fucking wingdi defines some terrible macro
#undef ERROR
#undef MemoryBarrier


#define THROW_HR(eval, msg) if (const common::HResultHolder hr___ = eval; !hr___) COMMON_THROWEX(DxException, hr___, msg)



namespace dxu
{

namespace detail
{

namespace pix
{
void LoadPixDll(common::fs::path dllPath);
bool CheckPixLoad();
void BeginEvent(ID3D12CommandQueue* context, UINT64 color, std::u16string_view str);
void BeginEvent(ID3D12GraphicsCommandList* context, UINT64 color, std::u16string_view str);
void EndEvent(ID3D12CommandQueue* context);
void EndEvent(ID3D12GraphicsCommandList* context);
void SetMarker(ID3D12CommandQueue* context, UINT64 color, std::u16string_view str);
void SetMarker(ID3D12GraphicsCommandList* context, UINT64 color, std::u16string_view str);
void NotifyWakeFromFenceSignal(HANDLE handle);
}

struct IIDPPVPair
{
    REFIID TheIID;
    void** PtrObj;
};

struct IIDData
{
    REFIID TheIID;
};

#define ProxyType(type, dxtype) \
struct type : public dxtype     \
{                               \
    using RealType = dxtype;    \
}
#define ExtendType(type, dxtype, ...)   \
struct type : public dxtype             \
{                                       \
    using RealType = dxtype;            \
    __VA_ARGS__                         \
}

ProxyType(Adapter,              IUnknown);
ProxyType(Device,               ID3D12Device);
ProxyType(QueryHeap,            ID3D12QueryHeap);
ProxyType(CmdAllocator,         ID3D12CommandAllocator);
// althought have different list types, they all called "GraphicsCommandList"
ProxyType(CmdList,              ID3D12GraphicsCommandList);
ProxyType(CmdQue,               ID3D12CommandQueue);
ProxyType(Fence,                ID3D12Fence);
ProxyType(Resource,             ID3D12Resource);
ProxyType(ResourceDesc,         D3D12_RESOURCE_DESC);
ProxyType(DescHeap,             ID3D12DescriptorHeap);
ExtendType(BindResourceDetail,  D3D12_SHADER_INPUT_BIND_DESC,
    common::StringPiece<char> NameSv;);
ProxyType(RootSignature,        ID3D12RootSignature);
ProxyType(PipelineState,        ID3D12PipelineState);

#undef ProxyType
#undef ExtendType


#define ClzProxy(clz, type, dxtype) \
struct clz::type : public dxtype    \
{                                   \
    using RealType = dxtype;        \
}


template<typename T>
struct OptRet
{
    T Data;
    common::HResultHolder HResult;
    constexpr explicit operator bool() const noexcept { return (bool)HResult; }
          T* operator->() noexcept
    {
        return &Data;
    }
    const T* operator->() const noexcept
    {
        return &Data;
    }
};

[[nodiscard]] uint32_t TexFormatToDXGIFormat(xziar::img::TextureFormat format) noexcept;
[[nodiscard]] std::u16string TryErrorString(std::u16string str, const Microsoft::WRL::ComPtr<ID3DBlob>& errBlob);

}


common::mlog::MiniLogger<false>& dxLog();

}
