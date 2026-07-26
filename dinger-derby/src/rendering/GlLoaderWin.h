#pragma once

// Windows' opengl32.dll only exports OpenGL 1.1 — everything GlRenderer.cpp
// needs beyond that (buffers, shaders, uniforms) must be resolved at runtime
// via wglGetProcAddress once a context is current. macOS and Linux expose
// these symbols directly from their GL library, which is why this loader is
// Windows-only.
#if defined(_WIN32)

#include <GL/gl.h>
#include <GL/glext.h>

inline PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
inline PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
inline PFNGLBUFFERDATAPROC glBufferData = nullptr;
inline PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
inline PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
inline PFNGLCREATESHADERPROC glCreateShader = nullptr;
inline PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
inline PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
inline PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
inline PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
inline PFNGLDELETESHADERPROC glDeleteShader = nullptr;
inline PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
inline PFNGLATTACHSHADERPROC glAttachShader = nullptr;
inline PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation = nullptr;
inline PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
inline PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
inline PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
inline PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
inline PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
inline PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
inline PFNGLUNIFORM1FPROC glUniform1f = nullptr;
inline PFNGLUNIFORM3FPROC glUniform3f = nullptr;
inline PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv = nullptr;
inline PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
inline PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
inline PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
inline PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;

// Resolves the pointers above. Must be called once a GL context is current
// (e.g. after window.setActive(true)) and before any of them are used.
inline bool loadWinGlFunctions() {
    static bool loaded = false;
    static bool ok = false;
    if (loaded) {
        return ok;
    }
    loaded = true;
    ok = true;

#define GLWIN_LOAD(type, name)                                 \
    name = reinterpret_cast<type>(wglGetProcAddress(#name));   \
    ok = ok && (name != nullptr);

    GLWIN_LOAD(PFNGLGENBUFFERSPROC, glGenBuffers)
    GLWIN_LOAD(PFNGLBINDBUFFERPROC, glBindBuffer)
    GLWIN_LOAD(PFNGLBUFFERDATAPROC, glBufferData)
    GLWIN_LOAD(PFNGLBUFFERSUBDATAPROC, glBufferSubData)
    GLWIN_LOAD(PFNGLDELETEBUFFERSPROC, glDeleteBuffers)
    GLWIN_LOAD(PFNGLCREATESHADERPROC, glCreateShader)
    GLWIN_LOAD(PFNGLSHADERSOURCEPROC, glShaderSource)
    GLWIN_LOAD(PFNGLCOMPILESHADERPROC, glCompileShader)
    GLWIN_LOAD(PFNGLGETSHADERIVPROC, glGetShaderiv)
    GLWIN_LOAD(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog)
    GLWIN_LOAD(PFNGLDELETESHADERPROC, glDeleteShader)
    GLWIN_LOAD(PFNGLCREATEPROGRAMPROC, glCreateProgram)
    GLWIN_LOAD(PFNGLATTACHSHADERPROC, glAttachShader)
    GLWIN_LOAD(PFNGLBINDATTRIBLOCATIONPROC, glBindAttribLocation)
    GLWIN_LOAD(PFNGLLINKPROGRAMPROC, glLinkProgram)
    GLWIN_LOAD(PFNGLGETPROGRAMIVPROC, glGetProgramiv)
    GLWIN_LOAD(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)
    GLWIN_LOAD(PFNGLDELETEPROGRAMPROC, glDeleteProgram)
    GLWIN_LOAD(PFNGLUSEPROGRAMPROC, glUseProgram)
    GLWIN_LOAD(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)
    GLWIN_LOAD(PFNGLUNIFORM1FPROC, glUniform1f)
    GLWIN_LOAD(PFNGLUNIFORM3FPROC, glUniform3f)
    GLWIN_LOAD(PFNGLUNIFORMMATRIX3FVPROC, glUniformMatrix3fv)
    GLWIN_LOAD(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)
    GLWIN_LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)
    GLWIN_LOAD(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray)
    GLWIN_LOAD(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)

#undef GLWIN_LOAD

    return ok;
}

#endif // _WIN32
