// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file r_glcommon.h
/// \brief Common OpenGL functions and structs shared by OpenGL backends

#ifndef _R_GLCOMMON_H_
#define _R_GLCOMMON_H_

#define GL_GLEXT_PROTOTYPES

#if defined(HAVE_GLES2)
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>
#elif defined(HAVE_GLES)
	#include <GLES/gl.h>
	#include <GLES/glext.h>
#else
	#ifdef HAVE_SDL
		#define _MATH_DEFINES_DEFINED

		#ifdef _MSC_VER
		#pragma warning(disable : 4214 4244)
		#endif

		#include "SDL_opengl.h" //Alam_GBC: Simple, yes?

		#ifdef _MSC_VER
		#pragma warning(default : 4214 4244)
		#endif
	#else
		#include <GL/gl.h>
		#include <GL/glu.h>
		#if defined(STATIC_OPENGL)
			#include <GL/glext.h>
		#endif
	#endif
#endif

// For GL_ExtensionAvailable
#ifdef HAVE_SDL
#define _MATH_DEFINES_DEFINED
#include "SDL.h"
#endif

#include "../../doomdata.h"
#include "../../doomtype.h"
#include "../../doomdef.h"

#include "../../hardware/hw_data.h"  // GLMipmap_s
#include "../../hardware/hw_defs.h"  // FTextureInfo
#include "../../hardware/hw_model.h" // model_t / mesh_t / mdlframe_t
#include "../r_opengl/r_vbo.h"

void GL_DBG_Printf(const char *format, ...);
void GL_MSG_Warning(const char *format, ...);
void GL_MSG_Error(const char *format, ...);

#ifdef DEBUG_TO_FILE
extern FILE *gllogstream;
#endif

#ifndef R_GL_APIENTRY
	#if defined(_WIN32)
		#define R_GL_APIENTRY APIENTRY
	#else
		#define R_GL_APIENTRY
	#endif
#endif

// ==========================================================================
//                                                                     PROTOS
// ==========================================================================

#ifdef STATIC_OPENGL
/* Miscellaneous */
#define pglClear glClear
#define pglGetFloatv glGetFloatv
#define pglGetIntegerv glGetIntegerv
#define pglGetString glGetString
#define pglClearColor glClearColor
#define pglColorMask glColorMask
#define pglAlphaFunc glAlphaFunc
#define pglBlendFunc glBlendFunc
#define pglCullFace glCullFace
#define pglPolygonOffset glPolygonOffset
#define pglEnable glEnable
#define pglDisable glDisable

/* Depth buffer */
#define pglDepthMask glDepthMask
#define pglDepthRange glDepthRange

/* Transformation */
#define pglViewport glViewport

/* Raster functions */
#define pglPixelStorei glPixelStorei
#define pglReadPixels glReadPixels

/* Texture mapping */
#define pglTexParameteri glTexParameteri
#define pglTexImage2D glTexImage2D
#define pglTexSubImage2D glTexSubImage2D

/* Drawing functions */
#define pglDrawArrays glDrawArrays
#define pglDrawElements glDrawElements

/* Texture objects */
#define pglGenTextures glGenTextures
#define pglDeleteTextures glDeleteTextures
#define pglBindTexture glBindTexture

/* Texture mapping */
#define pglCopyTexImage2D glCopyTexImage2D
#define pglCopyTexSubImage2D glCopyTexSubImage2D

/* 1.3 functions for multitexturing */
#define pglActiveTexture glActiveTexture
#else
/* Miscellaneous */
typedef void (R_GL_APIENTRY * PFNglClear) (GLbitfield mask);
extern PFNglClear pglClear;
typedef void (R_GL_APIENTRY * PFNglGetFloatv) (GLenum pname, GLfloat *params);
extern PFNglGetFloatv pglGetFloatv;
typedef void (R_GL_APIENTRY * PFNglGetIntegerv) (GLenum pname, GLint *params);
extern PFNglGetIntegerv pglGetIntegerv;
typedef const GLubyte * (R_GL_APIENTRY * PFNglGetString) (GLenum name);
extern PFNglGetString pglGetString;
typedef void (R_GL_APIENTRY * PFNglClearColor) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern PFNglClearColor pglClearColor;
typedef void (R_GL_APIENTRY * PFNglColorMask) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
extern PFNglColorMask pglColorMask;
typedef void (R_GL_APIENTRY * PFNglAlphaFunc) (GLenum func, GLclampf ref);
extern PFNglAlphaFunc pglAlphaFunc;
typedef void (R_GL_APIENTRY * PFNglBlendFunc) (GLenum sfactor, GLenum dfactor);
extern PFNglBlendFunc pglBlendFunc;
typedef void (R_GL_APIENTRY * PFNglCullFace) (GLenum mode);
extern PFNglCullFace pglCullFace;
typedef void (R_GL_APIENTRY * PFNglPolygonOffset) (GLfloat factor, GLfloat units);
extern PFNglPolygonOffset pglPolygonOffset;
typedef void (R_GL_APIENTRY * PFNglEnable) (GLenum cap);
extern PFNglEnable pglEnable;
typedef void (R_GL_APIENTRY * PFNglDisable) (GLenum cap);
extern PFNglDisable pglDisable;

/* Depth buffer */
typedef void (R_GL_APIENTRY * PFNglDepthFunc) (GLenum func);
extern PFNglDepthFunc pglDepthFunc;
typedef void (R_GL_APIENTRY * PFNglDepthMask) (GLboolean flag);
extern PFNglDepthMask pglDepthMask;

/* Transformation */
typedef void (R_GL_APIENTRY * PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
extern PFNglViewport pglViewport;

/* Raster functions */
typedef void (R_GL_APIENTRY * PFNglPixelStorei) (GLenum pname, GLint param);
extern PFNglPixelStorei pglPixelStorei;
typedef void (R_GL_APIENTRY * PFNglReadPixels) (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
extern PFNglReadPixels pglReadPixels;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglTexParameteri) (GLenum target, GLenum pname, GLint param);
extern PFNglTexParameteri pglTexParameteri;
typedef void (R_GL_APIENTRY * PFNglTexImage2D) (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
extern PFNglTexImage2D pglTexImage2D;
typedef void (R_GL_APIENTRY * PFNglTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
extern PFNglTexSubImage2D pglTexSubImage2D;

/* Drawing functions */
typedef void (R_GL_APIENTRY * PFNglDrawArrays) (GLenum mode, GLint first, GLsizei count);
extern PFNglDrawArrays pglDrawArrays;
typedef void (R_GL_APIENTRY * PFNglDrawElements) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
extern PFNglDrawElements pglDrawElements;

/* Texture objects */
typedef void (R_GL_APIENTRY * PFNglGenTextures) (GLsizei n, const GLuint *textures);
extern PFNglGenTextures pglGenTextures;
typedef void (R_GL_APIENTRY * PFNglDeleteTextures) (GLsizei n, const GLuint *textures);
extern PFNglDeleteTextures pglDeleteTextures;
typedef void (R_GL_APIENTRY * PFNglBindTexture) (GLenum target, GLuint texture);
extern PFNglBindTexture pglBindTexture;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglCopyTexImage2D) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
extern PFNglCopyTexImage2D pglCopyTexImage2D;
typedef void (R_GL_APIENTRY * PFNglCopyTexSubImage2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
extern PFNglCopyTexSubImage2D pglCopyTexSubImage2D;

/* 1.3 functions for multitexturing */
typedef void (R_GL_APIENTRY * PFNglActiveTexture) (GLenum);
extern PFNglActiveTexture pglActiveTexture;
#endif

//
// Multi-texturing
//

#ifndef HAVE_GLES2
	#ifdef STATIC_OPENGL
		#define pglMultiTexCoord2f glMultiTexCoord2f
		#define pglMultiTexCoord2fv glMultiTexCoord2fv
		#define pglClientActiveTexture glClientActiveTexture
	#else
		typedef void (R_GL_APIENTRY * PFNglMultiTexCoord2f) (GLenum, GLfloat, GLfloat);
		extern PFNglMultiTexCoord2f pglMultiTexCoord2f;
		typedef void (R_GL_APIENTRY * PFNglMultiTexCoord2fv) (GLenum target, const GLfloat *v);
		extern PFNglMultiTexCoord2fv pglMultiTexCoord2fv;
		typedef void (R_GL_APIENTRY * PFNglClientActiveTexture) (GLenum);
		extern PFNglClientActiveTexture pglClientActiveTexture;
	#endif
#endif

//
// Mipmapping
//

#ifdef HAVE_GLES
/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglGenerateMipmap) (GLenum target);
extern PFNglGenerateMipmap pglGenerateMipmap;
#endif

//
// Depth functions
//
#ifndef HAVE_GLES
	#ifdef STATIC_OPENGL
		#define pglClearDepth glClearDepth
		#define pglDepthFunc glDepthFunc
	#else
		typedef void (R_GL_APIENTRY * PFNglClearDepth) (GLclampd depth);
		extern PFNglClearDepth pglClearDepth;
		typedef void (R_GL_APIENTRY * PFNglDepthRange) (GLclampd near_val, GLclampd far_val);
		extern PFNglDepthRange pglDepthRange;
	#endif
#else
	#ifdef STATIC_OPENGL
		#define pglClearDepthf glClearDepthf
		#define pglDepthFuncf glDepthFuncf
	#else
		typedef void (R_GL_APIENTRY * PFNglClearDepthf) (GLclampf depth);
		extern PFNglClearDepthf pglClearDepthf;
		typedef void (R_GL_APIENTRY * PFNglDepthRangef) (GLclampf near_val, GLclampf far_val);
		extern PFNglDepthRangef pglDepthRangef;
	#endif
#endif

//
// Legacy functions
//

#ifndef HAVE_GLES2
#ifdef STATIC_OPENGL
/* Transformation */
#define pglMatrixMode glMatrixMode
#define pglViewport glViewport
#define pglPushMatrix glPushMatrix
#define pglPopMatrix glPopMatrix
#define pglLoadIdentity glLoadIdentity
#define pglMultMatrixf glMultMatrixf
#define pglRotatef glRotatef
#define pglScalef glScalef
#define pglTranslatef glTranslatef

/* Drawing functions */
#define pglVertexPointer glVertexPointer
#define pglNormalPointer glNormalPointer
#define pglTexCoordPointer glTexCoordPointer
#define pglColorPointer glColorPointer
#define pglEnableClientState glEnableClientState
#define pglDisableClientState glDisableClientState

/* Lighting */
#define pglShadeModel glShadeModel
#define pglLightfv glLightfv
#define pglLightModelfv glLightModelfv
#define pglMaterialfv glMaterialfv
#define pglMateriali glMateriali

/* Texture mapping */
#define pglTexEnvi glTexEnvi

#else // STATIC_OPENGL

typedef void (R_GL_APIENTRY * PFNglMatrixMode) (GLenum mode);
extern PFNglMatrixMode pglMatrixMode;
typedef void (R_GL_APIENTRY * PFNglViewport) (GLint x, GLint y, GLsizei width, GLsizei height);
extern PFNglViewport pglViewport;
typedef void (R_GL_APIENTRY * PFNglPushMatrix) (void);
extern PFNglPushMatrix pglPushMatrix;
typedef void (R_GL_APIENTRY * PFNglPopMatrix) (void);
extern PFNglPopMatrix pglPopMatrix;
typedef void (R_GL_APIENTRY * PFNglLoadIdentity) (void);
extern PFNglLoadIdentity pglLoadIdentity;
typedef void (R_GL_APIENTRY * PFNglMultMatrixf) (const GLfloat *m);
extern PFNglMultMatrixf pglMultMatrixf;
typedef void (R_GL_APIENTRY * PFNglRotatef) (GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
extern PFNglRotatef pglRotatef;
typedef void (R_GL_APIENTRY * PFNglScalef) (GLfloat x, GLfloat y, GLfloat z);
extern PFNglScalef pglScalef;
typedef void (R_GL_APIENTRY * PFNglTranslatef) (GLfloat x, GLfloat y, GLfloat z);
extern PFNglTranslatef pglTranslatef;

/* Drawing Functions */
typedef void (R_GL_APIENTRY * PFNglVertexPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglVertexPointer pglVertexPointer;
typedef void (R_GL_APIENTRY * PFNglNormalPointer) (GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglNormalPointer pglNormalPointer;
typedef void (R_GL_APIENTRY * PFNglTexCoordPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglTexCoordPointer pglTexCoordPointer;
typedef void (R_GL_APIENTRY * PFNglColorPointer) (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
extern PFNglColorPointer pglColorPointer;
typedef void (R_GL_APIENTRY * PFNglEnableClientState) (GLenum cap);
extern PFNglEnableClientState pglEnableClientState;
typedef void (R_GL_APIENTRY * PFNglDisableClientState) (GLenum cap);
extern PFNglDisableClientState pglDisableClientState;

/* Lighting */
typedef void (R_GL_APIENTRY * PFNglShadeModel) (GLenum mode);
extern PFNglShadeModel pglShadeModel;
typedef void (R_GL_APIENTRY * PFNglLightfv) (GLenum light, GLenum pname, GLfloat *params);
extern PFNglLightfv pglLightfv;
typedef void (R_GL_APIENTRY * PFNglLightModelfv) (GLenum pname, GLfloat *params);
extern PFNglLightModelfv pglLightModelfv;
typedef void (R_GL_APIENTRY * PFNglMaterialfv) (GLint face, GLenum pname, GLfloat *params);
extern PFNglMaterialfv pglMaterialfv;
typedef void (R_GL_APIENTRY * PFNglMateriali) (GLint face, GLenum pname, GLint param);
extern PFNglMateriali pglMateriali;

/* Texture mapping */
typedef void (R_GL_APIENTRY * PFNglTexEnvi) (GLenum target, GLenum pname, GLint param);
extern PFNglTexEnvi pglTexEnvi;
#endif
#endif // HAVE_GLES2

// Color
#ifdef HAVE_GLES
	#ifdef STATIC_OPENGL
		#define pglColor4f glColor4f
	#else
		typedef void (*PFNglColor4f) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
		extern PFNglColor4f pglColor4f;
	#endif
#else
	#ifdef STATIC_OPENGL
		#define pglColor4ubv glColor4ubv
	#else
		typedef void (R_GL_APIENTRY * PFNglColor4ubv) (const GLubyte *v);
		extern PFNglColor4ubv pglColor4ubv;
	#endif
#endif

/* 1.5 functions for buffers */
typedef void (R_GL_APIENTRY * PFNglGenBuffers) (GLsizei n, GLuint *buffers);
extern PFNglGenBuffers pglGenBuffers;
typedef void (R_GL_APIENTRY * PFNglBindBuffer) (GLenum target, GLuint buffer);
extern PFNglBindBuffer pglBindBuffer;
typedef void (R_GL_APIENTRY * PFNglBufferData) (GLenum target, GLsizei size, const GLvoid *data, GLenum usage);
extern PFNglBufferData pglBufferData;
typedef void (R_GL_APIENTRY * PFNglDeleteBuffers) (GLsizei n, const GLuint *buffers);
extern PFNglDeleteBuffers pglDeleteBuffers;

// ==========================================================================
//                                                                  FUNCTIONS
// ==========================================================================

void GLModel_GenerateVBOs(model_t *model);
void GLModel_AllocLerpBuffer(size_t size);
void GLModel_AllocLerpTinyBuffer(size_t size);

void  GLTexture_Flush(void);
void  GLTexture_SetFilterMode(INT32 mode);
INT32 GLTexture_GetMemoryUsage(FTextureInfo *head);

boolean GLBackend_Init(void);
boolean GLBackend_LoadFunctions(void);
boolean GLBackend_LoadExtraFunctions(void);
boolean GLBackend_LoadCommonFunctions(void);
boolean GLBackend_LoadLegacyFunctions(void);
void   *GLBackend_GetFunction(const char *proc);

boolean GL_ExtensionAvailable(const char *extension, const GLubyte *start);

void SetSurface(INT32 w, INT32 h);
void SetModelView(GLint w, GLint h);
void SetStates(void);

// ==========================================================================
//                                                                  CONSTANTS
// ==========================================================================

extern GLuint NOTEXTURE_NUM;

#define N_PI_DEMI               (M_PIl/2.0f)
#define ASPECT_RATIO            (1.0f)

#define FAR_CLIPPING_PLANE      32768.0f // Draw further! Tails 01-21-2001
extern float NEAR_CLIPPING_PLANE;

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

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

// ==========================================================================
//                                                                    STRUCTS
// ==========================================================================

struct GLRGBAFloat
{
	GLfloat red;
	GLfloat green;
	GLfloat blue;
	GLfloat alpha;
};
typedef struct GLRGBAFloat GLRGBAFloat;

// ==========================================================================
//                                                                    GLOBALS
// ==========================================================================

extern const GLubyte *gl_version;
extern const GLubyte *gl_renderer;
extern const GLubyte *gl_extensions;

extern RGBA_t myPaletteData[256];
extern GLint  textureformatGL;

extern GLint  screen_width;
extern GLint  screen_height;
extern GLbyte screen_depth;
extern GLint  maximumAnisotropy;

extern GLboolean MipMap;
extern GLint min_filter;
extern GLint mag_filter;
extern GLint anisotropic_filter;

extern FTextureInfo *gl_cachetail;
extern FTextureInfo *gl_cachehead;

extern GLuint    tex_downloaded;
extern GLfloat   fov;
extern FBITFIELD CurrentPolyFlags;

extern GLuint screentexture;
extern GLuint startScreenWipe;
extern GLuint endScreenWipe;
extern GLuint finalScreenTexture;

extern float *vertBuffer;
extern float *normBuffer;
extern short *vertTinyBuffer;
extern char *normTinyBuffer;

#ifdef HAVE_GLES2
typedef float fvector3_t[3];
typedef float fvector4_t[4];
typedef fvector4_t fmatrix4_t[4];

extern fmatrix4_t projMatrix;
extern fmatrix4_t viewMatrix;
extern fmatrix4_t modelMatrix;
#endif

#endif // _R_GLCOMMON_H_