#include "RenderCoreRely.h"
#include "Basic3DObject.h"

namespace rayr
{
using b3d::PI_float;


Pyramid::Pyramid(const float len) : Drawable(GetType(this), TYPENAME), sidelen(len)
{
    vbo.reset(oglu::BufferType::Array);
    constexpr double sqrt3 = 1.73205080757;
    constexpr float sqrt3_2 = float(sqrt3 / 2);
    constexpr float sqrt3_3 = float(sqrt3 / 3);
    constexpr float sqrt3_6 = float(sqrt3 / 6);
    Point pts[] =
    { 
        { {  0.0f, sqrt3_2,  0.0f },{ -0.86326f,0.33227f,0.37998f },{ 0.0f, 0.0f } },
        { { -0.5f, 0.0f, -sqrt3_6 },{ -0.86326f,0.33227f,0.37998f },{ 4.0f, 0.0f } },
        { {  0.0f, 0.0f,  sqrt3_3 },{ -0.86326f,0.33227f,0.37998f },{ 0.0f, 4.0f } },
        { {  0.0f, sqrt3_2,  0.0f },{  0.86326f,0.33227f,0.37998f },{ 0.0f, 0.0f } },
        { {  0.0f, 0.0f,  sqrt3_3 },{  0.86326f,0.33227f,0.37998f },{ 0.0f, 4.0f } },
        { {  0.5f, 0.0f, -sqrt3_6 },{  0.86326f,0.33227f,0.37998f },{ 4.0f, 4.0f } },
        { {  0.0f, sqrt3_2,  0.0f },{  0.0f, -0.316228f, 0.94868f },{ 0.0f, 0.0f } },
        { {  0.5f, 0.0f, -sqrt3_6 },{  0.0f, -0.316228f, 0.94868f },{ 4.0f, 4.0f } },
        { { -0.5f, 0.0f, -sqrt3_6 },{  0.0f, -0.316228f, 0.94868f },{ 4.0f, 0.0f } },
        { {  0.0f, 0.0f,  sqrt3_3 },{  0.0f, -1.0f, 0.0f },{ 0.0f, 4.0f } },
        { { -0.5f, 0.0f, -sqrt3_6 },{  0.0f, -1.0f, 0.0f },{ 4.0f, 0.0f } },
        { {  0.5f, 0.0f, -sqrt3_6 },{  0.0f, -1.0f, 0.0f },{ 4.0f, 4.0f } }
    };
    for (auto& pt : pts)
        pt.pos *= sidelen;
    vbo->write(pts);
}

void Pyramid::prepareGL(const oglu::oglProgram& prog, const map<string, string>& translator)
{
    oglu::oglVAO vao(oglu::VAODrawMode::Triangles);
    vao->setDrawSize(0, 12);
    defaultBind(prog, vao, vbo).end();
    setVAO(prog, vao);
}


vector<uint16_t> Sphere::CreateSphere(vectorEx<Point>& pts, const float radius, const uint16_t rings /*= 80*/, const uint16_t sectors /*= 80*/)
{
    const float rstep = 1.0f / (rings - 1);
    const float sstep = 1.0f / (sectors - 1);
    pts.clear();
    pts.reserve(rings*sectors);
    uint16_t rcnt = rings, scnt = sectors;
    for (float r = 0; rcnt--; r += rstep)
    {
        scnt = sectors;
        for (float s = 0; scnt--; s += sstep)
        {
            const auto x = cos(2 * PI_float * s) * sin(PI_float * r);
            const auto y = sin(PI_float * r - PI_float / 2);
            const auto z = sin(2 * PI_float * s) * sin(PI_float * r);
            Normal norm(x, y, z);
            Coord2D texc(s, r);
            Point pt(norm * radius, norm, texc);
            pts.push_back(pt);
        }
    }
    vector<uint16_t> indexs;
    indexs.reserve(rings*sectors * 6);
    for (uint16_t r = 0; r < rings - 1; r++)
    {
        for (uint16_t s = 0; s < sectors - 1; s++)
        {
            const auto idx0 = r * sectors + s;
            indexs.push_back(idx0);
            indexs.push_back(idx0 + sectors);
            indexs.push_back(idx0 + 1);
            indexs.push_back(idx0 + 1);
            indexs.push_back(idx0 + sectors);
            indexs.push_back(idx0 + sectors + 1);
        }
    }
    return indexs;
}

Sphere::Sphere(const float r) : Drawable(GetType(this), TYPENAME), radius(r), radius_sqr(r*r)
{
    vectorEx<Point> pts;
    auto indexs = CreateSphere(pts, radius);
    vbo.reset(oglu::BufferType::Array);
    vbo->write(pts);
    ebo.reset();
    ebo->writeCompat(indexs);
    ptcount = static_cast<uint32_t>(indexs.size());
}

void Sphere::prepareGL(const oglu::oglProgram& prog, const map<string, string>& translator)
{
    oglu::oglVAO vao(oglu::VAODrawMode::Triangles);
    vao->setDrawSize(0, ptcount);
    defaultBind(prog,vao,vbo)//bind vertex attribute
        .setIndex(ebo)//index draw
        .end();
    setVAO(prog, vao);
}


/*
 *      v4-------v7
 *      /:       /|
 *     / :      / |
 *   v0--:----v3  |
 *    | v5-----|-v6
 *    | /      | /
 *    |/       |/
 *   v1-------v2
 *
 **/
const Point BoxBasePts[] = 
{ 
    { { +0.5f,+0.5f,+0.5f },{ +1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },//v3
    { { +0.5f,-0.5f,+0.5f },{ +1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 0.0f } },//v2
    { { +0.5f,-0.5f,-0.5f },{ +1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },//v6
    { { +0.5f,+0.5f,+0.5f },{ +1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },//v3
    { { +0.5f,-0.5f,-0.5f },{ +1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },//v6
    { { +0.5f,+0.5f,-0.5f },{ +1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 0.0f } },//v7

    { { -0.5f,+0.5f,-0.5f },{ -1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f, 1.0f } },//v4
    { { -0.5f,-0.5f,-0.5f },{ -1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } },//v5
    { { -0.5f,-0.5f,+0.5f },{ -1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f, 1.0f } },//v1
    { { -0.5f,+0.5f,-0.5f },{ -1.0f, 0.0f, 0.0f },{ 0.0f, 1.0f, 1.0f } },//v4
    { { -0.5f,-0.5f,+0.5f },{ -1.0f, 0.0f, 0.0f },{ 1.0f, 0.0f, 1.0f } },//v1
    { { -0.5f,+0.5f,+0.5f },{ -1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f } },//v0

    { { -0.5f,+0.5f,-0.5f },{ 0.0f, +1.0f, 0.0f },{ 0.0f, 1.0f, 2.0f } },//v4
    { { -0.5f,+0.5f,+0.5f },{ 0.0f, +1.0f, 0.0f },{ 0.0f, 0.0f, 2.0f } },//v0
    { { +0.5f,+0.5f,+0.5f },{ 0.0f, +1.0f, 0.0f },{ 1.0f, 0.0f, 2.0f } },//v3
    { { -0.5f,+0.5f,-0.5f },{ 0.0f, +1.0f, 0.0f },{ 0.0f, 1.0f, 2.0f } },//v4
    { { +0.5f,+0.5f,+0.5f },{ 0.0f, +1.0f, 0.0f },{ 1.0f, 0.0f, 2.0f } },//v3
    { { +0.5f,+0.5f,-0.5f },{ 0.0f, +1.0f, 0.0f },{ 1.0f, 1.0f, 2.0f } },//v7

    { { -0.5f,-0.5f,+0.5f },{ 0.0f, -1.0f, 0.0f },{ 0.0f, 1.0f, 3.0f } },//v1
    { { -0.5f,-0.5f,-0.5f },{ 0.0f, -1.0f, 0.0f },{ 0.0f, 0.0f, 3.0f } },//v5
    { { +0.5f,-0.5f,-0.5f },{ 0.0f, -1.0f, 0.0f },{ 1.0f, 0.0f, 3.0f } },//v6
    { { -0.5f,-0.5f,+0.5f },{ 0.0f, -1.0f, 0.0f },{ 0.0f, 1.0f, 3.0f } },//v1
    { { +0.5f,-0.5f,-0.5f },{ 0.0f, -1.0f, 0.0f },{ 1.0f, 0.0f, 3.0f } },//v6
    { { +0.5f,-0.5f,+0.5f },{ 0.0f, -1.0f, 0.0f },{ 1.0f, 1.0f, 3.0f } },//v2

    { { -0.5f,+0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 0.0f, 1.0f, 4.0f } },//v0
    { { -0.5f,-0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 0.0f, 0.0f, 4.0f } },//v1
    { { +0.5f,-0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 1.0f, 0.0f, 4.0f } },//v2
    { { -0.5f,+0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 0.0f, 1.0f, 4.0f } },//v0
    { { +0.5f,-0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 1.0f, 0.0f, 4.0f } },//v2
    { { +0.5f,+0.5f,+0.5f },{ 0.0f, 0.0f, +1.0f },{ 1.0f, 1.0f, 4.0f } },//v3

    { { +0.5f,+0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 1.0f, 5.0f } },//v7
    { { +0.5f,-0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 0.0f, 5.0f } },//v6
    { { -0.5f,-0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 5.0f } },//v5
    { { +0.5f,+0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 0.0f, 1.0f, 5.0f } },//v7
    { { -0.5f,-0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 0.0f, 5.0f } },//v5
    { { -0.5f,+0.5f,-0.5f },{ 0.0f, 0.0f, -1.0f },{ 1.0f, 1.0f, 5.0f } },//v4
};

Box::Box(const float length, const float height, const float width) : Drawable(GetType(this), TYPENAME)
{
    size = Vec3(length, height, width);
    vector<Point> pts;
    pts.assign(BoxBasePts, BoxBasePts + 36);
    for (auto& pt : pts)
        pt.pos *= size;
    vbo.reset(oglu::BufferType::Array);
    vbo->write(pts);
}

void Box::prepareGL(const oglu::oglProgram& prog, const map<string, string>& translator)
{
    oglu::oglVAO vao(oglu::VAODrawMode::Triangles);
    vao->setDrawSize(0, 36);
    defaultBind(prog, vao, vbo).end();
    setVAO(prog, vao);
}


Plane::Plane(const float len, const float texRepeat) : Drawable(GetType(this), TYPENAME)
{
    vbo.reset(oglu::BufferType::Array);
    const Point pts[] =
    {
        { { -len,0.0f,-len },{ 0.0f, +1.0f, 0.0f },{ 0.0f, texRepeat } },//v4
        { { -len,0.0f,+len },{ 0.0f, +1.0f, 0.0f },{ 0.0f, 0.0f } },//v0
        { { +len,0.0f,+len },{ 0.0f, +1.0f, 0.0f },{ texRepeat, 0.0f } },//v3
        { { -len,0.0f,-len },{ 0.0f, +1.0f, 0.0f },{ 0.0f, texRepeat } },//v4
        { { +len,0.0f,+len },{ 0.0f, +1.0f, 0.0f },{ texRepeat, 0.0f } },//v3
        { { +len,0.0f,-len },{ 0.0f, +1.0f, 0.0f },{ texRepeat, texRepeat } },//v7
    };
    vbo->write(pts, sizeof(pts));
}

void Plane::prepareGL(const oglu::oglProgram& prog, const map<string, string>& translator)
{
    oglu::oglVAO vao(oglu::VAODrawMode::Triangles);
    vao->setDrawSize(0, 6);
    defaultBind(prog, vao, vbo).end();
    setVAO(prog, vao);
}

}
