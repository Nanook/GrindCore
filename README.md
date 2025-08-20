
# GrindCore Native

**GrindCore** is a multiplatform C library providing modern compression and hashing algorithms, designed primarily for integration with .NET and similar managed runtimes. It serves as the native core for the [GrindCore .NET wrapper](https://github.com/Nanook/GrindCore.net).

## Overview

GrindCore Native unifies multiple C compression and hashing libraries into a single, maintainable, and portable codebase. It is built using the same CMake-based approach as the .NET runtime, ensuring compatibility and ease of updates as upstream libraries evolve. The project is designed to:

- Provide byte-perfect, deterministic compression for scenarios requiring exact output (e.g., archival, checksumming, reproducible builds)
- Support multiple versions of key algorithms for maximum compatibility
- Enable easy integration with managed runtimes (e.g., .NET, Mono, CoreRT, WASM)
- Offer both streaming and block APIs for flexible usage

## Supported Platforms

GrindCore Native is tested and built for:

- Windows: x64, x86, ARM64
- Linux: x64, x86, ARM64, ARM
- macOS: x64, ARM64

Other platforms may work with minimal changes. The build system is CMake-based and supports cross-compilation.

## Key Features

- **Modern Compression**: Integrates Brotli, Zstd, LZ4, LZMA, Fast-LZMA2, ZLib, ZLib-NG, and more
- **Multiple Versions**: Includes multiple versions of Zstd, ZLib, and other libraries for compatibility
- **Hashing Suite**: Provides a wide range of cryptographic and non-cryptographic hashes
- **No Third-Party Binaries**: All code is built from source, no external binaries required
- **AOT/Native Friendly**: Designed for use in AOT and native scenarios (e.g., .NET Native, CoreRT)
- **Consistent APIs**: Streaming and block APIs for compression; hash APIs compatible with standard C patterns
- **Actively Maintained**: Regularly updated as upstream libraries release new versions

## Compression Algorithms

All algorithms are built from source and included in the repository. Supported algorithms and versions:

- **Brotli** v1.1.0 (from [.NET 9.0](https://github.com/dotnet/runtime/tree/release/9.0/src/native/external/brotli))
- **Copy** (no compression - direct stream copy)
- **LZ4** v1.10.0 (from [LZ4](https://github.com/lz4/lz4/tree/dev/lib))
- **LZMA** v25.1.0 (from [7Zip](https://sourceforge.net/projects/sevenzip/files/7-Zip/25.01/) App)
- **LZMA2** v25.1.0 (from [7Zip](https://sourceforge.net/projects/sevenzip/files/7-Zip/25.01/) App)
- **Fast-LZMA2** v1.0.1 (from [7Zip-mcmilk](https://github.com/mcmilk/7-Zip-zstd/tree/master/C/fast-lzma2))
- **ZLib** v1.3.1 (GZip, ZLib, Deflate - from [.NET 8.0](https://github.com/dotnet/runtime/tree/release/8.0/src/native/external/zlib))
- **ZLib-NG** v2.2.1 (GZip, ZLib, Deflate - from [.NET 9.0](https://github.com/dotnet/runtime/tree/release/9.0/src/native/external/zlib-ng))
- **ZStd** v1.5.7 & v1.5.2 (from [Facebook](https://github.com/facebook/zstd/tree/dev/lib))

**Notes:**
- Multiple versions are included to support projects requiring pinned/frozen algorithms for byte-perfect output
- All algorithms are available as both streaming and block APIs
- Dictionary and advanced options are planned for future releases

## Hashing Algorithms

GrindCore Native provides a comprehensive suite of hash functions:

- Blake3, Blake2sp
- MD5, MD4, MD2
- SHA1
- SHA2 [SHA256, SHA384, SHA512]
- SHA3 [SHA3-224, SHA3-256, SHA3-384, SHA3-512]
- XXHash [XXH32, XXH64]

## License

GrindCore Native is licensed under the MIT License. See [LICENSE](LICENSE) for details. Upstream libraries are included under their respective licenses.
