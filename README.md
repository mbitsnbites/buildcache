## This repo has moved to: https://gitlab.com/bits-n-bites/buildcache

# BuildCache [![Build and Test](https://github.com/mbitsnbites/buildcache/workflows/Build%20and%20Test/badge.svg)](https://github.com/mbitsnbites/buildcache/actions)

BuildCache is an advanced compiler accelerator that caches and reuses build
results to avoid unnecessary re-compilations, and thereby speeding up the build
process.

It is similar in spirit to [ccache](https://ccache.samba.org/),
[sccache](https://github.com/mozilla/sccache) and
[clcache](https://github.com/frerich/clcache).

## Download

Pre-built binaries of BuildCache can be downloaded [here](https://github.com/mbitsnbites/buildcache/releases/latest).

## Features

* Works on different operating systems:
  * Linux
  * macOS
  * Windows
  * Though untested, it probably works on most BSD:s
* A modular compiler support system:
  * Built-in support for [popular compilers](#supported-compilers-and-languages).
  * Extensible via custom [Lua](https://www.lua.org/) scripts.
  * In addition to caching compilation results, BuildCache can be used for
    caching almost any reproducible program artifacts (e.g. test results,
    [rendered images](https://en.wikipedia.org/wiki/Rendering_(computer_graphics)),
    etc).
* A fast local file system cache.
  * Suppports both preprocessor-based and fast preprocessorless cache lookup.
* Can optionally use a remote, shared server as a second level cache.
* Compression with [LZ4](https://github.com/lz4/lz4) or optionally
  [ZSTD](https://github.com/facebook/zstd) (with negligable overhead).


### Supported compilers and languages

Currently the following compilers and languages are supported:

| Compiler | Languages | Support |
| --- | --- | --- |
| [GCC](https://gcc.gnu.org/) | C, C++ | Built-in |
| [Clang](https://clang.llvm.org/) | C, C++ | Built-in |
| [Microsoft Visual C++](https://visualstudio.microsoft.com/vs/features/cplusplus/) | C, C++ | Built-in |
| [clang-cl](https://clang.llvm.org/docs/UsersManual.html#clang-cl) | C, C++ | Built-in |
| [QNX SDP (qcc)](https://blackberry.qnx.com/en/embedded-software/qnx-software-development-platform) | C, C++ | Built-in |
| [Green Hills Optimizing Compilers](https://www.ghs.com/products/compiler.html) | C, C++ | Built-in |
| [TI TMS320C6000 Optimizing Compiler](http://www.ti.com/tool/C6000-CGT) | C, C++ | Built-in |
| [TI ARM Optimizing C/C++ Compiler](http://www.ti.com/tool/ARM-CGT) | C, C++ | Built-in |
| TI ARP32 Optimizing C/C++ Compiler | C, C++ | Built-in |
| [scan-build static analyzer](https://clang-analyzer.llvm.org/scan-build.html) | C, C++ | Built-in |
| [Clang-Tidy](https://clang.llvm.org/extra/clang-tidy/) | C, C++ | Lua example |

New backends are relatively easy to add, both as built-in wrappers in C++ and as
[Lua wrappers](doc/lua.md).

## Status

BuildCache has been used daily in production environments for years with near
zero issues (any problem that has emerged has of course been fixed), which
gives it a good track record.

With that said, BuildCache is still considered to be under development and
things like configuration parameters and cache formats may change between 0.x
versions.

Once BuildCache has reached version 1.0, all releases will be fully backwards
compatible within a major version (e.g. 1.x).

## Documentation

* [Building BuildCache](doc/building.md)
* [Using BuildCache](doc/usage.md)
* [Configuration](doc/configuration.md)
* [Using custom Lua plugins](doc/lua.md)
* [Contributing to BuildCache](doc/contributing.md)
* [Benchmarks](doc/benchmarks.md)

The BuildCache internals are documented using Doxygen, and the latest
generated documentation can be found here:

* [https://mbitsnbites.github.io/buildcache/](https://mbitsnbites.github.io/buildcache/)

Feel free to ask questions and discuss ideas at:

* [BuildCache Discussions](https://github.com/mbitsnbites/buildcache/discussions)

