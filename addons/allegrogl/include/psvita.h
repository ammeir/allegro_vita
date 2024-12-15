
#ifndef PSVITA_H
#define PSVITA_H

#include "debug.h"

#define GL_VERSION_1_1 1


#define GLboolean     uint8_t
#define GLbyte        int8_t
#define GLubyte       uint8_t
//#define GLchar        char
#define GLshort       int16_t
#define GLushort      uint16_t
#define GLint         int32_t
#define GLuint        uint32_t
#define GLfixed       int32_t
#define GLint64       int64_t
#define GLuint64      uint64_t
#define GLsizei       int32_t
//#define GLenum        uint32_t
#define GLintptr      int32_t
//#define GLsizeiptr    uint32_t
#define GLsync        int32_t
//#define GLfloat       float
#define GLclampf      float
//#define GLdouble      double
#define GLclampd      double
//#define GLvoid        void

#define EGLBoolean    uint8_t
#define EGLDisplay    void*
#define EGLSurface    void*
#define EGLint        int32_t

typedef unsigned int	GLbitfield;

#define GL_TEXTURE_BINDING_2D			0x8069
#define GLU_VERSION   				100800
#define GL_COLOR_LOGIC_OP			0x0BF2

void glColor4ubv(const GLubyte *v);
void glRasterPos4fv(const float* v);
//void glRasterPos4fv(const GLfloat* v);
const GLubyte *glGetString(uint32_t name);

inline void glRasterPos2f(float i, float j) {}
inline void glDeleteLists(int i, int j) {}

BITMAP *allegro_gl_screen;


#endif