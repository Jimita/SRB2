// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_opengl.c
/// \brief OpenGL API for Sonic Robo Blast 2

#if defined (_WIN32)
//#define WIN32_LEAN_AND_MEAN
#define RPC_NO_WINDOWS_H
#include <windows.h>
#endif
#undef GETTEXT
#ifdef __GNUC__
#include <unistd.h>
#endif

#include <stdarg.h>
#include <math.h>
#include "r_opengl.h"
#include "r_vbo.h"

#include "../lzml.h"

#if defined (HWRENDER) && !defined (NOROPENGL)

struct GLRGBAFloat
{
	GLfloat red;
	GLfloat green;
	GLfloat blue;
	GLfloat alpha;
};
typedef struct GLRGBAFloat GLRGBAFloat;
static GLRGBAFloat white = {1.0f, 1.0f, 1.0f, 1.0f};
static GLRGBAFloat black = {0.0f, 0.0f, 0.0f, 1.0f};

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

// With OpenGL 1.1+, the first texture should be 1
static GLuint NOTEXTURE_NUM = 0;

#define      N_PI_DEMI               (M_PIl/2.0f) //(1.5707963268f)

#define      ASPECT_RATIO            (1.0f)  //(320.0f/200.0f)
#define      FAR_CLIPPING_PLANE      32768.0f // Draw further! Tails 01-21-2001
static float NEAR_CLIPPING_PLANE =   NZCLIP_PLANE;

#define Deg2Rad(x) ((x) * ((float)M_PIl / 180.0f))

typedef float fvector3_t[3];
typedef float fvector4_t[4];
typedef fvector4_t fmatrix4_t[4];

// **************************************************************************
//                                                                    GLOBALS
// **************************************************************************


static  GLuint      tex_downloaded  = 0;
static  GLfloat     fov             = 90.0f;
static  FBITFIELD   CurrentPolyFlags;

static  FTextureInfo *gl_cachetail = NULL;
static  FTextureInfo *gl_cachehead = NULL;

RGBA_t  myPaletteData[256];
GLint   screen_width    = 0;               // used by Draw2DLine()
GLint   screen_height   = 0;
GLbyte  screen_depth    = 0;
GLint   textureformatGL = 0;
GLint maximumAnisotropy = 0;
static GLboolean MipMap = GL_FALSE;
static GLint min_filter = GL_LINEAR;
static GLint mag_filter = GL_LINEAR;
static GLint anisotropic_filter = 0;
static boolean model_lighting = false;

const GLubyte *gl_version = NULL;
const GLubyte *gl_renderer = NULL;
const GLubyte *gl_extensions = NULL;

static fmatrix4_t projMatrix;
static fmatrix4_t viewMatrix;
static fmatrix4_t modelMatrix;

static GLint viewport[4];

// Sryder:	NextTexAvail is broken for these because palette changes or changes to the texture filter or antialiasing
//			flush all of the stored textures, leaving them unavailable at times such as between levels
//			These need to start at 0 and be set to their number, and be reset to 0 when deleted so that intel GPUs
//			can know when the textures aren't there, as textures are always considered resident in their virtual memory
static GLuint screentexture = 0;
static GLuint startScreenWipe = 0;
static GLuint endScreenWipe = 0;
static GLuint finalScreenTexture = 0;

// shortcut for ((float)1/i)
#define byte2float(x) (x / 255.0f)

// -----------------+
// GL_DBG_Printf    : Output debug messages to debug log if DEBUG_TO_FILE is defined,
//                  : else do nothing
// Returns          :
// -----------------+

#ifdef DEBUG_TO_FILE
FILE *gllogstream;
#endif

FUNCPRINTF void GL_DBG_Printf(const char *format, ...)
{
#ifdef DEBUG_TO_FILE
	char str[4096] = "";
	va_list arglist;

	if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");

	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

	fwrite(str, strlen(str), 1, gllogstream);
#else
	(void)format;
#endif
}

// -----------------+
// GL_MSG_Warning   : Raises a warning.
//                  :
// Returns          :
// -----------------+

static void GL_MSG_Warning(const char *format, ...)
{
	char str[4096] = "";
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

#ifdef HAVE_SDL
	CONS_Alert(CONS_WARNING, "%s", str);
#endif
#ifdef DEBUG_TO_FILE
	if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");
	fwrite(str, strlen(str), 1, gllogstream);
#endif
}

// -----------------+
// GL_MSG_Error     : Raises an error.
//                  :
// Returns          :
// -----------------+

static void GL_MSG_Error(const char *format, ...)
{
	char str[4096] = "";
	va_list arglist;

	va_start(arglist, format);
	vsnprintf(str, 4096, format, arglist);
	va_end(arglist);

#ifdef HAVE_SDL
	CONS_Alert(CONS_ERROR, "%s", str);
#endif
#ifdef DEBUG_TO_FILE
	if (!gllogstream)
		gllogstream = fopen("ogllog.txt", "w");
	fwrite(str, strlen(str), 1, gllogstream);
#endif
}

#ifdef STATIC_OPENGL
/* 1.0 functions */
/* Miscellaneous */
#define pglClearColor glClearColor
#define pglColorMask glColorMask
#define pglAlphaFunc glAlphaFunc
#define pglBlendFunc glBlendFunc
#define pglCullFace glCullFace
#define pglPolygonOffset glPolygonOffset
#define pglEnable glEnable
#define pglDisable glDisable
#define pglGetIntegerv glGetIntegerv
#define pglDisable glGetString

/* Depth Buffer */
#define pglClearDepth glClearDepth
#define pglDepthFunc glDepthFunc
#define pglDepthMask glDepthMask
#define pglDepthRange glDepthRange

/* Transformation */
#define pglViewport glViewport

/* Drawing Functions */
#define pglDrawArrays glDrawArrays
#define pglDrawElements glDrawElements
#define pglEnableVertexAttribArray glEnableVertexAttribArray
#define pglDisableVertexAttribArray glDisableVertexAttribArray
#define pglGenerateMipmap glGenerateMipmap
#define pglVertexAttribPointer glVertexAttribPointer

/* Raster functions */
#define pglPixelStorei glPixelStorei
#define pglReadPixels glReadPixels

/* Texture mapping */
#define pglTexEnvi glTexEnvi
#define pglTexParameteri glTexParameteri
#define pglTexImage2D glTexImage2D
#define pglTexSubImage2D glTexSubImage2D

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
#define pglGenTextures glGenTextures
#define pglDeleteTextures glDeleteTextures
#define pglBindTexture glBindTexture
/* texture mapping */ //GL_EXT_copy_texture
#define pglCopyTexImage2D glCopyTexImage2D
#define pglCopyTexSubImage2D glCopyTexSubImage2D

#else //!STATIC_OPENGL

/* 1.0 functions */
/* Miscellaneous */
typedef void (APIENTRY * PFNglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static PFNglClearColor pglClearColor;
typedef void (APIENTRY * PFNglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
static PFNglColorMask pglColorMask;
typedef void (APIENTRY * PFNglAlphaFunc) (GLenum func, GLclampf ref);
static PFNglAlphaFunc pglAlphaFunc;
typedef void (APIENTRY * PFNglBlendFunc) (GLenum sfactor, GLenum dfactor);
static PFNglBlendFunc pglBlendFunc;
typedef void (APIENTRY * PFNglCullFace) (GLenum mode);
static PFNglCullFace pglCullFace;
typedef void (APIENTRY * PFNglPolygonOffset) (GLfloat factor, GLfloat units);
static PFNglPolygonOffset pglPolygonOffset;
typedef void (APIENTRY * PFNglEnable) (GLenum cap);
static PFNglEnable pglEnable;
typedef void (APIENTRY * PFNglDisable) (GLenum cap);
static PFNglDisable pglDisable;

/* Depth Buffer */
typedef void (APIENTRY * PFNglClearDepth) (GLclampd depth);
static PFNglClearDepth pglClearDepth;
typedef void (APIENTRY * PFNglDepthFunc) (GLenum func);
static PFNglDepthFunc pglDepthFunc;
typedef void (APIENTRY * PFNglDepthMask) (GLboolean flag);
static PFNglDepthMask pglDepthMask;
typedef void (APIENTRY * PFNglDepthRange) (GLclampd near_val, GLclampd far_val);
static PFNglDepthRange pglDepthRange;

/* Transformation */
typedef void (APIENTRY * PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglViewport pglViewport;

/* Drawing Functions */
typedef void (APIENTRY * PFNglDrawArrays) (GLenum mode, GLint first, GLsizei count);
static PFNglDrawArrays pglDrawArrays;
typedef void (APIENTRY * PFNglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
static PFNglDrawElements pglDrawElements;
typedef void (APIENTRY * PFNglEnableVertexAttribArray) (GLuint index);
static PFNglEnableVertexAttribArray pglEnableVertexAttribArray;
typedef void (APIENTRY * PFNglDisableVertexAttribArray) (GLuint index);
static PFNglDisableVertexAttribArray pglDisableVertexAttribArray;
typedef void (APIENTRY * PFNglGenerateMipmap) (GLenum target);
static PFNglGenerateMipmap pglGenerateMipmap;
typedef void (APIENTRY * PFNglVertexAttribPointer) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void * pointer);
static PFNglVertexAttribPointer pglVertexAttribPointer;

/* Raster functions */
typedef void (APIENTRY * PFNglPixelStorei) (GLenum pname, GLint param);
static PFNglPixelStorei pglPixelStorei;
typedef void (APIENTRY  * PFNglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
static PFNglReadPixels pglReadPixels;

/* Texture mapping */
typedef void (APIENTRY * PFNglTexParameteri) (GLenum target, GLenum pname, GLint param);
static PFNglTexParameteri pglTexParameteri;
typedef void (APIENTRY * PFNglTexImage2D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexImage2D pglTexImage2D;
typedef void (APIENTRY * PFNglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
static PFNglTexSubImage2D pglTexSubImage2D;

/* 1.1 functions */
/* texture objects */ //GL_EXT_texture_object
typedef void (APIENTRY * PFNglGenTextures) (GLsizei n, const GLuint *textures);
static PFNglGenTextures pglGenTextures;
typedef void (APIENTRY * PFNglDeleteTextures) (GLsizei n, const GLuint *textures);
static PFNglDeleteTextures pglDeleteTextures;
typedef void (APIENTRY * PFNglBindTexture) (GLenum target, GLuint texture);
static PFNglBindTexture pglBindTexture;
/* texture mapping */ //GL_EXT_copy_texture
typedef void (APIENTRY * PFNglCopyTexImage2D) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
static PFNglCopyTexImage2D pglCopyTexImage2D;
typedef void (APIENTRY * PFNglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
static PFNglCopyTexSubImage2D pglCopyTexSubImage2D;
#endif

/* 1.3 functions for multitexturing */
typedef void (APIENTRY *PFNglActiveTexture) (GLenum);
static PFNglActiveTexture pglActiveTexture;

/* 1.5 functions for buffers */
typedef void (APIENTRY * PFNglGenBuffers) (GLsizei n, GLuint *buffers);
static PFNglGenBuffers pglGenBuffers;
typedef void (APIENTRY * PFNglBindBuffer) (GLenum target, GLuint buffer);
static PFNglBindBuffer pglBindBuffer;
typedef void (APIENTRY * PFNglBufferData) (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
static PFNglBufferData pglBufferData;
typedef void (APIENTRY * PFNglDeleteBuffers) (GLsizei n, const GLuint *buffers);
static PFNglDeleteBuffers pglDeleteBuffers;

/* 1.2 Parms */
/* GL_CLAMP_TO_EDGE_EXT */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_TEXTURE_MIN_LOD
#define GL_TEXTURE_MIN_LOD 0x813A
#endif
#ifndef GL_TEXTURE_MAX_LOD
#define GL_TEXTURE_MAX_LOD 0x813B
#endif

/* 1.3 GL_TEXTUREi */
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

/* 1.5 Parms */
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

boolean SetupGLfunc(void)
{
#ifndef STATIC_OPENGL
#define GETOPENGLFUNC(func, proc) \
	func = GetGLFunc(#proc); \
	if (!func) \
	{ \
		GL_MSG_Warning("failed to get OpenGL function: %s", #proc); \
	} \

	GETOPENGLFUNC(pglClearColor, glClearColor)

	GETOPENGLFUNC(pglClear, glClear)
	GETOPENGLFUNC(pglColorMask, glColorMask)
	GETOPENGLFUNC(pglAlphaFunc, glAlphaFunc)
	GETOPENGLFUNC(pglBlendFunc, glBlendFunc)
	GETOPENGLFUNC(pglCullFace, glCullFace)
	GETOPENGLFUNC(pglPolygonOffset, glPolygonOffset)
	GETOPENGLFUNC(pglEnable, glEnable)
	GETOPENGLFUNC(pglDisable, glDisable)
	GETOPENGLFUNC(pglGetIntegerv, glGetIntegerv)
	GETOPENGLFUNC(pglGetString, glGetString)

	GETOPENGLFUNC(pglClearDepth, glClearDepth)
	GETOPENGLFUNC(pglDepthFunc, glDepthFunc)
	GETOPENGLFUNC(pglDepthMask, glDepthMask)
	GETOPENGLFUNC(pglDepthRange, glDepthRange)

	GETOPENGLFUNC(pglViewport, glViewport)

	GETOPENGLFUNC(pglDrawArrays, glDrawArrays)
	GETOPENGLFUNC(pglDrawElements, glDrawElements)

	GETOPENGLFUNC(pglPixelStorei, glPixelStorei)
	GETOPENGLFUNC(pglReadPixels, glReadPixels)

	GETOPENGLFUNC(pglTexParameteri, glTexParameteri)
	GETOPENGLFUNC(pglTexImage2D, glTexImage2D)
	GETOPENGLFUNC(pglTexSubImage2D, glTexSubImage2D)

	GETOPENGLFUNC(pglGenTextures, glGenTextures)
	GETOPENGLFUNC(pglDeleteTextures, glDeleteTextures)
	GETOPENGLFUNC(pglBindTexture, glBindTexture)

	GETOPENGLFUNC(pglCopyTexImage2D, glCopyTexImage2D)
	GETOPENGLFUNC(pglCopyTexSubImage2D, glCopyTexSubImage2D)

#undef GETOPENGLFUNC

#endif
	return true;
}

typedef GLuint 	(APIENTRY *PFNglCreateShader)		(GLenum);
typedef void 	(APIENTRY *PFNglShaderSource)		(GLuint, GLsizei, const GLchar**, GLint*);
typedef void 	(APIENTRY *PFNglCompileShader)		(GLuint);
typedef void 	(APIENTRY *PFNglGetShaderiv)		(GLuint, GLenum, GLint*);
typedef void 	(APIENTRY *PFNglGetShaderInfoLog)	(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void 	(APIENTRY *PFNglDeleteShader)		(GLuint);
typedef GLuint 	(APIENTRY *PFNglCreateProgram)		(void);
typedef void 	(APIENTRY *PFNglAttachShader)		(GLuint, GLuint);
typedef void 	(APIENTRY *PFNglLinkProgram)		(GLuint);
typedef void 	(APIENTRY *PFNglGetProgramiv)		(GLuint, GLenum, GLint*);
typedef void 	(APIENTRY *PFNglUseProgram)			(GLuint);
typedef void 	(APIENTRY *PFNglUniform1i)			(GLint, GLint);
typedef void 	(APIENTRY *PFNglUniform1f)			(GLint, GLfloat);
typedef void 	(APIENTRY *PFNglUniform2f)			(GLint, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform3f)			(GLint, GLfloat, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform4f)			(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void 	(APIENTRY *PFNglUniform1fv)			(GLint, GLsizei, const GLfloat*);
typedef void 	(APIENTRY *PFNglUniform2fv)			(GLint, GLsizei, const GLfloat*);
typedef void 	(APIENTRY *PFNglUniform3fv)			(GLint, GLsizei, const GLfloat*);
typedef void 	(APIENTRY *PFNglUniformMatrix4fv)	(GLint, GLsizei, GLboolean, const GLfloat *);
typedef GLint 	(APIENTRY *PFNglGetUniformLocation)	(GLuint, const GLchar*);

static PFNglCreateShader pglCreateShader;
static PFNglShaderSource pglShaderSource;
static PFNglCompileShader pglCompileShader;
static PFNglGetShaderiv pglGetShaderiv;
static PFNglGetShaderInfoLog pglGetShaderInfoLog;
static PFNglDeleteShader pglDeleteShader;
static PFNglCreateProgram pglCreateProgram;
static PFNglAttachShader pglAttachShader;
static PFNglLinkProgram pglLinkProgram;
static PFNglGetProgramiv pglGetProgramiv;
static PFNglUseProgram pglUseProgram;
static PFNglUniform1i pglUniform1i;
static PFNglUniform1f pglUniform1f;
static PFNglUniform2f pglUniform2f;
static PFNglUniform3f pglUniform3f;
static PFNglUniform4f pglUniform4f;
static PFNglUniform1fv pglUniform1fv;
static PFNglUniform2fv pglUniform2fv;
static PFNglUniform3fv pglUniform3fv;
static PFNglUniformMatrix4fv pglUniformMatrix4fv;
static PFNglGetUniformLocation pglGetUniformLocation;

#define MAXSHADERS 16
#define MAXSHADERPROGRAMS 16

// 18032019
static char *gl_customvertexshaders[MAXSHADERS];
static char *gl_customfragmentshaders[MAXSHADERS];

// 08072020
typedef enum
{
	// transform
	gluniform_model,
	gluniform_view,
	gluniform_projection,

	// samplers
	gluniform_startscreen,
	gluniform_endscreen,
	gluniform_fademask,

	// lighting
	gluniform_poly_color,
	gluniform_tint_color,
	gluniform_fade_color,
	gluniform_lighting,
	gluniform_fade_start,
	gluniform_fade_end,

	// misc.
	gluniform_isfadingin,
	gluniform_istowhite,
	gluniform_leveltime,

	gluniform_max,
} gluniform_t;

typedef struct gl_shaderprogram_s
{
	GLuint program;
	boolean custom;
	GLint uniforms[gluniform_max+1];

	fmatrix4_t projMatrix;
	fmatrix4_t viewMatrix;
	fmatrix4_t modelMatrix;
} gl_shaderprogram_t;
static gl_shaderprogram_t gl_shaderprograms[MAXSHADERPROGRAMS];

static gl_shaderprogram_t *shader_base = NULL;
static gl_shaderprogram_t *shader_current = NULL;
static boolean shader_enabled = false;

// Shader info
static INT32 shader_leveltime = 0;

// Lactozilla: Set shader programs and uniforms
static boolean Shader_SetProgram(gl_shaderprogram_t *shader);
static void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade);
static void Shader_SetTransform(void);

enum
{
	LOC_POSITION  = 0,
	LOC_TEXCOORD  = 1,
	LOC_NORMAL    = 2,
	LOC_COLORS    = 3,

	LOC_TEXCOORD0 = LOC_TEXCOORD,
	LOC_TEXCOORD1 = LOC_NORMAL
};

#define GLSL_VERSION_MACRO "#version 330 core\n"

// ================
//  Vertex shaders
// ================

//
// GLSL generic vertex shader
//

#define GLSL_DEFAULT_VERTEX_SHADER \
	GLSL_VERSION_MACRO \
	"layout (location = 0) in vec3 aPos;\n" \
	"layout (location = 1) in vec2 aTexCoord;\n" \
	"layout (location = 2) in vec3 aNormal;\n" \
	"layout (location = 3) in vec4 aColors;\n" \
	"out vec2 TexCoord;\n" \
	"out vec3 Normal;\n" \
	"out vec4 Colors;\n" \
	"uniform mat4 model;\n" \
	"uniform mat4 view;\n" \
	"uniform mat4 projection;\n" \
	"void main()\n" \
	"{\n" \
		"gl_Position = projection * view * model * vec4(aPos, 1.0f);\n" \
		"TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n" \
	"}\0"

//
// Fade mask vertex shader
//

#define GLSL_FADEMASK_VERTEX_SHADER \
	GLSL_VERSION_MACRO \
	"layout (location = 0) in vec3 aPos;\n" \
	"layout (location = 1) in vec2 aTexCoord;\n" \
	"layout (location = 2) in vec2 aFadeMaskTexCoord;\n" \
	"out vec2 TexCoord;\n" \
	"out vec2 FadeMaskTexCoord;\n" \
	"uniform mat4 projection;\n" \
	"void main()\n" \
	"{\n" \
		"gl_Position = projection * vec4(aPos, 1.0f);\n" \
		"TexCoord = vec2(aTexCoord.x, aTexCoord.y);\n" \
		"FadeMaskTexCoord = vec2(aFadeMaskTexCoord.x, aFadeMaskTexCoord.y);\n" \
	"}\0"

static const char *vertex_shaders[] = {
	// Default vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Floor vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Wall vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Sprite vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Model vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Water vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Fog vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Sky vertex shader
	GLSL_DEFAULT_VERTEX_SHADER,

	// Fade mask vertex shader
	GLSL_FADEMASK_VERTEX_SHADER, GLSL_FADEMASK_VERTEX_SHADER,

	NULL,
};

// ==================
//  Fragment shaders
// ==================

#define GLSL_BASE_IN \
	"in vec2 TexCoord;\n" \
	"in vec3 Normal;\n" \
	"in vec4 Colors;\n" \

#define GLSL_BASE_OUT \
	"out vec4 FragColor;\n" \

#define GLSL_BASE_UNIFORMS \
	"uniform sampler2D TexSampler;\n" \
	"uniform vec4 PolyColor;\n" \

//
// Generic fragment shader
//

#define GLSL_DEFAULT_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	GLSL_BASE_OUT \
	GLSL_BASE_IN \
	GLSL_BASE_UNIFORMS \
	"void main(void) {\n" \
		"FragColor = texture(TexSampler, TexCoord) * PolyColor;\n" \
	"}\0"

//
// Sky fragment shader
//

#define GLSL_SKY_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	GLSL_BASE_OUT \
	GLSL_BASE_IN \
	GLSL_BASE_UNIFORMS \
	"void main(void) {\n" \
		"FragColor = texture(TexSampler, TexCoord) * Colors;\n" \
	"}\0"

//
// Fade mask fragment shader
//

#define GLSL_FADEMASK_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	"out vec4 FragColor;\n" \
	"in vec2 TexCoord;\n" \
	"in vec2 FadeMaskTexCoord;\n" \
	"uniform sampler2D StartScreen;\n" \
	"uniform sampler2D EndScreen;\n" \
	"uniform sampler2D FadeMask;\n" \
	"void main(void) {\n" \
		"vec4 StartTexel = texture(StartScreen, TexCoord);\n" \
		"vec4 EndTexel = texture(EndScreen, TexCoord);\n" \
		"vec4 MaskTexel = texture(FadeMask, FadeMaskTexCoord);\n" \
		"FragColor = mix(StartTexel, EndTexel, MaskTexel.r);\n" \
	"}\0"

// Lactozilla: Very simple shader that uses either additive
// or subtractive blending depending on the wipe style.
#define GLSL_FADEMASK_ADDITIVEANDSUBTRACTIVE_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	"out vec4 FragColor;\n" \
	"in vec2 TexCoord;\n" \
	"in vec2 FadeMaskTexCoord;\n" \
	"uniform sampler2D StartScreen;\n" \
	"uniform sampler2D EndScreen;\n" \
	"uniform sampler2D FadeMask;\n" \
	"uniform bool IsFadingIn;\n" \
	"uniform bool IsToWhite;\n" \
	"void main(void) {\n" \
		"vec4 MaskTexel = texture(FadeMask, FadeMaskTexCoord);\n" \
		"vec4 MixTexel;\n" \
		"vec4 FinalColor;\n" \
		"float FadeAlpha = MaskTexel.r;\n" \
		"if (IsFadingIn == true)\n" \
		"{\n" \
			"FadeAlpha = (1.0f - FadeAlpha);\n" \
			"MixTexel = texture(EndScreen, TexCoord);\n" \
		"}\n" \
		"else\n" \
			"MixTexel = texture(StartScreen, TexCoord);\n" \
		"float FadeRed = clamp((FadeAlpha * 3.0f), 0.0f, 1.0f);\n" \
		"float FadeGreen = clamp((FadeAlpha * 2.0f), 0.0f, 1.0f);\n" \
		"if (IsToWhite == true)\n" \
		"{\n" \
			"FinalColor.r = MixTexel.r + FadeRed;\n" \
			"FinalColor.g = MixTexel.g + FadeGreen;\n" \
			"FinalColor.b = MixTexel.b + FadeAlpha;\n" \
		"}\n" \
		"else\n" \
		"{\n" \
			"FinalColor.r = MixTexel.r - FadeRed;\n" \
			"FinalColor.g = MixTexel.g - FadeGreen;\n" \
			"FinalColor.b = MixTexel.b - FadeAlpha;\n" \
		"}\n" \
		"FinalColor.a = 1.0f;\n" \
		"FragColor = FinalColor;\n" \
	"}\0"

//
// Software fragment shader
//

#define GLSL_DOOM_UNIFORMS \
	GLSL_BASE_UNIFORMS \
	"uniform vec4 TintColor;\n" \
	"uniform vec4 FadeColor;\n" \
	"uniform float Lighting;\n" \
	"uniform float FadeStart;\n" \
	"uniform float FadeEnd;\n" \

#define GLSL_DOOM_COLORMAP \
	"float R_DoomColormap(float light, float z)\n" \
	"{\n" \
		"float lightnum = clamp(light / 17.0, 0.0, 15.0);\n" \
		"float lightz = clamp(z / 16.0, 0.0, 127.0);\n" \
		"float startmap = (15.0 - lightnum) * 4.0;\n" \
		"float scale = 160.0 / (lightz + 1.0);\n" \
		"return startmap - scale * 0.5;\n" \
	"}\n"

#define GLSL_DOOM_LIGHT_EQUATION \
	"float R_DoomLightingEquation(float light)\n" \
	"{\n" \
		"float z = gl_FragCoord.z / gl_FragCoord.w;\n" \
		"float colormap = floor(R_DoomColormap(light, z)) + 0.5;\n" \
		"return clamp(colormap, 0.0, 31.0) / 32.0;\n" \
	"}\n"

#define GLSL_SOFTWARE_TINT_EQUATION \
	"if (TintColor.a > 0.0) {\n" \
		"float color_bright = sqrt((BaseColor.r * BaseColor.r) + (BaseColor.g * BaseColor.g) + (BaseColor.b * BaseColor.b));\n" \
		"float strength = sqrt(9.0 * TintColor.a);\n" \
		"FinalColor.r = clamp((color_bright * (TintColor.r * strength)) + (BaseColor.r * (1.0 - strength)), 0.0, 1.0);\n" \
		"FinalColor.g = clamp((color_bright * (TintColor.g * strength)) + (BaseColor.g * (1.0 - strength)), 0.0, 1.0);\n" \
		"FinalColor.b = clamp((color_bright * (TintColor.b * strength)) + (BaseColor.b * (1.0 - strength)), 0.0, 1.0);\n" \
	"}\n"

#define GLSL_SOFTWARE_FADE_EQUATION \
	"float darkness = R_DoomLightingEquation(Lighting);\n" \
	"if (FadeStart != 0.0 || FadeEnd != 31.0) {\n" \
		"float fs = FadeStart / 31.0;\n" \
		"float fe = FadeEnd / 31.0;\n" \
		"float fd = fe - fs;\n" \
		"darkness = clamp((darkness - fs) * (1.0 / fd), 0.0, 1.0);\n" \
	"}\n" \
	"FinalColor = mix(FinalColor, FadeColor, darkness);\n"

#define GLSL_SOFTWARE_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	GLSL_BASE_OUT \
	GLSL_BASE_IN \
	GLSL_DOOM_UNIFORMS \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"vec4 texel = texture2D(TexSampler, TexCoord);\n" \
		"vec4 BaseColor = texel * PolyColor;\n" \
		"vec4 FinalColor = BaseColor;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"FinalColor.a = texel.a * PolyColor.a;\n" \
		"FragColor = FinalColor;\n" \
	"}\0"

//
// Water surface shader
//
// Mostly guesstimated, rather than the rest being built off Software science.
// Still needs to distort things underneath/around the water...
//

#define GLSL_WATER_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	GLSL_BASE_OUT \
	"in vec2 TexCoord;\n" \
	GLSL_DOOM_UNIFORMS \
	"uniform float LevelTime;\n" \
	"const float freq = 0.025;\n" \
	"const float amp = 0.025;\n" \
	"const float speed = 2.0;\n" \
	"const float pi = 3.14159;\n" \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"float z = (gl_FragCoord.z / gl_FragCoord.w) / 2.0;\n" \
		"float a = -pi * (z * freq) + (LevelTime * speed);\n" \
		"float sdistort = sin(a) * amp;\n" \
		"float cdistort = cos(a) * amp;\n" \
		"vec4 texel = texture(TexSampler, vec2(TexCoord.s - sdistort, TexCoord.t - cdistort));\n" \
		"vec4 BaseColor = texel * PolyColor;\n" \
		"vec4 FinalColor = BaseColor;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"FinalColor.a = texel.a * PolyColor.a;\n" \
		"FragColor = FinalColor;\n" \
	"}\0"

//
// Fog block shader
//
// Alpha of the planes themselves are still slightly off -- see HWR_FogBlockAlpha
//

#define GLSL_FOG_FRAGMENT_SHADER \
	GLSL_VERSION_MACRO \
	GLSL_BASE_OUT \
	"in vec2 TexCoord;\n" \
	GLSL_DOOM_UNIFORMS \
	GLSL_DOOM_COLORMAP \
	GLSL_DOOM_LIGHT_EQUATION \
	"void main(void) {\n" \
		"vec4 BaseColor = PolyColor;\n" \
		"vec4 FinalColor = BaseColor;\n" \
		GLSL_SOFTWARE_TINT_EQUATION \
		GLSL_SOFTWARE_FADE_EQUATION \
		"FragColor = FinalColor;\n" \
	"}\0"

static const char *fragment_shaders[] = {
	// Default fragment shader
	GLSL_DEFAULT_FRAGMENT_SHADER,

	// Floor fragment shader
	GLSL_SOFTWARE_FRAGMENT_SHADER,

	// Wall fragment shader
	GLSL_SOFTWARE_FRAGMENT_SHADER,

	// Sprite fragment shader
	GLSL_SOFTWARE_FRAGMENT_SHADER,

	// Model fragment shader
	GLSL_SOFTWARE_FRAGMENT_SHADER,

	// Water fragment shader
	GLSL_WATER_FRAGMENT_SHADER,

	// Fog fragment shader
	GLSL_FOG_FRAGMENT_SHADER,

	// Sky fragment shader
	GLSL_DEFAULT_FRAGMENT_SHADER,

	// Fade mask vertex shader
	GLSL_FADEMASK_FRAGMENT_SHADER, GLSL_FADEMASK_ADDITIVEANDSUBTRACTIVE_FRAGMENT_SHADER,

	NULL,
};

void SetupGLFunc4(void)
{
	pglActiveTexture = GetGLFunc("glActiveTexture");

	/* 1.5 funcs */
	pglGenBuffers = GetGLFunc("glGenBuffers");
	pglBindBuffer = GetGLFunc("glBindBuffer");
	pglBufferData = GetGLFunc("glBufferData");
	pglDeleteBuffers = GetGLFunc("glDeleteBuffers");

	pglEnableVertexAttribArray = GetGLFunc("glEnableVertexAttribArray");
	pglDisableVertexAttribArray = GetGLFunc("glDisableVertexAttribArray");
	pglGenerateMipmap = GetGLFunc("glGenerateMipmap");
	pglVertexAttribPointer = GetGLFunc("glVertexAttribPointer");

	pglCreateShader = GetGLFunc("glCreateShader");
	pglShaderSource = GetGLFunc("glShaderSource");
	pglCompileShader = GetGLFunc("glCompileShader");
	pglGetShaderiv = GetGLFunc("glGetShaderiv");
	pglGetShaderInfoLog = GetGLFunc("glGetShaderInfoLog");
	pglDeleteShader = GetGLFunc("glDeleteShader");
	pglCreateProgram = GetGLFunc("glCreateProgram");
	pglAttachShader = GetGLFunc("glAttachShader");
	pglLinkProgram = GetGLFunc("glLinkProgram");
	pglGetProgramiv = GetGLFunc("glGetProgramiv");
	pglUseProgram = GetGLFunc("glUseProgram");
	pglUniform1i = GetGLFunc("glUniform1i");
	pglUniform1f = GetGLFunc("glUniform1f");
	pglUniform2f = GetGLFunc("glUniform2f");
	pglUniform3f = GetGLFunc("glUniform3f");
	pglUniform4f = GetGLFunc("glUniform4f");
	pglUniform1fv = GetGLFunc("glUniform1fv");
	pglUniform2fv = GetGLFunc("glUniform2fv");
	pglUniform3fv = GetGLFunc("glUniform3fv");
	pglUniformMatrix4fv = GetGLFunc("glUniformMatrix4fv");
	pglGetUniformLocation = GetGLFunc("glGetUniformLocation");
}

// jimita
EXPORT boolean HWRAPI(LoadShaders) (void)
{
	GLuint gl_vertShader, gl_fragShader;
	GLint i, result;

	if (!pglUseProgram) return false;

	gl_customvertexshaders[0] = NULL;
	gl_customfragmentshaders[0] = NULL;

	shader_base = shader_current = &gl_shaderprograms[0];

	for (i = 0; vertex_shaders[i] && fragment_shaders[i]; i++)
	{
		gl_shaderprogram_t *shader;
		const GLchar* vert_shader = vertex_shaders[i];
		const GLchar* frag_shader = fragment_shaders[i];
		boolean custom = ((gl_customvertexshaders[i] || gl_customfragmentshaders[i]) && (i > 0));

		// 18032019
		if (gl_customvertexshaders[i])
			vert_shader = gl_customvertexshaders[i];
		if (gl_customfragmentshaders[i])
			frag_shader = gl_customfragmentshaders[i];

		if (i >= MAXSHADERS)
			break;
		if (i >= MAXSHADERPROGRAMS)
			break;

		shader = &gl_shaderprograms[i];
		shader->program = 0;
		shader->custom = custom;

		//
		// Load and compile vertex shader
		//
		gl_vertShader = pglCreateShader(GL_VERTEX_SHADER);
		if (!gl_vertShader)
		{
			GL_MSG_Error("LoadShaders: Error creating vertex shader %d\n", i);
			continue;
		}

		pglShaderSource(gl_vertShader, 1, &vert_shader, NULL);
		pglCompileShader(gl_vertShader);

		// check for compile errors
		pglGetShaderiv(gl_vertShader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE)
		{
			GLchar* infoLog;
			GLint logLength;

			pglGetShaderiv(gl_vertShader, GL_INFO_LOG_LENGTH, &logLength);

			infoLog = malloc(logLength);
			pglGetShaderInfoLog(gl_vertShader, logLength, NULL, infoLog);

			GL_MSG_Error("LoadShaders: Error compiling vertex shader %d\n%s", i, infoLog);
			continue;
		}

		//
		// Load and compile fragment shader
		//
		gl_fragShader = pglCreateShader(GL_FRAGMENT_SHADER);
		if (!gl_fragShader)
		{
			GL_MSG_Error("LoadShaders: Error creating fragment shader %d\n", i);
			continue;
		}

		pglShaderSource(gl_fragShader, 1, &frag_shader, NULL);
		pglCompileShader(gl_fragShader);

		// check for compile errors
		pglGetShaderiv(gl_fragShader, GL_COMPILE_STATUS, &result);
		if (result == GL_FALSE)
		{
			GLchar* infoLog;
			GLint logLength;

			pglGetShaderiv(gl_fragShader, GL_INFO_LOG_LENGTH, &logLength);

			infoLog = malloc(logLength);
			pglGetShaderInfoLog(gl_fragShader, logLength, NULL, infoLog);

			GL_MSG_Error("LoadShaders: Error compiling fragment shader %d\n%s", i, infoLog);
			continue;
		}

		shader->program = pglCreateProgram();
		pglAttachShader(shader->program, gl_vertShader);
		pglAttachShader(shader->program, gl_fragShader);
		pglLinkProgram(shader->program);

		// check link status
		pglGetProgramiv(shader->program, GL_LINK_STATUS, &result);

		// delete the shader objects
		pglDeleteShader(gl_vertShader);
		pglDeleteShader(gl_fragShader);

		// couldn't link?
		if (result != GL_TRUE)
		{
			shader->program = 0;
			shader->custom = false;
			GL_MSG_Error("LoadShaders: Error linking shader program %d\n", i);
			continue;
		}

		memset(shader->projMatrix, 0x00, sizeof(fmatrix4_t));
		memset(shader->viewMatrix, 0x00, sizeof(fmatrix4_t));
		memset(shader->modelMatrix, 0x00, sizeof(fmatrix4_t));

		// 09072020 / 13062019
#define GETUNI(uniform) pglGetUniformLocation(shader->program, uniform);

		// transform
		shader->uniforms[gluniform_model] = GETUNI("model");
		shader->uniforms[gluniform_view] = GETUNI("view");
		shader->uniforms[gluniform_projection] = GETUNI("projection");

		// samplers
		shader->uniforms[gluniform_startscreen] = GETUNI("StartScreen");
		shader->uniforms[gluniform_endscreen] = GETUNI("EndScreen");
		shader->uniforms[gluniform_fademask] = GETUNI("FadeMask");

		// lighting
		shader->uniforms[gluniform_poly_color] = GETUNI("PolyColor");
		shader->uniforms[gluniform_tint_color] = GETUNI("TintColor");
		shader->uniforms[gluniform_fade_color] = GETUNI("FadeColor");
		shader->uniforms[gluniform_lighting] = GETUNI("Lighting");
		shader->uniforms[gluniform_fade_start] = GETUNI("FadeStart");
		shader->uniforms[gluniform_fade_end] = GETUNI("FadeEnd");

		// misc.
		shader->uniforms[gluniform_isfadingin] = GETUNI("IsFadingIn");
		shader->uniforms[gluniform_istowhite] = GETUNI("IsToWhite");
		shader->uniforms[gluniform_leveltime] = GETUNI("LevelTime");

#undef GETUNI
	}

	pglUseProgram(shader_base->program);

	return true;
}

//
// Shader info
// Those are given to the uniforms.
//

EXPORT void HWRAPI(SetShaderInfo) (hwdshaderinfo_t info, INT32 value)
{
	switch (info)
	{
		case HWD_SHADERINFO_LEVELTIME:
			shader_leveltime = value;
			break;
		default:
			break;
	}
}

//
// Custom shader loading
//
EXPORT void HWRAPI(LoadCustomShader) (int number, char *shader, size_t size, boolean fragment)
{
	if (!pglUseProgram) return;
	if (number < 1 || number > MAXSHADERS)
		I_Error("LoadCustomShader(): cannot load shader %d (max %d)", number, MAXSHADERS);

	if (fragment)
	{
		gl_customfragmentshaders[number] = malloc(size+1);
		strncpy(gl_customfragmentshaders[number], shader, size);
		gl_customfragmentshaders[number][size] = 0;
	}
	else
	{
		gl_customvertexshaders[number] = malloc(size+1);
		strncpy(gl_customvertexshaders[number], shader, size);
		gl_customvertexshaders[number][size] = 0;
	}
}

EXPORT boolean HWRAPI(InitCustomShaders) (void)
{
	KillShaders();
	return LoadShaders();
}

static boolean Shader_SetProgram(gl_shaderprogram_t *shader)
{
	if (shader != shader_current)
	{
		shader_current = shader;
		pglUseProgram(shader->program);
		return true;
	}
	return false;
}

EXPORT void HWRAPI(SetShader) (int shader)
{
	if (Shader_SetProgram((shader_enabled) ? (&gl_shaderprograms[shader]) : shader_base))
		Shader_SetTransform();
}

EXPORT void HWRAPI(UnSetShader) (void)
{
	shader_current = shader_base;
	pglUseProgram(shader_base->program);

	Shader_SetUniforms(NULL, &white, NULL, NULL);
}

EXPORT void HWRAPI(KillShaders) (void)
{
	// unused.........................
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
			pglTexImage2D(GL_TEXTURE_2D, 0, textureformatGL, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitepixel);
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
	pglClearDepth(1.0f);
	pglDepthRange(0.0f, 1.0f);
	pglDepthFunc(GL_LEQUAL);

	pglEnable(GL_BLEND);
	pglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// this set CurrentPolyFlags to the actual configuration
	CurrentPolyFlags = 0xffffffff;
	SetBlend(0);

	tex_downloaded = 0;
	SetNoTexture();
}


// -----------------+
// Flush            : flush OpenGL textures
//                  : Clear list of downloaded mipmaps
// -----------------+
void Flush(void)
{
	//GL_DBG_Printf ("HWR_Flush()\n");

	while (gl_cachehead)
	{
		if (gl_cachehead->downloaded)
			pglDeleteTextures(1, (GLuint *)&gl_cachehead->downloaded);
		gl_cachehead->downloaded = 0;
		gl_cachehead = gl_cachehead->nextmipmap;
	}
	gl_cachetail = gl_cachehead = NULL; //Hurdler: well, gl_cachehead is already NULL

	tex_downloaded = 0;
}


// -----------------+
// isExtAvailable   : Look if an OpenGL extension is available
// Returns          : true if extension available
// -----------------+
INT32 isExtAvailable(const char *extension, const GLubyte *start)
{
	GLubyte         *where, *terminator;

	if (!extension || !start) return 0;
	where = (GLubyte *) strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	for (;;)
	{
		where = (GLubyte *) strstr((const char *) start, extension);
		if (!where)
			break;
		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return 1;
		start = terminator;
	}
	return 0;
}


// -----------------+
// Init             : Initialise the OpenGL interface API
// Returns          :
// -----------------+
EXPORT boolean HWRAPI(Init) (void)
{
	return LoadGL();
}


// -----------------+
// ClearMipMapCache : Flush OpenGL textures from memory
// -----------------+
EXPORT void HWRAPI(ClearMipMapCache) (void)
{
	// GL_DBG_Printf ("HWR_Flush(exe)\n");
	Flush();
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
	// GL_DBG_Printf ("ReadRect()\n");
	if (dst_stride == width*3)
	{
		GLubyte*top = (GLvoid*)dst_data, *bottom = top + dst_stride * (height - 1);
		GLubyte *row = malloc(dst_stride);
		if (!row) return;
		pglPixelStorei(GL_PACK_ALIGNMENT, 1);
		pglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, dst_data);
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
	else
	{
		INT32 j;
		GLubyte *image = malloc(width*height*3*sizeof (*image));
		if (!image) return;
		pglPixelStorei(GL_PACK_ALIGNMENT, 1);
		pglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, image);
		pglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		for (i = height-1; i >= 0; i--)
		{
			for (j = 0; j < width; j++)
			{
				dst_data[(height-1-i)*width+j] =
				(UINT16)(
				                 ((image[(i*width+j)*3]>>3)<<11) |
				                 ((image[(i*width+j)*3+1]>>2)<<5) |
				                 ((image[(i*width+j)*3+2]>>3)));
			}
		}
		free(image);
	}
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
		pglClearDepth(1.0f);     //Hurdler: all that are permanen states
		pglDepthRange(0.0f, 1.0f);
		pglDepthFunc(GL_LEQUAL);
		ClearMask |= GL_DEPTH_BUFFER_BIT;
	}

	SetBlend(DepthMask ? PF_Occlude | CurrentPolyFlags : CurrentPolyFlags&~PF_Occlude);

	pglClear(ClearMask);

	pglEnableVertexAttribArray(LOC_POSITION);
	pglEnableVertexAttribArray(LOC_TEXCOORD);
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

	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, p);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void Clamp2D(GLenum pname)
{
	pglTexParameteri(GL_TEXTURE_2D, pname, GL_CLAMP); // fallback clamp
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
	if (Xor & (PF_Blending|PF_RemoveYWrap|PF_ForceWrapX|PF_ForceWrapY|PF_Occlude|PF_NoTexture|PF_Modulated|PF_NoDepthTest|PF_Decal|PF_Invisible|PF_NoAlphaTest))
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
		if (Xor & PF_NoAlphaTest)
		{
			if (PolyFlags & PF_NoAlphaTest)
				pglDisable(GL_ALPHA_TEST);
			else
				pglEnable(GL_ALPHA_TEST);      // discard 0 alpha pixels (holes in texture)
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

	if (updatemipmap)
		pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, ptex);
	else
		pglTexImage2D(GL_TEXTURE_2D, 0, textureformatGL, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, ptex);

	if (MipMap)
		pglGenerateMipmap(GL_TEXTURE_2D);

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

static void Shader_SetTransform(void)
{
	if (shader_current == NULL)
		return;

	if (memcmp(projMatrix, shader_current->projMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader_current->projMatrix, projMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader_current->uniforms[gluniform_projection], 1, GL_FALSE, (float *)projMatrix);
	}

	if (memcmp(viewMatrix, shader_current->viewMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader_current->viewMatrix, viewMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader_current->uniforms[gluniform_view], 1, GL_FALSE, (float *)viewMatrix);
	}

	if (memcmp(modelMatrix, shader_current->modelMatrix, sizeof(fmatrix4_t)))
	{
		memcpy(shader_current->modelMatrix, modelMatrix, sizeof(fmatrix4_t));
		pglUniformMatrix4fv(shader_current->uniforms[gluniform_model], 1, GL_FALSE, (float *)modelMatrix);
	}
}

static void Shader_SetUniforms(FSurfaceInfo *Surface, GLRGBAFloat *poly, GLRGBAFloat *tint, GLRGBAFloat *fade)
{
	gl_shaderprogram_t *shader = shader_current;

	if (shader_current == NULL)
		return;

	if (!shader->program)
		return;

	#define UNIFORM_1(uniform, a, function) \
		if (uniform != -1) \
			function (uniform, a);

	#define UNIFORM_2(uniform, a, b, function) \
		if (uniform != -1) \
			function (uniform, a, b);

	#define UNIFORM_3(uniform, a, b, c, function) \
		if (uniform != -1) \
			function (uniform, a, b, c);

	#define UNIFORM_4(uniform, a, b, c, d, function) \
		if (uniform != -1) \
			function (uniform, a, b, c, d);

	if (poly)
		UNIFORM_4(shader->uniforms[gluniform_poly_color], poly->red, poly->green, poly->blue, poly->alpha, pglUniform4f);
	if (tint)
		UNIFORM_4(shader->uniforms[gluniform_tint_color], tint->red, tint->green, tint->blue, tint->alpha, pglUniform4f);
	if (fade)
		UNIFORM_4(shader->uniforms[gluniform_fade_color], fade->red, fade->green, fade->blue, fade->alpha, pglUniform4f);

	if (Surface != NULL)
	{
		UNIFORM_1(shader->uniforms[gluniform_lighting], Surface->LightInfo.light_level, pglUniform1f);
		UNIFORM_1(shader->uniforms[gluniform_fade_start], Surface->LightInfo.fade_start, pglUniform1f);
		UNIFORM_1(shader->uniforms[gluniform_fade_end], Surface->LightInfo.fade_end, pglUniform1f);
	}

	UNIFORM_1(shader->uniforms[gluniform_leveltime], ((float)shader_leveltime) / TICRATE, pglUniform1f);

	#undef UNIFORM_1
	#undef UNIFORM_2
	#undef UNIFORM_3
	#undef UNIFORM_4
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

	if ((PolyFlags & PF_Corona) && (oglflags & GLF_NOZBUFREAD))
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

	// this test is added for new coronas' code (without depth buffer)
	// I think I should do a separate function for drawing coronas, so it will be a little faster
	if (PolyFlags & PF_Corona) // check to see if we need to draw the corona
	{
		FUINT i;
		FUINT j;

		//rem: all 8 (or 8.0f) values are hard coded: it can be changed to a higher value
		GLfloat     buf[8][8];
		GLfloat    cx, cy, cz;
		GLfloat    px = 0.0f, py = 0.0f, pz = -1.0f;
		GLfloat     scalef = 0.0f;

		cx = (pOutVerts[0].x + pOutVerts[2].x) / 2.0f; // we should change the coronas' ...
		cy = (pOutVerts[0].y + pOutVerts[2].y) / 2.0f; // ... code so its only done once.
		cz = pOutVerts[0].z;

		// I dont know if this is slow or not
		GLProject(cx, cy, cz, &px, &py, &pz);
		//GL_DBG_Printf("Projection: (%f, %f, %f)\n", px, py, pz);

		if ((pz <  0.0l) ||
			(px < -8.0l) ||
			(py < viewport[1]-8.0l) ||
			(px > viewport[2]+8.0l) ||
			(py > viewport[1]+viewport[3]+8.0l))
			return;

		// the damned slow glReadPixels functions :(
		pglReadPixels((INT32)px-4, (INT32)py, 8, 8, GL_DEPTH_COMPONENT, GL_FLOAT, buf);
		//GL_DBG_Printf("DepthBuffer: %f %f\n", buf[0][0], buf[3][3]);

		for (i = 0; i < 8; i++)
			for (j = 0; j < 8; j++)
				scalef += (pz > buf[i][j]+0.00005f) ? 0 : 1;

		// quick test for screen border (not 100% correct, but looks ok)
		if (px < 4) scalef -= (GLfloat)(8*(4-px));
		if (py < viewport[1]+4) scalef -= (GLfloat)(8*(viewport[1]+4-py));
		if (px > viewport[2]-4) scalef -= (GLfloat)(8*(4-(viewport[2]-px)));
		if (py > viewport[1]+viewport[3]-4) scalef -= (GLfloat)(8*(4-(viewport[1]+viewport[3]-py)));

		scalef /= 64;
		//GL_DBG_Printf("Scale factor: %f\n", scalef);

		if (scalef < 0.05f)
			return;

		pSurf->PolyColor.s.alpha *= scalef; // change the alpha value (it seems better than changing the size of the corona)
	}

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

	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

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

	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].x);
	pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(FOutVector), &pOutVerts[0].s);

	pglDrawElements(GL_TRIANGLES, iNumPts, GL_UNSIGNED_INT, IndexArray);

	// the DrawPolygon variant of this has some code about polyflags and wrapping here but havent noticed any problems from omitting it?
}

typedef struct vbo_vertex_s
{
	float x, y, z;
	float u, v;
	float r, g, b, a;
} vbo_vertex_t;

typedef struct
{
	int mode;
	int vertexcount;
	int vertexindex;
	int use_texture;
} GLSkyLoopDef;

typedef struct
{
	unsigned int id;
	int rows, columns;
	int loopcount;
	GLSkyLoopDef *loops;
	vbo_vertex_t *data;
} GLSkyVBO;

static const boolean gl_ext_arb_vertex_buffer_object = true;

#define NULL_VBO_VERTEX ((vbo_vertex_t*)NULL)
#define sky_vbo_x (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->x : &vbo->data[0].x)
#define sky_vbo_u (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->u : &vbo->data[0].u)
#define sky_vbo_r (gl_ext_arb_vertex_buffer_object ? &NULL_VBO_VERTEX->r : &vbo->data[0].r)

// The texture offset to be applied to the texture coordinates in SkyVertex().
static int rows, columns;
static signed char yflip;
static int texw, texh;
static boolean foglayer;
static float delta = 0.0f;

static int gl_sky_detail = 16;

static INT32 lasttex = -1;

#define MAP_COEFF 128.0f

static void SkyVertex(vbo_vertex_t *vbo, int r, int c)
{
	const float radians = (float)(M_PIl / 180.0f);
	const float scale = 10000.0f;
	const float maxSideAngle = 60.0f;

	float topAngle = (c / (float)columns * 360.0f);
	float sideAngle = (maxSideAngle * (rows - r) / rows);
	float height = (float)(sin(sideAngle * radians));
	float realRadius = (float)(scale * cos(sideAngle * radians));
	float x = (float)(realRadius * cos(topAngle * radians));
	float y = (!yflip) ? scale * height : -scale * height;
	float z = (float)(realRadius * sin(topAngle * radians));
	float timesRepeat = (4 * (256.0f / texw));
	if (fpclassify(timesRepeat) == FP_ZERO)
		timesRepeat = 1.0f;

	if (!foglayer)
	{
		vbo->r = 1.0f;
		vbo->g = 1.0f;
		vbo->b = 1.0f;
		vbo->a = (r == 0 ? 0.0f : 1.0f);

		// And the texture coordinates.
		vbo->u = (-timesRepeat * c / (float)columns);
		if (!yflip)	// Flipped Y is for the lower hemisphere.
			vbo->v = (r / (float)rows) + 0.5f;
		else
			vbo->v = 1.0f + ((rows - r) / (float)rows) + 0.5f;
	}

	if (r != 4)
	{
		y += 300.0f;
	}

	// And finally the vertex.
	vbo->x = x;
	vbo->y = y + delta;
	vbo->z = z;
}

static GLSkyVBO sky_vbo;

static void gld_BuildSky(int row_count, int col_count)
{
	int c, r;
	vbo_vertex_t *vertex_p;
	int vertex_count = 2 * row_count * (col_count * 2 + 2) + col_count * 2;

	GLSkyVBO *vbo = &sky_vbo;

	if ((vbo->columns != col_count) || (vbo->rows != row_count))
	{
		free(vbo->loops);
		free(vbo->data);
		memset(vbo, 0, sizeof(&vbo));
	}

	if (!vbo->data)
	{
		memset(vbo, 0, sizeof(&vbo));
		vbo->loops = malloc((row_count * 2 + 2) * sizeof(vbo->loops[0]));
		// create vertex array
		vbo->data = malloc(vertex_count * sizeof(vbo->data[0]));
	}

	vbo->columns = col_count;
	vbo->rows = row_count;

	vertex_p = &vbo->data[0];
	vbo->loopcount = 0;

	for (yflip = 0; yflip < 2; yflip++)
	{
		vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_FAN;
		vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
		vbo->loops[vbo->loopcount].vertexcount = col_count;
		vbo->loops[vbo->loopcount].use_texture = false;
		vbo->loopcount++;

		delta = 0.0f;
		foglayer = true;
		for (c = 0; c < col_count; c++)
		{
			SkyVertex(vertex_p, 1, c);
			vertex_p->r = 1.0f;
			vertex_p->g = 1.0f;
			vertex_p->b = 1.0f;
			vertex_p->a = 1.0f;
			vertex_p++;
		}
		foglayer = false;

		delta = (yflip ? 5.0f : -5.0f) / MAP_COEFF;

		for (r = 0; r < row_count; r++)
		{
			vbo->loops[vbo->loopcount].mode = GL_TRIANGLE_STRIP;
			vbo->loops[vbo->loopcount].vertexindex = vertex_p - &vbo->data[0];
			vbo->loops[vbo->loopcount].vertexcount = 2 * col_count + 2;
			vbo->loops[vbo->loopcount].use_texture = true;
			vbo->loopcount++;

			for (c = 0; c <= col_count; c++)
			{
				SkyVertex(vertex_p++, r + (yflip ? 1 : 0), (c ? c : 0));
				SkyVertex(vertex_p++, r + (yflip ? 0 : 1), (c ? c : 0));
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static void RenderDome(INT32 skytexture)
{
	int i, j;
	int vbosize;
	GLSkyVBO *vbo = &sky_vbo;

	fvector3_t scale;
	scale[0] = scale[2] = 1.0f;

	rows = 4;
	columns = 4 * gl_sky_detail;

	vbosize = 2 * rows * (columns * 2 + 2) + columns * 2;

	// Build the sky dome! Yes!
	if (lasttex != skytexture)
	{
		// delete VBO when already exists
		if (gl_ext_arb_vertex_buffer_object)
		{
			if (vbo->id)
				pglDeleteBuffers(1, &vbo->id);
		}

		lasttex = skytexture;
		gld_BuildSky(rows, columns);

		if (gl_ext_arb_vertex_buffer_object)
		{
			// generate a new VBO and get the associated ID
			pglGenBuffers(1, &vbo->id);

			// bind VBO in order to use
			pglBindBuffer(GL_ARRAY_BUFFER, vbo->id);

			// upload data to VBO
			pglBufferData(GL_ARRAY_BUFFER, vbosize * sizeof(vbo->data[0]), vbo->data, GL_STATIC_DRAW);
		}
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
	pglEnableVertexAttribArray(LOC_COLORS);
	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_x);
	pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_u);
	pglVertexAttribPointer(LOC_COLORS, 4, GL_FLOAT, GL_FALSE, sizeof(vbo->data[0]), sky_vbo_r);

	// set transforms
	scale[1] = ((float)texh / 230.0f);
	lzml_matrix4_scale(viewMatrix, scale);
	lzml_matrix4_rotate_y(viewMatrix, Deg2Rad(270.0f));

	Shader_SetTransform();

	for (j = 0; j < 2; j++)
	{
		for (i = 0; i < vbo->loopcount; i++)
		{
			GLSkyLoopDef *loop = &vbo->loops[i];

			if (j == 0 ? loop->use_texture : !loop->use_texture)
				continue;

			pglDrawArrays(loop->mode, loop->vertexindex, loop->vertexcount);
		}
	}

	Shader_SetUniforms(NULL, &white, NULL, NULL);
	pglDisableVertexAttribArray(LOC_COLORS);

	// bind with 0, so, switch back to normal pointer operation
	if (gl_ext_arb_vertex_buffer_object)
		pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

EXPORT void HWRAPI(RenderSkyDome) (INT32 tex, INT32 texture_width, INT32 texture_height, FTransform transform)
{
	SetBlend(PF_Translucent|PF_NoDepthTest|PF_Modulated);
	SetTransform(&transform);
	texw = texture_width;
	texh = texture_height;
	RenderDome(tex);
	SetBlend(0);
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
					shader_enabled = true;
					break;
				default:
					shader_enabled = false;
					break;
			}
			break;

		case HWD_SET_TEXTUREFILTERMODE:
			switch (Value)
			{
				case HWD_SET_TEXTUREFILTER_TRILINEAR:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_LINEAR;
					MipMap = GL_TRUE;
					break;
				case HWD_SET_TEXTUREFILTER_BILINEAR:
					min_filter = mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_POINTSAMPLED:
					min_filter = mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED1:
					min_filter = GL_NEAREST;
					mag_filter = GL_LINEAR;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED2:
					min_filter = GL_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_FALSE;
					break;
				case HWD_SET_TEXTUREFILTER_MIXED3:
					min_filter = GL_LINEAR_MIPMAP_LINEAR;
					mag_filter = GL_NEAREST;
					MipMap = GL_TRUE;
					break;
				default:
					mag_filter = GL_LINEAR;
					min_filter = GL_NEAREST;
			}
			Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		case HWD_SET_TEXTUREANISOTROPICMODE:
			anisotropic_filter = min(Value,maximumAnisotropy);
			if (maximumAnisotropy)
				Flush(); //??? if we want to change filter mode by texture, remove this
			break;

		default:
			break;
	}
}

static float *vertBuffer = NULL;
static float *normBuffer = NULL;
static size_t lerpBufferSize = 0;
static short *vertTinyBuffer = NULL;
static char *normTinyBuffer = NULL;
static size_t lerpTinyBufferSize = 0;

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpBuffer(size_t size)
{
	if (lerpBufferSize >= size)
		return;

	if (vertBuffer != NULL)
		free(vertBuffer);

	if (normBuffer != NULL)
		free(normBuffer);

	lerpBufferSize = size;
	vertBuffer = malloc(lerpBufferSize);
	normBuffer = malloc(lerpBufferSize);
}

// Static temporary buffer for doing frame interpolation
// 'size' is the vertex size
static void AllocLerpTinyBuffer(size_t size)
{
	if (lerpTinyBufferSize >= size)
		return;

	if (vertTinyBuffer != NULL)
		free(vertTinyBuffer);

	if (normTinyBuffer != NULL)
		free(normTinyBuffer);

	lerpTinyBufferSize = size;
	vertTinyBuffer = malloc(lerpTinyBufferSize);
	normTinyBuffer = malloc(lerpTinyBufferSize / 2);
}

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

static void CreateModelVBO(mesh_t *mesh, mdlframe_t *frame)
{
	int bufferSize = sizeof(vbo64_t)*mesh->numTriangles * 3;
	vbo64_t *buffer = (vbo64_t*)malloc(bufferSize);
	vbo64_t *bufPtr = buffer;

	float *vertPtr = frame->vertices;
	float *normPtr = frame->normals;
	float *tanPtr = frame->tangents;
	float *uvPtr = mesh->uvs;
	float *lightPtr = mesh->lightuvs;
	char *colorPtr = frame->colors;

	int i;
	for (i = 0; i < mesh->numTriangles * 3; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr != NULL)
		{
			bufPtr->tan0 = *tanPtr++;
			bufPtr->tan1 = *tanPtr++;
			bufPtr->tan2 = *tanPtr++;
		}

		if (lightPtr != NULL)
		{
			bufPtr->s1 = *lightPtr++;
			bufPtr->t1 = *lightPtr++;
		}

		if (colorPtr)
		{
			bufPtr->r = *colorPtr++;
			bufPtr->g = *colorPtr++;
			bufPtr->b = *colorPtr++;
			bufPtr->a = *colorPtr++;
		}
		else
		{
			bufPtr->r = 255;
			bufPtr->g = 255;
			bufPtr->b = 255;
			bufPtr->a = 255;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void CreateModelVBOTiny(mesh_t *mesh, tinyframe_t *frame)
{
	int bufferSize = sizeof(vbotiny_t)*mesh->numTriangles * 3;
	vbotiny_t *buffer = (vbotiny_t*)malloc(bufferSize);
	vbotiny_t *bufPtr = buffer;

	short *vertPtr = frame->vertices;
	char *normPtr = frame->normals;
	float *uvPtr = mesh->uvs;
	char *tanPtr = frame->tangents;

	int i;
	for (i = 0; i < mesh->numVertices; i++)
	{
		bufPtr->x = *vertPtr++;
		bufPtr->y = *vertPtr++;
		bufPtr->z = *vertPtr++;

		bufPtr->nx = *normPtr++;
		bufPtr->ny = *normPtr++;
		bufPtr->nz = *normPtr++;

		bufPtr->s0 = *uvPtr++;
		bufPtr->t0 = *uvPtr++;

		if (tanPtr)
		{
			bufPtr->tanx = *tanPtr++;
			bufPtr->tany = *tanPtr++;
			bufPtr->tanz = *tanPtr++;
		}

		bufPtr++;
	}

	pglGenBuffers(1, &frame->vboID);
	pglBindBuffer(GL_ARRAY_BUFFER, frame->vboID);
	pglBufferData(GL_ARRAY_BUFFER, bufferSize, buffer, GL_STATIC_DRAW);
	free(buffer);

	// Don't leave the array buffer bound to the model,
	// since this is called mid-frame
	pglBindBuffer(GL_ARRAY_BUFFER, 0);
}

EXPORT void HWRAPI(CreateModelVBOs) (model_t *model)
{
	int i;
	for (i = 0; i < model->numMeshes; i++)
	{
		mesh_t *mesh = &model->meshes[i];

		if (mesh->frames)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				mdlframe_t *frame = &mesh->frames[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBO(mesh, frame);
			}
		}
		else if (mesh->tinyframes)
		{
			int j;
			for (j = 0; j < model->meshes[i].numFrames; j++)
			{
				tinyframe_t *frame = &mesh->tinyframes[j];
				if (frame->vboID)
					pglDeleteBuffers(1, &frame->vboID);
				frame->vboID = 0;
				CreateModelVBOTiny(mesh, frame);
			}
		}
	}
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

	pglEnableVertexAttribArray(LOC_NORMAL);

	Shader_SetUniforms(Surface, &poly, &tint, &fade);

	pglEnable(GL_CULL_FACE);
	pglEnable(GL_NORMALIZE);

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

				pglVertexAttribPointer(LOC_POSITION, 3, GL_SHORT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(0));
				pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short) * 3 + sizeof(char) * 6));
				pglVertexAttribPointer(LOC_NORMAL, 3, GL_BYTE, GL_FALSE, sizeof(vbotiny_t), BUFFER_OFFSET(sizeof(short)*3));

				pglDrawElements(GL_TRIANGLES, mesh->numTriangles * 3, GL_UNSIGNED_SHORT, mesh->indices);
				pglBindBuffer(GL_ARRAY_BUFFER, 0);
			}
			else
			{
				short *vertPtr;
				char *normPtr;
				int j = 0;

				// Dangit, I soooo want to do this in a GLSL shader...
				AllocLerpTinyBuffer(mesh->numVertices * sizeof(short) * 3);
				vertPtr = vertTinyBuffer;
				normPtr = normTinyBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = (short)(frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j])));
					*normPtr++ = (char)(frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j])));
				}

				pglVertexAttribPointer(LOC_POSITION, 3, GL_SHORT, GL_FALSE, 0, vertTinyBuffer);
				pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(LOC_NORMAL, 3, GL_BYTE, GL_FALSE, 0, normTinyBuffer);

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

				pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(0));
				pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 6));
				pglVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(vbo64_t), BUFFER_OFFSET(sizeof(float) * 3));

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
				AllocLerpBuffer(mesh->numVertices * sizeof(float) * 3);
				vertPtr = vertBuffer;
				normPtr = normBuffer;

				for (j = 0; j < mesh->numVertices * 3; j++)
				{
					// Interpolate
					*vertPtr++ = frame->vertices[j] + (pol * (nextframe->vertices[j] - frame->vertices[j]));
					*normPtr++ = frame->normals[j] + (pol * (nextframe->normals[j] - frame->normals[j]));
				}

				pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, vertBuffer);
				pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, mesh->uvs);
				pglVertexAttribPointer(LOC_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, normBuffer);

				pglDrawArrays(GL_TRIANGLES, 0, mesh->numVertices);
			}
		}
	}

	lzml_matrix4_identity(modelMatrix);
	Shader_SetTransform();

	pglDisableVertexAttribArray(LOC_NORMAL);

	pglDisable(GL_CULL_FACE);
	pglDisable(GL_NORMALIZE);
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

EXPORT INT32  HWRAPI(GetTextureUsed) (void)
{
	FTextureInfo *tmp = gl_cachehead;
	INT32 res = 0;

	while (tmp)
	{
		// Figure out the correct bytes-per-pixel for this texture
		// I don't know which one the game actually _uses_ but this
		// follows format2bpp in hw_cache.c
		int bpp = 1;
		int format = tmp->format;
		if (format == GL_TEXFMT_RGBA)
			bpp = 4;
		else if (format == GL_TEXFMT_ALPHA_INTENSITY_88 || format == GL_TEXFMT_AP_88)
			bpp = 2;

		// Add it up!
		res += tmp->height*tmp->width*bpp;
		tmp = tmp->nextmipmap;
	}

	return res;
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

	pglDisableVertexAttribArray(LOC_TEXCOORD);

	// Draw a black square behind the screen texture,
	// so nothing shows through the edges
	Shader_SetUniforms(NULL, &black, NULL, NULL);
	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, blackBack);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	pglEnableVertexAttribArray(LOC_TEXCOORD);
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

			pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, stCoords);

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

			pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, vertCoords);

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

	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, fix);
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

	pglDisableVertexAttribArray(LOC_COLORS);
	pglEnableVertexAttribArray(LOC_TEXCOORD1);

	shader = &gl_shaderprograms[tinted ? SHADER_FADEMASK_ADDITIVEANDSUBTRACTIVE : SHADER_FADEMASK];
	changed = Shader_SetProgram(shader);

	if (changed)
	{
#define SETSAMPLER(uni, a) \
	if (shader->uniforms[uni] != -1) \
		pglUniform1i(shader->uniforms[uni], a);

		SETSAMPLER(gluniform_startscreen, 0);
		SETSAMPLER(gluniform_endscreen, 1);
		SETSAMPLER(gluniform_fademask, 2);

		if (tinted)
		{
			SETSAMPLER(gluniform_isfadingin, isfadingin);
			SETSAMPLER(gluniform_istowhite, istowhite);
		}

#undef SETSAMPLER

		Shader_SetUniforms(NULL, &white, NULL, NULL);
		Shader_SetTransform();
	}

	pglActiveTexture(GL_TEXTURE0 + 0);
	pglBindTexture(GL_TEXTURE_2D, startScreenWipe);
	pglActiveTexture(GL_TEXTURE0 + 1);
	pglBindTexture(GL_TEXTURE_2D, endScreenWipe);
	pglActiveTexture(GL_TEXTURE0 + 2);
	pglBindTexture(GL_TEXTURE_2D, fademaskdownloaded);

	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, screenVerts);
	pglVertexAttribPointer(LOC_TEXCOORD0, 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglVertexAttribPointer(LOC_TEXCOORD1, 2, GL_FLOAT, GL_FALSE, 0, defaultST);

	pglActiveTexture(GL_TEXTURE0);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	pglDisableVertexAttribArray(LOC_TEXCOORD1);

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
	pglVertexAttribPointer(LOC_POSITION, 3, GL_FLOAT, GL_FALSE, 0, off);
	pglVertexAttribPointer(LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, fix);
	pglDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	tex_downloaded = finalScreenTexture;
}

#endif //HWRENDER
