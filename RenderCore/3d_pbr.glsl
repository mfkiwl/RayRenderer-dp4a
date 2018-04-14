#version 430 core
precision mediump float;
precision lowp sampler2D;
//@@$$VERT|FRAG

const float oglu_PI = 3.1415926f;

struct LightData
{
    vec3 position, direction;
    lowp vec4 color, attenuation;
    float coang, exponent;
    int type;
};
layout(std140) uniform lightBlock
{
    LightData lights[16];
};
uniform uint lightCount = 0;

struct MaterialData
{
    vec4 basic;
    vec4 other;
    uvec4 mappos;
};
layout(std140) uniform materialBlock
{
    MaterialData materials[32];
};

//@@->ProjectMat|matProj
layout(location = 0) uniform mat4 matProj;
//@@->ViewMat|matView
layout(location = 1) uniform mat4 matView;
//@@->ModelMat|matModel
layout(location = 2) uniform mat4 matModel;
//@@->MVPMat|matMVP
layout(location = 3) uniform mat4 matMVP;
//@@->CamPosVec|vecCamPos
layout(location = 4) uniform vec3 vecCamPos;



GLVARY perVert
{
    vec3 pos;
    vec3 pt2cam;
    vec3 norm;
    vec4 tannorm;
    vec2 tpos;
    flat uint drawId;
};

////////////////
#ifdef OGLU_VERT

//@@->VertPos|vertPos
layout(location = 0) in vec3 vertPos;
//@@->VertNorm|vertNorm
layout(location = 1) in vec3 vertNorm;
//@@->VertTexc|vertTexc
layout(location = 2) in vec2 vertTexc;
//@@->VertTan|vertTan
layout(location = 3) in vec4 vertTan;
//@@->DrawID|ogluDrawId
layout(location = 4) in uint ogluDrawId;

void main() 
{
    gl_Position = matMVP * vec4(vertPos, 1.0f);
    pos = (matModel * vec4(vertPos, 1.0f)).xyz;
    pt2cam = vecCamPos - pos;
    const mat3 matModel3 = mat3(matModel);
    norm = matModel3 * vertNorm;
    tannorm = vec4(matModel3 * vertTan.xyz, vertTan.w);
    tpos = vertTexc;
    drawId = ogluDrawId;
}

#endif


////////////////
#ifdef OGLU_FRAG

uniform sampler2D tex[16];
uniform sampler2DArray texs[16];

//@@##srgbTexture|BOOL|whether input texture is srgb space
uniform bool srgbTexture = true;
//@@##gamma|FLOAT|gamma correction|0.4|3.2
uniform float gamma = 2.2f;
//@@##envAmbient|COLOR|environment ambient color
uniform lowp vec4 envAmbient;

out vec4 FragColor;

subroutine vec3 LightModel();
subroutine uniform LightModel lighter;

subroutine vec3 NormalCalc(const uint);
subroutine uniform NormalCalc getNorm;

subroutine vec3 AlbedoCalc(const uint);
subroutine uniform AlbedoCalc getAlbedo;

subroutine(NormalCalc)
vec3 vertedNormal(const uint id)
{
    return normalize(norm);
}
subroutine(NormalCalc)
vec3 mappedNormal(const uint id)
{
    const vec3 ptNorm = normalize(norm);
    const vec3 ptTan = normalize(tannorm.xyz);
    vec3 bitanNorm = cross(ptNorm, ptTan);
    if(tannorm.w < 0.0f) bitanNorm *= -1.0f;
    const mat3 TBN = mat3(ptTan, bitanNorm, ptNorm);

    const uint pos = materials[id].mappos.y;
    const uint bank = pos >> 16;
    const float layer = float(pos & 0xffff);//let it be wrong
    const vec3 newtpos = vec3(tpos, layer);
    const vec3 ptNormTex = texture(texs[bank], newtpos).rgb * 2.0f - 1.0f;
    //const vec3 ptNormTex = texture(tex[1], tpos).rgb * 2.0f - 1.0f;
    const vec3 ptNorm2 = TBN * ptNormTex;
    return ptNorm2;
}
subroutine(NormalCalc)
vec3 bothNormal(const uint id)
{
    return (materials[id].mappos.y == 0xffff) ? vertedNormal(id) : mappedNormal(id);
}

subroutine(AlbedoCalc)
vec3 materialAlbedo(const uint id)
{
    const vec3 srgbAlbedo = materials[id].basic.xyz;
    if (srgbTexture)
        return pow(srgbAlbedo, vec3(2.2f));
    else
        return srgbAlbedo;
}
subroutine(AlbedoCalc)
vec3 mappedAlbedo(const uint id)
{
    const uint pos = materials[id].mappos.x;
    const uint bank = pos >> 16;
    const float layer = float(pos & 0xffff);
    const vec3 newtpos = vec3(tpos, layer);
    const vec3 srgbAlbedo = texture(texs[bank], newtpos).rgb;
    if (srgbTexture)
        return pow(srgbAlbedo, vec3(2.2f));
    else
        return srgbAlbedo;
}
subroutine(AlbedoCalc)
vec3 bothAlbedo(const uint id)
{
    return (materials[id].mappos.x == 0xffff) ? materialAlbedo(id) : mappedAlbedo(id);
}


void parseLight(const uint id, out vec3 p2l, out vec3 color)
{
    float atten = lights[id].attenuation.w;
    if (lights[id].type == 0) // parallel light
    {
        p2l = -lights[id].direction;
    }
    else if (lights[id].type == 1) // point light
    {
        p2l = lights[id].position - pos;
        const float inv_dist = inversesqrt(dot(p2l, p2l));
        p2l *= inv_dist; // normalize
        atten *= inv_dist * inv_dist;
    }
    color = lights[id].color.rgb * atten;
}

float ClampDot(const vec3 v1, const vec3 v2)
{
    return max(dot(v1, v2), 0.0f);
}

vec3 GammaCorrect(const vec3 color)
{
    return pow(color, vec3(1.0f / gamma));
}

subroutine(LightModel)
vec3 tex0()
{
    const vec4 texColor = texture(texs[0], vec3(tpos, 0.0f));
    return texColor.rgb;
}
subroutine(LightModel)
vec3 tanvec()
{
    const vec3 ptNorm = normalize(tannorm.xyz);
    return (ptNorm + 1.0f) * 0.5f;
}
subroutine(LightModel)
vec3 normal()
{
    const vec3 ptNorm = getNorm(drawId);
    return (ptNorm + 1.0f) * 0.5f;
}
subroutine(LightModel)
vec3 normdiff()
{
    if(materials[drawId].mappos.y == 0xffff)
        return vec3(0.5f, 0.5f, 0.5f);
    const vec3 ptNorm = vertedNormal(drawId);
    const vec3 ptNorm2 = mappedNormal(drawId);
    const vec3 diff = ptNorm2 - ptNorm;
    return (diff + 1.0f) * 0.5f;
}
subroutine(LightModel)
vec3 dist()
{
    float depth = pow(gl_FragCoord.z, 5);
    return vec3(depth);
}
subroutine(LightModel)
vec3 view()
{
    const vec3 viewRay = normalize(pt2cam);
    return (viewRay + 1.0f) * 0.5f;
}
subroutine(LightModel)
vec3 lgt0()
{
    vec3 p2l, color;
    parseLight(0, p2l, color);
    return (p2l + 1.0f) * 0.5f;
}
subroutine(LightModel)
vec3 drawidx()
{
    const uint didX = drawId % 3 + 1, didY = (drawId / 3) % 3 + 1, didZ = drawId / 9 + 1;
    const float stride = 0.25f;
    return vec3(didX * stride, didY * stride, didZ * stride);
}
subroutine(LightModel)
vec3 mat0()
{
    const uint pos = materials[0].mappos.x;
    if(pos == 0xffff)
        return vec3(1.0f, 0.0f, 0.0f);
    const uint bank = pos >> 16;
    const float layer = float(pos & 0xffff);
    return vec3(0.0f, bank / 16.0f, layer / 16.0f);
}

//NDF(n,h,r) = a^2 / PI((n.h)^2 * (a^2-1) + 1)^2
//roughness4 = a^2 = roughness^4
//distribution of normal of the microfacet
float D_GGXTR(const vec3 ptNorm, const vec3 halfVec, const float roughness4)
{
    const float nh = ClampDot(ptNorm, halfVec);
    const float r2 = roughness4;
    const float tmp1 = (nh * r2 - nh) * nh + 1.0f;
    return r2 / (oglu_PI * tmp1 * tmp1);
}

//G = n.v / (n.v) * (1-k) + k 
float G_SchlickGGX(const float nv, const float k, const float one_k)
{
    return nv / (nv * one_k + k);
}

//G = G(n,v,k) * G(n,l,k)
//percentage of acceptable specular(not in shadow or mask)
float G_GGX(const float n_eye, const float n_lgt, const float k, const float one_k)
{
    //Geometry Obstruction
    const float geoObs = G_SchlickGGX(n_eye, k, one_k);
    //Geometry Shadowing
    const float geoShd = G_SchlickGGX(n_lgt, k, one_k);
    return geoObs * geoShd;
}

//fresnel reflection caused by the microfacet
vec3 F_Schlick(const vec3 halfVec, const vec3 viewRay, const vec3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - ClampDot(halfVec, viewRay), 5.0f);
}

void PBR(const lowp vec3 albedo, inout lowp vec3 diffuseColor, inout lowp vec3 specularColor)
{
    const vec3 viewRay = normalize(pt2cam);
    const vec3 ptNorm = getNorm(drawId);
    const float metallic = materials[drawId].basic.w;
    const vec3 diffuse_PI = (1.0f - metallic) * albedo / oglu_PI;
    const vec3 F0 = mix(vec3(0.04f), albedo, metallic);
    const float roughness = max(materials[drawId].other.x, 0.002f);
    const float roughness4 = roughness * roughness * roughness * roughness;
    const float r_1 = roughness + 1.0f;
    const float k = (r_1 * r_1) / 8.0f;
    const float one_k = 1.0f - k;
    for (int id = 0; id < lightCount; id++)
    {
        vec3 p2l, lgtColor;
        parseLight(id, p2l, lgtColor);

        const vec3 halfVec = normalize(p2l + viewRay);
        const float NDF = D_GGXTR(ptNorm, halfVec, roughness4);
        const float n_eye = ClampDot(ptNorm, viewRay), n_lgt = ClampDot(ptNorm, p2l);
        const float G = G_GGX(n_eye, n_lgt, k, one_k);
        const vec3 F = F_Schlick(halfVec, viewRay, F0);
       
        lgtColor *= n_lgt;
        //diffuse = (1-F)(1-metal)albedo/pi
        const vec3 diffuse = diffuse_PI - diffuse_PI * F;
        diffuseColor += diffuse * lgtColor;
        const vec3 specular = F * (NDF * G / (4.0f * n_eye * n_lgt + 0.001f));
        specularColor += specular * lgtColor;
    }
}

subroutine(LightModel)
vec3 albedoOnly()
{
    const vec3 albedo = getAlbedo(drawId);
    return GammaCorrect(albedo);
}
subroutine(LightModel)
vec3 metalOnly()
{
    const float metallic = materials[drawId].basic.w;
    return vec3(metallic);
}
subroutine(LightModel)
vec3 basic()
{
    const float AO = materials[drawId].other.z;
    const lowp vec3 ambientColor = envAmbient.rgb * AO;
    lowp vec3 diffuseColor = vec3(0.0f);
    lowp vec3 specularColor = vec3(0.0f);
    const lowp vec3 albedo = getAlbedo(drawId);
    PBR(albedo, diffuseColor, specularColor);
    lowp vec3 finalColor = ambientColor + diffuseColor + specularColor;
    return GammaCorrect(finalColor);
}
subroutine(LightModel)
vec3 diffuse()
{
    const float AO = materials[drawId].other.z;
    const lowp vec3 ambientColor = envAmbient.rgb * AO;
    lowp vec3 diffuseColor = vec3(0.0f);
    lowp vec3 specularColor = vec3(0.0f);
    const lowp vec3 albedo = getAlbedo(drawId);
    PBR(albedo, diffuseColor, specularColor);
    lowp vec3 finalColor = ambientColor + diffuseColor;
    return GammaCorrect(finalColor);
}

subroutine(LightModel)
vec3 specular()
{
    const float AO = materials[drawId].other.z;
    const lowp vec3 ambientColor = envAmbient.rgb * AO;
    lowp vec3 diffuseColor = vec3(0.0f);
    lowp vec3 specularColor = vec3(0.0f);
    const lowp vec3 albedo = getAlbedo(drawId);
    PBR(albedo, diffuseColor, specularColor);
    lowp vec3 finalColor = ambientColor + specularColor;
    return GammaCorrect(finalColor);
}

void main() 
{
    const lowp vec3 color = lighter();
    FragColor = vec4(color, 1.0f);
}

#endif