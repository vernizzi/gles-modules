# gles_modules

Thin C++20-module wrappers over the OpenGL ES 2.0 / 3.0 APIs, so a modules-based
codebase can use GL without ever including a GL header.

The wrappers live in `namespace gl` and simply re-expose the Khronos headers:

| GL spelling      | Wrapper            |
| ---------------- | ------------------ |
| `GLfloat`        | `gl::Float`        |
| `GL_TRIANGLES`   | `gl::TRIANGLES`    |
| `glDrawArrays`   | `gl::DrawArrays`   |

Type aliases are PascalCase (`gl::Float`, `gl::Int`, `gl::Byte`, ...) rather than the
lowercase GL spelling, because many of those names (`float`, `int`, `short`, `void`,
`char`, `bool`) are C++ keywords and cannot be aliased.

## Modules

| Module        | Unit              | Contents                                            |
| ------------- | ----------------- | --------------------------------------------------- |
| `gles.gl2`    | `gles2.cppm`      | The GLES 2.0 surface.                               |
| `gles.gl3`    | `gles3.cppm`      | GLES 3.0; `export import`s `gles.gl2`.              |
| `gles.debug`  | `gles_debug.cppm` | Optional KHR_debug helper (see below).              |

```cpp
import gles.gl3;     // or gles.gl2 for ES2-only

gl::ClearColor(0.f, 0.f, 0.f, 1.f);
gl::Clear(gl::COLOR_BUFFER_BIT);
```

## Options

| Option                | Default            | Effect                                                       |
| --------------------- | ------------------ | ------------------------------------------------------------ |
| `GLES_MODULES_ES3`    | `ON`               | Build `gles.gl3` on top of `gles.gl2`.                       |
| `GLES_MODULES_DEBUG`  | `OFF`              | Enable the KHR_debug helper.                                 |
| `GLES_MODULES_PIC`    | `OFF`              | Position-independent code.                                   |
| `GLES_MODULES_INSTALL`| top-level only     | Generate `install` + `find_package` rules.                   |

## Debug helper

`import gles.debug;` is always valid. `gl::enable_debug()` installs a synchronous
KHR_debug callback that prints medium/high-severity messages to stderr — so you can
turn on GL debugging without pulling in GL headers yourself.

It does real work only when the library is built with `GLES_MODULES_DEBUG=ON`;
otherwise both overloads are no-ops, so calling it unconditionally in your init code
is safe.

The entry points are resolved through a caller-supplied loader, so the helper is not
tied to EGL:

```cpp
gl::enable_debug();                       // convenience: uses eglGetProcAddress
gl::enable_debug(my_get_proc_address);    // any void*(const char*) loader (EAGL, SDL, …)
```

The no-argument overload exists only where EGL does. EGL is not universal: iOS
uses EAGL, an unrelated API with no `eglGetProcAddress`. So when the build finds no
EGL the convenience overload is compiled out and you pass your own loader to the
`proc_loader` overload. EGL is the only part of this library that depends on EGL,
and it is linked only when `GLES_MODULES_DEBUG=ON` and EGL is actually present.

## Platform scope

The wrappers themselves are platform-agnostic, they only rename what the Khronos
headers declare. All platform specifics live in context/surface creation, which
this library deliberately does not do. What varies is whether a given platform
can give you a GLES context and the `libGLESv2` symbols:

| Platform        | GLES via                 | Notes                                              |
| --------------- | ------------------------ | -------------------------------------------------- |
| Linux (X11)     | EGL (Mesa)               | Supported.                                         |
| Linux (Wayland) | EGL (only option)        | Supported.                                         |
| Android         | EGL (native)             | Supported.                                         |
| Windows         | ANGLE / vendor emulators | No native GLES; ship ANGLE.                        |
| macOS           | ANGLE only               | No native GLES or EGL.                             |
| iOS             | EAGL                     | `eglGetProcAddress` is unavailable — use the loader overload. |

CMake's `FindOpenGL` only provides the `OpenGL::EGL` / `OpenGL::GLES2` / `OpenGL::GLES3`
imported targets on **Unix**. On Windows/macOS you must point the build at an ANGLE (or
vendor) GLES yourself.

## Consuming it

### FetchContent / add_subdirectory

A module library's real interface is its BMI, which is compiler- and
version-specific and therefore not distributable as a binary. Every consumer
recompiles the module sources with their own toolchain. Building from source is
the most robust path:

```cmake
include(FetchContent)
FetchContent_Declare(gles_modules GIT_REPOSITORY https://github.com/vernizzi/gles-modules.git GIT_TAG master)
FetchContent_MakeAvailable(gles_modules)

target_link_libraries(app PRIVATE gles_modules::gles_modules)
```

### Installed package

```sh
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build --prefix /your/sysroot
```

```cmake
find_package(gles_modules REQUIRED)
target_link_libraries(app PRIVATE gles_modules::gles_modules)
```

The install ships the module sources (not BMIs). The consumer's CMake recompiles
them, so the consuming toolchain must be compatible with the one used to build.

## Requirements

- CMake ≥ 3.30, Ninja (or another generator with C++ module support)
- Clang ≥ 16 or GCC ≥ 14
- OpenGL ES 2.0/3.0 (and EGL when `GLES_MODULES_DEBUG=ON`)
