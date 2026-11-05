"""GrindCore native test suite (ctypes, stdlib only, Python 3.6+). Run: python -m gctest --lib <library> [--json out.json]

The loaded library is gctest.LIB; test modules are gctest/test_*.py (unittest).
"""
import unittest

LIB = None
RID = None  # the library's RID: --rid, else gcnative.target() (the legacy builds can't be told apart at run time)

# What some tests need besides the library. A test that can't run without one calls need(), which skips it; a test
# that only leaves a check out calls missing(). Both record it, and the run fails at the end unless that input can't
# be had for this RID (unavailable()) or the run accepts its absence (--without), so a reference that a run forgot,
# or a Python without a module, can't pass as a skip. --refs points every GC_REF_* at a folder of references.
INPUTS = {
    "zlib": "official zlib 1.3.1 and zlib-ng 2.2.1, GC_REF_ZLIB (ref/build_zlib_ref.py)",
    "zstd": "official zstd 1.5.2 and 1.5.7, GC_REF_ZSTD (ref/build_zstd_ref.py)",
    "lz4": "official LZ4 1.10.0, GC_REF_LZ4 (ref/build_lz4_ref.py)",
    "fl2": "official fast-lzma2 1.0.1 and master, GC_REF_FL2 (ref/build_fl2_ref.py)",
    "brotli": "official Brotli 1.1.0, GC_REF_BROTLI (ref/build_brotli_ref.py)",
    "brotli-fixtures": "Brotli 1.1.0's test streams, GC_REF_BROTLI (ref/build_brotli_ref.py --fixtures-only)",
    "7zip": "official 7-Zip 25.01, GC_REF_7ZIP (ref/build_7zip_ref.py)",
    "7zip-asm": "official 7-Zip 25.01's assembler decoder, lib7zref-asm next to GC_REF_7ZIP (ref/build_7zip_ref.py --asm)",
    "python-lzma": "Python's lzma module (liblzma, the independent LZMA/LZMA2 decoder)",
    "python-bz2": "Python's bz2 module (libbzip2, the independent decoder)",
}
MISSING = {}  # input -> ids of the tests that went without it


def unavailable(name, rid):
    """Why input `name` can't be had for `rid`, or None if it can."""
    if name == "7zip-asm" and rid != "win-x64" and "arm64" not in rid:
        if "x64" not in rid:
            return "7-Zip has assembler decoders for x86-64 and arm64 only"
        return "7-Zip's x86-64 decoder is MASM source, which off Windows needs asmc or uasm (not used here)"
    return None


def missing(test, name):
    """Record that `test` (a TestCase, or its class in setUpClass) left a check out for want of input `name`."""
    tid = test.id() if isinstance(test, unittest.TestCase) else "%s.%s" % (test.__module__, test.__name__)
    MISSING.setdefault(name, set()).add(tid)


def need(test, name, detail=""):
    """Skip `test` for want of input `name`, recorded as missing() does. `detail` says what was found instead."""
    missing(test, name)
    raise unittest.SkipTest("missing %s: %s%s" % (name, INPUTS[name], ("; " + detail) if detail else ""))


def address_sanitizer():
    """True under AddressSanitizer: a sanitizer build of the library with its runtime preloaded (SAN=1 in
    tools/native-build/hosts/native-tests.sh). Its shadow memory reserves terabytes of address space."""
    import ctypes
    try:
        return hasattr(ctypes.CDLL(None), "__asan_init")
    except (OSError, TypeError):  # Windows has no global symbol namespace to look in
        return False

# Defects in the shipped binaries that tests detect. On a library that has one, the test reports it (skip, with this
# reason) instead of failing; on a fixed library the same test must pass. Each has its audit reference.
KNOWN_DEFECTS = {
    "md2-streaming": "MD2_Update skips bytes and over-reads when a partial block is pending (md2.c:106); "
                     "fixed in the PAL on audit/native-fixes; audit/hashes.md 6.5",
    "sha3-bitsize": "SZ_SHA3_Init accepts any bitSize; > 800 runs the sponge index past the state; "
                    "fixed in the PAL on audit/native-fixes; audit/hashes.md 6.5",
    "sha1-prepareblock-size": "SZ_Sha1_PrepareBlock with a size that isn't a multiple of 4 <= 52 never stops writing; "
                              "fixed in the PAL on audit/native-fixes; audit/hashes.md 6.5",
    "lzma-multicallmode": "LzmaEnc_Construct leaves CLzmaEnc.multicallMode uninitialised; a leftover 2 truncates LZMA2 "
                          "chunks (and hangs LZMA MemEncode); fixed in LzmaEnc.c on audit/native-fixes; audit/lzma.md 3.3",
    "lzma-stream-onesymbol": "the LZMA multi-call path compares its per-call pack limit with the whole stream's output, so "
                             "past about a dictionary of output every call encodes one symbol (slow, output correct); "
                             "fixed in LzmaEnc_LzmaCodeMultiCall on audit/native-fixes; audit/lzma.md 3.5",
    "zlib-crc32-negative-length": "the PAL's DN8_/DN9_..._Crc32 pass their int32_t length to zlib's unsigned uInt, so "
                                  "a negative length reads up to 4 GiB past the buffer: a crash; fixed in the PAL on "
                                  "audit/native-fixes (a negative length is no data); audit/build-exam.md W1",
    "zlib-null-z-stream": "the zlib PALs' Deflate, DeflateEnd, Inflate and InflateEnd write through a NULL z_stream, "
                          "which Init leaves when its calloc fails, and GrindCore.net's handle calls DeflateEnd or "
                          "InflateEnd on it when released: a crash instead of the out-of-memory exception; fixed in "
                          "the PAL on audit/native-fixes; audit/memory.md M10",
    "pal-create-overwrite": "the PAL's create and init functions (zstd's contexts, dictionaries and seekable streams, "
                            "zlib's and zlib-ng's Init, LZ4's Init, LZ4F's contexts) store a new object into the "
                            "caller's struct without checking for one already there, which then leaks; GrindCore.net "
                            "always passes a zeroed struct; fixed in the PAL on audit/native-fixes; audit/memory.md M10",
    "zlib-init-null-stream": "the zlib PALs' DeflateInit2_ and InflateInit2_ write through a NULL stream (only an "
                             "assert, compiled out of a release build, checks it); GrindCore.net never passes NULL; "
                             "fixed in the PAL on audit/native-fixes; audit/memory.md M10",
    "zlibng-arm32-neon-align": "zlib-ng's NEON Adler-32 uses :256-aligned loads (clang 14-19 on 32-bit ARM) on data "
                               "aligned to 16: SIGBUS on every call; fixed by the zlib-ng_arm32_neon_ld4.h header on "
                               "audit/native-fixes; audit/platforms.md 10.7",
    "lzma2-flush-data-loss": "the multi-call LZMA2 encoder (Lzma2Stream's solid mode) latches the end of its input when "
                             "a Flush() drains it (LzFind.c:172-175), and the hook never clears it, so everything "
                             "written after the first flush is lost; fixed on audit/native-fixes with a liblzma-style "
                             "sync flush; audit/lzma.md 3.1",
    "lzma2-prepare-leak": "Lzma2Enc_EncodeMultiCallPrepare after a multithreaded Encode2 leaks the workers' encoders, "
                          "the output buffers and the MtCoder threads; fixed in Lzma2Enc.c on audit/native-fixes; "
                          "audit/lzma.md 3.3",
    "zstd-null-args-zero": "the PAL's block functions and DecompressStream return 0 for NULL arguments, which zstd "
                           "defines as a valid result (0 bytes; frame complete), and reject a NULL buffer even with "
                           "size 0, which is how .NET pins an empty array; fixed in the PAL on audit/native-fixes; "
                           "audit/zstd.md 2.2",
    "lz4-usingdict-empty": "the PAL's SZ_Lz4_v1_10_0_DecompressUsingDict tests `decompressedSize > 0`, so a valid "
                           "block that decodes to 0 bytes (an empty input) is reported as DECOMPRESSFAIL; "
                           "fixed in the PAL on audit/native-fixes; audit/other-codecs.md 4.1.1",
    "lz4-stream-decode-state": "the PAL's Stream holds the LZ4_stream_t that Init creates (the compressor's state), and "
                               "DecompressSafeContinue uses it as an LZ4_streamDecode_t: fine on a fresh stream, but "
                               "after compression or LoadDict it reads hash-table entries as history pointers; "
                               "fixed in the PAL on audit/native-fixes; audit/other-codecs.md 4.1.1",
    "lz4-stream-null-state": "the PAL's CompressFastContinue, DecompressSafeContinue, LoadDict and SaveDict don't check "
                             "the stream's state, so a Stream used after End (or never initialised) passes NULL to LZ4 "
                             "and crashes; fixed in the PAL on audit/native-fixes; audit/other-codecs.md 4.1.1",
    "lz4-transfer-adopts": "the PAL's SZ_Lz4_v1_10_0_TransferStateToPalLZ4Stream made the stream adopt the caller's "
                           "LZ4_stream_t, which End then freed: freed twice when it came from another stream; "
                           "fixed in the PAL on audit/native-fixes; audit/other-codecs.md 4.1.1",
    "lz4f-comprehc-stream": "the PAL's SZ_Lz4F_v1_10_0_CompressHC_Stream uses an LZ4F compression context (an "
                            "LZ4F_cctx) as an LZ4_streamHC_t, about 256 KB, and writes far past it: heap corruption; "
                            "no managed caller; fixed in the PAL on audit/native-fixes; audit/other-codecs.md 4.1.1",
    "zstd-null-size-pointers": "the PAL's DecompressStream writes through a NULL inSize or outSize, and FlushStream "
                               "and EndStream through a NULL inSize (only CompressStream checks it): a crash; fixed in "
                               "the PAL on audit/native-fixes; audit/zstd.md 2.2",
    "brotli-exit-on-oom": "Brotli 1.1.0 defaults to BROTLI_ENCODER_EXIT_ON_OOM: a failed encoder allocation calls "
                          "exit(EXIT_FAILURE) and ends the whole process; built with BROTLI_ENCODER_CLEANUP_ON_OOM on "
                          "audit/native-fixes, so the encoder returns BROTLI_FALSE; audit/other-codecs.md 4.3.1",
    "brotli-null-args": "the Brotli PAL passes every pointer through, and Brotli dereferences a NULL state, size or "
                        "cursor pointer, or a NULL buffer with a nonzero size: a crash; fixed in the PAL on "
                        "audit/native-fixes; audit/other-codecs.md 4.3.1",
    "fl2-handlerepeat-overread": "fast-lzma2 1.0.1's RMF_handleRepeat (radix_mf.c:303) extends a repeat without "
                                 "checking the block's end, so FL2_compressCCtx reads past the end of its source "
                                 "(levels 4 and 6, data ending in a run with a period of 3-16): a crash when the "
                                 "source ends at a page boundary; fixed upstream after 1.0.1 (a793db99), which GrindCore "
                                 "takes on audit/native-fixes; "
                                 "audit/other-codecs.md 4.4.1",
    "fl2-empty-source-overread": "fast-lzma2 1.0.1's FL2_decompressDCtx reads the property byte of a 0-byte source "
                                 "and decrements srcSize to SIZE_MAX, then decodes the memory after the source as "
                                 "input: a crash at a page boundary, garbage accepted otherwise; unfixed upstream, fixed "
                                 "in fl2_decompress.c on audit/native-fixes; "
                                 "audit/other-codecs.md 4.4.1",
    "fl2-estimate-level-unchecked": "fast-lzma2 1.0.1's FL2_estimateCStreamSize indexes the level table without a "
                                    "check and adds what it reads to FL2_estimateCCtxSize's error code: levels "
                                    "below 0 or above 11 read past the table and return garbage, and level 0 leaves "
                                    "the dictionary out; unused by GrindCore.net; unfixed upstream, fixed in "
                                    "fl2_compress.c on audit/native-fixes; audit/other-codecs.md 4.4.1 finding 8",
    "fl2-timeout-wait-race": "fast-lzma2 1.0.1's FL2POOL_waitAll with a timeout (fl2_pool.c:181-190) reports 'done' "
                             "when the job is queued but not yet started, so under load FL2_waitDStream and "
                             "FL2_waitCStream return early: wrong decoded output or FL2_error_stage_wrong, and "
                             "encoded frames that don't decode. Only with FL2_setDStreamTimeout/FL2_setCStreamTimeout, "
                             "which GrindCore.net doesn't use; unfixed upstream, fixed in fl2_pool.c on "
                             "audit/native-fixes; audit/other-codecs.md 4.4.1",
    "fl2-decoder-tail-overread": "fast-lzma2's LZMA2 decoder decodes a chunk that's all there with LZMA_FINISH_END, "
                                 "whose fast loop runs to the chunk's last byte (lzma2_dec.c:870), so a corrupt chunk "
                                 "is read up to 19 bytes past its end: past the caller's buffer for the last chunk "
                                 "(FL2_decompress, FL2_decompressMt, FL2_decompressDCtx, the single-threaded stream). "
                                 "Read-only, corrupt input only; a crash only at a page boundary. Unfixed in 1.0.1 and "
                                 "master, fixed in lzma2_dec.c on audit/native-fixes (the last bytes are decoded from "
                                 "a padded copy); audit/other-codecs.md 4.4.1 finding 10",
    "fl2-lzma-chunk-without-props": "fast-lzma2's LZMA2 decoder never checks need_init_prop, so after a dictionary "
                                    "reset it decodes an LZMA chunk that carries no properties, with pb never set "
                                    "(lzma2_dec.c:959-961 set lc twice): out-of-bounds probability accesses on "
                                    "crafted input. Unfixed in 1.0.1 and master, fixed in lzma2_dec.c on "
                                    "audit/native-fixes (the user's decision, for Nanook's review); "
                                    "audit/other-codecs.md 4.4.1 finding 11",
    "lzma-null-args": "the LZMA/LZMA2 PAL passes every pointer through, and 7-Zip dereferences a NULL decoder, encoder, "
                      "length, status or properties pointer (even Dec_Free(NULL)), and decodes with a freed decoder or "
                      "past its dictionary: a crash; fixed in the PAL on audit/native-fixes; audit/lzma.md 3.6",
    "zstd-seekable-read-past-end": "upstream ZSTD_seekable_decompress clamps with `len = eos - offset`, which wraps "
                                   "when offset is past the end: it returns a near-2^64 length (success, beyond ~120 "
                                   "bytes out) without writing; fixed in both zstdseek_decompress.c copies on audit/native-fixes "
                                   "(beyond the end is frameIndex_tooLarge, at the end 0 bytes); audit/other-codecs.md 4.8.2",
    "zstd-seekable-checksum-gaps": "upstream ZSTD_seekable_decompress checks a frame's checksum only when zstd reports "
                                   "the frame's end inside the read, so a read that ends at a compressed frame's end "
                                   "(every DecompressFrame, every read to the end of the file) returns corrupt data "
                                   "unverified, as does a read ending inside a corrupt frame that decodes longer than "
                                   "the seek table says; fixed in both zstdseek_decompress.c copies on audit/native-fixes; "
                                   "audit/other-codecs.md 4.8.2",
    "zstd-seekable-numframes-overflow": "upstream loadSeekTable doesn't bound Number_Of_Frames by "
                                        "ZSTD_SEEKABLE_MAXFRAMES and sizes the table in 32 bits: a one-bit change "
                                        "makes it allocate and fill ~26 GB on 64-bit, and overflow the heap on 32-bit; "
                                        "fixed in both zstdseek_decompress.c copies on audit/native-fixes, as upstream "
                                        "PRs #4685/#4717 (open); audit/other-codecs.md 4.8.2",
    "zstd-seekable-frame-size-mismatch": "upstream ZSTD_seekable_decompress trusts the seek table's frame sizes: a "
                                         "frame that decodes shorter makes it re-decode that frame forever from a "
                                         "callback source, and a longer one overwrites the next frame's output "
                                         "unreported; fixed in both zstdseek_decompress.c copies on audit/native-fixes; "
                                         "audit/other-codecs.md 4.8.2",
    "zstd-seekable-table-unchecked": "upstream loadSeekTable takes each entry's sizes as they are: a size within zstd's "
                                     "error-code range below 4 GiB reads as an error code from 32-bit "
                                     "GetFrame*Size, and a table may claim more data than its frames hold or place "
                                     "frames past itself (a false GetDecompressedSize); checked in both seekable PALs "
                                     "on audit/native-fixes; audit/other-codecs.md 4.8.2",
    "zstd-seekable-framesize-overread":"upstream getFrameDecompressedSize accepts frameIndex == frame count and "
                                        "reads one entry past the seek table; fixed in both copies on audit/native-fixes, as "
                                        "upstream PR #4758 (open); audit/other-codecs.md 4.8.2",
}

# Documented defects the suite checks for but doesn't fail on (they're audit findings, tracked there). A change in
# any of them -- fixed, or a new gap -- does fail, so the table has to be updated along with the fix.
KNOWN_SYMBOL_GAPS = {
    "DN8_ZLib_v1_3_1_crc32": "P/Invoke name has the wrong case (export is ..._Crc32); abi-verification.md 4.6, hub row 16",
    "DN9_ZLibNg_v2_2_1_crc32": "P/Invoke name has the wrong case (export is ..._Crc32); abi-verification.md 4.6, hub row 16",
    "SZ_Lz4_v1_10_0_CompressPartial": "P/Invoked but defined nowhere in the shipped binaries; implemented on audit/native-fixes (other-codecs.md 4.1); platforms.md 10.6",
    "z7_Black2sp_Prepare": "P/Invoke names an unexported symbol (export is SZ_Black2sp_Prepare); abi-verification.md 4.6",
    "SZ_Lz4_v1_10_0_AttachDict": "dllexport only, missing from entrypoints.c: absent on Linux/macOS; other-codecs.md 4.1",
    "SZ_Lz4_v1_10_0_DecompressPartialUsingDict": "dllexport only, missing from entrypoints.c; other-codecs.md 4.1",
    "SZ_Lz4_v1_10_0_DecompressUsingDict": "dllexport only, missing from entrypoints.c; other-codecs.md 4.1",
    "SZ_Lz4_v1_10_0_Flush": "dllexport only, missing from entrypoints.c; other-codecs.md 4.1",
    "SZ_Lz4_v1_10_0_ResetStream": "dllexport only, missing from entrypoints.c; other-codecs.md 4.1",
}
