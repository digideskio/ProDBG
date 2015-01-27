#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <assert.h>
#include <stb_truetype.h>
#include "core/file.h"
#include "ui_render.h"

#include "scintilla/include/Platform.h"

#include <imgui.h> // TODO: TEMP! Hook up properly to ImGui

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline uint32_t MakeRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFF)
{
    return a << 24 | b << 16 | g << 8 | r;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ColourDesired Platform::Chrome()
{
    return MakeRGBA(0xe0, 0xe0, 0xe0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ColourDesired Platform::ChromeHighlight()
{
    return MakeRGBA(0xff, 0xff, 0xff);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* Platform::DefaultFont()
{
    return "Lucida Console";
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Platform::DefaultFontSize()
{
    return 10;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned int Platform::DoubleClickTime()
{
    return 500;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Platform::MouseButtonBounce()
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__clang__)
__attribute__((noreturn)///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

              ) void Platform::Assert(const char* error, const char* filename, int line)
#else
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Platform::Assert(const char* error, const char* filename, int line)
#endif
{
    printf("Assertion [%s] failed at %s %d\n", error, filename, line);
    assert(false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SetClipboardTextUTF8(const char* text, size_t len, int additionalFormat)
{
    (void)text;
    (void)len;
    (void)additionalFormat;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GetClipboardTextUTF8(char* text, size_t len)
{
    (void)text;
    (void)len;

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SurfaceImpl : public Surface
{
public:
    SurfaceImpl();
    virtual ~SurfaceImpl();

    bool InitBgfx();

    void Init(WindowID wid);
    virtual void Init(SurfaceID sid, WindowID wid);
    void InitPixMap(int width, int height, Surface* surface_, WindowID wid);

    void Release();
    bool Initialised();
    void PenColour(ColourDesired fore);
    int LogPixelsY();
    int DeviceHeightFont(int points);
    void MoveTo(int x_, int y_);
    void LineTo(int x_, int y_);
    void Polygon(Point* pts, int npts, ColourDesired fore, ColourDesired back);
    void RectangleDraw(PRectangle rc, ColourDesired fore, ColourDesired back);
    void FillRectangle(PRectangle rc, ColourDesired back);
    void FillRectangle(PRectangle rc, Surface& surfacePattern);
    void RoundedRectangle(PRectangle rc, ColourDesired fore, ColourDesired back);
    void AlphaRectangle(PRectangle rc, int cornerSize, ColourDesired fill, int alphaFill, ColourDesired outline, int alphaOutline, int flags);
    void DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char* pixelsImage);
    void Ellipse(PRectangle rc, ColourDesired fore, ColourDesired back);
    void Copy(PRectangle rc, Point from, Surface& surfaceSource);

    void DrawTextNoClip(PRectangle rc, Font& font_, XYPOSITION ybase, const char* s, int len, ColourDesired fore, ColourDesired back);
    void DrawTextClipped(PRectangle rc, Font& font_, XYPOSITION ybase, const char* s, int len, ColourDesired fore, ColourDesired back);
    void DrawTextTransparent(PRectangle rc, Font& font_, XYPOSITION ybase, const char* s, int len, ColourDesired fore);
    void MeasureWidths(Font& font_, const char* s, int len, XYPOSITION* positions);
    void DrawTextBase(PRectangle rc, Font& font_, float ybase, const char* s, int len, ColourDesired f);

    XYPOSITION WidthText(Font& font_, const char* s, int len);
    XYPOSITION WidthChar(Font& font_, char ch);
    XYPOSITION Ascent(Font& font_);
    XYPOSITION Descent(Font& font_);
    XYPOSITION InternalLeading(Font& font_);
    XYPOSITION ExternalLeading(Font& font_);
    XYPOSITION Height(Font& font_);
    XYPOSITION AverageCharWidth(Font& font_);

    void SetClip(PRectangle rc);
    void FlushCachedState();

    void SetUnicodeMode(bool unicodeMode_);
    void SetDBCSMode(int codePage);
private:

    ColourDesired m_penColour;
    int x;
    int y;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct stbtt_Font
{
    stbtt_fontinfo fontinfo;
    stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
    bgfx::TextureHandle ftex;
    float scale;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font::Font() : fid(0)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Font::~Font()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Font::Create(const FontParameters& fp)
{
    stbtt_Font* newFont = new stbtt_Font;

    size_t len;

    // TODO: Remove hard-coded value

    unsigned char* bmp = new unsigned char[512 * 512];
    void* data = File_loadToMemory(fp.faceName, &len, 0);

    stbtt_BakeFontBitmap((unsigned char*)data, 0, fp.size, bmp, 512, 512, 32, 96, newFont->cdata); // no guarantee this fits!

    const bgfx::Memory* mem = bgfx::alloc(512 * 512);
    memcpy(mem->data, bmp, 512 * 512);

    newFont->ftex = bgfx::createTexture2D(512, 512, 1, bgfx::TextureFormat::R8, BGFX_TEXTURE_NONE, mem);

    stbtt_InitFont(&newFont->fontinfo, (unsigned char*)data, 0);

    newFont->scale = stbtt_ScaleForPixelHeight(&newFont->fontinfo, fp.size);

    delete[] bmp;

    fid = newFont;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ImageData
{
    bgfx::TextureHandle tex;
    float scalex, scaley;
    bool initialised;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UpdateImageData(ImageData& image, int w, int h, const unsigned char* data)
{
    const int byteSize = w * h * (int)sizeof(unsigned char) * 4; // RGBA image

    if (!image.initialised)
        image.tex = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_NONE, 0);

    const bgfx::Memory* mem = bgfx::alloc((uint32_t)byteSize);
    memcpy(mem->data, data, (uint32_t)byteSize);

    image.initialised = true;
    image.scalex = 1.0f / (float)w;
    image.scaley = 1.0f / (float)h;

    bgfx::updateTexture2D(image.tex, 0, 0, 0, (uint16_t)w, (uint16_t)h, mem);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SurfaceImpl::SurfaceImpl() : x(0), y(0)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SurfaceImpl::~SurfaceImpl()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Release()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::PenColour(ColourDesired fore)
{
    (void)fore;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SurfaceImpl::LogPixelsY()
{
    return 72;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int SurfaceImpl::DeviceHeightFont(int points)
{
    int logPix = LogPixelsY();
    return (int)((points * logPix + logPix / 2) / 72.0f);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::MoveTo(int x_, int y_)
{
    x = x_;
    y = y_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::LineTo(int x_, int y_)
{
    // TODO: implement
    x = x_;
    y = y_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Polygon(Point* pts, int npts, ColourDesired fore, ColourDesired back)
{
    (void)pts;
    (void)fore;
    (void)back;
    (void)npts;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::RectangleDraw(PRectangle rc, ColourDesired fore, ColourDesired back)
{
    (void)fore;

    FillRectangle(rc, back);
    /*
       glColor4ubv((GLubyte*)&fore);
       glDisable(GL_TEXTURE_2D);
       glBegin(GL_LINE_STRIP);
       glVertex2f(rc.left+0.5f,  rc.top+0.5f);
       glVertex2f(rc.right-0.5f, rc.top+0.5f);
       glVertex2f(rc.right-0.5f, rc.bottom-0.5f);
       glVertex2f(rc.left+0.5f,  rc.bottom-0.5f);
       glVertex2f(rc.left+0.5f,  rc.top+0.5f);
       glEnd();
     */
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SurfaceImpl::Initialised()
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Init(WindowID wid)
{
    (void)wid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Init(SurfaceID sid, WindowID wid)
{
    (void)wid;
    (void)sid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::InitPixMap(int width, int height, Surface* surface_, WindowID wid)
{
    (void)width;
    (void)height;
    (void)surface_;
    (void)wid;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char* pixelsImage)
{
    ImageData image;
    memset(&image, 0x0, sizeof(image));

    // GW-TODO: Cache this!
    UpdateImageData(image, width, height, pixelsImage);

    bgfx::TransientVertexBuffer tvb;

    float offsetx = 0.0f;
    float offsety = 0.0f;

    float w = (rc.right - rc.left) * image.scalex, h = (rc.bottom - rc.top) * image.scaley;
    float u1 = offsetx * image.scalex, v1 = offsety * image.scaley, u2 = u1 + w, v2 = v1 + h;

    // TODO: Use program that doesn't set color

    UIRender_allocPosTexColorTb(&tvb, 6);

    PosTexColorVertex* vb = (PosTexColorVertex*)tvb.data;

    // First triangle

    vb[0].x = rc.left;
    vb[0].y = rc.top;
    vb[0].u = u1;
    vb[0].v = v1;
    vb[0].color = 0xffffffff;

    vb[1].x = rc.right;
    vb[1].y = rc.top;
    vb[1].u = u2;
    vb[1].v = v1;
    vb[1].color = 0xffffffff;

    vb[2].x = rc.right;
    vb[2].y = rc.bottom;
    vb[2].u = u2;
    vb[2].v = v2;
    vb[2].color = 0xffffffff;

    // Second triangle

    vb[3].x = rc.left;
    vb[3].y = rc.top;
    vb[3].u = u1;
    vb[3].v = v1;
    vb[3].color = 0xffffffff;

    vb[4].x = rc.right;
    vb[4].y = rc.bottom;
    vb[4].u = u2;
    vb[4].v = v2;
    vb[4].color = 0xffffffff;

    vb[5].x = rc.left;
    vb[5].y = rc.bottom;
    vb[5].u = u1;
    vb[5].v = v2;
    vb[5].color = 0xffffffff;

    bgfx::setState(0
                   | BGFX_STATE_RGB_WRITE
                   | BGFX_STATE_ALPHA_WRITE
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                   | BGFX_STATE_MSAA);

    UIRender_posTexColor(&tvb, 0, 6, image.tex);

    bgfx::destroyTexture(image.tex); // GW-TODO: Lol
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void fillRectangle(PRectangle rc, ColourDesired b)
{
    bgfx::TransientVertexBuffer tvb;

    const uint32_t back = (uint32_t)b.AsLong();

    UIRender_allocPosColorTb(&tvb, 6);

    PosColorVertex* vb = (PosColorVertex*)tvb.data;

    // First triangle

    vb[0].x = rc.left;
    vb[0].y = rc.top;
    vb[0].color = back;

    vb[1].x = rc.right;
    vb[1].y = rc.top;
    vb[1].color = back;

    vb[2].x = rc.right;
    vb[2].y = rc.bottom;
    vb[2].color = back;

    // Second triangle

    vb[3].x = rc.left;
    vb[3].y = rc.top;
    vb[3].color = back;

    vb[4].x = rc.right;
    vb[4].y = rc.bottom;
    vb[4].color = back;

    vb[5].x = rc.left;
    vb[5].y = rc.bottom;
    vb[5].color = back;

    bgfx::setState(0
                   | BGFX_STATE_RGB_WRITE
                   | BGFX_STATE_ALPHA_WRITE
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                   | BGFX_STATE_MSAA);

    UIRender_posColor(&tvb, 0, 6);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::FillRectangle(PRectangle rc, ColourDesired b)
{
	// TODO: Figure out why we need to do this.
	//fillRectangle(rc, b);
	fillRectangle(rc, b);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::FillRectangle(PRectangle rc, Surface&)
{
    // GW: This probably needs to be a blit from incoming surface?

	(void)rc;
    assert(false);
/*
    bgfx::TransientVertexBuffer tvb;

    const uint32_t back = 0xFFFFFF; // GW-TODO: Likely need to track the current fore\back color as per style

    UIRender_allocPosColorTb(&tvb, 6);

    PosColorVertex* vb = (PosColorVertex*)tvb.data;

    // First triangle

    vb[0].x = rc.left;
    vb[0].y = rc.top;
    vb[0].color = back;

    vb[1].x = rc.right;
    vb[1].y = rc.top;
    vb[1].color = back;

    vb[2].x = rc.right;
    vb[2].y = rc.bottom;
    vb[2].color = back;

    // Second triangle

    vb[3].x = rc.left;
    vb[3].y = rc.top;
    vb[3].color = back;

    vb[4].x = rc.right;
    vb[4].y = rc.bottom;
    vb[4].color = back;

    vb[5].x = rc.left;
    vb[5].y = rc.bottom;
    vb[5].color = back;

    bgfx::setState(0
                   | BGFX_STATE_RGB_WRITE
                   | BGFX_STATE_ALPHA_WRITE
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                   | BGFX_STATE_MSAA);

    UIRender_posColor(&tvb, 0, 6);
*/
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::RoundedRectangle(PRectangle, ColourDesired, ColourDesired)
{
    assert(false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::AlphaRectangle(
    PRectangle rc, int cornerSize, ColourDesired fill, int alphaFill,
    ColourDesired outline, int alphaOutline, int flags)
{
    unsigned int back = (uint32_t)((fill.AsLong() & 0xffffff) | ((alphaFill & 0xff) << 24));

    (void)cornerSize;
    (void)outline;
    (void)alphaOutline;
    (void)flags;

    FillRectangle(rc, ColourDesired(back));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Ellipse(PRectangle rc, ColourDesired fore, ColourDesired back)
{
    (void)fore;
    (void)back;
    //assert(0);

    FillRectangle(rc, fore);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Font::Release()
{
    if (fid)
    {
        free(((stbtt_Font*)fid)->fontinfo.data);
        delete (stbtt_Font*)fid;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::DrawTextBase(PRectangle rc, Font& font_, float ybase, const char* s, int len, ColourDesired f)
{
    uint32_t realLength = 0;
    float xt = rc.left;
    float yt = ybase;

    uint32_t fore = (uint32_t)f.AsLong();

    bgfx::TransientVertexBuffer tvb;

    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();

    UIRender_allocPosTexColorTb(&tvb, (uint32_t)(len * 6)); // * 6 vertices per character (2 triangles)

    PosTexColorVertex* vb = (PosTexColorVertex*)tvb.data;

    while (len--)
    {
        if (*s >= 32 && *s < 127)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(realFont->cdata, 512, 512, *s - 32, &xt, &yt, &q, 1);

            // First triangle

            vb[0].x = q.x0;
            vb[0].y = q.y0;
            vb[0].u = q.s0;
            vb[0].v = q.t0;
            vb[0].color = fore;

            vb[1].x = q.x1;
            vb[1].y = q.y0;
            vb[1].u = q.s1;
            vb[1].v = q.t0;
            vb[1].color = fore;

            vb[2].x = q.x1;
            vb[2].y = q.y1;
            vb[2].u = q.s1;
            vb[2].v = q.t1;
            vb[2].color = fore;

            // Second triangle

            vb[3].x = q.x0;
            vb[3].y = q.y0;
            vb[3].u = q.s0;
            vb[3].v = q.t0;
            vb[3].color = fore;

            vb[4].x = q.x1;
            vb[4].y = q.y1;
            vb[4].u = q.s1;
            vb[4].v = q.t1;
            vb[4].color = fore;

            vb[5].x = q.x0;
            vb[5].y = q.y1;
            vb[5].u = q.s0;
            vb[5].v = q.t1;
            vb[5].color = fore;

            vb += 6;
            realLength++;
        }

        ++s;
    }

    bgfx::setState(0 
                   | BGFX_STATE_RGB_WRITE
                   | BGFX_STATE_ALPHA_WRITE
                   | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                   | BGFX_STATE_MSAA);

    UIRender_posTexRColor(&tvb, 0, realLength * 6, realFont->ftex);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::DrawTextNoClip(PRectangle rc, Font& font_, float ybase, const char* s, int len,
                                 ColourDesired fore, ColourDesired /*back*/)
{
    DrawTextBase(rc, font_, ybase, s, len, fore);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::DrawTextClipped(PRectangle rc, Font& font_, float ybase, const char* s, int len, ColourDesired fore, ColourDesired /*back*/)
{
    DrawTextBase(rc, font_, ybase, s, len, fore);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::DrawTextTransparent(PRectangle rc, Font& font_, float ybase, const char* s, int len, ColourDesired fore)
{
    DrawTextBase(rc, font_, ybase, s, len, fore);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::MeasureWidths(Font& font_, const char* s, int len, float* positions)
{
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();

    //TODO: implement proper UTF-8 handling

    float position = 0;
    while (len--)
    {
        int advance, leftBearing;

        stbtt_GetCodepointHMetrics(&realFont->fontinfo, *s++, &advance, &leftBearing);

        position += advance;//TODO: +Kerning
        *positions++ = position * realFont->scale;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::WidthText(Font& font_, const char* s, int len)
{
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();

    //TODO: implement proper UTF-8 handling

    float position = 0;
    while (len--)
    {
        int advance, leftBearing;
        stbtt_GetCodepointHMetrics(&realFont->fontinfo, *s++, &advance, &leftBearing);
        position += advance * realFont->scale;//TODO: +Kerning
    }

    return position;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::WidthChar(Font& font_, char ch)
{
    int advance, leftBearing;
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();
    stbtt_GetCodepointHMetrics(&realFont->fontinfo, ch, &advance, &leftBearing);

    return advance * realFont->scale;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::Ascent(Font& font_)
{
    int ascent, descent, lineGap;
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();
    stbtt_GetFontVMetrics(&realFont->fontinfo, &ascent, &descent, &lineGap);

    return ascent * realFont->scale;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::Descent(Font& font_)
{
    int ascent, descent, lineGap;
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();
    stbtt_GetFontVMetrics(&realFont->fontinfo, &ascent, &descent, &lineGap);

    return -descent * realFont->scale;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::InternalLeading(Font&)
{
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::ExternalLeading(Font& font_)
{
    stbtt_Font* realFont = (stbtt_Font*)font_.GetID();
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&realFont->fontinfo, &ascent, &descent, &lineGap);
    return lineGap * realFont->scale;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::Height(Font& font_)
{
    return Ascent(font_) + Descent(font_);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float SurfaceImpl::AverageCharWidth(Font& font_)
{
    return WidthChar(font_, 'n');
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::SetClip(PRectangle rc)
{
	float xt = rc.left;
	float yt = rc.top;
	float width = rc.right - rc.left;
	float height = rc.bottom - rc.top;

	(void)xt;
	(void)yt;
	(void)width;
	(void)height;

	//bgfx::setScissor((uint16_t)x, (uint16_t)y, (uint16_t)width, (uint16_t)height);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::FlushCachedState()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::SetDBCSMode(int codePage)
{
    (void)codePage;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::SetUnicodeMode(bool unicodeMode)
{
    (void)unicodeMode;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SurfaceImpl::Copy(PRectangle rc, Point from, Surface& surfaceSource)
{
    (void)rc;
    (void)from;
    (void)surfaceSource;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Surface* Surface::Allocate(int technology)
{
    (void)technology;
    return new SurfaceImpl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Window::~Window()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::Destroy()
{
    wid = 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Window::HasFocus()
{
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PRectangle Window::GetPosition()
{
    // TODO: TEMP! Hook up properly to ImGui
    ImGuiIO& io = ImGui::GetIO();
    return PRectangle::FromInts(0, 0, int(io.DisplaySize.x), int(io.DisplaySize.y));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::SetPosition(PRectangle rc)
{
    (void)rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::SetPositionRelative(PRectangle rc, Window w)
{
    (void)rc;
    (void)w;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PRectangle Window::GetClientPosition()
{
    // TODO: TEMP! Hook up properly to ImGui
    ImGuiIO& io = ImGui::GetIO();
    return PRectangle::FromInts(0, 0, int(io.DisplaySize.x), int(io.DisplaySize.y));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::Show(bool show)
{
    (void)show;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::InvalidateAll()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::InvalidateRectangle(PRectangle rc)
{
    (void)rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::SetFont(Font& font)
{
    (void)font;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::SetCursor(Cursor curs)
{
    cursorLast = cursorText;
    (void)curs;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Window::SetTitle(const char* s)
{
    (void)s;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PRectangle Window::GetMonitorRect(Point pt)
{
    (void)pt;
    return PRectangle();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Platform::Minimum(int a, int b)
{
    if (a < b)
        return a;
    else
        return b;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Platform::Maximum(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Platform::Clamp(int val, int minVal, int maxVal)
{
    if (val > maxVal)
        val = maxVal;
    if (val < minVal)
        val = minVal;
    return val;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Platform::DebugPrintf(const char* format, ...)
{
    char buffer[2000];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    printf(buffer);
}

