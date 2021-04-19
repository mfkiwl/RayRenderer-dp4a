# RayRenderer

[![CI](https://github.com/XZiar/RayRenderer/workflows/CI/badge.svg)](https://github.com/XZiar/RayRenderer/actions)

No it's not a renderer.

That's just an excuse for me to write a collection of utilities which "will be used for the renderer".

Maintaing and growing the project is much more fun than make it actually renderable. :sweat_smile:

### Changes from old project

The old preject is [here](https://github.com/XZiar/RayTrace)

* fixed pipeline --> GLSL shader
* x64/AVX2 only RayTracing --> multi versions of acceleration RayTracing(may also include OpenCL)
* GLUT based GUI --> simple GUI with FreeGLUT, complex GUI with WPF
* coupling code --> reusable components

## Component

| Component | Description | Language | Platform |
|:-------|:-------:|:---:|:-------:|
| [3rdParty](./3rdParty) | 3rd party library | C,C++ | N/A |
| [common](./common) | Basic but useful things | Multi | N/A |
| [3DBasic](./3DBasic) | Self-made BLAS library and simple 3D things | C++ | N/A |
| [StringUtil](./StringUtil) | String Charset Library | C++ | Windows & Linux |
| [SystemCommon](./SystemCommon) | System-level common library | C++ | Windows & Linux |
| [MiniLogger](./MiniLogger) | Mini Logger | C++ | Windows & Linux |
| [Nailang](./Nailang) | A customizable language | C++ | Windows & Linux |
| [AsyncExecutor](./AsyncExecutor) | Async Executor | C++ | Windows & Linux |
| [ImageUtil](./ImageUtil) | Image Read/Write Utility | C++ | Windows & Linux |
| [XComputeBase](./XComputeBase) | Base Library for Cross-Compute | C++ | Windows & Linux |
| [OpenGLUtil](./OpenGLUtil) | Wrapper of OpenGL things | C++ | Windows & Linux |
| [OpenCLUtil](./OpenCLUtil) | Wrapper of OpenCL things | C++ | Windows & Linux |
| [OpenCLInterop](./OpenCLInterop) | OpenCL Interoperation utility | C++ | Windows & Linux |
| [FontHelper](./FontHelper) | Helper for displaying font in OpenGL | C++ | Windows & Linux |
| [TextureUtil](./TextureUtil) | Texture Utility | C++ | Windows & Linux |
| [WindowHost](./WindowHost) | Multi-threaded GUI host | C++ | Windows & Linux |
| [FreeGLUTView](./FreeGLUTView) | Wrapper of FreeGLUT | C++ | Windows & Linux |
| [ResourcePackager](./ResourcePackager) | Resource (de)serialize support | C++ | Windows & Linux |
| [RenderCore](./RenderCore) | Core of RayRenderer | C++ | Windows & Linux |
| [RenderCoreWrap](./RenderCoreWrap) | C++/CLI Wrapper for RayRender core | C++/CLI | Windows |
| [CommonUtil](./CommonUtil) | Basic utilities for C# | C# | Windows |
| [AnyDock](./AnyDock) | Flexible dock layout like AvalonDock for WPF | C# | Windows |
| [OpenGLView](./OpenGLView) | Wrapper of OpenGL window in WinForm | C++/CLI | Windows |
| [GLUTTest](./GLUTTest) | Test Program(C++) (using FreeGLUTView) | C++ | Windows & Linux |
| [UtilTest](./Tests/UtilTest) | Utilities Test Program(C++) | C++ | Windows & Linux |
| [WPFTest](./WPFTest) | Test Program(C#) in WPF (using OpenGLView) | C#  | Windows |

## Platform Requirements

Since C++/CLI is used for C# bindings, and multiple DLL hacks are token for DLL-embedding, it's Windows-only.

For Windows parts, Windows SDK Target is `10(latest)` and .Net Framework 4.8 needed for C# components.

To use `xzbuild`, python3.6+ is required.

## Build

To build C++ parts, an C++17 compiler is needed. For Linux, GCC7.3 and later are tested. For Windows, VS2019(`16.0`) is needed for the vcproj version.

Project uses `VisualStudio2019` on Windows, and uses `xzbuild` with make on Linux. Utilities that have `xzbuild.proj.json` inside are capable to be compiled on Linux, tested on GCC(7.3&8.0) and Clang(7.0&8.0).

They can be built by execute [`xzbuild.py`](xzbuild.py) (python3.6+).

### Additional Requirements

`boost` headers folder should be found inside `include path`. It can be added in [SolutionInclude.props](./SolutionInclude.props) in Windows, or specified by environment variable `CPP_DEPENDENCY_PATH`.

`gl.h` and `glu.h` headers should be found inside `include path\GL`.

[nasm](https://www.nasm.us/) needed for [libjpeg-turbo](./3rdParty/libjpeg-turbo) --- add it to system environment path

[ispc compiler](https://ispc.github.io/downloads.html) needed for [ispc_texcomp](./3rdParty/ispc_texcomp) --- add it to system environment path

## Dependency

* [gsl](https://github.com/microsoft/GSL) `submodule` 3.1.0
  
  [MIT License](./3rdParty/gsl/LICENSE)

* [Google Test](https://github.com/google/googletest) `submodule` 1.10.x

  [BSD 3-Clause License](https://github.com/google/googletest/blob/master/LICENSE)

* [FreeGLUT](http://freeglut.sourceforge.net)  3.2.1

  [MIT License](./3rdParty/freeglut/license.txt)

* [boost](http://www.boost.org/)  1.75.0 (not included in this repo)

  [Boost Software License](./License/boost.txt)

* [fmt](http://fmtlib.net) [`submodule`](https://github.com/XZiar/fmt) 7.1.2 (customized with utf-support)

  [MIT License](https://github.com/XZiar/fmt/blob/master/LICENSE.rst)

* [ghc-filesystem](https://github.com/gulrak/filesystem) `submodule` 1.5.4

  [MIT License](https://github.com/gulrak/filesystem/blob/master/LICENSE)

* [rapidjson](http://rapidjson.org/) `submodule` 1.1.0 master

  [MIT License](https://github.com/Tencent/rapidjson/blob/master/license.txt)

* [digestpp](https://github.com/kerukuro/digestpp) `submodule` master
  
  [Public Domain](https://github.com/kerukuro/digestpp/blob/master/LICENSE)

* [FreeType2](https://www.freetype.org/) 2.10.1

  [The FreeType License](./3rdParty/freetype2/license.txt)

* [ISPCTextureCompressor](https://github.com/GameTechDev/ISPCTextureCompressor)
  
  [MIT License](./3rdParty/ispc_texcomp/license.txt)

* [half](http://half.sourceforge.net/) 2.1.0
  
  [MIT License](./3rdParty/half/LICENSE.txt)

* [libcpuid](http://libcpuid.sourceforge.net/)  `submodule` 0.5.1

  [BSD License](./3rdParty/cpuid/COPYING)

## License

RayRenderer (including its component) is licensed under the [MIT license](License.txt).
