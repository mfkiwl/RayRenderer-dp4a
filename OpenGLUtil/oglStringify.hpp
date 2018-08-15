#pragma once

#include "oglRely.h"

namespace oglu::detail
{ 

using namespace std::literals;

static const std::map<GLenum, string_view> GLENUM_STR =
{ 
    { GL_FLOAT, "float"sv },
    { GL_FLOAT_VEC2, "vec2"sv },
    { GL_FLOAT_VEC3, "vec3"sv },
    { GL_FLOAT_VEC4, "vec4"sv },
    { GL_DOUBLE, "double"sv },
    { GL_DOUBLE_VEC2, "dvec2"sv },
    { GL_DOUBLE_VEC3, "dvec3"sv },
    { GL_DOUBLE_VEC4, "dvec4"sv },
    { GL_INT, "int"sv },
    { GL_INT_VEC2, "ivec2"sv },
    { GL_INT_VEC3, "ivec3"sv },
    { GL_INT_VEC4, "ivec4"sv },
    { GL_UNSIGNED_INT, "uint"sv },
    { GL_UNSIGNED_INT_VEC2, "uvec2"sv },
    { GL_UNSIGNED_INT_VEC3, "uvec3"sv },
    { GL_UNSIGNED_INT_VEC4, "uvec4"sv },
    { GL_BOOL, "bool"sv },
    { GL_BOOL_VEC2, "bvec2"sv },
    { GL_BOOL_VEC3, "bvec3"sv },
    { GL_BOOL_VEC4, "bvec4"sv },
    { GL_FLOAT_MAT2, "mat2"sv },
    { GL_FLOAT_MAT3, "mat3"sv },
    { GL_FLOAT_MAT4, "mat4"sv },
    { GL_FLOAT_MAT2x3, "mat2x3"sv },
    { GL_FLOAT_MAT2x4, "mat2x4"sv },
    { GL_FLOAT_MAT3x2, "mat3x2"sv },
    { GL_FLOAT_MAT3x4, "mat3x4"sv },
    { GL_FLOAT_MAT4x2, "mat4x2"sv },
    { GL_FLOAT_MAT4x3, "mat4x3"sv },
    { GL_DOUBLE_MAT2, "dmat2"sv },
    { GL_DOUBLE_MAT3, "dmat3"sv },
    { GL_DOUBLE_MAT4, "dmat4"sv },
    { GL_DOUBLE_MAT2x3, "dmat2x3"sv },
    { GL_DOUBLE_MAT2x4, "dmat2x4"sv },
    { GL_DOUBLE_MAT3x2, "dmat3x2"sv },
    { GL_DOUBLE_MAT3x4, "dmat3x4"sv },
    { GL_DOUBLE_MAT4x2, "dmat4x2"sv },
    { GL_DOUBLE_MAT4x3, "dmat4x3"sv },
    { GL_SAMPLER_1D, "sampler1D"sv },
    { GL_SAMPLER_2D, "sampler2D"sv },
    { GL_SAMPLER_3D, "sampler3D"sv },
    { GL_SAMPLER_CUBE, "samplerCube"sv },
    { GL_SAMPLER_1D_SHADOW, "sampler1DShadow"sv },
    { GL_SAMPLER_2D_SHADOW, "sampler2DShadow"sv },
    { GL_SAMPLER_1D_ARRAY, "sampler1DArray"sv },
    { GL_SAMPLER_2D_ARRAY, "sampler2DArray"sv },
    { GL_SAMPLER_1D_ARRAY_SHADOW, "sampler1DArrayShadow"sv },
    { GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow"sv },
    { GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS"sv },
    { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray"sv },
    { GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow"sv },
    { GL_SAMPLER_BUFFER, "samplerBuffer"sv },
    { GL_SAMPLER_2D_RECT, "sampler2DRect"sv },
    { GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow"sv },
    { GL_INT_SAMPLER_1D, "isampler1D"sv },
    { GL_INT_SAMPLER_2D, "isampler2D"sv },
    { GL_INT_SAMPLER_3D, "isampler3D"sv },
    { GL_INT_SAMPLER_CUBE, "isamplerCube"sv },
    { GL_INT_SAMPLER_1D_ARRAY, "isampler1DArray"sv },
    { GL_INT_SAMPLER_2D_ARRAY, "isampler2DArray"sv },
    { GL_INT_SAMPLER_2D_MULTISAMPLE, "isampler2DMS"sv },
    { GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, "isampler2DMSArray"sv },
    { GL_INT_SAMPLER_BUFFER, "isamplerBuffer"sv },
    { GL_INT_SAMPLER_2D_RECT, "isampler2DRect"sv },
    { GL_UNSIGNED_INT_SAMPLER_1D, "usampler1D"sv },
    { GL_UNSIGNED_INT_SAMPLER_2D, "usampler2D"sv },
    { GL_UNSIGNED_INT_SAMPLER_3D, "usampler3D"sv },
    { GL_UNSIGNED_INT_SAMPLER_CUBE, "usamplerCube"sv },
    { GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, "usampler1DArray"sv },
    { GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, "usampler2DArray"sv },
    { GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, "usampler2DMS"sv },
    { GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, "usampler2DMSArray"sv },
    { GL_UNSIGNED_INT_SAMPLER_BUFFER, "usamplerBuffer"sv },
    { GL_UNSIGNED_INT_SAMPLER_2D_RECT, "usampler2DRect"sv }
};



}

