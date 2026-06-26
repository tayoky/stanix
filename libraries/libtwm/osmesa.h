#ifndef TWM_OSMESA_H
#define TWM_OSMESA_H

#include <stdint.h>
#include <stddef.h>
#include <gl.h>

// Context Creation Formats (OSMESA_FORMAT)
#define OSMESA_COLOR_INDEX	GL_COLOR_INDEX
#define OSMESA_RGBA		GL_RGBA
#define OSMESA_BGRA		0x1
#define OSMESA_ARGB		0x2
#define OSMESA_RGB		GL_RGB
#define OSMESA_BGR		0x4
#define OSMESA_RGB_565		0x5


// Context Attributes Names
#define OSMESA_FORMAT               0x21
#define OSMESA_DEPTH_BITS           0x22
#define OSMESA_STENCIL_BITS         0x23
#define OSMESA_ACCUM_BITS           0x24
#define OSMESA_PROFILE              0x25
#define OSMESA_CORE_PROFILE         0x26
#define OSMESA_COMPAT_PROFILE       0x27
#define OSMESA_CONTEXT_MAJOR_VERSION 0x28
#define OSMESA_CONTEXT_MINOR_VERSION 0x29
#define OSMESA_API                  0x2a
#define OSMESA_GL_API               0x2b
#define OSMESA_GLES1_API            0x2c
#define OSMESA_GLES2_API            0x2d

// GetIntegerv pnames
#define OSMESA_MAJOR_VERSION 11
#define OSMESA_MINOR_VERSION 2
#define OSMESA_PATCH_VERSION 0

// Pixel Store Parameters
#define OSMESA_ROW_LENGTH           0x10
#define OSMESA_Y_UP                 0x11

// Opaque pointer types
typedef struct osmesa_context *OSMesaContext;

// Essential Function Signatures

typedef OSMesaContext(*OSMesaCreateContext_t)(GLenum format, OSMesaContext sharelist);

typedef OSMesaContext(*OSMesaCreateContextAttribs_t)(const int *attribList, OSMesaContext sharelist);

typedef void (*OSMesaDestroyContext_t)(OSMesaContext ctx);

typedef GLboolean(*OSMesaMakeCurrent_t)(OSMesaContext ctx, void *buffer, GLenum type, GLsizei width, GLsizei height);

typedef void (*OSMesaPixelStore_t)(GLint param, GLint value);

typedef void (*OSMesaGetIntegerv_t)(GLint pname, GLint *value);

// Function pointer type for extension loading
typedef void (*OSMESAproc)(void);
typedef OSMESAproc(*OSMesaGetProcAddress_t)(const char *funcName);

#endif
