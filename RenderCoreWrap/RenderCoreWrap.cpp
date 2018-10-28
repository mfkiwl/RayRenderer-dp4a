#include "RenderCoreWrapRely.h"
#include "RenderCoreWrap.h"
#include "ImageUtil.h"
#include "common/CLIAsync.hpp"


namespace Dizz
{


RenderCore::RenderCore() : Core(new rayr::RenderCore())
{
    Core->TestSceneInit();
    theScene = gcnew Scene(Core);
    theScene->Drawables->CollectionChanged += gcnew NotifyCollectionChangedEventHandler(this, &RenderCore::OnDrawablesChanged);
    PostProc = gcnew Common::Controllable(Core->GetPostProc());
}

RenderCore::!RenderCore()
{
    if (const auto ptr = ExchangeNullptr(Core); ptr)
        delete ptr;
}

void RenderCore::OnDrawablesChanged(Object ^ sender, NotifyCollectionChangedEventArgs^ e)
{
    switch (e->Action)
    {
    case NotifyCollectionChangedAction::Add:
        for each (Object^ item in e->NewItems)
        {
            const auto drawable = static_cast<Drawable^>(item)->GetSelf(); // type promised
            for (const auto& shd : Core->GetRenderPasses())
                shd->RegistDrawable(drawable);
        }
        break;
    default: break;
    }
}

void RenderCore::Draw()
{
    Core->Draw();
    theScene->PrepareScene();
}

void RenderCore::Resize(const uint32_t w, const uint32_t h)
{
    Core->Resize(w, h);
}


Drawable^ ConstructDrawable(CLIWrapper<Wrapper<rayr::Model>>^ theModel)
{
    return gcnew Drawable(theModel->Extract());
}
Task<Drawable^>^ RenderCore::LoadModelAsync(String^ fname)
{
    return doAsync3<Drawable^>(gcnew Func<CLIWrapper<Wrapper<rayr::Model>>^, Drawable^>(&ConstructDrawable),
        &rayr::RenderCore::LoadModelAsync, *Core, ToU16Str(fname));
}

void RenderCore::Serialize(String^ path)
{
    Core->Serialize(ToU16Str(path));
}

void RenderCore::DeSerialize(String^ path)
{
    Core->DeSerialize(ToU16Str(path));
    delete theScene;
    theScene = gcnew Scene(Core);
    RaisePropertyChanged("TheScene");
}

Action<String^>^ RenderCore::Screenshot()
{
    auto saver = gcnew XZiar::Img::ImageHolder(Core->Screenshot());
    return gcnew Action<String^>(saver, &XZiar::Img::ImageHolder::Save);
}


}