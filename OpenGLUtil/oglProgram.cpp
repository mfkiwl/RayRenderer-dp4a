#include "oglRely.h"
#include "oglException.h"
#include "oglProgram.h"
#include "oglContext.h"
#include "oglUtil.h"
#include "BindingManager.h"
#include "oglStringify.hpp"


namespace oglu
{
using common::container::FindInSet;
using common::container::FindInMap;
using common::container::FindInVec;
using common::container::ReplaceInVec;


static detail::ContextResource<GLuint> CTX_PROG_MAP;



GLint ProgramResource::getValue(const GLuint pid, const GLenum prop)
{
    GLint ret;
    glGetProgramResourceiv(pid, type, ifidx, 1, &prop, 1, NULL, &ret);
    return ret;
}


const char* ProgramResource::GetTypeName() const noexcept
{
    switch (type)
    {
    case GL_UNIFORM_BLOCK:
        return "UniBlk";
    case GL_UNIFORM:
        return isTexture() ? "TexUni" : "Uniform";
    case GL_PROGRAM_INPUT:
        return "Attrib";
    default:
        return nullptr;
    }
}


const char* ProgramResource::GetValTypeName() const noexcept
{
    switch (valtype)
    {
    case GL_UNIFORM_BLOCK:
        return "uniBlock";
    default:
        return FindInMap(detail::GLENUM_STR, valtype, std::in_place).value_or(nullptr);
    }
}

void ProgramResource::initData(const GLuint pid, const GLint idx)
{
    ifidx = (uint8_t)idx;
    if (type == GL_UNIFORM_BLOCK)
    {
        valtype = GL_UNIFORM_BLOCK;
        size = getValue(pid, GL_BUFFER_DATA_SIZE);
    }
    else
    {
        valtype = (GLenum)getValue(pid, GL_TYPE);
        len = getValue(pid, GL_ARRAY_SIZE);
    }
}

bool ProgramResource::isTexture() const noexcept
{
    if (type != GL_UNIFORM)
        return false;
    if (valtype >= GL_SAMPLER_1D && valtype <= GL_SAMPLER_2D_RECT_SHADOW)
        return true;
    else if (valtype >= GL_SAMPLER_1D_ARRAY && valtype <= GL_SAMPLER_CUBE_SHADOW)
        return true;
    else if (valtype >= GL_INT_SAMPLER_1D && valtype <= GL_UNSIGNED_INT_SAMPLER_BUFFER)
        return true;
    else if (valtype >= GL_SAMPLER_2D_MULTISAMPLE && valtype <= GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
        return true;
    else
        return false;
}


namespace detail
{

void ProgState::init()
{
    for (const auto& sru : prog.SubroutineRess)
    {
        srSettings[&sru] = &sru.Routines[0];
        srCache[sru.Stage].push_back(sru.Routines[0].Id);
    }
}

ProgState::ProgState(_oglProgram& prog_) :prog(prog_)
{
}

void ProgState::setTexture(TextureManager& texMan, const GLint pos, const oglTexture& tex, const bool shouldPin)
{
    auto& obj = uniBindCache[pos];
    const GLsizei val = tex ? texMan.bind(tex, shouldPin) : 0;
    if (obj == val)//no change
        return;
    //change value and update uniform-hold map
    glProgramUniform1i(prog.programID, pos, obj = val);
}

void ProgState::setTexture(TextureManager& texMan, const map<GLuint, oglTexture>& texs, const bool shouldPin)
{
    switch (texs.size())
    {
    case 0:
        return;
    case 1:
        setTexture(texMan, texs.begin()->first, texs.begin()->second);
        break;
    default:
        texMan.bindAll(prog.programID, texs, uniBindCache, shouldPin);
        break;
    }
}

void ProgState::setUBO(UBOManager& uboMan, const GLint pos, const oglUBO& ubo, const bool shouldPin)
{
    auto& obj = uniBindCache[pos];
    const auto val = ubo ? uboMan.bind(ubo, shouldPin) : 0;
    if (obj == val)//no change
        return;
    //change value and update uniform-hold map
    glUniformBlockBinding(prog.programID, pos, obj = val);
}

void ProgState::setUBO(UBOManager& uboMan, const map<GLuint, oglUBO>& ubos, const bool shouldPin)
{
    switch (ubos.size())
    {
    case 0:
        return;
    case 1:
        setUBO(uboMan, ubos.begin()->first, ubos.begin()->second);
        break;
    default:
        uboMan.bindAll(prog.programID, ubos, uniBindCache, shouldPin);
        break;
    }
}

void ProgState::setSubroutine()
{
    for (const auto& [stage, subrs] : srCache)
    {
        GLsizei cnt = (GLsizei)subrs.size();
        if(cnt > 0)
            glUniformSubroutinesuiv((GLenum)stage, cnt, subrs.data());
    }
}


void ProgState::end()
{
    if (_oglProgram::usethis(prog, false)) //self used, then changed to keep pinned status
    {
        auto& texMan = _oglTexture::getTexMan();
        texMan.unpin();
        auto& uboMan = _oglUniformBuffer::getUBOMan();
        uboMan.unpin();
        setTexture(texMan, texCache, true);
        setUBO(uboMan, uboCache, true);
        setSubroutine();
    }
}


ProgState& ProgState::setTexture(const oglTexture& tex, const string& name, const GLuint idx)
{
    const auto it = prog.TexRess.find(name);
    if (it != prog.TexRess.end() && idx < it->len)//legal
    {
        const auto pos = it->location + idx;
        texCache.insert_or_assign(pos, tex);
    }
    return *this;
}

ProgState& ProgState::setTexture(const oglTexture& tex, const GLuint pos)
{
    if (pos < uniBindCache.size())
    {
        texCache.insert_or_assign(pos, tex);
    }
    return *this;
}

ProgState& ProgState::setUBO(const oglUBO& ubo, const string& name, const GLuint idx)
{
    const auto it = prog.UBORess.find(name);
    if (it != prog.UBORess.end() && idx < it->len)//legal
    {
        const auto pos = it->location + idx;
        uboCache.insert_or_assign(pos, ubo);
    }
    return *this;
}

ProgState& ProgState::setUBO(const oglUBO& ubo, const GLuint pos)
{
    if (pos < uniBindCache.size())
    {
        uboCache.insert_or_assign(pos, ubo);
    }
    return *this;
}

ProgState& ProgState::setSubroutine(const SubroutineResource::Routine& sr)
{
    if (auto ppSru = FindInMap(prog.subrLookup, &sr))
    {
        auto& theSr = srSettings[*ppSru];
        auto& srvec = srCache[(**ppSru).Stage];
        std::replace(srvec.begin(), srvec.end(), theSr->Id, sr.Id); //ensured has value
        theSr = &sr;
    }
    else
        oglLog().warning(u"cannot find subroutine {}\n", sr.Name);
    return *this;
}

ProgState& ProgState::setSubroutine(const string& sruname, const string& srname)
{
    if (auto pSru = FindInSet(prog.SubroutineRess, sruname))
    {
        if (auto pSr = FindInVec(pSru->Routines, [&srname](const SubroutineResource::Routine& srr) { return srr.Name == srname; }))
        {
            auto& theSr = srSettings[pSru];
            auto& srvec = srCache[pSru->Stage];
            std::replace(srvec.begin(), srvec.end(), theSr->Id, pSr->Id); //ensured has value
            theSr = pSr;
        }
        else
            oglLog().warning(u"cannot find subroutine {} for object {}\n", srname, sruname);
    }
    else
        oglLog().warning(u"cannot find subroutine object {}\n", sruname);
    return *this;
}

ProgState& ProgState::getSubroutine(const string& sruname, string& srname)
{
    if (const SubroutineResource* pSru = FindInSet(prog.SubroutineRess, sruname))
        srname = srSettings[pSru]->Name;
    else
        oglLog().warning(u"cannot find subroutine object {}\n", sruname);
    return *this;
}


ProgDraw::ProgDraw(ProgState& pstate, const Mat4x4& modelMat, const Mat3x3& normMat) noexcept
    : ProgState(pstate.prog), gState(pstate), TexMan(_oglTexture::getTexMan()), UboMan(_oglUniformBuffer::getUBOMan())
{
    _oglProgram::usethis(prog);
    SetPosition(modelMat, normMat);
}
ProgDraw::~ProgDraw()
{
    Restore();
}

ProgDraw& ProgDraw::Restore()
{
    for (const auto& [pos, binding] : UniBindBackup)
    {
        auto& obj = gState.uniBindCache[pos];
        const auto val = binding.first;
        if (obj != val)
        {
            if (binding.second) //tex
                glProgramUniform1i(prog.programID, pos, obj = val);
            else //ubo
                glUniformBlockBinding(prog.programID, pos, obj = val);
        }
    }
    UniBindBackup.clear();
    for (const auto& [pos, val] : UniValBackup)
    {
        std::visit([&](auto&& arg) { prog.SetUniform(pos, arg, false); }, val);
    }
    UniValBackup.clear();
    return *this;
}
std::weak_ptr<_oglProgram> ProgDraw::GetProg() const noexcept
{
    return prog.weak_from_this();
}


ProgDraw& ProgDraw::SetPosition(const Mat4x4& modelMat, const Mat3x3& normMat)
{
    if (prog.Uni_mvpMat != GL_INVALID_INDEX)
    {
        const auto mvpMat = prog.matrix_Proj * prog.matrix_View * modelMat;
        prog.SetUniform(prog.Uni_mvpMat, mvpMat);
    }
    prog.SetUniform(prog.Uni_modelMat, modelMat, false);
    prog.SetUniform(prog.Uni_normalMat, normMat, false);
    return *this;
}
ProgDraw& ProgDraw::draw(const oglVAO& vao, const uint32_t size, const uint32_t offset)
{
    gState.setTexture(TexMan, texCache);
    gState.setUBO(UboMan, uboCache);
    ProgState::setSubroutine();
    vao->draw(size, offset);
    texCache.clear();
    uboCache.clear();
    srCache.clear();
    return *this;
}

ProgDraw& ProgDraw::draw(const oglVAO& vao)
{
    gState.setTexture(TexMan, texCache);
    gState.setUBO(UboMan, uboCache);
    ProgState::setSubroutine();
    vao->draw();
    texCache.clear();
    uboCache.clear();
    srCache.clear();
    return *this;
}

oglu::detail::ProgDraw& ProgDraw::setTexture(const oglTexture& tex, const string& name, const GLuint idx)
{
    const auto it = prog.TexRess.find(name);
    if (it != prog.TexRess.cend() && idx < it->len)
    {
        const auto pos = it->location + idx;
        texCache.insert_or_assign(pos, tex);
        const auto oldVal = gState.uniBindCache[pos];
        if(oldVal != GL_INVALID_INDEX)
            UniBindBackup.try_emplace(pos, std::make_pair(oldVal, true));
    }
    return *this;
}

oglu::detail::ProgDraw& ProgDraw::setTexture(const oglTexture& tex, const GLuint pos)
{
    if (pos < gState.uniBindCache.size())
    {
        texCache.insert_or_assign(pos, tex);
        const auto oldVal = gState.uniBindCache[pos];
        if (oldVal != GL_INVALID_INDEX)
            UniBindBackup.try_emplace(pos, std::make_pair(oldVal, true));
    }
    return *this;
}

oglu::detail::ProgDraw& ProgDraw::setUBO(const oglUBO& ubo, const string& name, const GLuint idx)
{
    const auto it = prog.UBORess.find(name);
    if (it != prog.UBORess.cend() && idx < it->len)//legal
    {
        const auto pos = it->location + idx;
        uboCache.insert_or_assign(pos, ubo);
        const auto oldVal = gState.uniBindCache[pos];
        if (oldVal != GL_INVALID_INDEX)
            UniBindBackup.try_emplace(pos, std::make_pair(oldVal, false));
    }
    return *this;
}

oglu::detail::ProgDraw& ProgDraw::setUBO(const oglUBO& ubo, const GLuint pos)
{
    if (pos < gState.uniBindCache.size())
    {
        uboCache.insert_or_assign(pos, ubo);
        const auto oldVal = gState.uniBindCache[pos];
        if (oldVal != GL_INVALID_INDEX)
            UniBindBackup.try_emplace(pos, oldVal, false);
    }
    return *this;
}


_oglProgram::_oglProgram(const u16string& name) :gState(*this), Name(name)
{
    programID = glCreateProgram();
}

_oglProgram::~_oglProgram()
{
    if (programID != 0 && usethis(*this, false)) //need unuse
    {
        CTX_PROG_MAP.InsertOrAssign([&](auto dummy)
        {
            glUseProgram(0);
            glDeleteProgram(programID);
            return 0;
        });
        programID = 0;
    }
}

bool _oglProgram::usethis(_oglProgram& prog, const bool change)
{
    if (CTX_PROG_MAP.TryGet() == prog.programID)
        return true;
    if (!change)//only return status
        return false;
    CTX_PROG_MAP.InsertOrAssign([&](auto dummy) 
    {
        glUseProgram(prog.programID);
        return prog.programID;
    });

    prog.RecoverState();
    return true;
}

void _oglProgram::RecoverState()
{
    gState.setSubroutine();
    auto& texMan = _oglTexture::getTexMan();
    texMan.unpin();
    auto& uboMan = _oglUniformBuffer::getUBOMan();
    uboMan.unpin();
    gState.setTexture(texMan, gState.texCache, true);
    gState.setUBO(uboMan, gState.uboCache, true);
}


void _oglProgram::InitLocs()
{
    set<ProgramResource, std::less<>> dataMap;
    string nameBuf;
    GLenum datatypes[] = { GL_UNIFORM_BLOCK,GL_UNIFORM,GL_PROGRAM_INPUT };
    for (const GLenum dtype : datatypes)
    {
        GLint cnt = 0;
        glGetProgramInterfaceiv(programID, dtype, GL_ACTIVE_RESOURCES, &cnt);
        GLint maxNameLen = 0;
        glGetProgramInterfaceiv(programID, dtype, GL_MAX_NAME_LENGTH, &maxNameLen);
        for (GLint a = 0; a < cnt; ++a)
        {
            string_view resName;
            {
                nameBuf.resize(maxNameLen);
                GLsizei nameLen = 0;
                glGetProgramResourceName(programID, dtype, a, maxNameLen, &nameLen, nameBuf.data());
                resName = string_view(nameBuf.c_str(), nameLen);
            }
            common::str::CutStringViewSuffix(resName, '.');//remove struct
            uint32_t arraylen = 0;
            if (auto cutpos = common::str::CutStringViewSuffix(resName, '['); cutpos != resName.npos)//array
            {
                arraylen = atoi(nameBuf.c_str() + cutpos + 1) + 1;
            }
            nameBuf.resize(resName.size());
            if (auto it = FindInSet(dataMap, nameBuf))
            {
                it->len = common::max(it->len, arraylen);
                continue;
            }
            ProgramResource datinfo(dtype, nameBuf);
            datinfo.initData(programID, a);
            if (dtype == GL_UNIFORM_BLOCK)
                datinfo.location = glGetProgramResourceIndex(programID, dtype, nameBuf.c_str());
            else
                datinfo.location = glGetProgramResourceLocation(programID, dtype, nameBuf.c_str());

            if (datinfo.location != GL_INVALID_INDEX)
            {
                dataMap.insert(datinfo); //must not exist
            }
        }
    }
    oglLog().debug(u"Active {} locations\n", dataMap.size());
    GLuint maxUniLoc = 0;
    for (const auto& info : dataMap)
    {
        if (info.isAttrib())
            AttrRess.insert(info);
        else
        {
            maxUniLoc = common::max(maxUniLoc, info.location + info.len);
            if (info.isUniformBlock())
                UBORess.insert(info);
            else if (info.isTexture())
                TexRess.insert(info);
        }
        ProgRess.insert(info);
        oglLog().debug(u"--{:>7}{:<3} -[{:^5}]- {}[{}]({}) size[{}]\n", info.GetTypeName(), info.ifidx, info.location, info.Name, info.len, info.GetValTypeName(), info.size);
    }
    gState.uniBindCache.clear();
    gState.uniBindCache.resize(maxUniLoc, GL_INVALID_INDEX);
}

void _oglProgram::InitSubroutines()
{
    const GLenum stages[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
    char strbuf[4096];
    auto& writer = common::mlog::detail::StrFormater<char16_t>::GetWriter();
    writer.clear();
    writer.write(u"SubRoutine Resource: \n");
    SubroutineRess.clear();
    subrLookup.clear();
    for (auto stage : stages)
    {
        GLint count;
        glGetProgramStageiv(programID, stage, GL_ACTIVE_SUBROUTINE_UNIFORMS, &count);
        for (int a = 0; a < count; ++a)
        {
            glGetActiveSubroutineUniformName(programID, stage, a, sizeof(strbuf), nullptr, strbuf);
            string uniformName(strbuf);
            auto uniformLoc = glGetSubroutineUniformLocation(programID, stage, strbuf);
            writer.write(u"SubRoutine {} at [{}]:\n", uniformName, uniformLoc);
            GLint srcnt;
            glGetActiveSubroutineUniformiv(programID, stage, a, GL_NUM_COMPATIBLE_SUBROUTINES, &srcnt);
            vector<GLint> compSRs(srcnt, GL_INVALID_INDEX);
            vector<SubroutineResource::Routine> routines;
            glGetActiveSubroutineUniformiv(programID, stage, a, GL_COMPATIBLE_SUBROUTINES, compSRs.data());
            for(auto idx : compSRs)
            {
                glGetActiveSubroutineName(programID, stage, idx, sizeof(strbuf), nullptr, strbuf);
                string subrname(strbuf);
                writer.write(u"--[{}]: {}\n", idx, subrname);
                routines.push_back(SubroutineResource::Routine(subrname, idx));
            }
            auto[it, isAdd] = SubroutineRess.emplace(stage, uniformLoc, uniformName, std::move(routines));
            for (auto& routine : it->Routines)
                subrLookup[&routine] = &(*it);
        }
    }
    oglLog().debug(writer.c_str());
    gState.init();
}

void _oglProgram::FilterProperties()
{
    set<ShaderExtProperty, std::less<>> newProperties;
    for (const auto& prop : ShaderProperties)
    {
        if (auto res = FindInSet(ProgRess, prop.Name))
            if (prop.MatchType(res->valtype))
            {
                newProperties.insert(prop);
                switch (prop.Type)
                {
                case ShaderPropertyType::Color:
                    {
                        Vec4 vec;
                        glGetUniformfv(programID, res->location, vec);
                        UniValCache.insert_or_assign(res->location, vec);
                    } break;
                case ShaderPropertyType::Range:
                    {
                        b3d::Coord2D vec;
                        glGetUniformfv(programID, res->location, vec);
                        UniValCache.insert_or_assign(res->location, vec);
                    } break;
                case ShaderPropertyType::Bool:
                    {
                        int32_t flag = 0;
                        glGetUniformiv(programID, res->location, &flag);
                        UniValCache.insert_or_assign(res->location, (bool)flag);
                    } break;
                case ShaderPropertyType::Int:
                    {
                        int32_t val = 0;
                        glGetUniformiv(programID, res->location, &val);
                        UniValCache.insert_or_assign(res->location, val);
                    } break;
                case ShaderPropertyType::Uint:
                    {
                        uint32_t val = 0;
                        glGetUniformuiv(programID, res->location, &val);
                        UniValCache.insert_or_assign(res->location, val);
                    } break;
                case ShaderPropertyType::Float:
                    {
                        float val = 0;
                        glGetUniformfv(programID, res->location, &val);
                        UniValCache.insert_or_assign(res->location, val);
                    } break;
                default:
                    oglLog().verbose(u"ExtProp [{}] of type[{}] and valtype[{}] is not supported to load initial value.\n", prop.Name, (uint8_t)prop.Type, res->GetValTypeName());
                }
            }
            else
                oglLog().warning(u"ExtProp [{}] mismatch type[{}] with valtype[{}]\n", prop.Name, (uint8_t)prop.Type, res->GetValTypeName());
        else
            oglLog().warning(u"ExtProp [{}] cannot find active uniform\n", prop.Name);
    }
    ShaderProperties.swap(newProperties);
}


void _oglProgram::addShader(const oglShader& shader)
{
    auto [it, isAdd] = shaders.insert(shader);
    if (isAdd)
        glAttachShader(programID, shader->shaderID);
    else
        oglLog().warning(u"Repeat adding shader {} to program [{}]\n", shader->shaderID, Name);
}

void _oglProgram::AddExtShaders(const string& src)
{
    ShaderExtInfo info;
    auto shaders = oglShader::loadFromExSrc(src, info);
    for (auto shader : shaders)
    {
        shader->compile();
        addShader(shader);
    }
    ShaderProperties = std::move(info.Properties);
    for (const auto& prop : ShaderProperties)
        oglLog().debug(u"prop[{}], typeof [{}], data[{}]\n", prop.Name, (uint8_t)prop.Type, prop.Data.has_value() ? "Has" : "None");
    ResBindMapping.clear();
    for (const auto&[target, name] : info.ResMappings)
        ResBindMapping.insert_or_assign((ProgramMappingTarget)hash_(target), name);
}


void _oglProgram::link()
{
    glLinkProgram(programID);

    int result;
    glGetProgramiv(programID, GL_LINK_STATUS, &result);
    int len;
    glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &len);
    string logstr((size_t)len, '\0');
    glGetProgramInfoLog(programID, len, &len, logstr.data());

    if (!result)
    {
        oglLog().warning(u"Link program failed.\n{}\n", logstr);
        glDeleteProgram(programID);
        COMMON_THROW(OGLException, OGLException::GLComponent::Compiler, L"Link program failed", logstr);
    }
    oglLog().success(u"Link program success.\n{}\n", logstr);
    InitLocs();
    InitSubroutines();
    FilterProperties();

    static const map<ProgramMappingTarget, string> DefaultMapping =
    {
        { ProgramMappingTarget::ProjectMat,  "oglu_matProj" },
        { ProgramMappingTarget::ViewMat,     "oglu_matView" },
        { ProgramMappingTarget::ModelMat,    "oglu_matModel" },
        { ProgramMappingTarget::MVPNormMat,  "oglu_matNormal" },
        { ProgramMappingTarget::MVPMat,      "oglu_matMVP" },
        { ProgramMappingTarget::CamPosVec,   "oglu_camPos" },
        { ProgramMappingTarget::VertPos,     "oglu_vertPos" },
        { ProgramMappingTarget::VertNorm,    "oglu_vertNorm" },
        { ProgramMappingTarget::VertTexc,    "oglu_texPos" },
        { ProgramMappingTarget::VertColor,   "oglu_vertColor" },
        { ProgramMappingTarget::VertTan,     "oglu_vertTan" },
    };
    ResBindMapping.insert(DefaultMapping.cbegin(), DefaultMapping.cend());
    RegisterLocation(ResBindMapping);
}


void _oglProgram::RegisterLocation(const map<ProgramMappingTarget, string>& bindMapping)
{
    for (const auto&[target, name] : bindMapping)
    {
        switch (target)
        {
        case ProgramMappingTarget::ProjectMat:  Uni_projMat = GetLoc(name, GL_FLOAT_MAT4); break; //projectMatrix
        case ProgramMappingTarget::ViewMat:     Uni_viewMat = GetLoc(name, GL_FLOAT_MAT4); break; //viewMatrix
        case ProgramMappingTarget::ModelMat:    Uni_modelMat = GetLoc(name, GL_FLOAT_MAT4); break; //modelMatrix
        case ProgramMappingTarget::MVPMat:      Uni_mvpMat = GetLoc(name, GL_FLOAT_MAT4); break; //model-view-project-Matrix
        case ProgramMappingTarget::MVPNormMat:  Uni_normalMat = GetLoc(name, GL_FLOAT_MAT4); break; //model-view-project-Matrix
        case ProgramMappingTarget::CamPosVec:   Uni_camPos = GetLoc(name, GL_FLOAT_MAT4); break;
        case ProgramMappingTarget::VertPos:     Attr_Vert_Pos = GetLoc(name, GL_FLOAT_VEC3); break;
        case ProgramMappingTarget::VertNorm:    Attr_Vert_Norm = GetLoc(name, GL_FLOAT_VEC3); break;
        case ProgramMappingTarget::VertTexc:    Attr_Vert_Texc = GetLoc(name, GL_FLOAT_VEC2); break;
        case ProgramMappingTarget::VertColor:   Attr_Vert_Color = GetLoc(name, GL_FLOAT_VEC4); break;
        case ProgramMappingTarget::VertTan:     Attr_Vert_Tan = GetLoc(name, GL_FLOAT_VEC3); break;
        }
    }
}

const ProgramResource* _oglProgram::getResource(const string& name) const
{
    return FindInSet(ProgRess, name);
}

const SubroutineResource* _oglProgram::getSubroutines(const string& name) const
{
    return FindInSet(SubroutineRess, name);
}

GLint _oglProgram::GetLoc(const ProgramResource* res, GLenum valtype) const
{
    if (res && res->valtype == valtype)
        return res->location;
    return GL_INVALID_INDEX;
}
GLint _oglProgram::GetLoc(const string& name, GLenum valtype) const
{
    if (auto obj = FindInSet(ProgRess, name))
        if (obj->valtype == valtype)
            return obj->location;
    return GL_INVALID_INDEX;
}
GLint _oglProgram::getLoc(const string& name) const
{
    if (auto obj = FindInSet(ProgRess, name))
        return obj->location;
    return GL_INVALID_INDEX;
}

void _oglProgram::setProject(const Camera& cam, const int wdWidth, const int wdHeight)
{
    //glViewport((wdWidth & 0x3f) / 2, (wdHeight & 0x3f) / 2, cam.width & 0xffc0, cam.height & 0xffc0);
    //assert(cam.aspect > std::numeric_limits<float>::epsilon());
    if (cam.aspect <= std::numeric_limits<float>::epsilon())
        return;
    const float cotHalfFovy = 1 / tan(b3d::ang2rad(cam.fovy / 2));
    const float viewDepthR = 1 / (cam.zFar - cam.zNear);
    /*   cot(fovy/2)
    *  -------------		0		   0			0
    *     aspect
    *
    *       0         cot(fovy/2)	   0			0
    *
    *								  F+N         2*F*N
    *       0              0		 -----	   - -------
    *								  F-N          F-N
    *
    *       0              0           1			0
    *
    **/
    matrix_Proj = Mat4x4(Vec4(cotHalfFovy / cam.aspect, 0.f, 0.f, 0.f),
        Vec4(0.f, cotHalfFovy, 0.f, 0.f),
        Vec4(0.f, 0.f, (cam.zFar + cam.zNear) * viewDepthR, (-2 * cam.zFar * cam.zNear) * viewDepthR),
        Vec4(0.f, 0.f, 1.f, 0.f));

    SetUniform(Uni_projMat, matrix_Proj);
}

void _oglProgram::setCamera(const Camera & cam)
{
    //LookAt
    //matrix_View = glm::lookAt(vec3(cam.position), vec3(Vertex(cam.position + cam.n)), vec3(cam.v));
    const auto rMat = cam.camMat.inv();
    matrix_View = Mat4x4::TranslateMat(cam.position * -1, rMat);

    SetUniform(Uni_viewMat, matrix_View);
    if (Uni_camPos != GL_INVALID_INDEX)
        glProgramUniform3fv(programID, Uni_camPos, 1, cam.position);
}

ProgDraw _oglProgram::draw(const Mat4x4& modelMat, const Mat3x3& normMat) noexcept
{
    return ProgDraw(gState, modelMat, normMat);
}
ProgDraw _oglProgram::draw(const Mat4x4& modelMat) noexcept
{
    return draw(modelMat, (Mat3x3)modelMat);
}
ProgDraw _oglProgram::draw(topIT begin, topIT end) noexcept
{
    Mat4x4 matModel = Mat4x4::identity();
    Mat3x3 matNormal = Mat3x3::identity();
    for (topIT cur = begin; cur != end; ++cur)
    {
        const TransformOP& trans = *cur;
        oglUtil::applyTransform(matModel, matNormal, trans);
    }
    return draw(matModel, matNormal);
}

ProgState& _oglProgram::globalState() noexcept
{
    return gState;
}


void _oglProgram::SetUniform(const GLint pos, const b3d::Coord2D& vec, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, vec);
        glProgramUniform2fv(programID, pos, 1, vec);
    }
}
void _oglProgram::SetUniform(const GLint pos, const miniBLAS::Vec3& vec, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, vec);
        glProgramUniform3fv(programID, pos, 1, vec);
    }
}
void _oglProgram::SetUniform(const GLint pos, const miniBLAS::Vec4& vec, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, vec);
        glProgramUniform4fv(programID, pos, 1, vec);
    }
}
void _oglProgram::SetUniform(const GLint pos, const miniBLAS::Mat3x3& mat, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, mat);
        glProgramUniformMatrix4fv(programID, pos, 1, GL_FALSE, mat.inv());
    }
}
void _oglProgram::SetUniform(const GLint pos, const miniBLAS::Mat4x4& mat, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, mat);
        glProgramUniformMatrix4fv(programID, pos, 1, GL_FALSE, mat.inv());
    }
}
void _oglProgram::SetUniform(const GLint pos, const bool val, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, val);
        glProgramUniform1i(programID, pos, val);
    }
}
void _oglProgram::SetUniform(const GLint pos, const int32_t val, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, val);
        glProgramUniform1i(programID, pos, val);
    }
}
void _oglProgram::SetUniform(const GLint pos, const uint32_t val, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, val);
        glProgramUniform1ui(programID, pos, val);
    }
}
void _oglProgram::SetUniform(const GLint pos, const float val, const bool keep)
{
    if (pos != GL_INVALID_INDEX)
    {
        if (keep)
            UniValCache.insert_or_assign(pos, val);
        glProgramUniform1f(programID, pos, val);
    }
}


}

}