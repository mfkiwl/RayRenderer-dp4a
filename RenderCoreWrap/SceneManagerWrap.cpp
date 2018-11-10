#include "RenderCoreWrapRely.h"
#include "SceneManagerWrap.h"
#include "ImageUtil.h"
#include "common/CLIAsync.hpp"


namespace Dizz
{


std::shared_ptr<rayr::PBRMaterial> PBRMaterial::GetSelf()
{
    return std::static_pointer_cast<rayr::PBRMaterial>(GetControl()); // type promised
}

PBRMaterial::PBRMaterial(const std::shared_ptr<rayr::PBRMaterial>& material) : Controllable(material)
{
}

TexHolder^ PBRMaterial::DiffuseMap::get()
{
    return TexHolder::CreateTexHolder(GetSelf()->DiffuseMap);
}
TexHolder^ PBRMaterial::NormalMap::get()
{
    return TexHolder::CreateTexHolder(GetSelf()->NormalMap);
}
TexHolder^ PBRMaterial::MetalMap::get()
{
    return TexHolder::CreateTexHolder(GetSelf()->MetalMap);
}
TexHolder^ PBRMaterial::RoughMap::get()
{
    return TexHolder::CreateTexHolder(GetSelf()->RoughMap);
}
TexHolder^ PBRMaterial::AOMap::get()
{
    return TexHolder::CreateTexHolder(GetSelf()->AOMap);
}

std::shared_ptr<rayr::Drawable> Drawable::GetSelf()
{
    return std::static_pointer_cast<rayr::Drawable>(GetControl()); // type promised
}
bool Drawable::CreateMaterials()
{
    const std::shared_ptr<rayr::Drawable>& drawable = GetSelf();
    const auto matCount = drawable->MaterialHolder.GetSize();
    if (matCount == 0)
        return false;
    
    array<PBRMaterial^>^ matArray = gcnew array<PBRMaterial^>(matCount);
    uint8_t idx = 0;
    for (const auto& mat : drawable->MaterialHolder)
        matArray[idx++] = gcnew PBRMaterial(mat);
    materials = Array::AsReadOnly(matArray);
    RaisePropertyChanged("Materials");
    return true;
}
Drawable::Drawable(const Wrapper<rayr::Drawable>& drawable) : Controllable(drawable)
{
    CreateMaterials();
    Uid = ToGuid(drawable->GetUid());
    DrawableType = ToStr(drawable->GetType());
}
Drawable::Drawable(Wrapper<rayr::Drawable>&& drawable) : Controllable(drawable), TempHandle(new Wrapper<rayr::Drawable>(drawable))
{
    CreateMaterials();
    Uid = ToGuid((*TempHandle)->GetUid());
    DrawableType = ToStr((*TempHandle)->GetType());
}

void Drawable::ReleaseTempHandle()
{
    if (const auto ptr = ExchangeNullptr(TempHandle); ptr)
        delete ptr;
}

Drawable::!Drawable()
{
    ReleaseTempHandle();
}

void Drawable::Move(const float dx, const float dy, const float dz)
{
    GetSelf()->Move(dx, dy, dz);
    RaisePropertyChanged("Position");
}

void Drawable::Rotate(const float dx, const float dy, const float dz)
{
    GetSelf()->Rotate(dx, dy, dz);
    RaisePropertyChanged("Rotation");
}

String^ Drawable::ToString()
{
    return "[" + DrawableType + "]" + ToStr(GetSelf()->Name);
}


std::shared_ptr<rayr::Light> Light::GetSelf()
{
    return std::static_pointer_cast<rayr::Light>(GetControl()); // type promised
}

Light::Light(const Wrapper<rayr::Light>& light) : Controllable(light)
{
    LgtType = static_cast<LightType>(light->Type);
}
Light::Light(Wrapper<rayr::Light>&& light) : Controllable(light), TempHandle(new Wrapper<rayr::Light>(light))
{
    LgtType = static_cast<LightType>((*TempHandle)->Type);
}

void Light::ReleaseTempHandle()
{
    if (const auto ptr = ExchangeNullptr(TempHandle); ptr)
        delete ptr;
}

void Light::Move(const float dx, const float dy, const float dz)
{
    GetSelf()->Move(dx, dy, dz);
    RaisePropertyChanged("Position");
}

void Light::Rotate(const float dx, const float dy, const float dz)
{
    GetSelf()->Rotate(dx, dy, dz);
    RaisePropertyChanged("Direction");
}

String^ Light::ToString()
{
    return "[" + LgtType.ToString() + "]" + ToStr(GetSelf()->Name);
}

#pragma managed(push, off)
static Wrapper<rayr::Light> CreateLight(rayr::LightType type)
{
    Wrapper<rayr::Light> light;
    switch (type)
    {
    case rayr::LightType::Parallel:
        light = Wrapper<rayr::ParallelLight>(std::in_place);
        light->Color = b3d::Vec4(1.0, 0.3, 0.3, 1.0);
        break;
    case rayr::LightType::Point:
        light = Wrapper<rayr::PointLight>(std::in_place);
        light->Color = b3d::Vec4(0.3, 1.0, 0.3, 1.0);
        break;
    case rayr::LightType::Spot:
        light = Wrapper<rayr::SpotLight>(std::in_place);
        light->Color = b3d::Vec4(0.3, 0.3, 1.0, 1.0);
        break;
    }
    light->Direction = b3d::Vec4(0, 0, 1, 0);
    return light;
}
#pragma managed(pop)
Light^ Light::NewLight(LightType type)
{
    return gcnew Light(CreateLight((rayr::LightType)type));
}


std::shared_ptr<rayr::Camera> Camera::GetSelf()
{
    return std::static_pointer_cast<rayr::Camera>(GetControl()); // type promised
}

Camera::Camera(const Wrapper<rayr::Camera>& camera) : Controllable(camera)
{
}

void Camera::Move(const float dx, const float dy, const float dz)
{
    GetSelf()->Move(dx, dy, dz);
    RaisePropertyChanged("Position");
}

void Camera::Rotate(const float dx, const float dy, const float dz)
{
    GetSelf()->Rotate(dx, dy, dz);
    RaisePropertyChanged("Direction");
}

//rotate along x-axis, radius
void Camera::Pitch(const float radx)
{
    GetSelf()->Pitch(radx);
    RaisePropertyChanged("Direction");
}

//rotate along y-axis, radius
void Camera::Yaw(const float rady)
{
    GetSelf()->Yaw(rady);
    RaisePropertyChanged("Direction");
}

//rotate along z-axis, radius
void Camera::Roll(const float radz)
{
    GetSelf()->Roll(radz);
    RaisePropertyChanged("Direction");
}


static bool PrepareDrawableMaterial(Drawable^ drawable)
{
    return !drawable->CreateMaterials();
}
static Scene::Scene()
{
    DrawablePrepareFunc = gcnew Func<Drawable^, bool>(PrepareDrawableMaterial);
}
Scene::Scene(const rayr::RenderCore * core) : Core(core)
{
    const auto& scene = Core->GetScene();
    TheScene = new std::weak_ptr<rayr::Scene>(scene);
    MainCamera = gcnew Camera(scene->GetCamera());
    Drawables = gcnew ObservableProxyContainer<Drawable^>();
    Drawables->BeforeAddObject += gcnew AddObjectEventHandler<Drawable^>(this, &Scene::BeforeAddModel);
    Drawables->BeforeDelObject += gcnew DelObjectEventHandler<Drawable^>(this, &Scene::BeforeDelModel);
    Drawables->CollectionChanged += gcnew NotifyCollectionChangedEventHandler(this, &Scene::OnDrawablesChanged);
    Lights = gcnew ObservableProxyContainer<Light^>();
    Lights->BeforeAddObject += gcnew AddObjectEventHandler<Light^>(this, &Scene::BeforeAddLight);
    Lights->BeforeDelObject += gcnew DelObjectEventHandler<Light^>(this, &Scene::BeforeDelLight);
    Lights->ObjectPropertyChanged += gcnew ObjectPropertyChangedEventHandler<Light^>(this, &Scene::OnLightPropertyChanged);
    //Lights->CollectionChanged += gcnew NotifyCollectionChangedEventHandler(this, &Scene::OnLightsChanged);
    WaitDrawables = gcnew List<Drawable^>();
    RefreshScene();
}

void Scene::PrepareScene()
{
    WaitDrawables = Linq::Enumerable::ToList(Linq::Enumerable::Where(WaitDrawables, DrawablePrepareFunc));
}

void Scene::RefreshScene()
{
    const auto scene = TheScene->lock();

    for (const auto& drw : common::container::ValSet(scene->GetDrawables()))
    {
        Drawables->InnerAdd(gcnew Drawable(drw));
    }

    for (const auto& lgt : scene->GetLights())
    {
        Lights->InnerAdd(gcnew Light(lgt));
    }
}

void Scene::BeforeAddModel(Object^ sender, Drawable^ object, bool% shouldAdd)
{
    if (TheScene->lock()->AddObject(object->GetSelf()))
    {
        object->ReleaseTempHandle();
        shouldAdd = true;
    }
}

void Scene::BeforeDelModel(Object^ sender, Drawable^ object, bool% shouldDel)
{
    shouldDel = TheScene->lock()->DelObject(FromGuid(object->Uid));
}

void Scene::BeforeAddLight(Object^ sender, Light^ object, bool% shouldAdd)
{
    if (TheScene->lock()->AddLight(object->GetSelf()))
    {
        object->ReleaseTempHandle();
        shouldAdd = true;
    }
}

void Scene::BeforeDelLight(Object^ sender, Light^ object, bool% shouldDel)
{
    shouldDel = TheScene->lock()->DelLight(object->GetSelf());
}

void Scene::OnDrawablesChanged(Object ^ sender, NotifyCollectionChangedEventArgs ^ e)
{
    switch (e->Action)
    {
    case NotifyCollectionChangedAction::Add:
        for each (Drawable^ drw in Linq::Enumerable::Cast<Drawable^>(e->NewItems))
        {
            if (drw->Materials == nullptr)
                WaitDrawables->Add(drw);
        }
        break;
    default: break;
    }
}

//void Scene::OnLightsChanged(Object^ sender, NotifyCollectionChangedEventArgs^ e)
//{
//    switch (e->Action)
//    {
//    case NotifyCollectionChangedAction::Add:
//        break;
//    default: break;
//    }
//}

void Scene::OnLightPropertyChanged(Object^ sender, Light^ object, PropertyChangedEventArgs^ e)
{
    TheScene->lock()->ReportChanged(rayr::SceneChange::Light);
}

Scene::!Scene()
{
    if (const auto ptr = ExchangeNullptr(TheScene); ptr)
        delete ptr;
}


}