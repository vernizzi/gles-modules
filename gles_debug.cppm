/**
 * @file gles_debug.cppm
 * @brief Minimal KHR_debug setup for GLES debug output.
 *
 * `gl::enable_debug()` resolves the KHR_debug entry points at runtime, enables
 * synchronous debug output, and installs a default callback that prints
 * medium/high severity messages to stderr — so GL debugging can be turned on
 * without ever including a GL header.
 *
 * The entry points are resolved through a caller-supplied `proc_loader`, so the
 * helper is not tied to any one windowing layer. A convenience no-argument
 * overload defaults to `eglGetProcAddress`; it exists only where EGL does, which
 * is the only place this library touches EGL. Platforms without EGL (e.g. iOS,
 * which uses EAGL) simply pass their own loader to the `proc_loader` overload.
 *
 * Feature gating:
 *   GLES_MODULES_DEBUG  the helper does real work (otherwise both overloads are
 *                       no-ops, so unconditional calls compile away)
 *   GLES_MODULES_EGL    the EGL convenience overload uses eglGetProcAddress
 */
module;

#if defined(GLES_MODULES_DEBUG)
#include <GLES2/gl2.h>

#include <cstdio>
#endif

#if defined(GLES_MODULES_EGL)
#include <EGL/egl.h>
#endif

export module gles.debug;

export namespace gl {

/// Resolves a GL/extension entry point by name, or returns nullptr.
using proc_loader = void* (*)(const char* name);

/**
 * @brief Load KHR_debug via @p load and enable synchronous GL error reporting.
 *
 * Platform-agnostic: pass any loader (a wrapper around `eglGetProcAddress`,
 * `SDL_GL_GetProcAddress`, a custom EAGL loader, ...). Does nothing if the entry
 * points cannot be resolved, or in builds without `GLES_MODULES_DEBUG`.
 */
void enable_debug(proc_loader load) noexcept;

#if defined(GLES_MODULES_EGL)
/**
 * @brief Convenience overload that loads via `eglGetProcAddress`.
 *
 * Declared only in builds with EGL (`GLES_MODULES_EGL`); on EGL-less platforms
 * (e.g. iOS) this overload does not exist — pass your own loader to the
 * `proc_loader` overload instead.
 */
void enable_debug() noexcept;
#endif

} // namespace gl

#if defined(GLES_MODULES_DEBUG)

namespace gl::detail {

/* KHR_debug constants (not present in the base GLES headers). */
inline constexpr GLenum debug_output{0x92E0};
inline constexpr GLenum debug_output_synchronous{0x8242};
inline constexpr GLenum debug_type_error{0x824C};
inline constexpr GLenum debug_severity_high{0x9146};
inline constexpr GLenum debug_severity_medium{0x9147};

using debug_proc_t = void(GL_APIENTRY*)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, const void*);
using debug_message_callback_t = void(GL_APIENTRY*)(debug_proc_t, const void*);
using debug_message_control_t = void(GL_APIENTRY*)(GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean);

/* Mesa performance hint about shader recompilation on first use — harmless, suppressed. */
inline constexpr GLuint shader_recompile_id{131218};

void GL_APIENTRY default_debug_callback([[maybe_unused]] GLenum source, GLenum type, GLuint id, GLenum severity, [[maybe_unused]] GLsizei length,
    const GLchar* message, [[maybe_unused]] const void* user_param) {

    if (id == shader_recompile_id) {
        return;
    }

    if (severity == debug_severity_medium || severity == debug_severity_high) {
        const auto* kind = (type == debug_type_error) ? "ERROR" : "WARNING";
        std::fprintf(stderr, "[gl %s %u]: %s\n", kind, id, message);
    }
}

} // namespace gl::detail

namespace gl {

void enable_debug(proc_loader load) noexcept {
    if (!load) {
        return;
    }

    auto* set_callback = reinterpret_cast<detail::debug_message_callback_t>(load("glDebugMessageCallback"));
    auto* set_control = reinterpret_cast<detail::debug_message_control_t>(load("glDebugMessageControl"));

    if (!(set_callback && set_control)) {
        std::fprintf(stderr, "[gl] KHR_debug not available — GL debug output disabled\n");
        return;
    }

    glEnable(detail::debug_output);
    glEnable(detail::debug_output_synchronous);

    set_callback(detail::default_debug_callback, nullptr);
    set_control(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
}

} // namespace gl

#else

namespace gl {

void enable_debug([[maybe_unused]] proc_loader load) noexcept {}

} // namespace gl

#endif

#if defined(GLES_MODULES_EGL)

namespace gl {

void enable_debug() noexcept {
    enable_debug(+[](const char* name) -> void* { return reinterpret_cast<void*>(eglGetProcAddress(name)); });
}

} // namespace gl

#endif
