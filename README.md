# GrindCore

DotNet Native Compression and Hashing for:
- win-x64
- win-x86
- win-arm64
- linux-x64
- linux-x86
- linux-arm64
- linux-arm
- osx-x64
- osx-arm64

### Wrapped by dotnet C# projet:

- https://github.com/Nanook/GrindCore.net

This wrapper/interop provides dotnet compatible Streaming and Block compression as well as hashing compatible with HashAlgorithm. 

### Available in nuget:

- https://www.nuget.org/packages/GrindCore

## Current Features

### Compression

- Brotli v1.1.0 (From DotNet 9.0)
- LZ4 v1.9.4 (From 7Zip-mcmilk)
- Lzma v24.7.0 (From 7Zip-mcmilk)
- Lzma2 v24.7.0 (From 7Zip-mcmilk)
- Fast-Lzma2 v1.0.1 (From 7Zip-mcmilk)
- ZLib v1.3.1 [GZip, ZLib, Deflate] (From DotNet 8.0)
- ZLib-NG v2.2.1 [GZip, ZLib, Deflate] (From DotNet 9.0)
- ZStd v1.5.7, v1.5.2 (From Facebook)

### Hashing

- Blake3, Blake2sp
- MD5, MD4, MD2
- SHA1
- SHA2 [SHA256, SHA384, SHA512]
- SHA3 [SHA3-224, SHA3-256, SHA3-384, SHA3-512]
- XXHash [XXH32, XXH64]
