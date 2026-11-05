"""zstd helpers shared by test_zstd.py: GrindCore's PAL (SZ_ZStd_v1_5_2_* / SZ_ZStd_v1_5_7_*), the official reference
libraries built by ref/build_zstd_ref.py, and drivers that push the same call sequence through either side so their
output can be compared byte for byte."""
import ctypes
import os
import struct
import sys

vp, sz, i32, u32 = ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int, ctypes.c_uint

MAGIC = b"\x28\xb5\x2f\xfd"                 # a zstd frame
SKIPPABLE_SEEK_TABLE = 0x184D2A5E           # the seek table is skippable frame variant 14
SEEK_TABLE_FOOTER_MAGIC = 0x8F92EAB1
FRAMEINDEX_TOOLARGE = (1 << 64) - 2         # ZSTD_SEEKABLE_FRAMEINDEX_TOOLARGE
MAX_FRAME = {"1.5.2": 0x80000000, "1.5.7": 0x40000000}  # ZSTD_SEEKABLE_MAX_FRAME_DECOMPRESSED_SIZE per release
# ZSTD_cParameter ids. targetCBlockSize is experimental (1003) in 1.5.2 and stable (130) in 1.5.7 (zstd.md 9.3).
C_LEVEL, C_WINDOWLOG, C_WORKERS, C_JOBSIZE = 100, 101, 400, 401
C_TARGETCBLOCK = {"1.5.2": 1003, "1.5.7": 130}
C_CONTENTSIZE, C_CHECKSUM, C_DICTID = 200, 201, 202     # the frame flags, stable in both
FLAG_SETS = [(cs, ck, di) for cs in (1, 0) for ck in (0, 1) for di in (1, 0)]   # (content size, checksum, dict ID)
CONTINUE, FLUSH, END, END_FRAME = 0, 1, 2, "end_frame"


class ZErr(Exception):
    def __init__(self, what, code, name):
        Exception.__init__(self, "%s: %s" % (what, name))
        self.code, self.name = code, name


class NoProgress(Exception):
    pass


def bound(n):
    """ZSTD_compressBound."""
    return n + (n >> 8) + (((128 << 10) - n) >> 11 if n < (128 << 10) else 0)


class _Buf(object):
    """Input bytes at a fixed address (ctypes copies a bytes argument's pointer, but offsets need real addresses)."""

    def __init__(self, data):
        self.keep = ctypes.create_string_buffer(bytes(data), max(len(data), 1))
        self.addr, self.n = ctypes.addressof(self.keep), len(data)


# ------------------------------------------------------------------------------------------------ GrindCore's PAL


class ZStd(object):
    """One version of GrindCore's zstd PAL: ZStd(L, "1.5.7")."""

    def __init__(self, L, version):
        self.L, self.version = L, version
        self.P = "SZ_ZStd_v%s_" % version.replace(".", "_")

    def fn(self, name):
        return getattr(self.L, self.P + name)

    def struct(self, name):
        return getattr(self.L.struct, self.P + name)()

    def is_error(self, r):
        return bool(self.fn("IsError")(r))

    def error_name(self, r):
        return self.fn("GetErrorName")(r).decode()

    def check(self, r, what):
        if self.is_error(r):
            raise ZErr(what, r, self.error_name(r))
        return r

    # --- contexts ---

    def cctx(self, level=None, workers=None, job=None, block=None, cdict=None):
        c = self.struct("CompressionContext")
        if self.fn("CreateCompressionContext")(ctypes.byref(c)) != 0:
            raise MemoryError("CreateCompressionContext")
        for name, value in (("SetCompressionLevel", level), ("SetNbWorkers", workers), ("SetJobSize", job),
                            ("SetBlockSize", block)):
            if value is not None:
                # the PAL returns ZSTD_CCtx_setParameter's result as int32: the value set, or -(error code); its own
                # argument checks return -1
                rc = self.fn(name)(ctypes.byref(c), value)
                if rc < 0:
                    self.fn("FreeCompressionContext")(ctypes.byref(c))
                    raise ValueError("%s(%r) returned %d" % (name, value, rc))
        if cdict is not None and self.fn("SetCompressionDict")(ctypes.byref(c), ctypes.byref(cdict)) != 0:
            raise ValueError("SetCompressionDict")
        return c

    def free_cctx(self, c):
        self.fn("FreeCompressionContext")(ctypes.byref(c))

    def dctx(self, ddict=None):
        d = self.struct("DecompressionContext")
        if self.fn("CreateDecompressionContext")(ctypes.byref(d)) != 0:
            raise MemoryError("CreateDecompressionContext")
        if ddict is not None and self.fn("SetDecompressionDict")(ctypes.byref(d), ctypes.byref(ddict)) != 0:
            raise ValueError("SetDecompressionDict")
        return d

    def free_dctx(self, d):
        self.fn("FreeDecompressionContext")(ctypes.byref(d))

    def cdict(self, content, level, window_log=0):
        d = self.struct("CompressionDict")
        rc = self.fn("CreateCompressionDict")(ctypes.byref(d), content, len(content), level, window_log)
        return rc, d

    def ddict(self, content):
        d = self.struct("DecompressionDict")
        rc = self.fn("CreateDecompressionDict")(ctypes.byref(d), content, len(content))
        return rc, d

    # --- one-shot ---

    def compress_block(self, data, level, capacity=None, cdict=None):
        """(return value, output or None)."""
        c = self.cctx()
        try:
            cap = bound(len(data)) if capacity is None else capacity
            dst, src = ctypes.create_string_buffer(max(cap, 1)), _Buf(data)
            if cdict is None:
                r = self.fn("CompressBlock")(ctypes.byref(c), dst, cap, src.addr, src.n, level)
            else:
                r = self.fn("CompressBlockWithDict")(ctypes.byref(c), ctypes.byref(cdict), dst, cap, src.addr, src.n)
            return r, (None if self.is_error(r) else ctypes.string_at(dst, r))
        finally:
            self.free_cctx(c)

    def compress_block_advanced(self, data, level, flags=(1, 0, 1), capacity=None, cdict=None):
        """CompressBlockAdvanced, or CompressBlockWithDictAdvanced with a cdict (which carries the level), with the
        frame flags (content size, checksum, dictionary ID): (return value, output or None)."""
        c = self.cctx()
        try:
            cap = bound(len(data)) if capacity is None else capacity
            dst, src = ctypes.create_string_buffer(max(cap, 1)), _Buf(data)
            if cdict is None:
                r = self.fn("CompressBlockAdvanced")(ctypes.byref(c), dst, cap, src.addr, src.n, level, *flags)
            else:
                r = self.fn("CompressBlockWithDictAdvanced")(ctypes.byref(c), ctypes.byref(cdict), dst, cap, src.addr,
                                                              src.n, *flags)
            return r, (None if self.is_error(r) else ctypes.string_at(dst, r))
        finally:
            self.free_cctx(c)

    def decompress_block(self, stream, capacity, ddict=None):
        d = self.dctx()
        try:
            dst, src = ctypes.create_string_buffer(max(capacity, 1)), _Buf(stream)
            if ddict is None:
                r = self.fn("DecompressBlock")(ctypes.byref(d), dst, capacity, src.addr, src.n)
            else:
                r = self.fn("DecompressBlockWithDict")(ctypes.byref(d), ctypes.byref(ddict), dst, capacity, src.addr,
                                                       src.n)
            return r, (None if self.is_error(r) else ctypes.string_at(dst, r))
        finally:
            self.free_dctx(d)

    # --- the objects drive() pushes calls through ---

    def stream_encoder(self, **kw):
        z, c = self, self.cctx(**kw)
        names = {CONTINUE: "CompressStream", FLUSH: "FlushStream", END: "EndStream"}

        class Enc(object):
            is_error = staticmethod(z.is_error)
            error_name = staticmethod(z.error_name)

            def call(self, op, dst, cap, src, n):
                i, o = ctypes.c_int64(), ctypes.c_int64()
                r = z.fn(names[op])(ctypes.byref(c), dst, cap, src, n, ctypes.byref(i), ctypes.byref(o))
                return r, i.value, o.value

            def close(self):
                z.free_cctx(c)
        return Enc()

    def stream_decoder(self, ddict=None):
        z, d = self, self.dctx(ddict)

        class Dec(object):
            is_error = staticmethod(z.is_error)
            error_name = staticmethod(z.error_name)

            def call(self, dst, cap, src, n):
                i, o = ctypes.c_int64(), ctypes.c_int64()
                r = z.fn("DecompressStream")(ctypes.byref(d), dst, cap, src, n, ctypes.byref(i), ctypes.byref(o))
                return r, i.value, o.value

            def close(self):
                z.free_dctx(d)
        return Dec()

    def seekable_encoder(self, level, checksum, max_frame):
        z, c = self, self.struct("SeekableCStream")
        if z.fn("Seekable_CreateCStream")(ctypes.byref(c)) != 0:
            raise MemoryError("Seekable_CreateCStream")
        init = z.fn("Seekable_InitCStream")(ctypes.byref(c), level, checksum, max_frame)

        class Enc(object):
            is_error = staticmethod(z.is_error)
            error_name = staticmethod(z.error_name)
            init_result = init

            def call(self, op, dst, cap, src, n):
                o = ctypes.c_int64()
                if op == CONTINUE:
                    i = ctypes.c_int64()
                    r = z.fn("Seekable_CompressStream")(ctypes.byref(c), dst, cap, src, n, ctypes.byref(i),
                                                        ctypes.byref(o))
                    return r, i.value, o.value
                r = z.fn("Seekable_EndFrame" if op == END_FRAME else "Seekable_EndStream")(ctypes.byref(c), dst, cap,
                                                                                         ctypes.byref(o))
                return r, 0, o.value

            def close(self):
                z.fn("Seekable_FreeCStream")(ctypes.byref(c))
        return Enc()

    def seekable(self, stream, callbacks=False):
        return Seekable(self, stream, callbacks)


def callback_types(stdcall=False):
    """(read, seek) prototypes for ZSTD_seekable_read(opaque, buffer, n) and ZSTD_seekable_seek(opaque, offset,
    origin). The callback typedefs name no calling convention, so they take the build's default: GrindCore's win-x86
    build compiles C with /Gz, making them __stdcall there (hub row 17). test_zstd probes which one a library uses."""
    f = ctypes.WINFUNCTYPE if stdcall else ctypes.CFUNCTYPE
    return f(ctypes.c_int, vp, vp, sz), f(ctypes.c_int, vp, ctypes.c_longlong, ctypes.c_int)


SEEK_READ, SEEK_SEEK = callback_types()
CALLBACK_STDCALL = False        # set by test_zstd from its probe; only ever True on 32-bit Windows


class Seekable(object):
    """GrindCore's seekable decoder over an in-memory stream: through Seekable_InitBuff (which keeps a pointer to
    it), or with callbacks=True through Seekable_InitAdvanced and read/seek callbacks, the path GrindCore.net's
    stream-based ZStdSeekableDecoder takes. `reads` counts the callback reads."""

    def __init__(self, z, stream, callbacks=False):
        self.z, self.src = z, _Buf(stream)
        self.s = z.struct("Seekable")
        if z.fn("Seekable_Create")(ctypes.byref(self.s)) != 0:
            raise MemoryError("Seekable_Create")
        if not callbacks:
            self.init = z.fn("Seekable_InitBuff")(ctypes.byref(self.s), self.src.addr, self.src.n)
            return
        data, state = bytes(stream), {"pos": 0}
        self.reads = 0

        def read(opaque, buffer, n):
            self.reads += 1
            if state["pos"] + n > len(data):
                return -1
            ctypes.memmove(buffer, data[state["pos"]:state["pos"] + n], n)
            state["pos"] += n
            return 0

        def seek(opaque, offset, origin):
            base = {0: 0, 1: state["pos"], 2: len(data)}.get(origin)
            if base is None or not 0 <= base + offset <= len(data):
                return -1
            state["pos"] = base + offset
            return 0
        read_t, seek_t = callback_types(CALLBACK_STDCALL)
        self.callbacks = (read_t(read), seek_t(seek))           # kept alive as long as the decoder
        self.init = z.fn("Seekable_InitAdvanced")(ctypes.byref(self.s), None, ctypes.cast(self.callbacks[0], vp),
                                                  ctypes.cast(self.callbacks[1], vp))

    def f(self, name, *a):
        return self.z.fn("Seekable_" + name)(ctypes.byref(self.s), *a)

    def frames(self):
        return self.f("GetNumFrames")

    def size(self):
        return self.f("GetDecompressedSize")

    def read(self, offset, n):
        dst = ctypes.create_string_buffer(max(n, 1))
        r = self.f("Decompress", dst, n, offset)
        return r, (None if self.z.is_error(r) else ctypes.string_at(dst, min(r, n)))

    def frame(self, index, capacity):
        dst = ctypes.create_string_buffer(max(capacity, 1))
        r = self.f("DecompressFrame", dst, capacity, index)
        return r, (None if self.z.is_error(r) else ctypes.string_at(dst, min(r, capacity)))

    def close(self):
        self.f("Free")


# ------------------------------------------------------------------------------------------------ official zstd


class _In(ctypes.Structure):
    _fields_ = [("src", vp), ("size", sz), ("pos", sz)]


class _Out(ctypes.Structure):
    _fields_ = [("dst", vp), ("size", sz), ("pos", sz)]


class _CMem(ctypes.Structure):
    _fields_ = [("customAlloc", vp), ("customFree", vp), ("opaque", vp)]


class _FParams(ctypes.Structure):        # ZSTD_frameParameters, passed by value
    _fields_ = [("contentSizeFlag", ctypes.c_int), ("checksumFlag", ctypes.c_int), ("noDictIDFlag", ctypes.c_int)]


def reference(version):
    """The official library for this version from $GC_REF_ZSTD, or None."""
    folder = os.environ.get("GC_REF_ZSTD")
    if not folder:
        return None
    ext = ".dylib" if sys.platform == "darwin" else ".dll" if sys.platform == "win32" else ".so"
    path = os.path.join(folder, "libzstdref-%s%s" % (version, ext))
    return Ref(path, version) if os.path.exists(path) else None


# Every official function the tests call. ref/build_zstd_ref.py exports exactly these from its Windows DLLs (MSVC
# needs a .def); off Windows every global symbol is exported anyway.
REF_FUNCTIONS = [
    ("ZSTD_versionNumber", u32, []), ("ZSTD_isError", u32, [sz]), ("ZSTD_getErrorName", ctypes.c_char_p, [sz]),
    ("ZSTD_createCCtx", vp, []), ("ZSTD_freeCCtx", sz, [vp]),
    ("ZSTD_compressCCtx", sz, [vp, vp, sz, vp, sz, i32]),
    ("ZSTD_CCtx_setParameter", sz, [vp, i32, i32]), ("ZSTD_CCtx_refCDict", sz, [vp, vp]),
    ("ZSTD_compressStream2", sz, [vp, ctypes.POINTER(_Out), ctypes.POINTER(_In), i32]),
    ("ZSTD_createCDict", vp, [vp, sz, i32]), ("ZSTD_freeCDict", sz, [vp]),
    ("ZSTD_createCCtxParams", vp, []), ("ZSTD_freeCCtxParams", sz, [vp]),
    ("ZSTD_CCtxParams_init", sz, [vp, i32]), ("ZSTD_CCtxParams_setParameter", sz, [vp, i32, i32]),
    ("ZSTD_createCDict_advanced2", vp, [vp, sz, i32, i32, vp, _CMem]),
    ("ZSTD_compress_usingCDict", sz, [vp, vp, sz, vp, sz, vp]),
    ("ZSTD_compress2", sz, [vp, vp, sz, vp, sz]),
    ("ZSTD_compress_usingCDict_advanced", sz, [vp, vp, sz, vp, sz, vp, _FParams]),
    ("ZSTD_decompress", sz, [vp, sz, vp, sz]),
    ("ZSTD_CStreamInSize", sz, []), ("ZSTD_CStreamOutSize", sz, []),
    ("ZSTD_writeSkippableFrame", sz, [vp, sz, vp, sz, u32]),
    ("ZSTD_seekable_createCStream", vp, []), ("ZSTD_seekable_freeCStream", sz, [vp]),
    ("ZSTD_seekable_initCStream", sz, [vp, i32, i32, u32]),
    ("ZSTD_seekable_compressStream", sz, [vp, ctypes.POINTER(_Out), ctypes.POINTER(_In)]),
    ("ZSTD_seekable_endFrame", sz, [vp, ctypes.POINTER(_Out)]),
    ("ZSTD_seekable_endStream", sz, [vp, ctypes.POINTER(_Out)]),
    ("ZSTD_seekable_create", vp, []), ("ZSTD_seekable_free", sz, [vp]),
    ("ZSTD_seekable_initBuff", sz, [vp, vp, sz]),
    ("ZSTD_seekable_decompress", sz, [vp, vp, sz, ctypes.c_ulonglong]),
    ("ZSTD_seekable_getNumFrames", u32, [vp])]


class Ref(object):
    """Official zstd and its seekable contrib, by their own names."""

    def __init__(self, path, version):
        r = self.dll = ctypes.CDLL(path)
        self.version, self.path = version, path
        for name, res, args in REF_FUNCTIONS:
            fn = getattr(r, name)
            fn.restype, fn.argtypes = res, args
            setattr(self, name[5:], fn)

    def is_error(self, v):
        return bool(self.isError(v))

    def error_name(self, v):
        return self.getErrorName(v).decode()

    def cctx(self, level=None, workers=None, job=None, block=None, cdict=None):
        c = self.createCCtx()
        for param, value in ((C_LEVEL, level), (C_WORKERS, workers), (C_JOBSIZE, job),
                             (C_TARGETCBLOCK[self.version], block)):
            if value is not None and self.is_error(self.CCtx_setParameter(c, param, value)):
                raise ValueError("ZSTD_CCtx_setParameter(%d, %r)" % (param, value))
        if cdict is not None and self.is_error(self.CCtx_refCDict(c, cdict)):
            raise ValueError("ZSTD_CCtx_refCDict")
        return c

    def compress_block(self, data, level, cdict=None, capacity=None):
        c, src = self.createCCtx(), _Buf(data)
        try:
            cap = bound(len(data)) if capacity is None else capacity
            dst = ctypes.create_string_buffer(max(cap, 1))
            if cdict is None:
                r = self.compressCCtx(c, dst, cap, src.addr, src.n, level)
            else:
                r = self.compress_usingCDict(c, dst, cap, src.addr, src.n, cdict)
            return r, (None if self.is_error(r) else ctypes.string_at(dst, r))
        finally:
            self.freeCCtx(c)

    def compress_block_advanced(self, data, level, flags=(1, 0, 1), cdict=None):
        """zstd's advanced API, as python-zstandard and Python's compression.zstd use it: the level and the frame
        flags set as parameters, then ZSTD_compress2. With a cdict, ZSTD_compress_usingCDict_advanced, the dictionary
        call that takes the flags."""
        c, src = self.createCCtx(), _Buf(data)
        try:
            dst = ctypes.create_string_buffer(max(bound(len(data)), 1))
            if cdict is None:
                for param, value in zip((C_LEVEL, C_CONTENTSIZE, C_CHECKSUM, C_DICTID), (level,) + tuple(flags)):
                    if self.is_error(self.CCtx_setParameter(c, param, value)):
                        raise ValueError("ZSTD_CCtx_setParameter(%d, %r)" % (param, value))
                r = self.compress2(c, dst, len(dst), src.addr, src.n)
            else:
                cs, ck, di = flags
                r = self.compress_usingCDict_advanced(c, dst, len(dst), src.addr, src.n, cdict, _FParams(cs, ck, 1 - di))
            return r, (None if self.is_error(r) else ctypes.string_at(dst, r))
        finally:
            self.freeCCtx(c)

    def make_cdict(self, content, level, window_log=0):
        """As GrindCore's CreateCompressionDict does it: createCDict, or createCDict_advanced2 with a windowLog."""
        if not window_log:
            return self.createCDict(content, len(content), level)
        p = self.createCCtxParams()
        self.CCtxParams_init(p, level)
        self.CCtxParams_setParameter(p, C_WINDOWLOG, window_log)
        d = self.createCDict_advanced2(content, len(content), 0, 0, p, _CMem())   # ZSTD_dlm_byCopy, ZSTD_dct_auto
        self.freeCCtxParams(p)
        return d

    def stream_encoder(self, **kw):
        ref, c = self, self.cctx(**kw)

        class Enc(object):
            is_error = staticmethod(ref.is_error)
            error_name = staticmethod(ref.error_name)

            def call(self, op, dst, cap, src, n):
                o, i = _Out(ctypes.addressof(dst), cap, 0), _In(src, n, 0)
                r = ref.compressStream2(c, ctypes.byref(o), ctypes.byref(i), op)
                return r, i.pos, o.pos

            def close(self):
                ref.freeCCtx(c)
        return Enc()

    def seekable_encoder(self, level, checksum, max_frame):
        ref, z = self, self.seekable_createCStream()
        init = self.seekable_initCStream(z, level, checksum, max_frame)

        class Enc(object):
            is_error = staticmethod(ref.is_error)
            error_name = staticmethod(ref.error_name)
            init_result = init

            def call(self, op, dst, cap, src, n):
                o = _Out(ctypes.addressof(dst), cap, 0)
                if op == CONTINUE:
                    i = _In(src, n, 0)
                    r = ref.seekable_compressStream(z, ctypes.byref(o), ctypes.byref(i))
                    return r, i.pos, o.pos
                r = (ref.seekable_endFrame if op == END_FRAME else ref.seekable_endStream)(z, ctypes.byref(o))
                return r, 0, o.pos

            def close(self):
                ref.seekable_freeCStream(z)
        return Enc()

    def seekable_read_all(self, stream):
        """(frames, data) through the official seekable decoder."""
        z, src = self.seekable_create(), _Buf(stream)
        try:
            r = self.seekable_initBuff(z, src.addr, src.n)
            if self.is_error(r):
                raise ZErr("ZSTD_seekable_initBuff", r, self.error_name(r))
            frames = self.seekable_getNumFrames(z)
            out, pos = bytearray(), 0
            dst = ctypes.create_string_buffer(1 << 16)
            while True:
                r = self.seekable_decompress(z, dst, len(dst), pos)
                if self.is_error(r):
                    raise ZErr("ZSTD_seekable_decompress", r, self.error_name(r))
                if r == 0:
                    return frames, bytes(out)
                out += ctypes.string_at(dst, r)
                pos += r
        finally:
            self.seekable_free(z)


def drop_trailing_empty_frame(stream):
    """A seekable stream minus its last frame and seek-table entry when that frame is empty: official 1.5.7's output
    as GrindCore's 1.5.7 writes it, since GrindCore keeps 1.5.2's endStream check (no empty frame at the end)."""
    n, desc = struct.unpack("<IB", stream[-9:-4])
    size = 12 if desc >> 7 else 8
    table_len = 8 + n * size + 9
    if n == 0 or len(stream) < table_len:
        return stream
    frames, entries = stream[:-table_len], stream[-table_len + 8:-9]
    csize, dsize = struct.unpack("<II", entries[-size:][:8])
    if dsize:
        return stream
    table = entries[:-size] + struct.pack("<IBI", n - 1, desc, SEEK_TABLE_FOOTER_MAGIC)
    return frames[:len(frames) - csize] + struct.pack("<II", SKIPPABLE_SEEK_TABLE, len(table)) + table


# ------------------------------------------------------------------------------------------------ drivers


def drive(enc, data, step, out_cap=1 << 17, flush_at=(), end_frame_at=(), finish=True, max_idle=64):
    """Push `data` through an encoder in `step`-byte pieces, each piece until it's consumed. After a piece ending at a
    position in flush_at, FLUSH until done (or END_FRAME for end_frame_at, the seekable encoder's own frame end);
    then END until done. The same arguments give the same native call sequence on either side."""
    src = _Buf(data)
    dst = ctypes.create_string_buffer(out_cap)
    out = bytearray()
    state = {"idle": 0}

    def call(op, p, n):
        r, i, o = enc.call(op, dst, out_cap, src.addr + p, n)
        if enc.is_error(r):
            raise ZErr("op %s at %d" % (op, p), r, enc.error_name(r))
        out.extend(ctypes.string_at(dst, o))
        state["idle"] = state["idle"] + 1 if (i == 0 and o == 0) else 0
        if state["idle"] > max_idle:
            raise NoProgress("op %s at %d: %d calls with no progress" % (op, p, state["idle"]))
        return r, i

    def until_done(op, p):
        while True:
            r, _ = call(op, p, 0)
            if r == 0:
                return

    pos = 0
    while pos < len(data):
        n = min(step, len(data) - pos)
        off = 0
        while off < n:
            off += call(CONTINUE, pos + off, n - off)[1]
        pos += n
        if pos in flush_at:
            until_done(FLUSH, pos)
        if pos in end_frame_at:
            until_done(END_FRAME, pos)
    if finish:
        until_done(END, pos)
    return bytes(out)


def encode(enc, data, step, **kw):
    try:
        return drive(enc, data, step, **kw)
    finally:
        enc.close()


def drain(dec, stream, in_step, out_cap=1 << 17, max_idle=64):
    """Stream-decode in in_step pieces. Returns (output, last return value): 0 if the last frame ended, else
    the input zstd still wants."""
    src = _Buf(stream)
    dst = ctypes.create_string_buffer(out_cap)
    out, pos, idle, r = bytearray(), 0, 0, None
    try:
        while True:
            n = min(in_step, len(stream) - pos)
            r, i, o = dec.call(dst, out_cap, src.addr + pos, n)
            if dec.is_error(r):
                raise ZErr("DecompressStream at %d" % pos, r, dec.error_name(r))
            out.extend(ctypes.string_at(dst, o))
            pos += i
            idle = idle + 1 if (i == 0 and o == 0) else 0
            if idle > max_idle:
                raise NoProgress("DecompressStream at %d: no progress" % pos)
            # done when the input is used up and zstd either says the frame ended (0) or can make no more progress
            # (a truncated stream: its hint is what it still wants)
            if pos == len(stream) and (r == 0 or (i == 0 and o < out_cap)):
                return bytes(out), r
    finally:
        dec.close()
