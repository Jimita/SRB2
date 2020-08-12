// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2020 by Jaime "Lactozilla" Passos.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_gles2.c
/// \brief OpenGL ES 2.0 API for Sonic Robo Blast 2

#include <stdarg.h>
#include <math.h>

#include "r_gles.h"
#include "../r_opengl/r_vbo.h"

#include "../shaders/gl_shaders.h"

#include "lzml.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

static GLRGBAFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
static GLRGBAFloat black = {0.0f, 0.0f, 0.0f, 1.0f};

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

#define Deg2Rad(x) ((x) * ((float)M_PIl / 180.0f))

// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************

static boolean model_lighting = false;

fmatrix4_t projMatrix;
fmatrix4_t viewMatrix;
fmatrix4_t modelMatrix;

static GLint viewport[4];

// shortcut for ((float)1/i)
#define byte2float(x) (x / 255.0f)

/* Depth Buffer */
typedef void (*PFNglClearDepthf) (GLclampf depth);
static PFNglClearDepthf pglClearDepthf;
typedef void (*PFNglDepthRangef) (GLclampf near_val, GLclampf far_val);
static PFNglDepthRangef pglDepthRangef;

/* Drawing Functions */
typedef void (*PFNglEnableVertexAttribArray) (GLuint index);
static PFNglEnableVertexAttribArray pglEnableVertexAttribArray;
typedef void (*PFNglDisableVertexAttribArray) (GLuint index);
static PFNglDisableVertexAttribArray pglDisableVertexAttribArray;
typedef void (*PFNglGenerateMipmap) (GLenum target);
static PFNglGenerateMipmap pglGenerateMipmap;
typedef void (*PFNglVertexAttribPointer) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
static PFNglVertexAttribPointer pglVertexAttribPointer;

boolean GLBackend_LoadFunctions(void)
{
#define GETOPENGLFUNC(func) \
	p ## gl ## func = GLBackend_GetFunction("gl" #func); \
	if (!(p ## gl ## func)) \
	{ \
		GL_MSG_Error("failed to get OpenGL function: %s", #func); \
		return false; \
	}

	if (!GLBackend_LoadCommonFunctions())
		return false;
	GLBackend_LoadExtraFunctions();

	GETOPENGLFUNC(ClearDepthf)
	GETOPENGLFUNC(DepthRangef)

	Shader_LoadFunctions();
	Shader_Compile();

	return true;
}

boolean GLBackend_LoadExtraFunctions(void)
{
	GETOPENGLFUNC(ActiveTexture)

	GETOPENGLFUNC(GenBuffers)
	GETOPENGLFUNC(BindBuffer)
	GETOPENGLFUNC(BufferData)
	GETOPENGLFUNC(DeleteBuffers)

	GETOPENGLFUNC(EnableVertexAttribArray)
	GETOPENGLFUNC(DisableVertexAttribArray)
	GETOPENGLFUNC(VertexAttribPointer)
	GETOPENGLFUNC(GenerateMipmap)

	return true;
}

#undef GETOPENGLFUNC

// jimita
EXPORT boolean HWRAPI(LoadShaders) (void)
{
	return Shader_Compile();
}

EXPORT void HWRAPI(SetShaderInfo) (hwdshaderinfo_t info, INT32 value)
{
	Shader_SetInfo(info, value);
}

EXPORT void HWRAPI(LoadCustomShader) (int number, char *shader, size_t size, boolean fragment)
{
	Shader_LoadCustom(number, shader, size, fragment);
}

EXPORT boolean HWRAPI(InitCustomShaders) (void)
{
	return Shader_InitCustom();
}

EXPORT void HWRAPI(SetShader) (int shader)
{
	Shader_Set(shader);
}

EXPORT void HWRAPI(UnSetShader) (void)
{
	Shader_UnSet();
}

EXPORT void HWRAPI(KillShaders) (void)
{
	// Nothing
}

// -----------------+
// SetNoTexture     : Disable texture
// -----------------+
static void SetNoTexture(void)
{
	// Disable texture.
	if (tex_downloaded != NOTEXTURE_NUM)
	{
		if (NOTEXTURE_NUM == 0)
		{
			// Generate a 1x1 white pixel as the blank texture
			// (funny how something like this actually used to be here before)
			UINT8 whitepixel[4] = {255, 255, 255, 255};
			pglGenTextures(1, &NOTEXTURE_NUM);
			pglBindTexture(GL_TEXTURE_2D, NOTEXTURE_NUM);
			pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitepixel);
		}
		else
			pglBindTexture(GL_TEXTURE_2D, NOTEXTURE_NUM);

		tex_downloaded = NOTEXTURE_NUM;
	}
}

static void GLPerspective(GLfloat fovy, GLfloat aspect)
{
	fmatrix4_t perspectiveMatrix;
	lzml_matrix4_perspective(perspectiveMatrix, Deg2Rad((float)fovy), (float)aspect, NEAR_CLIPPING_PLANE, FAR_CLIPPING_PLANE);
	lzml_matrix4_multiply(projMatrix, perspectiveMatrix);
}

static void GLProject(GLfloat objX, GLfloat objY, GLfloat objZ,
                      GLfloat* winX, GLfloat* winY, GLfloat* winZ)
{
	GLfloat in[4], out[4];
	int i;

	for (i=0; i<4; i++)
	{
		out[i] =
			objX * modelMatrix[0][i] +
			objY * modelMatrix[1][i] +
			objZ * modelMatrix[2][i] +
			modelMatrix[3][i];
	}
	for (i=0; i<4; i++)
	{
		in[i] =
			out[0] * projMatrix[0][i] +
			out[1] * projMatrix[1][i] +
			out[2] * projMatrix[2][i] +
			out[3] * projMatrix[3][i];
	}
	if (fpclassify(in[3]) == FP_ZERO) return;
	in[0] /= in[3];
	in[1] /= in[3];
	in[2] /= in[3];
	/* Map x, y and z to range 0-1 */
	in[0] = in[0] * 0.5f + 0.5f;
	in[1] = in[1] * 0.5f + 0.5f;
	in[2] = in[2] * 0.5f + 0.5f;

	/* Map x,y to viewport */
	in[0] = in[0] * viewport[2] + viewport[0];
	in[1] = in[1] * viewport[3] + viewport[1];

	*winX=in[0];
	*winY=in[1];
	*winZ=in[2];
}

// -----------------+
// SetModelView     :
// -----------------+
void SetModelView(GLint w, GLint h)
{
//	GL_DBG_Printf("SetModelView(): %dx%d\n", (int)w, (int)h);

	// The screen textures need to be flushed if the width or height change so that they be remade for the correct size
	if (screen_width != w || screen_height != h)
		FlushScreenTextures();

	screen_width = w;
	screen_height = h;

	pglViewport(0, 0, w, h);

	lzml_matrix4_identity(projMatrix);
	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	Shader_SetTransform();
}


// -----------------+
// SetStates        : Set permanent states
// -----------------+
void SetStates(void)
{
//	GL_DBG_Printf("SetStates()\n");

	pglAlphaFunc(GL_NOTEQUAL, 0.0f);

	pglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	pglEnable(GL_DEPTH_TEST);    // check the depth buffer
	pglDepthMask(GL_TRUE);             // enable writing to depth buffer
	pglClearDepthf(1.0f);
	pglDepthRangef(0.0f, 1.0f);
	pglDepthFunc(GL_LEQUAL);

	pglEnable(GL_BLEND);
	pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// this set CurrentPolyFlags to the actual configuration
	CurrentPolyFlags = 0xffffffff;
	SetBlend(0);

	if (shader_current)
	{
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_POSITION));
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	}

	tex_downloaded = 0;
	SetNoTexture();
}


// -----------------+
// Init             : Initialise the OpenGL interface API
// Returns          :
// -----------------+
EXPORT boolean HWRAPI(Init) (void)
{
	return GLBackend_Init();
}


// -----------------+
// ClearMipMapCache : Flush OpenGL textures from memory
// -----------------+
EXPORT void HWRAPI(ClearMipMapCache) (void)
{
	// GL_DBG_Printf ("HWR_Flush(exe)\n");
	GLTexture_Flush();
}


// -----------------+
// ReadRect         : Read a rectangle region of the truecolor framebuffer
//                  : store pixels as 16bit 565 RGB
// Returns          : 16bit 565 RGB pixel array stored in dst_data
// -----------------+
EXPORT void HWRAPI(ReadRect) (INT32 x, INT32 y, INT32 width, INT32 height,
                                INT32 dst_stride, UINT16 * dst_data)
{
	INT32 i;
	GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (height - 1);
	GLubyte *row = malloc(dst_stride);
	// DBG_Printf ("ReadRect()\n");
	if (!row) return;
	pglPixelStorei(GL_PACK_ALIGNMENT, 1);
	pglReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst_data);
	pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for(i = 0; i < height/2; i++)
	{
		memcpy(row, top, dst_stride);
		memcpy(top, bottom, dst_stride);
		memcpy(bottom, row, dst_stride);
		top += dst_stride;
		bottom -= dst_stride;
	}
	free(row);
}


// -----------------+
// GClipRect        : Defines the 2D hardware clipping window
// -----------------+
EXPORT void HWRAPI(GClipRect) (INT32 minx, INT32 miny, INT32 maxx, INT32 maxy, float nearclip)
{
	// GL_DBG_Printf ("GClipRect(%d, %d, %d, %d)\n", minx, miny, maxx, maxy);

	pglViewport(minx, screen_height-maxy, maxx-minx, maxy-miny);
	NEAR_CLIPPING_PLANE = nearclip;

	lzml_matrix4_identity(projMatrix);
	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	Shader_SetTransform();
}


// -----------------+
// ClearBuffer      : Clear the color/alpha/depth buffer(s)
// -----------------+
EXPORT void HWRAPI(ClearBuffer) (FBOOLEAN ColorMask,
                                    FBOOLEAN DepthMask,
                                    FRGBAFloat * ClearColor)
{
	// GL_DBG_Printf ("ClearBuffer(%d)\n", alpha);
	GLbitfield ClearMask = 0;

	if (ColorMask)
	{
		if (ClearColor)
			pglClearColor(ClearColor->red,
			              ClearColor->green,
			              ClearColor->blue,
			              ClearColor->alpha);
		ClearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (DepthMask)
	{
		pglClearDepthf(1.0f);     //Hurdler: all that are permanen states
		pglDepthRangef(0.0f, 1.0f);
		pglDepthFunc(GL_LEQUAL);
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);

	pglClear(ClearMask);

	if (shader_current)
	{
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_POSITION));
		pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	}
}


// -----------------+
// HWRAPI Draw2DLine: Render a 2D line
// -----------------+
EXPORT void HWRAPI(Draw2DLine) (F2DCoord * v1,
                                   F2DCoord * v2,
                                   RGBA_t Color)
{
	// GL_DBG_Printf ("DrawLine() (%f %f %f) %d\n", v1->x, -v1->y, -v1->z, v1->argb);
	GLfloat p[12];
	GLfloat dx, dy;
	GLfloat angle;

	GLRGBAFloat fcolor = {byte2float(Color.s.red), byte2float(Color.s.green), byte2float(Color.s.blue), byte2float(Color.s.alpha)};

	if (shader_current == NULL)
		return;

	SetNoTexture();

	// This is the preferred, 'modern' way of rendering lines -- creating a polygon.
	if (fabsf(v2->x - v1->x) > FLT_EPSILON)
		angle = (float)atan((v2->y-v1->y)/(v2->x-v1->x));
	else
		angle = (float)N_PI_DEMI;
	dx = (float)sin(angle) / (float)screen_width;
	dy = (float)cos(angle) / (float)screen_height;

	p[0] = v1->x - dx;  p[1] = -(v1->y + dy); p[2] = 1;
	p[3] = v2->x - dx;  p[4] = -(v2->y + dy); p[5] = 1;
	p[6] = v2->x + dx;  p[7] = -(v2->y - dy); p[8] = 1;
	p[9] = v1->x + dx;  p[10] = -(v1->y - dy); p[11] = 1;

	Shader_SetUniforms(NULL, &fcolor, NULL, NULL);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void Clamp2D(GLenum pname)
{
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP_TO_EDGE);
}


// -----------------+
// SetBlend         : Set render mode
// -----------------+
// PF_Masked - we could use an ALPHA_TEST of GL_EQUAL, and alpha ref of 0,
//             is it faster when pixels are discarded ?
EXPORT void HWRAPI(SetBlend) (FBITFIELD PolyFlags)
{
	FBITFIELD Xor;
	Xor = CurrentPolyFlags^PolyFlags;
	if (Xor & (PF_Blending|PF_RemoveYWrap|PF_ForceWrapX|PF_ForceWrapY|PF_Occlude|PF_NoTexture|PF_Modulated|PF_NoDepthTest|PF_Decal|PF_Invisible))
	{
		if (Xor&(PF_Blending)) // if blending mode must be changed
		{
			switch (PolyFlags & PF_Blending) {
				case PF_Translucent & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // alpha = level of transparency
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Masked & PF_Blending:
					// Hurdler: does that mean lighting is only made by alpha src?
					// it sounds ok, but not for polygonsmooth
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);                // 0 alpha = holes in texture
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
				case PF_Additive & PF_Blending:
					pglBlendFunc(GL_SRC_ALPHA, GL_ONE);                 // src * alpha + dest
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Environment & PF_Blending:
					pglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Substractive & PF_Blending:
					// good for shadow
					// not really but what else ?
					pglBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
					pglAlphaFunc(GL_NOTEQUAL, 0.0f);
					break;
				case PF_Fog & PF_Fog:
					// Sryder: Fog
					// multiplies input colour by input alpha, and destination colour by input colour, then adds them
					pglBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
					pglAlphaFunc(GL_ALWAYS, 0.0f); // Don't discard zero alpha fragments
					break;
				default : // must be 0, otherwise it's an error
					// No blending
					pglBlendFunc(GL_ONE, GL_ZERO);   // the same as no blending
					pglAlphaFunc(GL_GREATER, 0.5f);
					break;
			}
		}

		if (Xor & PF_Decal)
		{
			if (PolyFlags & PF_Decal)
				pglEnable(GL_POLYGON_OFFSET_FILL);
			else
				pglDisable(GL_POLYGON_OFFSET_FILL);
		}

		if (Xor&PF_NoDepthTest)
		{
			if (PolyFlags & PF_NoDepthTest)
				pglDepthFunc(GL_ALWAYS);
			else
				pglDepthFunc(GL_LEQUAL);
		}

		if (Xor&PF_RemoveYWrap)
		{
			if (PolyFlags & PF_RemoveYWrap)
				Clamp2D(GL_TEXTURE_WRAP_T);
		}

		if (Xor&PF_ForceWrapX)
		{
			if (PolyFlags & PF_ForceWrapX)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		}

		if (Xor&PF_ForceWrapY)
		{
			if (PolyFlags & PF_ForceWrapY)
				pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}

		if (Xor & PF_Occlude) // depth test but (no) depth write
		{
			if (PolyFlags&PF_Occlude)
				pglDepthMask(1);
			else
				pglDepthMask(0);
		}

		if (Xor & PF_Invisible)
		{
			if (PolyFlags&PF_Invisible)
				pglBlendFunc(GL_ZERO, GL_ONE);         // transparent blending
			else
			{   // big hack: (TODO: manage that better)
				// we test only for PF_Masked because PF_Invisible is only used
				// (for now) with it (yeah, that's crappy, sorry)
				if ((PolyFlags&PF_Blending)==PF_Masked)
					pglBlendFunc(GL_SRC_ALPHA, GL_ZERO);
			}
		}
		if (PolyFlags & PF_NoTexture)
		{
			SetNoTexture();
		}
	}
	CurrentPolyFlags = PolyFlags;
}

// -----------------+
// UpdateTexture    : Updates the texture data.
// -----------------+
EXPORT void HWRAPI(UpdateTexture) (FTextureInfo *pTexInfo)
{
	// Download a mipmap
	boolean updatemipmap = true;
	static RGBA_t   tex[2048*2048];
	const GLvoid   *ptex = tex;
	INT32             w, h;
	GLuint texnum = 0;

	if (!pTexInfo->downloaded)
	{
		pglGenTextures(1, &texnum);
		pTexInfo->downloaded = texnum;
		updatemipmap = false;
	}
	else
		texnum = pTexInfo->downloaded;

	//GL_DBG_Printf ("DownloadMipmap %d %x\n",(INT32)texnum,pTexInfo->data);

	w = pTexInfo->width;
	h = pTexInfo->height;

	if ((pTexInfo->format == GL_TEXFMT_P_8) ||
		(pTexInfo->format == GL_TEXFMT_AP_88))
	{
		const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
		INT32 i, j;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				if ((*pImgData == HWR_PATCHES_CHROMAKEY_COLORINDEX) &&
					(pTexInfo->flags & TF_CHROMAKEYED))
				{
					tex[w*j+i].s.red   = 0;
					tex[w*j+i].s.green = 0;
					tex[w*j+i].s.blue  = 0;
					tex[w*j+i].s.alpha = 0;
					pTexInfo->flags |= TF_TRANSPARENT; // there is a hole in it
				}
				else
				{
					tex[w*j+i].s.red   = myPaletteData[*pImgData].s.red;
					tex[w*j+i].s.green = myPaletteData[*pImgData].s.green;
					tex[w*j+i].s.blue  = myPaletteData[*pImgData].s.blue;
					tex[w*j+i].s.alpha = myPaletteData[*pImgData].s.alpha;
				}

				pImgData++;

				if (pTexInfo->format == GL_TEXFMT_AP_88)
				{
					if (!(pTexInfo->flags & TF_CHROMAKEYED))
						tex[w*j+i].s.alpha = *pImgData;
					pImgData++;
				}

			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_RGBA)
	{
		// corona test : passed as ARGB 8888, which is not in glide formats
		// Hurdler: not used for coronas anymore, just for dynamic lighting
		ptex = pTexInfo->data;
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_INTENSITY_88)
	{
		const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
		INT32 i, j;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = *pImgData;
				tex[w*j+i].s.green = *pImgData;
				tex[w*j+i].s.blue  = *pImgData;
				pImgData++;
				tex[w*j+i].s.alpha = *pImgData;
				pImgData++;
			}
		}
	}
	else if (pTexInfo->format == GL_TEXFMT_ALPHA_8) // Used for fade masks
	{
		const GLubyte *pImgData = (const GLubyte *)pTexInfo->data;
		INT32 i, j;

		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				tex[w*j+i].s.red   = *pImgData;
				tex[w*j+i].s.green = 255;
				tex[w*j+i].s.blue  = 255;
				tex[w*j+i].s.alpha = 255;
				pImgData++;
			}
		}
	}
	else
		GL_MSG_Warning ("SetTexture(bad format) %ld\n", pTexInfo->format);

	// the texture number was already generated by pglGenTextures
	pglBindTexture(GL_TEXTURE_2D, texnum);
	tex_downloaded = texnum;

	// disable texture filtering on any texture that has holes so there's no dumb borders or blending issues
	if (pTexInfo->flags & TF_TRANSPARENT)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	}

	if (pTexInfo->flags & TF_WRAPX)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	else
		Clamp2D(GL_TEXTURE_WRAP_S);

	if (pTexInfo->flags & TF_WRAPY)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	else
		Clamp2D(GL_TEXTURE_WRAP_T);

	if (maximumAnisotropy)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropic_filter);

	if (updatemipmap)
		pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
	else
		pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

	if (MipMap)
		pglGenerateMipmap(GL_TEXTURE_2D);
}

// -----------------+
// SetTexture       : The mipmap becomes the current texture source
// -----------------+
EXPORT void HWRAPI(SetTexture) (FTextureInfo *pTexInfo)
{
	if (!pTexInfo)
	{
		SetNoTexture();
		return;
	}
	else if (pTexInfo->downloaded)
	{
		if (pTexInfo->downloaded != tex_downloaded)
		{
			pglBindTexture(GL_TEXTURE_2D, pTexInfo->downloaded);
			tex_downloaded = pTexInfo->downloaded;
		}
	}
	else
	{
		UpdateTexture(pTexInfo);
		pTexInfo->nextmipmap = NULL;
		if (gl_cachetail)
		{ // insertion at the tail
			gl_cachetail->nextmipmap = pTexInfo;
			gl_cachetail = pTexInfo;
		}
		else // initialization of the linked list
			gl_cachetail = gl_cachehead =  pTexInfo;
	}
}

// code that is common between DrawPolygon and DrawIndexedTriangles
// the corona thing is there too, i have no idea if that stuff works with DrawIndexedTriangles and batching
static void PreparePolygon(FSurfaceInfo *pSurf, FOutVector *pOutVerts, FBITFIELD PolyFlags)
{
	GLRGBAFloat poly = {1.0f, 1.0f, 1.0f, 1.0f};
	GLRGBAFloat tint = {1.0f, 1.0f, 1.0f, 1.0f};
	GLRGBAFloat fade = {1.0f, 1.0f, 1.0f, 1.0f};

	GLRGBAFloat *c_poly = NULL, *c_tint = NULL, *c_fade = NULL;
	boolean modulated;

	if (PolyFlags & PF_Corona)
		PolyFlags &= ~(PF_NoDepthTest|PF_Corona);

	SetBlend(PolyFlags);    //TODO: inline (#pragma..)
	modulated = (CurrentPolyFlags & PF_Modulated);

	// If Modulated, mix the surface colour to the texture
	if (pSurf && modulated)
	{
		// Poly color
		poly.red   = byte2float(pSurf->PolyColor.s.red);
		poly.green = byte2float(pSurf->PolyColor.s.green);
		poly.blue  = byte2float(pSurf->PolyColor.s.blue);
		poly.alpha = byte2float(pSurf->PolyColor.s.alpha);

		// Tint color
		tint.red   = byte2float(pSurf->TintColor.s.red);
		tint.green = byte2float(pSurf->TintColor.s.green);
		tint.blue  = byte2float(pSurf->TintColor.s.blue);
		tint.alpha = byte2float(pSurf->TintColor.s.alpha);

		// Fade color
		fade.red   = byte2float(pSurf->FadeColor.s.red);
		fade.green = byte2float(pSurf->FadeColor.s.green);
		fade.blue  = byte2float(pSurf->FadeColor.s.blue);
		fade.alpha = byte2float(pSurf->FadeColor.s.alpha);

		c_poly = &poly;
		c_tint = &tint;
		c_fade = &fade;
	}
	else
		c_poly = &white;

	Shader_SetUniforms(pSurf, c_poly, c_tint, c_fade);
}

// -----------------+
// DrawPolygon      : Render a polygon, set the texture, set render mode
// -----------------+
EXPORT void HWRAPI(DrawPolygon) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags)
{
	if (shader_current == NULL)
		return;

	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglBindBuffer(GL_ARRAY_BUFFER, 0);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

	pglDrawArrays(GL_TRIANGLE_FAN, 0, iNumPts);

	if (PolyFlags & PF_RemoveYWrap)
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	if (PolyFlags & PF_ForceWrapX)
		Clamp2D(GL_TEXTURE_WRAP_S);

	if (PolyFlags & PF_ForceWrapY)
		Clamp2D(GL_TEXTURE_WRAP_T);
}

EXPORT void HWRAPI(DrawIndexedTriangles) (FSurfaceInfo *pSurf, FOutVector *pOutVerts, FUINT iNumPts, FBITFIELD PolyFlags, UINT32 *IndexArray)
{
	if (shader_current == NULL)
		return;

	PreparePolygon(pSurf, pOutVerts, PolyFlags);

	pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

	pglDrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

static const boolean gl_ext_arb_vertex_buffer_object = true;

#define NULL_VBO_VERTEX ((gl_skyvertex_t*)NULL)
#define sky_vbo_x (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->x : &sky->data[0].x)
#define sky_vbo_u (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->u : &sky->data[0].u)
#define sky_vbo_r (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->r : &sky->data[0].r)

EXPORT void HWRAPI(RenderSkyDome) (gl_sky_t *sky)
{
	int i, j;

	fvector3_t scale;
	scale[0] = scale[2] = 1.0f;

	Shader_SetUniforms(NULL, NULL, NULL, NULL);

	// Build the sky dome! Yes!
	if (sky->rebuild)
	{
		// delete VBO when already exists
		if (gl_ext_arb_vertex_buffer_object)
		{
			if (sky->vbo)
				pglDeleteBuffers(1, &sky->vbo);
		}

		if (gl_ext_arb_vertex_buffer_object)
		{
			// generate a new VBO and get the associated ID
			pglGenBuffers(1, &sky->vbo);

			// bind VBO in order to use
			pglBindBuffer(GL_ARRAY_BUFFER, sky->vbo);

			// upload data to VBO
			pglBufferData(GL_ARRAY_BUFFER, sky->vertex_count * sizeof(sky->data[0]), sky->data, GL_STATIC_DRAW);
		}

		sky->rebuild = false;
	}

	if (shader_current == NULL)
	{
		if (gl_ext_arb_vertex_buffer_object)
			pglBindBuffer(GL_ARRAY_BUFFER, 0);
		return;
	}

	// bind VBO in order to use
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

	// activate and specify pointers to arrays
	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_x);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_u);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_COLORS), 4, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_r);

	// set transforms
	scale[1] = ((float)texh / 230.0f);
	lzml_matrix4_scale(viewMatrix, scale);
	lzml_matrix4_rotate_y(viewMatrix, Deg2Rad(270.0f));

	Shader_SetTransform();

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < sky->loopcount; i++)
		{
			gl_skyloopdef_t *loop = &sky->loops[i];
			unsigned int mode = 0;

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			switch (loop->mode)
			{
				case HWD_SKYLOOP_FAN:
					mode = GL_TRIANGLE_FAN;
					break;
				case HWD_SKYLOOP_STRIP:
					mode = GL_TRIANGLE_STRIP;
					break;
				default:
					continue;
			}

			pglDrawArrays(mode, loop->vertexindex, loop->vertexcount);
		}
	}

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));

	// bind with 0, so, switch back to normal pointer operation
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, 0);
}


// ==========================================================================
//
// ==========================================================================
EXPORT void HWRAPI(SetSpecialState) (hwdspecialstate_t IdState, INT32 Value)
{
	switch (IdState)
	{
		case HWD_SET_MODEL_LIGHTING:
			model_lighting = Value;
			break;

		case HWD_SET_SHADERS:
			switch (Value)
			{
				case 1:
					gl_shadersenabled = true;
					break;
				default:
					gl_shadersenabled = false;
					break;
			}
			break;

		case HWD_SET_TEXTUREFILTERMODE:
			GLTexture_SetFilterMode(Value);
			GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		case HWD_SET_TEXTUREANISOTROPICMODE:
			anisotropic_filter = min(Value,maximumAnisotropy);
			if (maximumAnisotropy)
				GLTexture_Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		default:
			break;
	}
}

EXPORT void HWRAPI(CreateModelVBOs) (model_t *model)
{
	GLModel_GenerateVBOs(model);
}

#define BUFFER_OFFSET(i) ((void*)(i))

static void DrawModelEx(model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	static GLRGBAFloat poly = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat tint = {1.0f, 1.0f, 1.0f, 1.0f};
	static GLRGBAFloat fade = {1.0f, 1.0f, 1.0f, 1.0f};

	float pol = 0.0f;

	boolean useTinyFrames;

	fvector3_t v_scale;
	fvector3_t translate;

	int i;

	if (shader_current == NULL)
		return;

	// Affect input model scaling
	scale *= 0.5f;
	v_scale[0] = v_scale[1] = v_scale[2] = scale;

	if (duration != 0 && duration != -1 && tics != -1) // don't interpolate if instantaneous or infinite in length
	{
		UINT32 newtime = (duration - tics); // + 1;

		pol = (newtime)/(float)duration;

		if (pol > 1.0f)
			pol = 1.0f;

		if (pol < 0.0f)
			pol = 0.0f;
	}

	poly.red   = byte2float(Surface->PolyColor.s.red);
	poly.green = byte2float(Surface->PolyColor.s.green);
	poly.blue  = byte2float(Surface->PolyColor.s.blue);
	poly.alpha = byte2float(Surface->PolyColor.s.alpha);

	SetBlend((poly.alpha < 1 ? PF_Translucent : (PF_Masked|PF_Occlude))|PF_Modulated);

	tint.red   = byte2float(Surface->TintColor.s.red);
	tint.green = byte2float(Surface->TintColor.s.green);
	tint.blue  = byte2float(Surface->TintColor.s.blue);
	tint.alpha = byte2float(Surface->TintColor.s.alpha);

	fade.red   = byte2float(Surface->FadeColor.s.red);
	fade.green = byte2float(Surface->FadeColor.s.green);
	fade.blue  = byte2float(Surface->FadeColor.s.blue);
	fade.alpha = byte2float(Surface->FadeColor.s.alpha);

	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_NORMAL));

	Shader_SetUniforms(Surface, &poly, &tint, &fade);

	pglEnable(GL_CULL_FACE);

#ifdef USE_FTRANSFORM_MIRROR
	// flipped is if the object is vertically flipped
	// hflipped is if the object is horizontally flipped
	// pos->flip is if the screen is flipped vertically
	// pos->mirror is if the screen is flipped horizontally
	// XOR all the flips together to figure out what culling to use!
	{
		boolean reversecull = (flipped ^ hflipped ^ pos->flip ^ pos->mirror);
		if (reversecull)
			pglCullFace(GL_FRONT);
		else
			pglCullFace(GL_BACK);
	}
#else
	// pos->flip is if the screen is flipped too
	if (flipped ^ hflipped ^ pos->flip) // If one or three of these are active, but not two, invert the model's culling
	{
		pglCullFace(GL_FRONT);
	}
	else
	{
		pglCullFace(GL_BACK);
	}
#endif

	lzml_matrix4_identity(modelMatrix);

	translate[0] = pos->x;
	translate[1] = pos->z;
	translate[2] = pos->y;
	lzml_matrix4_translate(modelMatrix, translate);

	if (flipped)
		v_scale[1] = -v_scale[1];
	if (hflipped)
		v_scale[2] = -v_scale[2];

	if (pos->roll)
	{
		float roll = (1.0f * pos->rollflip);
		fvector3_t rotate;

		translate[0] = pos->centerx;
		translate[1] = pos->centery;
		translate[2] = 0.0f;
		lzml_matrix4_translate(modelMatrix, translate);

		rotate[0] = rotate[1] = rotate[2] = 0.0f;

		if (pos->rotaxis == 2) // Z
			rotate[2] = roll;
		else if (pos->rotaxis == 1) // Y
			rotate[1] = roll;
		else // X
			rotate[0] = roll;

		lzml_matrix4_rotate_by_vector(modelMatrix, rotate, Deg2Rad(pos->rollangle));

		translate[0] = -translate[0];
		translate[1] = -translate[1];
		lzml_matrix4_translate(modelMatrix, translate);
	}

#ifdef USE_FTRANSFORM_ANGLEZ
	lzml_matrix4_rotate_z(modelMatrix, -Deg2Rad(pos->anglez)); // rotate by slope from Kart
#endif
	lzml_matrix4_rotate_y(modelMatrix, -Deg2Rad(pos->angley));
	lzml_matrix4_rotate_x(modelMatrix, Deg2Rad(pos->anglex));

	lzml_matrix4_scale(modelMatrix, v_scale);

	useTinyFrames = (model->meshes[0].tinyframes != NULL);
	if (useTinyFrames)
	{
		v_scale[0] = v_scale[1] = v_scale[2] = (1 / 64.0f);
		lzml_matrix4_scale(modelMatrix, v_scale);
	}

	Shader_SetTransform();

	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (useTinyFrames)
		{
			tinyframe_t *frame = &mesh->tinyframes[frameIndex % mesh->numFrames];
			tinyframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->tinyframes[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_SHORT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(0));
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_BYTE, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));

				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_SHORT, GL_FALSE, 0, vertTinyBuffer);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_BYTE, GL_FALSE, 0, normTinyBuffer);

				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
			}
		}
		else
		{
			mdlframe_t *frame = &mesh->frames[frameIndex % mesh->numFrames];
			mdlframe_t *nextframe = NULL;

			if (nextFrameIndex != -1)
				nextframe = &mesh->frames[nextFrameIndex % mesh->numFrames];

			if (!nextframe || fpclassify(pol) == FP_ZERO)
			{
				// Zoom! Take advantage of just shoving the entire arrays to the GPU.
				pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(0));
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numTriangles * 3);

				// No tinyframes, no mesh indices
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				float *vertPtr;
				float *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				GLModel_AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, vertBuffer);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(Shader_AttribLoc(LOC_NORMAL), 3, GL_FLOAT, GL_FALSE, 0, normBuffer);

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	lzml_matrix4_identity(modelMatrix);
	Shader_SetTransform();

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_NORMAL));

	pglDisable(GL_CULL_FACE);
}

// -----------------+
// HWRAPI DrawModel : Draw a model
// -----------------+
EXPORT void HWRAPI(DrawModel) (model_t *model, INT32 frameIndex, INT32 duration, INT32 tics, INT32 nextFrameIndex, FTransform *pos, float scale, UINT8 flipped, UINT8 hflipped, FSurfaceInfo *Surface)
{
	DrawModelEx(model, frameIndex, duration, tics, nextFrameIndex, pos, scale, flipped, hflipped, Surface);
}

// -----------------+
// SetTransform     :
// -----------------+
EXPORT void HWRAPI(SetTransform) (FTransform *stransform)
{
	static boolean special_splitscreen;
	boolean shearing = false;
	float used_fov;

	fvector3_t scale;

	lzml_matrix4_identity(viewMatrix);
	lzml_matrix4_identity(modelMatrix);

	if (stransform)
	{
		used_fov = stransform->fovxangle;

#ifdef USE_FTRANSFORM_MIRROR
		// mirroring from Kart
		if (stransform->mirror)
		{
			scale[0] = -stransform->scalex;
			scale[1] = -stransform->scaley;
			scale[2] = -stransform->scalez;
		}
		else
#endif
		if (stransform->flip)
		{
			scale[0] = stransform->scalex;
			scale[1] = -stransform->scaley;
			scale[2] = -stransform->scalez;
		}
		else
		{
			scale[0] = stransform->scalex;
			scale[1] = stransform->scaley;
			scale[2] = -stransform->scalez;
		}

		lzml_matrix4_scale(viewMatrix, scale);

		if (stransform->roll)
			lzml_matrix4_rotate_z(viewMatrix, Deg2Rad(stransform->rollangle));
		lzml_matrix4_rotate_x(viewMatrix, Deg2Rad(stransform->anglex));
		lzml_matrix4_rotate_y(viewMatrix, Deg2Rad(stransform->angley + 270.0f));

		lzml_matrix4_translate_x(viewMatrix, -stransform->x);
		lzml_matrix4_translate_y(viewMatrix, -stransform->z);
		lzml_matrix4_translate_z(viewMatrix, -stransform->y);

		special_splitscreen = stransform->splitscreen;
		shearing = stransform->shearing;
	}
	else
		used_fov = fov;

	lzml_matrix4_identity(projMatrix);

	if (stransform)
	{
		// jimita 14042019
		// Simulate Software's y-shearing
		// https://zdoom.org/wiki/Y-shearing
		if (shearing)
		{
			float fdy = stransform->viewaiming * 2;
			if (stransform->flip)
				fdy *= -1.0f;
			lzml_matrix4_translate_y(projMatrix, (-fdy / BASEVIDHEIGHT));
		}

		if (special_splitscreen)
		{
			used_fov = atan(tan(used_fov*M_PI/360)*0.8)*360/M_PI;
			GLPerspective(used_fov, 2*ASPECT_RATIO);
		}
		else
			GLPerspective(used_fov, ASPECT_RATIO);
	}

	Shader_SetTransform();
}

EXPORT INT32 HWRAPI(GetTextureUsed) (void)
{
	return GLTexture_GetMemoryUsage(gl_cachehead);
}

EXPORT void HWRAPI(PostImgRedraw) (float points[SCREENVERTS][SCREENVERTS][2])
{
	INT32 x, y;
	float float_x, float_y, float_nextx, float_nexty;
	float xfix, yfix;
	INT32 texsize = 2048;

	const float blackBack[16] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	if (shader_current == NULL)
		return;

	// Use a power of two texture, dammit
	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	// X/Y stretch fix for all resolutions(!)
	xfix = (float)(texsize)/((float)((screen_width)/(float)(SCREENVERTS-1)));
	yfix = (float)(texsize)/((float)((screen_height)/(float)(SCREENVERTS-1)));

	pglDisable(GL_DEPTH_TEST);
	pglDisable(GL_BLEND);

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	Shader_SetUniforms(NULL, &black, NULL, NULL);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD));
	Shader_SetUniforms(NULL, &white, NULL, NULL);

	for(x=0;x<SCREENVERTS-1;x++)
	{
		for(y=0;y<SCREENVERTS-1;y++)
		{
			float stCoords[8];
			float vertCoords[12];

			// Used for texture coordinates
			// Annoying magic numbers to scale the square texture to
			// a non-square screen..
			float_x = (float)(x/(xfix));
			float_y = (float)(y/(yfix));
			float_nextx = (float)(x+1)/(xfix);
			float_nexty = (float)(y+1)/(yfix);

			// float stCoords[8];
			stCoords[0] = float_x;
			stCoords[1] = float_y;
			stCoords[2] = float_x;
			stCoords[3] = float_nexty;
			stCoords[4] = float_nextx;
			stCoords[5] = float_nexty;
			stCoords[6] = float_nextx;
			stCoords[7] = float_y;

			pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, stCoords);

			// float vertCoords[12];
			vertCoords[0] = points[x][y][0] / 4.5f;
			vertCoords[1] = points[x][y][1] / 4.5f;
			vertCoords[2] = 1.0f;
			vertCoords[3] = points[x][y + 1][0] / 4.5f;
			vertCoords[4] = points[x][y + 1][1] / 4.5f;
			vertCoords[5] = 1.0f;
			vertCoords[6] = points[x + 1][y + 1][0] / 4.5f;
			vertCoords[7] = points[x + 1][y + 1][1] / 4.5f;
			vertCoords[8] = 1.0f;
			vertCoords[9] = points[x + 1][y][0] / 4.5f;
			vertCoords[10] = points[x + 1][y][1] / 4.5f;
			vertCoords[11] = 1.0f;

			pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, vertCoords);

			pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}
	}

	pglEnable(GL_DEPTH_TEST);
	pglEnable(GL_BLEND);
}

// Sryder:	This needs to be called whenever the screen changes resolution in order to reset the screen textures to use
//			a new size
EXPORT void HWRAPI(FlushScreenTextures) (void)
{
	pglDeleteTextures(1, &screentexture);
	pglDeleteTextures(1, &startScreenWipe);
	pglDeleteTextures(1, &endScreenWipe);
	pglDeleteTextures(1, &finalScreenTexture);
	screentexture = 0;
	startScreenWipe = 0;
	endScreenWipe = 0;
	finalScreenTexture = 0;
}

// Create Screen to fade from
EXPORT void HWRAPI(StartScreenWipe) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (startScreenWipe == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &startScreenWipe);
	pglBindTexture(GL_TEXTURE_2D, startScreenWipe);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = startScreenWipe;
}

// Create Screen to fade to
EXPORT void HWRAPI(EndScreenWipe)(void)
{
	INT32 texsize = 2048;
	boolean firstTime = (endScreenWipe == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &endScreenWipe);
	pglBindTexture(GL_TEXTURE_2D, endScreenWipe);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = endScreenWipe;
}


// Draw the last scene under the intermission
EXPORT void HWRAPI(DrawIntermissionBG)(void)
{
	float xfix, yfix;
	INT32 texsize = 2048;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	if (shader_current == NULL)
		return;

	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	pglBindTexture(GL_TEXTURE_2D, screentexture);
	Shader_SetUniforms(NULL, &white, NULL, NULL);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = screentexture;
}

// Do screen fades!
static void DoWipe(boolean tinted, boolean isfadingin, boolean istowhite)
{
	INT32 texsize = 2048;
	float xfix, yfix;

	INT32 fademaskdownloaded = tex_downloaded; // the fade mask that has been set
	gl_shaderprogram_t *shader;
	boolean changed = false;

	const float screenVerts[12] =
	{
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, -1.0f, 1.0f
	};

	float fix[8];

	const float defaultST[8] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	if (shader_current == NULL)
		return;

	// Use a power of two texture, dammit
	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	// const float screenVerts[12]

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	SetBlend(PF_Modulated|PF_Translucent|PF_NoDepthTest);

	shader = &gl_shaderprograms[tinted ? SHADER_FADEMASK_ADDITIVEANDSUBTRACTIVE : SHADER_FADEMASK];
	changed = Shader_SetProgram(shader);

	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_COLORS));
	pglEnableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD1));

	if (changed)
	{
		Shader_SetSampler(shader, gluniform_startscreen, 0);
		Shader_SetSampler(shader, gluniform_endscreen, 1);
		Shader_SetSampler(shader, gluniform_fademask, 2);

		if (tinted)
		{
			Shader_SetIntegerUniform(shader, gluniform_isfadingin, isfadingin);
			Shader_SetIntegerUniform(shader, gluniform_istowhite, istowhite);
		}

		Shader_SetUniforms(NULL, &white, NULL, NULL);
		Shader_SetTransform();
	}

	pglActiveTexture(GL_TEXTURE0 + 0);
	pglBindTexture(GL_TEXTURE_2D, startScreenWipe);
	pglActiveTexture(GL_TEXTURE0 + 1);
	pglBindTexture(GL_TEXTURE_2D, endScreenWipe);
	pglActiveTexture(GL_TEXTURE0 + 2);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD0), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD1), 2, GL_FLOAT, GL_FALSE, 0, defaultST);

	pglActiveTexture(GL_TEXTURE0);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	pglDisableVertexAttribArray(Shader_AttribLoc(LOC_TEXCOORD1));

	UnSetShader();
	tex_downloaded = endScreenWipe;
}

EXPORT void HWRAPI(DoScreenWipe)(void)
{
	DoWipe(false, false, false);
}

EXPORT void HWRAPI(DoTintedWipe)(boolean isfadingin, boolean istowhite)
{
	DoWipe(true, isfadingin, istowhite);
}

// Create a texture from the screen.
EXPORT void HWRAPI(MakeScreenTexture) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (screentexture == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &screentexture);
	pglBindTexture(GL_TEXTURE_2D, screentexture);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = screentexture;
}

EXPORT void HWRAPI(MakeScreenFinalTexture) (void)
{
	INT32 texsize = 2048;
	boolean firstTime = (finalScreenTexture == 0);

	// Use a power of two texture, dammit
	if(screen_width <= 512)
		texsize = 512;
	else if(screen_width <= 1024)
		texsize = 1024;

	// Create screen texture
	if (firstTime)
		pglGenTextures(1, &finalScreenTexture);
	pglBindTexture(GL_TEXTURE_2D, finalScreenTexture);

	if (firstTime)
	{
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		Clamp2D(GL_TEXTURE_WRAP_S);
		Clamp2D(GL_TEXTURE_WRAP_T);
		pglCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, texsize, texsize, 0);
	}
	else
		pglCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texsize, texsize);

	tex_downloaded = finalScreenTexture;
}

EXPORT void HWRAPI(DrawScreenFinalTexture)(int width, int height)
{
	float xfix, yfix;
	float origaspect, newaspect;
	float xoff = 1, yoff = 1; // xoffset and yoffset for the polygon to have black bars around the screen
	FRGBAFloat clearColour;
	INT32 texsize = 2048;

	float off[12];
	float fix[8];

	if (shader_current == NULL)
		return;

	if(screen_width <= 1024)
		texsize = 1024;
	if(screen_width <= 512)
		texsize = 512;

	xfix = 1/((float)(texsize)/((float)((screen_width))));
	yfix = 1/((float)(texsize)/((float)((screen_height))));

	origaspect = (float)screen_width / screen_height;
	newaspect = (float)width / height;
	if (origaspect < newaspect)
	{
		xoff = origaspect / newaspect;
		yoff = 1;
	}
	else if (origaspect > newaspect)
	{
		xoff = 1;
		yoff = newaspect / origaspect;
	}

	// float off[12];
	off[0] = -xoff;
	off[1] = -yoff;
	off[2] = 1.0f;
	off[3] = -xoff;
	off[4] = yoff;
	off[5] = 1.0f;
	off[6] = xoff;
	off[7] = yoff;
	off[8] = 1.0f;
	off[9] = xoff;
	off[10] = -yoff;
	off[11] = 1.0f;

	// float fix[8];
	fix[0] = 0.0f;
	fix[1] = 0.0f;
	fix[2] = 0.0f;
	fix[3] = yfix;
	fix[4] = xfix;
	fix[5] = yfix;
	fix[6] = xfix;
	fix[7] = 0.0f;

	pglViewport(0, 0, width, height);

	clearColour.red = clearColour.green = clearColour.blue = 0;
	clearColour.alpha = 1;
	ClearBuffer(true, false, &clearColour);
	pglBindTexture(GL_TEXTURE_2D, finalScreenTexture);

	Shader_SetUniforms(NULL, &white, NULL, NULL);

	pglBindBuffer(GL_ARRAY_BUFFER, 0);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_POSITION), 3, GL_FLOAT, GL_FALSE, 0, off);
	pglVertexAttribPointer(Shader_AttribLoc(LOC_TEXCOORD), 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = finalScreenTexture;
}

#endif //HWRENDER