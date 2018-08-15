#pragma once
#include "oglRely.h"
#include "oglShader.h"
#include "oglBuffer.h"
#include "oglTexture.h"
#include "oglVAO.h"
#include "oglFBO.h"
#include "oglUtil.h"

namespace oglu
{

using UniformValue = std::variant<miniBLAS::Vec3, miniBLAS::Vec4, miniBLAS::Mat3x3, miniBLAS::Mat4x4, b3d::Coord2D, bool, int32_t, uint32_t, float>;

namespace detail
{
class _oglProgram;
class ProgState;
class ProgDraw;
}

struct OGLUAPI ProgramResource : public common::container::NamedSetValue<ProgramResource, string>
{
    friend class detail::_oglProgram;
    friend class detail::ProgState;
    friend class detail::ProgDraw;
private:
    GLint GetValue(const GLuint pid, const GLenum prop);
    void InitData(const GLuint pid, const GLint idx);
public:
    GLint location = GL_INVALID_INDEX;
    //length of array
    mutable GLuint len = 1;
    GLenum type;
    GLenum valtype;
    uint16_t size = 0;
    uint8_t ifidx;
    const string Name;
    ProgramResource(const GLenum type_, const string& name) :type(type_), Name(name) { }
    string_view GetTypeName() const noexcept;
    string_view GetValTypeName() const noexcept;
    bool isUniformBlock() const noexcept { return type == GL_UNIFORM_BLOCK; }
    bool isAttrib() const noexcept { return type == GL_PROGRAM_INPUT; }
    bool isTexture() const noexcept;
};

class OGLUAPI SubroutineResource : public common::container::NamedSetValue<SubroutineResource, string>
{
    friend class detail::_oglProgram;
    friend class detail::ProgState;
    friend class detail::ProgDraw;
public:
    struct OGLUAPI Routine
    {
        friend class SubroutineResource;
        friend class ::oglu::detail::_oglProgram;
        friend class ::oglu::detail::ProgState;
        friend class ::oglu::detail::ProgDraw;
        const string Name;
    private:
        const GLuint Id;
        Routine(const string& name, const GLuint id) : Name(name), Id(id) {}
    };
    const string Name;
    const vector<Routine> Routines;
    const ShaderType Stage;
private:
    const GLint UniLoc;
public:
    SubroutineResource(const GLenum stage, const GLint location, const string& name, vector<Routine>&& routines)
        : Name(name), Routines(std::move(routines)), Stage(ShaderType(stage)), UniLoc(location) {}
};


enum class ProgramMappingTarget : uint64_t 
{ 
    ProjectMat = "ProjectMat"_hash, ViewMat = "ViewMat"_hash, ModelMat = "ModelMat"_hash, MVPMat = "MVPMat"_hash, MVPNormMat = "MVPNormMat"_hash,
    CamPosVec = "CamPosVec"_hash,
    VertPos = "VertPos"_hash, VertNorm = "VertNorm"_hash, VertTexc = "VertTexc"_hash, VertColor = "VertColor"_hash, VertTan = "VertTan"_hash, DrawID = "DrawID"_hash
};

namespace detail
{


class OGLUAPI _oglProgram : public NonCopyable, public NonMovable, public common::AlignBase<32>, public std::enable_shared_from_this<_oglProgram>
{
    friend class TextureManager;
    friend class UBOManager;
    friend class ProgState;
protected:
    Mat4x4 matrix_Proj, matrix_View;
    set<oglShader> shaders;
    string ExtShaderSource;
    ShaderExtInfo ExtInfo;
    set<ShaderExtProperty, std::less<>> ShaderProperties;
    map<string, const ProgramResource*, std::less<>> ResNameMapping;
    set<ProgramResource, std::less<>> ProgRess;
    set<ProgramResource, std::less<>> TexRess;
    set<ProgramResource, std::less<>> UBORess;
    set<ProgramResource, std::less<>> AttrRess;
    set<SubroutineResource, std::less<>> SubroutineRess;
    map<const SubroutineResource::Routine*, const SubroutineResource*> subrLookup;
    map<GLint, UniformValue> UniValCache;
    vector<GLint> UniBindCache; 
    map<GLuint, oglTexBase> TexBindings;
    map<GLuint, oglUBO> UBOBindings;
    map<const SubroutineResource*, const SubroutineResource::Routine*> SubroutineBindings;
    map<ShaderType, vector<GLuint>> SubroutineSettings;
    GLuint programID = 0; //zero means invalid program

    static bool usethis(_oglProgram& prog, const bool change = true);
    void RecoverState();
    void InitLocs();
    void InitSubroutines();
    void FilterProperties();
    virtual void OnPrepare() = 0;
    GLint GetLoc(const ProgramResource* res, GLenum valtype) const;
    GLint GetLoc(const string& name, GLenum valtype) const;

    void SetTexture(TextureManager& texMan, const GLint pos, const oglTexBase& tex, const bool shouldPin = false);
    void SetTexture(TextureManager& texMan, const map<GLuint, oglTexBase>& texs, const bool shouldPin = false);
    void SetUBO(UBOManager& uboMan, const GLint pos, const oglUBO& ubo, const bool shouldPin = false);
    void SetUBO(UBOManager& uboMan, const map<GLuint, oglUBO>& ubos, const bool shouldPin = false);
    void SetSubroutine();
    void SetSubroutine(const SubroutineResource* subr, const SubroutineResource::Routine* routine);

    void SetUniform(const GLint pos, const b3d::Coord2D& vec, const bool keep = true);
    void SetUniform(const GLint pos, const miniBLAS::Vec3& vec, const bool keep = true);
    void SetUniform(const GLint pos, const miniBLAS::Vec4& vec, const bool keep = true);
    void SetUniform(const GLint pos, const miniBLAS::Mat4x4& mat, const bool keep = true);
    void SetUniform(const GLint pos, const miniBLAS::Mat3x3& mat, const bool keep = true);
    void SetUniform(const GLint pos, const bool val, const bool keep = true);
    void SetUniform(const GLint pos, const int32_t val, const bool keep = true);
    void SetUniform(const GLint pos, const uint32_t val, const bool keep = true);
    void SetUniform(const GLint pos, const float val, const bool keep = true);
public:
    u16string Name;
    _oglProgram(const u16string& name);
    virtual ~_oglProgram();

    const set<ProgramResource, std::less<>>& getResources() const { return ProgRess; }
    const set<ShaderExtProperty, std::less<>>& getResourceProperties() const { return ExtInfo.Properties; }
    const set<SubroutineResource, std::less<>>& getSubroutineResources() const { return SubroutineRess; }
    const set<oglShader>& getShaders() const { return shaders; }
    const map<GLint, UniformValue>& getCurUniforms() const { return UniValCache; }

    void AddShader(const oglShader& shader);
    const string& GetExtShaderSource() const { return ExtShaderSource; }
    void Link();
    GLint GetLoc(const string& name) const;
    const ProgramResource* GetResource(const string& name) const { return FindInSet(ProgRess, name); }
    const SubroutineResource* GetSubroutines(const string& name) const { return FindInSet(SubroutineRess, name); }
    const SubroutineResource::Routine* GetSubroutine(const string& sruname);
    ProgState State() noexcept;
    void SetVec(const ProgramResource* res, const float x, const float y) { SetVec(res, b3d::Coord2D(x, y)); }
    void SetVec(const ProgramResource* res, const float x, const float y, const float z) { SetVec(res, miniBLAS::Vec3(x, y, z)); }
    void SetVec(const ProgramResource* res, const float x, const float y, const float z, const float w) { SetVec(res, miniBLAS::Vec4(x, y, z, w)); }
    void SetVec(const ProgramResource* res, const b3d::Coord2D& vec) { SetUniform(GetLoc(res, GL_FLOAT_VEC2), vec); }
    void SetVec(const ProgramResource* res, const miniBLAS::Vec3& vec) { SetUniform(GetLoc(res, GL_FLOAT_VEC3), vec); }
    void SetVec(const ProgramResource* res, const miniBLAS::Vec4& vec) { SetUniform(GetLoc(res, GL_FLOAT_VEC4), vec); }
    void SetMat(const ProgramResource* res, const miniBLAS::Mat3x3& mat) { SetUniform(GetLoc(res, GL_FLOAT_MAT3), mat); }
    void SetMat(const ProgramResource* res, const miniBLAS::Mat4x4& mat) { SetUniform(GetLoc(res, GL_FLOAT_MAT4), mat); }
    void SetUniform(const ProgramResource* res, const bool val) { SetUniform(GetLoc(res, GL_BOOL), val); }
    void SetUniform(const ProgramResource* res, const int32_t val) { SetUniform(GetLoc(res, GL_INT), val); }
    void SetUniform(const ProgramResource* res, const uint32_t val) { SetUniform(GetLoc(res, GL_UNSIGNED_INT), val); }
    void SetUniform(const ProgramResource* res, const float val) { SetUniform(GetLoc(res, GL_FLOAT), val); }
    void SetVec(const string& name, const float x, const float y) { SetVec(name, b3d::Coord2D(x, y)); }
    void SetVec(const string& name, const float x, const float y, const float z) { SetVec(name, miniBLAS::Vec3(x, y, z)); }
    void SetVec(const string& name, const float x, const float y, const float z, const float w) { SetVec(name, miniBLAS::Vec4(x, y, z, w)); }
    void SetVec(const string& name, const b3d::Coord2D& vec) { SetUniform(GetLoc(name, GL_FLOAT_VEC2), vec); }
    void SetVec(const string& name, const miniBLAS::Vec3& vec) { SetUniform(GetLoc(name, GL_FLOAT_VEC3), vec); }
    void SetVec(const string& name, const miniBLAS::Vec4& vec) { SetUniform(GetLoc(name, GL_FLOAT_VEC4), vec); }
    void SetMat(const string& name, const miniBLAS::Mat3x3& mat) { SetUniform(GetLoc(name, GL_FLOAT_MAT3), mat); }
    void SetMat(const string& name, const miniBLAS::Mat4x4& mat) { SetUniform(GetLoc(name, GL_FLOAT_MAT4), mat); }
    void SetUniform(const string& name, const bool val) { SetUniform(GetLoc(name, GL_BOOL), val); }
    void SetUniform(const string& name, const int32_t val) { SetUniform(GetLoc(name, GL_INT), val); }
    void SetUniform(const string& name, const uint32_t val) { SetUniform(GetLoc(name, GL_UNSIGNED_INT), val); }
    void SetUniform(const string& name, const float val) { SetUniform(GetLoc(name, GL_FLOAT), val); }
};

class OGLUAPI ProgState : public NonCopyable
{
    friend class _oglProgram;
private:
    _oglProgram & Prog;
    ProgState(_oglProgram& prog) : Prog(prog) { }
    ProgState(ProgState&& other) = default;
public:
    ~ProgState();
    ProgState& SetTexture(const oglTexBase& tex, const string& name, const GLuint idx = 0);
    //no check on pos, carefully use
    ProgState& SetTexture(const oglTexBase& tex, const GLuint pos);
    ProgState& SetUBO(const oglUBO& ubo, const string& name, const GLuint idx = 0);
    //no check on pos, carefully use
    ProgState& SetUBO(const oglUBO& ubo, const GLuint pos);
    ProgState& SetSubroutine(const SubroutineResource::Routine* routine);
    ProgState& SetSubroutine(const string& subrName, const string& routineName);
};


class OGLUAPI _oglDrawProgram : public _oglProgram
{
    friend class ProgDraw;
private:
    virtual void OnPrepare() override;
    GLint
        Uni_projMat = GL_INVALID_INDEX,
        Uni_viewMat = GL_INVALID_INDEX,
        Uni_modelMat = GL_INVALID_INDEX,
        Uni_normalMat = GL_INVALID_INDEX,
        Uni_mvpMat = GL_INVALID_INDEX,
        Uni_camPos = GL_INVALID_INDEX;
public:
    using _oglProgram::_oglProgram;
    virtual ~_oglDrawProgram() override {}

    void AddExtShaders(const string& src);
    void RegisterLocation();
    void SetProject(const Mat4x4 &);
    void SetView(const Mat4x4 &);

    ProgDraw Draw(const Mat4x4& modelMat, const Mat3x3& normMat) noexcept;
    ProgDraw Draw(const Mat4x4& modelMat = Mat4x4::identity()) noexcept;
    template<typename Iterator>
    ProgDraw Draw(const Iterator& begin, const Iterator& end) noexcept;
};

class OGLUAPI ProgDraw
{
    friend class _oglDrawProgram;
private:
    _oglDrawProgram & Prog;
    TextureManager & TexMan;
    UBOManager& UboMan;
    map<GLuint, oglTexBase> TexCache;
    map<GLuint, oglUBO> UBOCache;
    map<const SubroutineResource*, const SubroutineResource::Routine*> SubroutineCache;
    map<GLuint, std::pair<GLint, bool>> UniBindBackup;
    map<GLint, UniformValue> UniValBackup;
    ProgDraw(_oglDrawProgram& prog_, const Mat4x4& modelMat, const Mat3x3& normMat) noexcept;
    template<typename T>
    GLint GetLoc(const T& res, const GLenum valtype)
    {
        const GLint loc = Prog.GetLoc(res, valtype);
        if (loc != (GLint)GL_INVALID_INDEX)
            if (const auto it = common::container::FindInMap(Prog.UniValCache, loc))
                UniValBackup.insert_or_assign(loc, *it);
        return loc;
    }
public:
    ProgDraw(ProgDraw&& other) = default;
    ~ProgDraw();
    ///<summary>restore current drawing state</summary>  
    ///<param name="quick">whether perform quick restore</param>
    ///<returns>self</returns>
    ProgDraw& Restore(const bool quick = false);
    std::weak_ptr<_oglDrawProgram> GetProg() const noexcept;
    ProgDraw& SetPosition(const Mat4x4& modelMat, const Mat3x3& normMat);
    ProgDraw& SetPosition(const Mat4x4& modelMat) { return SetPosition(modelMat, (Mat3x3)modelMat); }
    ///<summary>draw actual vao</summary>  
    ///<param name="size">elements count being drawed</param>
    ///<param name="offset">elements offset</param>
    ///<returns>self</returns>
    ProgDraw& Draw(const oglVAO& vao, const uint32_t size, const uint32_t offset = 0);
    ProgDraw& Draw(const oglVAO& vao);
    ProgDraw& SetTexture(const oglTexBase& tex, const string& name, const GLuint idx = 0);
    ProgDraw& SetTexture(const oglTexBase& tex, const GLuint pos);
    ProgDraw& SetUBO(const oglUBO& ubo, const string& name, const GLuint idx = 0);
    ProgDraw& SetUBO(const oglUBO& ubo, const GLuint pos);
    ProgDraw& SetSubroutine(const SubroutineResource::Routine* routine);
    ProgDraw& SetSubroutine(const string& subrName, const string& routineName);

    ProgDraw& SetVec(const ProgramResource* res, const float x, const float y) { SetVec(res, b3d::Coord2D(x, y)); return *this; }
    ProgDraw& SetVec(const ProgramResource* res, const float x, const float y, const float z) { SetVec(res, miniBLAS::Vec3(x, y, z)); return *this; }
    ProgDraw& SetVec(const ProgramResource* res, const float x, const float y, const float z, const float w) { SetVec(res, miniBLAS::Vec4(x, y, z, w)); return *this; }
    ProgDraw& SetVec(const ProgramResource* res, const b3d::Coord2D& vec) { Prog.SetUniform(GetLoc(res, GL_FLOAT_VEC2), vec); return *this; }
    ProgDraw& SetVec(const ProgramResource* res, const miniBLAS::Vec3& vec) { Prog.SetUniform(GetLoc(res, GL_FLOAT_VEC3), vec, false); return *this; }
    ProgDraw& SetVec(const ProgramResource* res, const miniBLAS::Vec4& vec) { Prog.SetUniform(GetLoc(res, GL_FLOAT_VEC4), vec, false); return *this; }
    ProgDraw& SetMat(const ProgramResource* res, const miniBLAS::Mat3x3& mat) { Prog.SetUniform(GetLoc(res, GL_FLOAT_MAT3), mat, false); return *this; }
    ProgDraw& SetMat(const ProgramResource* res, const miniBLAS::Mat4x4& mat) { Prog.SetUniform(GetLoc(res, GL_FLOAT_MAT4), mat, false); return *this; }
    ProgDraw& SetUniform(const ProgramResource* res, const bool val) { Prog.SetUniform(GetLoc(res, GL_BOOL), val, false); return *this; }
    ProgDraw& SetUniform(const ProgramResource* res, const int32_t val) { Prog.SetUniform(GetLoc(res, GL_INT), val, false); return *this; }
    ProgDraw& SetUniform(const ProgramResource* res, const uint32_t val) { Prog.SetUniform(GetLoc(res, GL_UNSIGNED_INT), val, false); return *this; }
    ProgDraw& SetUniform(const ProgramResource* res, const float val) { Prog.SetUniform(GetLoc(res, GL_FLOAT), val, false); return *this; }
    ProgDraw& SetVec(const string& name, const float x, const float y) { SetVec(name, b3d::Coord2D(x, y)); return *this; }
    ProgDraw& SetVec(const string& name, const float x, const float y, const float z) { SetVec(name, miniBLAS::Vec3(x, y, z)); return *this; }
    ProgDraw& SetVec(const string& name, const float x, const float y, const float z, const float w) { SetVec(name, miniBLAS::Vec4(x, y, z, w)); return *this; }
    ProgDraw& SetVec(const string& name, const b3d::Coord2D& vec) { Prog.SetUniform(GetLoc(name, GL_FLOAT_VEC2), vec); return *this; }
    ProgDraw& SetVec(const string& name, const miniBLAS::Vec3& vec) { Prog.SetUniform(GetLoc(name, GL_FLOAT_VEC3), vec, false); return *this; }
    ProgDraw& SetVec(const string& name, const miniBLAS::Vec4& vec) { Prog.SetUniform(GetLoc(name, GL_FLOAT_VEC4), vec, false); return *this; }
    ProgDraw& SetMat(const string& name, const miniBLAS::Mat3x3& mat) { Prog.SetUniform(GetLoc(name, GL_FLOAT_MAT3), mat, false); return *this; }
    ProgDraw& SetMat(const string& name, const miniBLAS::Mat4x4& mat) { Prog.SetUniform(GetLoc(name, GL_FLOAT_MAT4), mat, false); return *this; }
    ProgDraw& SetUniform(const string& name, const bool val) { Prog.SetUniform(GetLoc(name, GL_BOOL), val, false); return *this; }
    ProgDraw& SetUniform(const string& name, const int32_t val) { Prog.SetUniform(GetLoc(name, GL_INT), val, false); return *this; }
    ProgDraw& SetUniform(const string& name, const uint32_t val) { Prog.SetUniform(GetLoc(name, GL_UNSIGNED_INT), val, false); return *this; }
    ProgDraw& SetUniform(const string& name, const float val) { Prog.SetUniform(GetLoc(name, GL_FLOAT), val, false); return *this; }
};

template<typename Iterator>
ProgDraw _oglDrawProgram::Draw(const Iterator& begin, const Iterator& end) noexcept
{
    static_assert(std::is_same_v<TransformOP, std::iterator_traits<Iterator>::value_type>, "Element insinde the range should be TransformOP.");
    Mat4x4 matModel = Mat4x4::identity();
    Mat3x3 matNormal = Mat3x3::identity();
    for (auto cur = begin; cur != end; ++cur)
    {
        const TransformOP& trans = *cur;
        oglUtil::applyTransform(matModel, matNormal, trans);
    }
    return Draw(matModel, matNormal);
}


class OGLUAPI _oglComputeProgram : public _oglProgram
{
private:
    virtual void OnPrepare() override {}
    using _oglProgram::AddShader;
public:
    _oglComputeProgram(const u16string name, const oglShader& shader);
    _oglComputeProgram(const u16string name, const string& src);
    virtual ~_oglComputeProgram() override {}

    void Run(const uint32_t groupX, const uint32_t groupY = 1, const uint32_t groupZ = 1);
};


}


using oglProgram = Wrapper<detail::_oglProgram>;
using oglDrawProgram = Wrapper<detail::_oglDrawProgram>;
using oglComputeProgram = Wrapper<detail::_oglComputeProgram>;

}