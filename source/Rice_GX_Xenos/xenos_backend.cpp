#include "stdafx.h"

#include "xenos_backend.h"

#include <xenos/xe.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <zlx/zlx.h>
#include <debug.h>
#include <byteswap.h>

extern Matrix g_MtxReal;
extern uObjMtxReal gObjMtxReal;

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#define MAX_VERTEX_COUNT (8192*3)

#define NEAR (-1.0)
#define FAR  (1.0)

#if 0
const struct XenosVBFFormat VertexBufferFormat = {
    4, {
        {XE_USAGE_POSITION, 0, XE_TYPE_FLOAT4},
        {XE_USAGE_COLOR,    0, XE_TYPE_UBYTE4},
        {XE_USAGE_COLOR,    1, XE_TYPE_UBYTE4},
	    {XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2},
    }
};
#else
const struct XenosVBFFormat VertexBufferFormat = {
    4, {
        {XE_USAGE_POSITION, 0, XE_TYPE_FLOAT4},
	    {XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2},
	    {XE_USAGE_TEXCOORD, 1, XE_TYPE_FLOAT2},
        {XE_USAGE_COLOR,    0, XE_TYPE_UBYTE4},
    }
};
#endif

typedef struct
#ifdef __GNUC__
    __attribute__((__packed__)) VertexStruct
#endif
{
	float x,y,z,w;
	float u0,v0;
	float u1,v1;
	union
	{
		unsigned long color;
		struct
		{
			u8 a;
			u8 b;
			u8 g;
			u8 r;
		};
	};
} TVertex;

struct XenosDevice *xe;
struct XenosShader *sh_ps_fb, *sh_vs;
struct XenosVertexBuffer *vertexBuffer;

extern u32 vs_table_count;
extern void * vs_data_table[];
extern u32 vs_size_table[];
extern u32 vs_indice_count;
extern u32 vs_indices[];

extern u32 ps_fb_table_count;
extern void * ps_fb_data_table[];
extern u32 ps_fb_size_table[];
extern u32 ps_fb_indice_count;
extern u32 ps_fb_indices[];

// used for double buffering
struct XenosSurface * framebuffer[2] = {NULL};
int curFB=0;

bool needSync=false;
bool hadTriangles=false;
bool drawPrepared=false;
int prevVertexCount=0;

TVertex * firstVertex;
TVertex * currentVertex;

int xe_cull;
bool xe_zcompare;
bool xe_zenable;
bool xe_zwrite;


int vertexCount()
{
    return currentVertex-firstVertex;
}

void resetLockVB()
{
	Xe_SetStreamSource(xe, 0, vertexBuffer, 0, 4);
	firstVertex=currentVertex=(TVertex *)Xe_VB_Lock(xe,vertexBuffer,0,MAX_VERTEX_COUNT*sizeof(TVertex),XE_LOCK_WRITE);
	prevVertexCount=0;
}

void nextVertex()
{
	++currentVertex;
	
    if (vertexCount()>=MAX_VERTEX_COUNT)
	{
		printf("[rice_xenos] too many vertices !\n");
		exit(1);
    }
}

void drawVB()
{
	if (vertexCount()>prevVertexCount)
	{
		Xe_DrawPrimitive(xe,XE_PRIMTYPE_TRIANGLELIST,prevVertexCount,(vertexCount()-prevVertexCount)/3);
        prevVertexCount=vertexCount();
		hadTriangles=true;
    }
}

void setZStuff()
{
	Xe_SetZEnable(xe,xe_zenable?1:0);
	Xe_SetZWrite(xe,xe_zwrite?1:0);
	Xe_SetZFunc(xe,xe_zcompare?XE_CMP_LESSEQUAL:XE_CMP_ALWAYS);
}

////////////////////////////////////////////////////////////////////////////////
// CxeGraphicsContext
////////////////////////////////////////////////////////////////////////////////

CxeGraphicsContext::CxeGraphicsContext()
{
	static int done=0;

	xe = ZLX::g_pVideoDevice;

    XenosSurface * fb = Xe_GetFramebufferSurface(xe);

	drawPrepared=false;
	needSync=false;
	hadTriangles=false;

    if(!done)
	{

		/* initialize the GPU */

		Xe_SetRenderTarget(xe, fb);
		Xe_InvalidateState(xe);
		Xe_SetClearColor(xe,0);

		/* load pixel shaders */

		assert(ps_fb_table_count==1 && ps_fb_indice_count==1);
		sh_ps_fb = Xe_LoadShaderFromMemory(xe, ps_fb_data_table[0]);
		Xe_InstantiateShader(xe, sh_ps_fb, 0);

		/* load vertex shader */

		assert(vs_table_count==1 && vs_indice_count==1);
		sh_vs = Xe_LoadShaderFromMemory(xe, vs_data_table[0]);
		Xe_InstantiateShader(xe, sh_vs, 0);
		Xe_ShaderApplyVFetchPatches(xe, sh_vs, 0, &VertexBufferFormat);

		vertexBuffer = Xe_CreateVertexBuffer(xe,MAX_VERTEX_COUNT*sizeof(TVertex));

		// Create surface for double buffering
		framebuffer[0] = Xe_CreateTexture(xe, fb->width, fb->height, 0, XE_FMT_8888 | XE_FMT_BGRA, 1);
		framebuffer[1] = Xe_CreateTexture(xe, fb->width, fb->height, 0, XE_FMT_8888 | XE_FMT_BGRA, 1);        

		done=1;
	}
}

CxeGraphicsContext::~CxeGraphicsContext()
{

}

void CxeGraphicsContext::InitState(void)
{
}

void CxeGraphicsContext::InitOGLExtension(void)
{
    
}

bool CxeGraphicsContext::IsExtensionSupported(const char* pExtName)
{
    TRS(pExtName);
    return true;
}

bool CxeGraphicsContext::IsWglExtensionSupported(const char* pExtName)
{
    TRS(pExtName);
    return true;
}

void CxeGraphicsContext::CleanUp()
{
    if (vertexBuffer->lock.start) Xe_VB_Unlock(xe,vertexBuffer);
	Xe_SetScissor(xe,0,0,0,0,0);
    Xe_SetFrameBufferSurface(xe,&xe->default_fb);
    Xe_SetRenderTarget(xe,&xe->default_fb);
}

void CxeGraphicsContext::Clear(ClearFlag dwFlags, uint32 color, float depth)
{
    uint32 flag=0;
    if( dwFlags&CLEAR_COLOR_BUFFER )    flag |= XE_CLEAR_COLOR;
    if( dwFlags&CLEAR_DEPTH_BUFFER )    flag |= XE_CLEAR_DS;

	Xe_SetClearColor(xe,color);
	Xe_Clear(xe,flag);
}

void CxeGraphicsContext::UpdateFrame(bool swaponly)
{
	status.gFrameCount++;

	if(!swaponly && hadTriangles && drawPrepared)
	{
		drawVB();
		
		Xe_VB_Unlock(xe,vertexBuffer);

		Xe_ResolveInto(xe,xe->rt,XE_SOURCE_COLOR,0);
		Xe_Execute(xe); // render everything in background !
		
#if 0
		Xe_Sync(xe);
#else
		needSync=true;
#endif
		
		drawPrepared=false;
	}
}

bool CxeGraphicsContext::SetFullscreenMode()
{
	return false;
}

bool CxeGraphicsContext::SetWindowMode()
{
	return false;
}
int CxeGraphicsContext::ToggleFullscreen()
{
	return false;
}

// This is a static function, will be called when the plugin DLL is initialized
void CxeGraphicsContext::InitDeviceParameters()
{

}

// Get methods
bool CxeGraphicsContext::IsSupportAnisotropicFiltering()
{
	return true;
}

int CxeGraphicsContext::getMaxAnisotropicFiltering()
{
	return 2;
}

////////////////////////////////////////////////////////////////////////////////
// CxeRender
////////////////////////////////////////////////////////////////////////////////

UVFlagMap xeXUVFlagMaps[] =
{
{TEXTURE_UV_FLAG_WRAP, XE_TEXADDR_WRAP},
{TEXTURE_UV_FLAG_MIRROR, XE_TEXADDR_MIRROR},
{TEXTURE_UV_FLAG_CLAMP, XE_TEXADDR_CLAMP},
};

CxeRender::CxeRender()
{
    m_bSupportFogCoordExt = false;
    m_bMultiTexture = true;
    m_bSupportClampToEdge = true;
    for( int i=0; i<8; i++ )
    {
        m_curBoundTex[i]=0;
        m_texUnitEnabled[i]=FALSE;
    }
    m_bEnableMultiTexture = true;
}

CxeRender::~CxeRender()
{
    ClearDeviceObjects();
}

bool CxeRender::InitDeviceObjects()
{
    // enable Z-buffer by default
    ZBufferEnable(true);
    return true;
}

bool CxeRender::ClearDeviceObjects()
{
    return true;
}

void CxeRender::Initialize(void)
{
    XenosSurface * fb = Xe_GetFramebufferSurface(xe);

    windowSetting.uDisplayWidth = fb->width;
    windowSetting.uDisplayHeight = fb->height;

	xe_cull=XE_CULL_NONE;
	xe_zenable=true;
	xe_zwrite=true;
	xe_zcompare=true;
	
    glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);
	
	RenderReset();
}

void CxeRender::ApplyTextureFilter()
{

}

void CxeRender::SetShadeMode(RenderShadeMode mode)
{

}

void CxeRender::ZBufferEnable(BOOL bZBuffer)
{
	gRSP.bZBufferEnabled = bZBuffer;
    if( g_curRomInfo.bForceDepthBuffer )
        bZBuffer = TRUE;

	xe_zenable=bZBuffer;
	setZStuff();
}

void CxeRender::ClearBuffer(bool cbuffer, bool zbuffer)
{
    int flag=0;
    if( cbuffer )    flag |= CLEAR_COLOR_BUFFER;
    if( zbuffer )    flag |= CLEAR_DEPTH_BUFFER;

	CGraphicsContext::Get()->Clear((ClearFlag)flag,0,1.0);
}

void CxeRender::ClearZBuffer(float depth)
{
	CGraphicsContext::Get()->Clear(CLEAR_DEPTH_BUFFER,0,depth);
}

void CxeRender::SetZCompare(BOOL bZCompare)
{
    if( g_curRomInfo.bForceDepthBuffer )
        bZCompare = TRUE;

    gRSP.bZBufferEnabled = bZCompare;

	xe_zcompare=bZCompare;
	setZStuff();
}

void CxeRender::SetZUpdate(BOOL bZUpdate)
{
    if( g_curRomInfo.bForceDepthBuffer )
        bZUpdate = TRUE;

	xe_zwrite=bZUpdate;
	setZStuff();
}

void CxeRender::ApplyZBias(int bias)
{
	
//	float f1 = bias > 0 ? -3.0f : 0.0f;  // z offset = -3.0 * max(abs(dz/dx),abs(dz/dy)) per pixel delta z slope
//	loat f2 = bias > 0 ? -3.0f : 0.0f;  // z offset += -3.0 * 1 bit
}

void CxeRender::SetZBias(int bias)
{
#if defined(DEBUGGER)
    if( pauseAtNext == true )
      DebuggerAppendMsg("Set zbias = %d", bias);
#endif
    // set member variable and apply the setting in opengl
    m_dwZBias = bias;
    ApplyZBias(bias);
}

void CxeRender::SetAlphaRef(uint32 dwAlpha)
{
    if (m_dwAlpha != dwAlpha)
    {
        m_dwAlpha = dwAlpha;
        Xe_SetAlphaRef(xe,(dwAlpha-1)/255.0f);
    }
}

void CxeRender::ForceAlphaRef(uint32 dwAlpha)
{
    float ref = (dwAlpha-1)/255.0f;
    Xe_SetAlphaRef(xe,ref);
	Xe_SetAlphaFunc(xe,XE_CMP_GREATER);
}

void CxeRender::SetFillMode(FillMode mode)
{
/*	if( mode == RICE_FILLMODE_WINFRAME )
    {
        Xe_SetFillMode(xe,XE_FILL_WIREFRAME,XE_FILL_WIREFRAME);
    }
    else
    {
        Xe_SetFillMode(xe,XE_FILL_SOLID,XE_FILL_SOLID);
    }*/
}

void CxeRender::SetCullMode(bool bCullFront, bool bCullBack)
{
    CRender::SetCullMode(bCullFront, bCullBack);
    if( bCullFront && bCullBack )
    {
        xe_cull=XE_CULL_NONE;
    }
    else if( bCullFront )
    {
        xe_cull=XE_CULL_CCW;
    }
    else if( bCullBack )
    {
        xe_cull=XE_CULL_CW;
    }
    else
    {
        xe_cull=XE_CULL_NONE;
    }

	Xe_SetCullMode(xe,xe_cull);
}

bool CxeRender::SetCurrentTexture(int tile, CTexture *handler,uint32 dwTileWidth, uint32 dwTileHeight, TxtrCacheEntry *pTextureEntry)
{
    RenderTexture &texture = g_textures[tile];
    texture.pTextureEntry = pTextureEntry;

    if( handler!= NULL  && texture.m_lpsTexturePtr != handler->GetTexture() )
    {
        texture.m_pCTexture = handler;
        texture.m_lpsTexturePtr = handler->GetTexture();

        texture.m_dwTileWidth = dwTileWidth;
        texture.m_dwTileHeight = dwTileHeight;

        if( handler->m_bIsEnhancedTexture )
        {
            texture.m_fTexWidth = (float)pTextureEntry->pTexture->m_dwCreatedTextureWidth;
            texture.m_fTexHeight = (float)pTextureEntry->pTexture->m_dwCreatedTextureHeight;
        }
        else
        {
            texture.m_fTexWidth = (float)handler->m_dwCreatedTextureWidth;
            texture.m_fTexHeight = (float)handler->m_dwCreatedTextureHeight;
        }
    }
	
	
    return true;
}

bool CxeRender::SetCurrentTexture(int tile, TxtrCacheEntry *pEntry)
{
    if (pEntry != NULL && pEntry->pTexture != NULL)
    {   
        SetCurrentTexture( tile, pEntry->pTexture,  pEntry->ti.WidthToCreate, pEntry->ti.HeightToCreate, pEntry);
        return true;
    }
    else
    {
        SetCurrentTexture( tile, NULL, 64, 64, NULL );
        return false;
    }
    return true;
}

void CxeRender::SetAddressUAllStages(uint32 dwTile, TextureUVFlag dwFlag)
{
    SetTextureUFlag(dwFlag, dwTile);
}

void CxeRender::SetAddressVAllStages(uint32 dwTile, TextureUVFlag dwFlag)
{
    SetTextureVFlag(dwFlag, dwTile);
}

void CxeRender::SetTextureUFlag(TextureUVFlag dwFlag, uint32 dwTile)
{
    TileUFlags[dwTile] = dwFlag;
    if( dwTile == gRSP.curTile )    // For basic OGL, only support the 1st texel
    {
        CxeTexture* pTexture = g_textures[gRSP.curTile].m_pCxeTexture;
        if( pTexture )
        {
            EnableTexUnit(0,TRUE);
			pTexture->tex->u_addressing=xeXUVFlagMaps[dwFlag].realFlag;
            BindTexture(pTexture->tex, 0);
        }
    }
}
void CxeRender::SetTextureVFlag(TextureUVFlag dwFlag, uint32 dwTile)
{
    TileVFlags[dwTile] = dwFlag;
    if( dwTile == gRSP.curTile )    // For basic OGL, only support the 1st texel
    {
        CxeTexture* pTexture = g_textures[gRSP.curTile].m_pCxeTexture;
        if( pTexture ) 
        {
            EnableTexUnit(0,TRUE);
			pTexture->tex->v_addressing=xeXUVFlagMaps[dwFlag].realFlag;
            BindTexture(pTexture->tex, 0);
        }
    }
}

// Basic render drawing functions

bool CxeRender::RenderTexRect()
{
	glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);

	Xe_SetCullMode(xe,XE_CULL_NONE);

    float depth =-(g_texRectTVtx[3].z*2-1);

	currentVertex->x=g_texRectTVtx[0].x;
	currentVertex->y=g_texRectTVtx[0].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[0].r;
	currentVertex->g=g_texRectTVtx[0].g;
	currentVertex->b=g_texRectTVtx[0].b;
	currentVertex->a=g_texRectTVtx[0].a;
	currentVertex->u0=g_texRectTVtx[0].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[0].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[0].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[0].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[1].x;
	currentVertex->y=g_texRectTVtx[1].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[1].r;
	currentVertex->g=g_texRectTVtx[1].g;
	currentVertex->b=g_texRectTVtx[1].b;
	currentVertex->a=g_texRectTVtx[1].a;
	currentVertex->u0=g_texRectTVtx[1].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[1].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[1].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[1].tcord[1].v;
	nextVertex();
	
	currentVertex->x=g_texRectTVtx[2].x;
	currentVertex->y=g_texRectTVtx[2].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[2].r;
	currentVertex->g=g_texRectTVtx[2].g;
	currentVertex->b=g_texRectTVtx[2].b;
	currentVertex->a=g_texRectTVtx[2].a;
	currentVertex->u0=g_texRectTVtx[2].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[2].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[2].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[2].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[0].x;
	currentVertex->y=g_texRectTVtx[0].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[0].r;
	currentVertex->g=g_texRectTVtx[0].g;
	currentVertex->b=g_texRectTVtx[0].b;
	currentVertex->a=g_texRectTVtx[0].a;
	currentVertex->u0=g_texRectTVtx[0].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[0].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[0].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[0].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[2].x;
	currentVertex->y=g_texRectTVtx[2].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[2].r;
	currentVertex->g=g_texRectTVtx[2].g;
	currentVertex->b=g_texRectTVtx[2].b;
	currentVertex->a=g_texRectTVtx[2].a;
	currentVertex->u0=g_texRectTVtx[2].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[2].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[2].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[2].tcord[1].v;
	nextVertex();
	
	currentVertex->x=g_texRectTVtx[3].x;
	currentVertex->y=g_texRectTVtx[3].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=g_texRectTVtx[3].r;
	currentVertex->g=g_texRectTVtx[3].g;
	currentVertex->b=g_texRectTVtx[3].b;
	currentVertex->a=g_texRectTVtx[3].a;
	currentVertex->u0=g_texRectTVtx[3].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[3].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[3].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[3].tcord[1].v;
	nextVertex();
	
	drawVB();

#if 0	
    glBegin(GL_TRIANGLE_FAN);
	
    glColor4f(g_texRectTVtx[3].r, g_texRectTVtx[3].g, g_texRectTVtx[3].b, g_texRectTVtx[3].a);
    TexCoord(g_texRectTVtx[3]);
    glVertex3f(g_texRectTVtx[3].x, g_texRectTVtx[3].y, depth);
    
    glColor4f(g_texRectTVtx[2].r, g_texRectTVtx[2].g, g_texRectTVtx[2].b, g_texRectTVtx[2].a);
    TexCoord(g_texRectTVtx[2]);
    glVertex3f(g_texRectTVtx[2].x, g_texRectTVtx[2].y, depth);

    glColor4f(g_texRectTVtx[1].r, g_texRectTVtx[1].g, g_texRectTVtx[1].b, g_texRectTVtx[1].a);
    TexCoord(g_texRectTVtx[1]);
    glVertex3f(g_texRectTVtx[1].x, g_texRectTVtx[1].y, depth);

    glColor4f(g_texRectTVtx[0].r, g_texRectTVtx[0].g, g_texRectTVtx[0].b, g_texRectTVtx[0].a);
    TexCoord(g_texRectTVtx[0]);
    glVertex3f(g_texRectTVtx[0].x, g_texRectTVtx[0].y, depth);

    glEnd();
    OPENGL_CHECK_ERRORS;
#endif
	
	Xe_SetCullMode(xe,xe_cull);

    return true;
}

bool CxeRender::RenderFillRect(uint32 dwColor, float depth)
{
	glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);

	Xe_SetCullMode(xe,XE_CULL_NONE);
	
	u8 r = (u8) ((g_texRectTVtx[0].dcDiffuse>>16)&0xFF);
	u8 g = (u8) ((g_texRectTVtx[0].dcDiffuse>>8)&0xFF);
	u8 b = (u8) (g_texRectTVtx[0].dcDiffuse&0xFF);
	u8 a = (u8) (g_texRectTVtx[0].dcDiffuse >>24);
	
	currentVertex->x=m_fillRectVtx[0].x;
	currentVertex->y=m_fillRectVtx[0].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();

	currentVertex->x=m_fillRectVtx[1].x;
	currentVertex->y=m_fillRectVtx[0].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();
	
	currentVertex->x=m_fillRectVtx[1].x;
	currentVertex->y=m_fillRectVtx[1].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();

	currentVertex->x=m_fillRectVtx[0].x;
	currentVertex->y=m_fillRectVtx[0].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();

	currentVertex->x=m_fillRectVtx[1].x;
	currentVertex->y=m_fillRectVtx[1].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();
	
	currentVertex->x=m_fillRectVtx[0].x;
	currentVertex->y=m_fillRectVtx[1].y;
	currentVertex->z=depth;
	currentVertex->w=1.0;
	currentVertex->r=r;currentVertex->g=g;currentVertex->b=b;currentVertex->a=a;
	currentVertex->u0=currentVertex->u1=currentVertex->v0=currentVertex->v1=0.0;
	nextVertex();
	
	drawVB();

#if 0	
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(r,g,b,a);
    glVertex4f(m_fillRectVtx[0].x, m_fillRectVtx[1].y, depth, 1);
    glVertex4f(m_fillRectVtx[1].x, m_fillRectVtx[1].y, depth, 1);
    glVertex4f(m_fillRectVtx[1].x, m_fillRectVtx[0].y, depth, 1);
    glVertex4f(m_fillRectVtx[0].x, m_fillRectVtx[0].y, depth, 1);
    glEnd();
    OPENGL_CHECK_ERRORS;
#endif

	Xe_SetCullMode(xe,xe_cull);

    return true;
}

bool CxeRender::RenderLine3D()
{
    ApplyZBias(0);  // disable z offsets

#if 0	
    glBegin(GL_TRIANGLE_FAN);

    glColor4f(m_line3DVtx[1].r, m_line3DVtx[1].g, m_line3DVtx[1].b, m_line3DVtx[1].a);
    glVertex3f(m_line3DVector[3].x, m_line3DVector[3].y, -m_line3DVtx[1].z);
    glVertex3f(m_line3DVector[2].x, m_line3DVector[2].y, -m_line3DVtx[0].z);
    
    glColor4ub(m_line3DVtx[0].r, m_line3DVtx[0].g, m_line3DVtx[0].b, m_line3DVtx[0].a);
    glVertex3f(m_line3DVector[1].x, m_line3DVector[1].y, -m_line3DVtx[1].z);
    glVertex3f(m_line3DVector[0].x, m_line3DVector[0].y, -m_line3DVtx[0].z);

    glEnd();
    OPENGL_CHECK_ERRORS;
#endif
	
    ApplyZBias(m_dwZBias);          // set Z offset back to previous value

    return true;
}

extern FiddledVtx * g_pVtxBase;

// This is so weired that I can not do vertex transform by myself. I have to use
// OpenGL internal transform
bool CxeRender::RenderFlushTris()
{
    if( !m_bSupportFogCoordExt )    
        SetFogFlagForNegativeW();
    else
    {
        if( !gRDP.bFogEnableInBlender && gRSP.bFogEnabled )
        {
//            glDisable(GL_FOG);
        }
    }

    ApplyZBias(m_dwZBias);                    // set the bias factors

    glViewportWrapper(windowSetting.vpLeftW, windowSetting.uDisplayHeight-windowSetting.vpTopW-windowSetting.vpHeightW+windowSetting.statusBarHeightToUse, windowSetting.vpWidthW, windowSetting.vpHeightW, false);

	u32 i;
	u32 nvr=gRSP.numVertices;
	
	for(i=0;i<nvr;++i)
	{
#if 0
		currentVertex->x=g_vtxBuffer[i].x;
		currentVertex->y=g_vtxBuffer[i].y;
		currentVertex->z=g_vtxBuffer[i].z;
		currentVertex->w=1.0f;//g_vtxBuffer[i].rhw;
		
		currentVertex->r=g_vtxBuffer[i].r;
		currentVertex->g=g_vtxBuffer[i].g;
		currentVertex->b=g_vtxBuffer[i].b;
		currentVertex->a=g_vtxBuffer[i].a;
#else
		int vi=g_vtxIndex[i];
#if 0
		float invW = (g_vtxProjected5[vi][3] != 0) ? 1/g_vtxProjected5[vi][3] : 0.0f;

		currentVertex->x=g_vtxProjected5[vi][0]*invW;
		currentVertex->y=g_vtxProjected5[vi][1]*invW;
		currentVertex->z=g_vtxProjected5[vi][2]*invW;
		currentVertex->w=1.0f;
#else
		currentVertex->x=g_vtxProjected5[vi][0];
		currentVertex->y=g_vtxProjected5[vi][1];
		currentVertex->z=g_vtxProjected5[vi][2];
		currentVertex->w=g_vtxProjected5[vi][3];
#endif
		
		currentVertex->r=g_oglVtxColors[vi][0];
		currentVertex->g=g_oglVtxColors[vi][1];
		currentVertex->b=g_oglVtxColors[vi][2];
		currentVertex->a=g_oglVtxColors[vi][3];
#endif		
		currentVertex->u0=g_vtxBuffer[i].tcord[0].u;
		currentVertex->v0=g_vtxBuffer[i].tcord[0].v;
		currentVertex->u1=g_vtxBuffer[i].tcord[1].u;
		currentVertex->v1=g_vtxBuffer[i].tcord[1].v;
		
//		printf("%.3f %.3f %.3f %.3f\n",currentVertex->x,currentVertex->y,currentVertex->z,currentVertex->w);
		
		nextVertex();
	}

	drawVB();
	
    if( !m_bSupportFogCoordExt )    
        RestoreFogFlag();
    else
    {
        if( !gRDP.bFogEnableInBlender && gRSP.bFogEnabled )
        {
//            glEnable(GL_FOG);
        }
    }
    return true;
}

void CxeRender::DrawSimple2DTexture(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, COLOR dif, COLOR spe, float z, float rhw)
{
	if( status.bVIOriginIsUpdated == true && currentRomOptions.screenUpdateSetting==SCREEN_UPDATE_AT_1ST_PRIMITIVE )
    {
        status.bVIOriginIsUpdated=false;
        CGraphicsContext::Get()->UpdateFrame();
        DEBUGGER_PAUSE_AND_DUMP_NO_UPDATE(NEXT_SET_CIMG,{DebuggerAppendMsg("Screen Update at 1st Simple2DTexture");});
    }

    StartDrawSimple2DTexture(x0, y0, x1, y1, u0, v0, u1, v1, dif, spe, z, rhw);

	Xe_SetCullMode(xe,XE_CULL_NONE);
	
    glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);

	currentVertex->x=g_texRectTVtx[0].x;
	currentVertex->y=g_texRectTVtx[0].y;
	currentVertex->z=-g_texRectTVtx[0].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[0].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[0].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[0].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[0].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[0].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[1].x;
	currentVertex->y=g_texRectTVtx[1].y;
	currentVertex->z=-g_texRectTVtx[1].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[1].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[1].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[1].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[1].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[1].tcord[1].v;
	nextVertex();
	
	currentVertex->x=g_texRectTVtx[2].x;
	currentVertex->y=g_texRectTVtx[2].y;
	currentVertex->z=-g_texRectTVtx[2].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[2].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[2].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[2].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[2].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[2].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[0].x;
	currentVertex->y=g_texRectTVtx[0].y;
	currentVertex->z=-g_texRectTVtx[0].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[0].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[0].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[0].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[0].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[0].tcord[1].v;
	nextVertex();

	currentVertex->x=g_texRectTVtx[2].x;
	currentVertex->y=g_texRectTVtx[2].y;
	currentVertex->z=-g_texRectTVtx[2].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[2].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[2].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[2].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[2].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[2].tcord[1].v;
	nextVertex();
	
	currentVertex->x=g_texRectTVtx[3].x;
	currentVertex->y=g_texRectTVtx[3].y;
	currentVertex->z=-g_texRectTVtx[3].z;
	currentVertex->w=rhw;
	currentVertex->color=g_texRectTVtx[3].dcDiffuse;
	currentVertex->u0=g_texRectTVtx[3].tcord[0].u;
	currentVertex->v0=g_texRectTVtx[3].tcord[0].v;
	currentVertex->u1=g_texRectTVtx[3].tcord[1].u;
	currentVertex->v1=g_texRectTVtx[3].tcord[1].v;
	nextVertex();
	
	drawVB();
	
#if 0    
	OPENGL_CHECK_ERRORS;

    glBegin(GL_TRIANGLES);
    float a = (g_texRectTVtx[0].dcDiffuse >>24)/255.0f;
    float r = ((g_texRectTVtx[0].dcDiffuse>>16)&0xFF)/255.0f;
    float g = ((g_texRectTVtx[0].dcDiffuse>>8)&0xFF)/255.0f;
    float b = (g_texRectTVtx[0].dcDiffuse&0xFF)/255.0f;
    glColor4f(r,g,b,a);

    CxeRender::TexCoord(g_texRectTVtx[0]);
    glVertex3f(g_texRectTVtx[0].x, g_texRectTVtx[0].y, -g_texRectTVtx[0].z);

    CxeRender::TexCoord(g_texRectTVtx[1]);
    glVertex3f(g_texRectTVtx[1].x, g_texRectTVtx[1].y, -g_texRectTVtx[1].z);

    CxeRender::TexCoord(g_texRectTVtx[2]);
    glVertex3f(g_texRectTVtx[2].x, g_texRectTVtx[2].y, -g_texRectTVtx[2].z);

    CxeRender::TexCoord(g_texRectTVtx[0]);
    glVertex3f(g_texRectTVtx[0].x, g_texRectTVtx[0].y, -g_texRectTVtx[0].z);

    CxeRender::TexCoord(g_texRectTVtx[2]);
    glVertex3f(g_texRectTVtx[2].x, g_texRectTVtx[2].y, -g_texRectTVtx[2].z);

    CxeRender::TexCoord(g_texRectTVtx[3]);
    glVertex3f(g_texRectTVtx[3].x, g_texRectTVtx[3].y, -g_texRectTVtx[3].z);
    
    glEnd();
    OPENGL_CHECK_ERRORS;
#endif
	
	Xe_SetCullMode(xe,xe_cull);
}

void CxeRender::DrawSimpleRect(int nX0, int nY0, int nX1, int nY1, uint32 dwColor, float depth, float rhw)
{
    StartDrawSimpleRect(nX0, nY0, nX1, nY1, dwColor, depth, rhw);

	Xe_SetCullMode(xe,XE_CULL_NONE);

    glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);

	currentVertex->x=m_simpleRectVtx[0].x;
	currentVertex->y=m_simpleRectVtx[0].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();

	currentVertex->x=m_simpleRectVtx[1].x;
	currentVertex->y=m_simpleRectVtx[0].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();
	
	currentVertex->x=m_simpleRectVtx[1].x;
	currentVertex->y=m_simpleRectVtx[1].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();

	currentVertex->x=m_simpleRectVtx[0].x;
	currentVertex->y=m_simpleRectVtx[0].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();

	currentVertex->x=m_simpleRectVtx[1].x;
	currentVertex->y=m_simpleRectVtx[1].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();
	
	currentVertex->x=m_simpleRectVtx[0].x;
	currentVertex->y=m_simpleRectVtx[1].y;
	currentVertex->z=-depth;
	currentVertex->w=rhw;
	currentVertex->color=dwColor;
	nextVertex();
	
	drawVB();

#if 0    
    glBegin(GL_TRIANGLE_FAN);

    float a = (dwColor>>24)/255.0f;
    float r = ((dwColor>>16)&0xFF)/255.0f;
    float g = ((dwColor>>8)&0xFF)/255.0f;
    float b = (dwColor&0xFF)/255.0f;
    glColor4f(r,g,b,a);
    glVertex3f(m_simpleRectVtx[1].x, m_simpleRectVtx[0].y, -depth);
    glVertex3f(m_simpleRectVtx[1].x, m_simpleRectVtx[1].y, -depth);
    glVertex3f(m_simpleRectVtx[0].x, m_simpleRectVtx[1].y, -depth);
    glVertex3f(m_simpleRectVtx[0].x, m_simpleRectVtx[0].y, -depth);
    
    glEnd();
    OPENGL_CHECK_ERRORS;

#endif
	
	Xe_SetCullMode(xe,xe_cull);
}

void CxeRender::DrawText(const char* str, RECT *rect)
{
    return;
}

void CxeRender::DrawSpriteR_Render()    // With Rotation
{
    glViewportWrapper(0, windowSetting.statusBarHeightToUse, windowSetting.uDisplayWidth, windowSetting.uDisplayHeight,true);

	Xe_SetCullMode(xe,XE_CULL_NONE);

#if 0    
    glBegin(GL_TRIANGLES);
    glColor4fv(gRDP.fvPrimitiveColor);

    OGLRender::TexCoord(g_texRectTVtx[0]);
    glVertex3f(g_texRectTVtx[0].x, g_texRectTVtx[0].y, -g_texRectTVtx[0].z);

    OGLRender::TexCoord(g_texRectTVtx[1]);
    glVertex3f(g_texRectTVtx[1].x, g_texRectTVtx[1].y, -g_texRectTVtx[1].z);

    OGLRender::TexCoord(g_texRectTVtx[2]);
    glVertex3f(g_texRectTVtx[2].x, g_texRectTVtx[2].y, -g_texRectTVtx[2].z);

    OGLRender::TexCoord(g_texRectTVtx[0]);
    glVertex3f(g_texRectTVtx[0].x, g_texRectTVtx[0].y, -g_texRectTVtx[0].z);

    OGLRender::TexCoord(g_texRectTVtx[2]);
    glVertex3f(g_texRectTVtx[2].x, g_texRectTVtx[2].y, -g_texRectTVtx[2].z);

    OGLRender::TexCoord(g_texRectTVtx[3]);
    glVertex3f(g_texRectTVtx[3].x, g_texRectTVtx[3].y, -g_texRectTVtx[3].z);

    glEnd();
#endif
	
	Xe_SetCullMode(xe,xe_cull);
}


void CxeRender::DrawObjBGCopy(uObjBg &info)
{
    if( IsUsedAsDI(g_CI.dwAddr) )
    {
        ErrorMsg("Unimplemented: write into Z buffer.  Was mostly commented out in Rice Video 6.1.0");
        return;
    }
    else
    {
        CRender::LoadObjBGCopy(info);
        CRender::DrawObjBGCopy(info);
    }
}


void CxeRender::InitCombinerBlenderForSimpleRectDraw(uint32 tile)
{
    EnableTexUnit(0,FALSE);
    Xe_SetBlendControl(xe,XE_BLEND_SRCALPHA,XE_BLENDOP_ADD,XE_BLEND_INVSRCALPHA,XE_BLEND_SRCALPHA,XE_BLENDOP_ADD,XE_BLEND_INVSRCALPHA);
}

COLOR CxeRender::PostProcessDiffuseColor(COLOR curDiffuseColor)
{
    uint32 color = curDiffuseColor;
    uint32 colorflag = m_pColorCombiner->m_pDecodedMux->m_dwShadeColorChannelFlag;
    uint32 alphaflag = m_pColorCombiner->m_pDecodedMux->m_dwShadeAlphaChannelFlag;
    if( colorflag+alphaflag != MUX_0 )
    {
        if( (colorflag & 0xFFFFFF00) == 0 && (alphaflag & 0xFFFFFF00) == 0 )
        {
            color = (m_pColorCombiner->GetConstFactor(colorflag, alphaflag, curDiffuseColor));
        }
        else
            color = (CalculateConstFactor(colorflag, alphaflag, curDiffuseColor));
    }

    //return (color<<8)|(color>>24);
    return color;
}

COLOR CxeRender::PostProcessSpecularColor()
{
    return 0;
}

void CxeRender::SetViewportRender()
{
    glViewportWrapper(windowSetting.vpLeftW, windowSetting.uDisplayHeight-windowSetting.vpTopW-windowSetting.vpHeightW+windowSetting.statusBarHeightToUse, windowSetting.vpWidthW, windowSetting.vpHeightW,true);
}

void CxeRender::RenderReset()
{
    CRender::RenderReset();

	if (drawPrepared) return;

	if(needSync)
		Xe_Sync(xe); // wait for background render to finish !

    Xe_SetFrameBufferSurface(xe,framebuffer[curFB]);
    curFB=1-curFB;
    Xe_SetRenderTarget(xe,framebuffer[curFB]);
    
	Xe_InvalidateState(xe);

	Xe_SetShader(xe,SHADER_TYPE_PIXEL,sh_ps_fb,0);
	Xe_SetShader(xe,SHADER_TYPE_VERTEX,sh_vs,0);

	drawPrepared=true;
	hadTriangles=false;
	resetLockVB();
}


void CxeRender::SetAlphaTestEnable(BOOL bAlphaTestEnable)
{
#ifdef DEBUGGER
    if( bAlphaTestEnable && debuggerEnableAlphaTest )
#else
    if( bAlphaTestEnable )
#endif
        Xe_SetAlphaTestEnable(xe,1);
    else
        Xe_SetAlphaTestEnable(xe,0);
}

void CxeRender::BindTexture(struct XenosSurface *tex, int unitno)
{

#ifdef DEBUGGER
    if( unitno != 0 )
    {
        DebuggerAppendMsg("Check me, base ogl bind texture, unit no != 0");
    }
#endif
    if( m_curBoundTex[unitno] != tex )
    {
		Xe_SetTexture(xe,unitno,tex);
        m_curBoundTex[unitno] = tex;
    }
}

void CxeRender::DisBindTexture(struct XenosSurface *tex, int unitno)
{
    //EnableTexUnit(0,FALSE);
    //glBindTexture(GL_TEXTURE_2D, 0);  //Not to bind any texture
}

void CxeRender::EnableTexUnit(int unitno, BOOL flag)
{

#ifdef DEBUGGER
    if( unitno != 0 )
    {
        DebuggerAppendMsg("Check me, in the base ogl render, unitno!=0");
    }
#endif
    if( m_texUnitEnabled[unitno] != flag )
    {
        m_texUnitEnabled[unitno] = flag;
        if( flag == TRUE )
	        Xe_SetTexture(xe,unitno,m_curBoundTex[unitno]);
        else
	        Xe_SetTexture(xe,unitno,NULL);
    }
}

void CxeRender::TexCoord2f(float u, float v)
{
//    glTexCoord2f(u, v);
}

void CxeRender::TexCoord(TLITVERTEX &vtxInfo)
{
//    glTexCoord2f(vtxInfo.tcord[0].u, vtxInfo.tcord[0].v);
}

void CxeRender::UpdateScissor()
{
    if( options.bEnableHacks && g_CI.dwWidth == 0x200 && gRDP.scissor.right == 0x200 && g_CI.dwWidth>(*g_GraphicsInfo.VI_WIDTH_REG & 0xFFF) )
    {
        // Hack for RE2
        uint32 width = *g_GraphicsInfo.VI_WIDTH_REG & 0xFFF;
        uint32 height = (gRDP.scissor.right*gRDP.scissor.bottom)/width;
		
		int x,y,w,h;
		
		x=0;
		y=int(height*windowSetting.fMultY+windowSetting.statusBarHeightToUse);
		w=int(width*windowSetting.fMultX);
		h=int(height*windowSetting.fMultY);
		
        Xe_SetScissor(xe,1,x,y,x+w-1,y+h-1);
    }
    else
    {
        UpdateScissorWithClipRatio();
    }
}

void CxeRender::ApplyRDPScissor(bool force)
{
    if( !force && status.curScissor == RDP_SCISSOR )    return;

    if( options.bEnableHacks && g_CI.dwWidth == 0x200 && gRDP.scissor.right == 0x200 && g_CI.dwWidth>(*g_GraphicsInfo.VI_WIDTH_REG & 0xFFF) )
    {
        // Hack for RE2
        uint32 width = *g_GraphicsInfo.VI_WIDTH_REG & 0xFFF;
        uint32 height = (gRDP.scissor.right*gRDP.scissor.bottom)/width;


		int x,y,w,h;
		
		x=0;
		y=int(height*windowSetting.fMultY+windowSetting.statusBarHeightToUse);
		w=int(width*windowSetting.fMultX);
		h=int(height*windowSetting.fMultY);
		
        Xe_SetScissor(xe,1,x,y,x+w-1,y+h-1);
    }
    else
    {
		int x,y,w,h;
		
		x=int(gRDP.scissor.left*windowSetting.fMultX);
		y=int((windowSetting.uViHeight-gRDP.scissor.bottom)*windowSetting.fMultY+windowSetting.statusBarHeightToUse);
		w=int((gRDP.scissor.right-gRDP.scissor.left)*windowSetting.fMultX);
		h=int((gRDP.scissor.bottom-gRDP.scissor.top)*windowSetting.fMultY );
		
        Xe_SetScissor(xe,1,x,y,x+w-1,y+h-1);
    }

    status.curScissor = RDP_SCISSOR;
}

void CxeRender::ApplyScissorWithClipRatio(bool force)
{
    if( !force && status.curScissor == RSP_SCISSOR )    return;

	
	int x,y,w,h;

	x=windowSetting.clipping.left;
	y=int((windowSetting.uViHeight-gRSP.real_clip_scissor_bottom)*windowSetting.fMultY)+windowSetting.statusBarHeightToUse;
	w=windowSetting.clipping.width;
	h=windowSetting.clipping.height;

	Xe_SetScissor(xe,1,x,y,x+w-1,y+h-1);

    status.curScissor = RSP_SCISSOR;
}

void CxeRender::SetFogMinMax(float fMin, float fMax)
{

#if 0
	glFogf(GL_FOG_START, gRSPfFogMin); // Fog Start Depth
    OPENGL_CHECK_ERRORS;
    glFogf(GL_FOG_END, gRSPfFogMax); // Fog End Depth
    OPENGL_CHECK_ERRORS;
#endif
}

void CxeRender::TurnFogOnOff(bool flag)
{

#if 0
    if( flag )
        glEnable(GL_FOG);
    else
        glDisable(GL_FOG);
    OPENGL_CHECK_ERRORS;
#endif
}

void CxeRender::SetFogEnable(bool bEnable)
{
    DEBUGGER_IF_DUMP( (gRSP.bFogEnabled != (bEnable==TRUE) && logFog ), TRACE1("Set Fog %s", bEnable? "enable":"disable"));

    gRSP.bFogEnabled = bEnable;
    

#if 0
    if( gRSP.bFogEnabled )
    {
        //TRACE2("Enable fog, min=%f, max=%f",gRSPfFogMin,gRSPfFogMax );
        glFogfv(GL_FOG_COLOR, gRDP.fvFogColor); // Set Fog Color
        OPENGL_CHECK_ERRORS;
        glFogf(GL_FOG_START, gRSPfFogMin); // Fog Start Depth
        OPENGL_CHECK_ERRORS;
        glFogf(GL_FOG_END, gRSPfFogMax); // Fog End Depth
        OPENGL_CHECK_ERRORS;
        glEnable(GL_FOG);
        OPENGL_CHECK_ERRORS;
    }
    else
    {
        glDisable(GL_FOG);
        OPENGL_CHECK_ERRORS;
    }
#endif
}

void CxeRender::SetFogColor(uint32 r, uint32 g, uint32 b, uint32 a)
{
    gRDP.fogColor = COLOR_RGBA(r, g, b, a); 
    gRDP.fvFogColor[0] = r/255.0f;      //r
    gRDP.fvFogColor[1] = g/255.0f;      //g
    gRDP.fvFogColor[2] = b/255.0f;          //b
    gRDP.fvFogColor[3] = a/255.0f;      //a

#if 0
    glFogfv(GL_FOG_COLOR, gRDP.fvFogColor); // Set Fog Color
    OPENGL_CHECK_ERRORS;
#endif
}

void CxeRender::DisableMultiTexture()
{
//    Xe_SetTexture(xe,0,NULL);
    Xe_SetTexture(xe,1,NULL);
}

void CxeRender::EndRendering(void)
{
    if( CRender::gRenderReferenceCount > 0 ) 
        CRender::gRenderReferenceCount--;
}

void CxeRender::glViewportWrapper(int x, int y, int width, int height, bool ortho)
{
    static int mx=0,my=0;
    static int m_width=0, m_height=0;
    static bool mflag=true;

    if( x!=mx || y!=my || width!=m_width || height!=m_height || mflag!=ortho)
    {
        mx=x;
        my=y;
        m_width=width;
        m_height=height;
        mflag=ortho;
		
//		printf("glViewportWrapper %d %d %d %d %d %d %d\n",x,y,width,height,windowSetting.uDisplayWidth,windowSetting.uDisplayHeight,ortho);
		
		float ortho_matrix[4][4] = {
			{2.0f/windowSetting.uDisplayWidth,0,0,-1},
			{0,-2.0f/windowSetting.uDisplayHeight,0,1},
			{0,0,1/(FAR-NEAR),NEAR/(NEAR-FAR)},
			{0,0,0,1},
		};

		float viewport_matrix[4][4] = {
			{(float)width/windowSetting.uDisplayWidth,0,0,2.0f*x/windowSetting.uDisplayWidth},
			{0,(float)height/windowSetting.uDisplayHeight,0,2.0f*y/windowSetting.uDisplayHeight},
			{0,0,1/(FAR-NEAR),NEAR/(NEAR-FAR)},
			{0,0,0,1},
		};

/*		float proj_matrix[4][4] = {
			{1,0,0,0},
			{0,1,0,0},
			{0,0,1/(FAR-NEAR),NEAR/(NEAR-FAR)},
			{0,0,0,1},
		};
		
		float ident_matrix[4][4] = {
			{1,0,0,0},
			{0,1,0,0},
			{0,0,1,0},
			{0,0,0,1},
		};*/

		if (ortho)
		{
			Xe_SetVertexShaderConstantF(xe,0,(float*)ortho_matrix,4);
		}
		else
		{
			Xe_SetVertexShaderConstantF(xe,0,(float*)viewport_matrix,4);
		}
    }
}

////////////////////////////////////////////////////////////////////////////////
// CxeColorCombiner
////////////////////////////////////////////////////////////////////////////////

CxeColorCombiner::CxeColorCombiner(CRender *pRender)
: CColorCombiner(pRender)
{

    m_pDecodedMux = new DecodedMux;
    m_pDecodedMux->m_maxConstants = 0;
    m_pDecodedMux->m_maxTextures = 2;
	m_pxeRender=(CxeRender*)pRender;
}

CxeColorCombiner::~CxeColorCombiner()
{

}

bool CxeColorCombiner::Initialize(void)
{
  return true;
}

void CxeColorCombiner::InitCombinerCycle12(void)
{
    m_pxeRender->DisableMultiTexture();
    if( !m_bTexelsEnable )
    {
//        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        m_pxeRender->EnableTexUnit(0,FALSE);
        return;
    }

//    uint32 mask = 0x1f;
    CxeTexture* pTexture = g_textures[gRSP.curTile].m_pCxeTexture;
    if( pTexture )
    {
        m_pxeRender->EnableTexUnit(0,TRUE);
        m_pxeRender->BindTexture(pTexture->tex, 0);
        m_pxeRender->SetAllTexelRepeatFlag();
    }
#ifdef _DEBUG
    else
    {
        DebuggerAppendMsg("Check me, texture is NULL");
    }
#endif
}

void CxeColorCombiner::DisableCombiner(void)
{

}

void CxeColorCombiner::InitCombinerCycleCopy(void)
{
    m_pxeRender->DisableMultiTexture();
    m_pxeRender->EnableTexUnit(0,TRUE);
    CxeTexture* pTexture = g_textures[gRSP.curTile].m_pCxeTexture;
    if( pTexture )
    {
        m_pxeRender->BindTexture(pTexture->tex, 0);
        m_pxeRender->SetTexelRepeatFlags(gRSP.curTile);
    }
#ifdef _DEBUG
    else
    {
        DebuggerAppendMsg("Check me, texture is NULL");
    }
#endif
}

void CxeColorCombiner::InitCombinerCycleFill(void)
{
	m_pxeRender->DisableMultiTexture();
	m_pxeRender->EnableTexUnit(0,FALSE);
}

void CxeColorCombiner::InitCombinerBlenderForSimpleTextureDraw(uint32 tile)
{
	m_pxeRender->DisableMultiTexture();
    if( g_textures[tile].m_pCxeTexture )
    {
        m_pxeRender->EnableTexUnit(0,TRUE);

		g_textures[tile].m_pCxeTexture->tex->use_filtering=1;
		g_textures[tile].m_pCxeTexture->tex->u_addressing=XE_TEXADDR_CLAMP;
		g_textures[tile].m_pCxeTexture->tex->v_addressing=XE_TEXADDR_CLAMP;

		Xe_SetTexture(xe,0,g_textures[tile].m_pCxeTexture->tex);
    }
    m_pxeRender->SetAllTexelRepeatFlag();
    m_pxeRender->SetAlphaTestEnable(FALSE);
}

////////////////////////////////////////////////////////////////////////////////
// CxeBlender
////////////////////////////////////////////////////////////////////////////////

uint32 xe_BlendFuncMaps [] =
{
    XE_BLEND_SRCALPHA,         //Nothing
    XE_BLEND_ZERO,             //BLEND_ZERO               = 1,
    XE_BLEND_ONE,              //BLEND_ONE                = 2,
    XE_BLEND_SRCCOLOR,         //BLEND_SRCCOLOR           = 3,
    XE_BLEND_INVSRCCOLOR,      //BLEND_INVSRCCOLOR        = 4,
    XE_BLEND_SRCALPHA,         //BLEND_SRCALPHA           = 5,
    XE_BLEND_INVSRCALPHA,      //BLEND_INVSRCALPHA        = 6,
    XE_BLEND_DESTALPHA,        //BLEND_DESTALPHA          = 7,
    XE_BLEND_INVDESTALPHA,     //BLEND_INVDESTALPHA       = 8,
    XE_BLEND_DESTCOLOR,        //BLEND_DESTCOLOR          = 9,
    XE_BLEND_INVDESTCOLOR,     //BLEND_INVDESTCOLOR       = 10,
    XE_BLEND_SRCALPHASAT,      //BLEND_SRCALPHASAT        = 11,
    XE_BLEND_SRCALPHASAT,      //BLEND_BOTHSRCALPHA       = 12,    
    XE_BLEND_SRCALPHASAT,      //BLEND_BOTHINVSRCALPHA    = 13,
};


void CxeBlender::NormalAlphaBlender(void)
{
	blend_src=XE_BLEND_SRCALPHA;
	blend_dst=XE_BLEND_INVSRCALPHA;
	Enable();
}

void CxeBlender::DisableAlphaBlender(void)
{
	blend_src=XE_BLEND_ONE;
	blend_dst=XE_BLEND_ZERO;
	Disable();
}

void CxeBlender::BlendFunc(uint32 srcFunc, uint32 desFunc)
{
	blend_src=xe_BlendFuncMaps[srcFunc];
	blend_dst=xe_BlendFuncMaps[desFunc];
}

void CxeBlender::Enable()
{
	Xe_SetBlendControl(xe,blend_src,XE_BLENDOP_ADD,blend_dst,blend_src,XE_BLENDOP_ADD,blend_dst);
}

void CxeBlender::Disable()
{
	Xe_SetBlendControl(xe,XE_BLEND_ONE,XE_BLENDOP_ADD,XE_BLEND_ZERO,XE_BLEND_ONE,XE_BLENDOP_ADD,XE_BLEND_ZERO);
}

////////////////////////////////////////////////////////////////////////////////
// CxeTexture
////////////////////////////////////////////////////////////////////////////////

CxeTexture::CxeTexture(uint32 dwWidth, uint32 dwHeight, TextureUsage usage) :
    CTexture(dwWidth,dwHeight,usage)
{
  // Make the width and height be the power of 2
    uint32 w;
    for (w = 1; w < dwWidth; w <<= 1);
    m_dwCreatedTextureWidth = w;
    for (w = 1; w < dwHeight; w <<= 1);
    m_dwCreatedTextureHeight = w;
   
	m_dwCreatedTextureWidth=max(m_dwCreatedTextureWidth,32);
	m_dwCreatedTextureHeight=max(m_dwCreatedTextureHeight,32);
	
    if (dwWidth*dwHeight > 256*256)
        TRACE4("Large texture: (%d x %d), created as (%d x %d)", 
            dwWidth, dwHeight,m_dwCreatedTextureWidth,m_dwCreatedTextureHeight);
    
    m_fYScale = (float)m_dwCreatedTextureHeight/(float)m_dwHeight;
    m_fXScale = (float)m_dwCreatedTextureWidth/(float)m_dwWidth;

	tex=Xe_CreateTexture(xe,m_dwCreatedTextureWidth,m_dwCreatedTextureHeight,0,XE_FMT_8888|XE_FMT_ARGB,0);
	
	m_pTexture = Xe_Surface_LockRect(xe,tex,0,0,0,0,XE_LOCK_WRITE);	
	Xe_Surface_Unlock(xe,tex);
}

CxeTexture::~CxeTexture()
{
	Xe_DestroyTexture(xe,tex);
	tex = NULL;
    m_pTexture = NULL;
    m_dwWidth = 0;
    m_dwHeight = 0;
}

bool CxeTexture::StartUpdate(DrawInfo *di)
{
    di->dwHeight = (uint16)m_dwHeight;
    di->dwWidth = (uint16)m_dwWidth;
    di->dwCreatedHeight = m_dwCreatedTextureHeight;
    di->dwCreatedWidth = m_dwCreatedTextureWidth;
    di->lpSurface = m_pTexture;
    di->lPitch = tex->wpitch;

    return true;
}

void CxeTexture::EndUpdate(DrawInfo *di)
{
	Xe_Surface_LockRect(xe,tex,0,0,0,0,XE_LOCK_WRITE);	
	Xe_Surface_Unlock(xe,tex);
}