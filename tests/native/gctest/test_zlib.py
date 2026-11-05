"""zlib 1.3.1 (DN8_ZLib_v1_3_1_*) and zlib-ng 2.2.1 (DN9_ZLibNg_v2_2_1_*) through GrindCore's PAL.

The PAL is .NET's pal_zlib.c (renamed exports) plus one-shot wrappers; Nanook's hooks in the vendored sources are
compress3/uncompress3 (compress2/uncompress2 with deflateInit2/inflateInit2 parameters). Every stream is checked by
an independent decoder, Python's zlib, and by the other GrindCore zlib. zlib-ng's NEON Adler-32 faults on the shipped
linux-arm build (platforms.md 10.7); that is probed once in a child process and reported as a known defect."""
import ctypes
import os
import sys
import zlib
import unittest

import gcnative
import gctest
from gctest import sample
from gctest.test_hashes import child, known

Z_NO_FLUSH, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH, Z_BLOCK = 0, 1, 2, 3, 4, 5
Z_OK, Z_STREAM_END, Z_NEED_DICT, Z_STREAM_ERROR, Z_DATA_ERROR, Z_BUF_ERROR = 0, 1, 2, -2, -3, -5
DEFLATED = 8
RAW, ZLIB, GZIP, AUTO = -15, 15, 31, 47      # windowBits for each framing; 47 = detect zlib or gzip

LIBS = (("zlib 1.3.1", "DN8_ZLib_v1_3_1_"), ("zlib-ng 2.2.1", "DN9_ZLibNg_v2_2_1_"))


class ZError(Exception):
    pass


def py_decode(stream, wbits):
    """Python's zlib: (decoded, complete, unused bytes after the stream)."""
    d = zlib.decompressobj(wbits)
    out = d.decompress(stream) + d.flush()
    return out, d.eof, d.unused_data


class Zlib(object):
    """One of GrindCore's zlibs through the PAL: stream deflate/inflate driven in any step sizes, and the one-shot
    wrappers."""

    def __init__(self, L, name, prefix):
        self.L, self.name, self.P = L, name, prefix

    def fn(self, n):
        return getattr(self.L, self.P + n)

    def _call(self, kind, s, out, step, res, mode):
        s.nextOut, s.availOut = out, step
        rc = self.fn(kind)(ctypes.byref(s), mode)
        res += ctypes.string_at(out, step - s.availOut)
        return rc

    def deflate(self, data, level=6, wbits=ZLIB, mem=8, strategy=0, in_step=0, out_step=0, flushes=None, offset=0):
        """Stream deflate. flushes = {input position: flush mode}; the input piece that reaches that position goes in
        with the flush, which is repeated until complete, and the end of the data likewise with Z_FINISH (as
        compress2 does). With the default steps that is one Z_FINISH call. Returns (output, {position: output length
        at that flush})."""
        s = self.L.struct.PAL_ZStream()
        rc = self.fn("DeflateInit2_")(ctypes.byref(s), level, DEFLATED, wbits, mem, strategy)
        if rc != Z_OK:
            self.fn("DeflateEnd")(ctypes.byref(s))
            raise ZError("DeflateInit2_ returned %d" % rc)
        try:
            src, addr = sample.placed(data, offset)
            in_step = in_step or max(len(data), 1)
            out_step = out_step or len(data) + len(data) // 8 + 1024
            obuf, oaddr = sample.placed(b"", 0, extra=out_step)
            res, marks, pos = bytearray(), {}, 0
            budget = 4 * (len(data) // in_step + 2 * len(data) // out_step + 64) + 2000
            for target, mode in sorted((flushes or {}).items()) + [(len(data), Z_FINISH)]:
                while True:
                    if s.availIn == 0 and pos < target:
                        n = min(in_step, target - pos)
                        s.nextIn, s.availIn, pos = addr + pos, n, pos + n
                    if pos == target:
                        break
                    rc = self._call("Deflate", s, oaddr, out_step, res, Z_NO_FLUSH)
                    if rc != Z_OK:
                        raise ZError("Deflate(NO_FLUSH) returned %d at input %d" % (rc, pos))
                    budget -= 1
                    if budget < 0:
                        raise ZError("Deflate made no progress (input %d, output %d)" % (pos, len(res)))
                while True:
                    rc = self._call("Deflate", s, oaddr, out_step, res, mode)
                    budget -= 1
                    if mode == Z_FINISH:
                        if rc == Z_STREAM_END:
                            break
                        if rc != Z_OK:
                            raise ZError("Deflate(FINISH) returned %d" % rc)
                    elif rc not in (Z_OK, Z_BUF_ERROR):
                        raise ZError("Deflate(%d) returned %d" % (mode, rc))
                    elif s.availOut:
                        break
                    if budget < 0:
                        raise ZError("Deflate(%d) did not complete" % mode)
                marks[target] = len(res)
            return bytes(res), marks
        finally:
            self.fn("DeflateEnd")(ctypes.byref(s))

    def inflate(self, stream, wbits=ZLIB, in_step=0, out_step=0, offset=0, out_offset=0, flush=Z_NO_FLUSH):
        """Stream inflate. Returns (last return code, output, input bytes left over, msg or None)."""
        s = self.L.struct.PAL_ZStream()
        rc = self.fn("InflateInit2_")(ctypes.byref(s), wbits)
        if rc != Z_OK:
            self.fn("InflateEnd")(ctypes.byref(s))
            raise ZError("InflateInit2_ returned %d" % rc)
        try:
            src, addr = sample.placed(stream, offset)
            in_step = in_step or max(len(stream), 1)
            out_step = out_step or 65536
            obuf, oaddr = sample.placed(b"", out_offset, extra=out_step)
            res, pos = bytearray(), 0
            while True:
                if s.availIn == 0 and pos < len(stream):
                    n = min(in_step, len(stream) - pos)
                    s.nextIn, s.availIn, pos = addr + pos, n, pos + n
                before = (s.availIn, len(res))
                rc = self._call("Inflate", s, oaddr, out_step, res, flush)
                if rc != Z_OK:
                    break
                if (s.availIn, len(res)) == before and pos == len(stream):
                    break           # no progress and no more input (zlib normally says Z_BUF_ERROR here)
            msg = ctypes.string_at(s.msg) if s.msg else None
            return rc, bytes(res), s.availIn + len(stream) - pos, msg
        finally:
            self.fn("InflateEnd")(ctypes.byref(s))

    def compress(self, data, level=None, params=None, capacity=None):
        """Compress / Compress2(level) / Compress3(level, *params). Returns (rc, output, reported length)."""
        cap = len(data) + len(data) // 8 + 64 if capacity is None else capacity
        dst = ctypes.create_string_buffer(max(cap, 1))
        n = ctypes.c_uint32(cap)
        if params is not None:
            rc = self.fn("Compress3")(dst, ctypes.byref(n), data, len(data), level, *params)
        elif level is not None:
            rc = self.fn("Compress2")(dst, ctypes.byref(n), data, len(data), level)
        else:
            rc = self.fn("Compress")(dst, ctypes.byref(n), data, len(data))
        return rc, dst.raw[:min(n.value, cap)], n.value

    def uncompress(self, stream, capacity, wbits=None, variant=""):
        """Uncompress (variant "") / Uncompress2 ("2") / Uncompress3 ("3", wbits). Returns (rc, output, consumed)."""
        dst = ctypes.create_string_buffer(max(capacity, 1))
        n = ctypes.c_uint32(capacity)
        m = ctypes.c_uint32(len(stream))
        if variant == "3":
            rc = self.fn("Uncompress3")(dst, ctypes.byref(n), stream, ctypes.byref(m), wbits)
        elif variant == "2":
            rc = self.fn("Uncompress2")(dst, ctypes.byref(n), stream, ctypes.byref(m))
        else:
            rc = self.fn("Uncompress")(dst, ctypes.byref(n), stream, len(stream))
        return rc, dst.raw[:min(n.value, capacity)], m.value

    def crc32(self, crc, data, offset=0):
        buf, addr = sample.placed(data, offset)
        return self.fn("Crc32")(crc, addr, len(data))


# ------------------------------------------------------------------ official zlib 1.3.1 and zlib-ng 2.2.1


class ZStream(ctypes.Structure):
    """zlib's z_stream (zlib.h), which zlib-ng's compat mode keeps."""
    _fields_ = [("next_in", ctypes.c_void_p), ("avail_in", ctypes.c_uint), ("total_in", ctypes.c_ulong),
                ("next_out", ctypes.c_void_p), ("avail_out", ctypes.c_uint), ("total_out", ctypes.c_ulong),
                ("msg", ctypes.c_char_p), ("state", ctypes.c_void_p), ("zalloc", ctypes.c_void_p),
                ("zfree", ctypes.c_void_p), ("opaque", ctypes.c_void_p), ("data_type", ctypes.c_int),
                ("adler", ctypes.c_ulong), ("reserved", ctypes.c_ulong)]


# Every official function the tests call. ref/build_zlib_ref.py exports exactly these from zlib's Windows DLL (MSVC
# needs a .def); zlib-ng's CMake exports its whole API, and off Windows every global symbol is exported anyway.
_vp, _int, _ulong, _zs = ctypes.c_void_p, ctypes.c_int, ctypes.c_ulong, ctypes.POINTER(ZStream)
REF_FUNCTIONS = [
    ("zlibVersion", ctypes.c_char_p, []),
    ("compressBound", _ulong, [_ulong]),
    ("compress2", _int, [_vp, ctypes.POINTER(_ulong), _vp, _ulong, _int]),
    ("deflateInit2_", _int, [_zs, _int, _int, _int, _int, _int, ctypes.c_char_p, _int]),
    ("deflate", _int, [_zs, _int]),
    ("deflateEnd", _int, [_zs]),
]
REF_FILES = {"zlib 1.3.1": "libzlibref-1.3.1", "zlib-ng 2.2.1": "libzlibngref-2.2.1"}


class Ref(object):
    """Official zlib or zlib-ng by their own names."""

    def __init__(self, path):
        self.dll = ctypes.CDLL(path)
        for name, res, args in REF_FUNCTIONS:
            fn = getattr(self.dll, name)
            fn.restype, fn.argtypes = res, args
        self.version = self.dll.zlibVersion().decode()

    def compress(self, data, level):
        n = ctypes.c_ulong(self.dll.compressBound(len(data)))
        dst = ctypes.create_string_buffer(n.value)
        rc = self.dll.compress2(dst, ctypes.byref(n), data, len(data), level)
        return rc, dst.raw[:n.value]

    def deflate(self, data, level, wbits, mem, strategy):
        """One deflateInit2 and one deflate(Z_FINISH) into room for everything: (rc, output)."""
        s = ZStream()
        rc = self.dll.deflateInit2_(ctypes.byref(s), level, DEFLATED, wbits, mem, strategy, b"1.3.1", ctypes.sizeof(s))
        if rc != Z_OK:
            return rc, b""
        cap = self.dll.compressBound(len(data)) + 64    # gzip's header and trailer, beyond compressBound's zlib framing
        src, dst = ctypes.create_string_buffer(data, max(len(data), 1)), ctypes.create_string_buffer(cap)
        s.next_in, s.avail_in = ctypes.cast(src, _vp), len(data)
        s.next_out, s.avail_out = ctypes.cast(dst, _vp), cap
        rc = self.dll.deflate(ctypes.byref(s), Z_FINISH)
        out = dst.raw[:cap - s.avail_out]
        self.dll.deflateEnd(ctypes.byref(s))
        return rc, out


def reference(name):
    """Official `name` ("zlib 1.3.1" or "zlib-ng 2.2.1") from $GC_REF_ZLIB, or None."""
    folder = os.environ.get("GC_REF_ZLIB")
    if not folder:
        return None
    ext = ".dylib" if sys.platform == "darwin" else ".dll" if sys.platform == "win32" else ".so"
    path = os.path.join(folder, REF_FILES[name] + ext)
    return Ref(path) if os.path.exists(path) else None


class _ZlibTests(object):
    """The same tests for both libraries; the subclasses pick one."""
    NAME = PREFIX = None
    probe = None

    @classmethod
    def setUpClass(cls):
        cls.z = Zlib(gctest.LIB, cls.NAME, cls.PREFIX)
        # the other zlib cross-checks every stream, unless it is zlib-ng on a build where it crashes
        cls.others = [Zlib(gctest.LIB, n, p) for n, p in LIBS
                      if p != cls.PREFIX and (p != LIBS[1][1] or zlibng_state()[0] == "ok")]
        cls.ref = reference(cls.NAME)

    def setUp(self):
        if self.probe is not None:
            self.probe(self)

    # --- checks -------------------------------------------------------------------------------------------------

    def assertDecodes(self, stream, wbits, expected, what=""):
        out, eof, unused = py_decode(stream, wbits)
        self.assertTrue(eof, "%s: Python's zlib found the stream incomplete %s" % (self.z.name, what))
        self.assertEqual(unused, b"", "%s: bytes after the stream end %s" % (self.z.name, what))
        self.assertEqual(out, expected, "%s: Python's zlib decodes different data %s" % (self.z.name, what))
        for o in self.others:
            rc, got, left, msg = o.inflate(stream, AUTO if wbits > 0 else wbits)
            self.assertEqual((rc, left), (Z_STREAM_END, 0), "%s can't decode %s's stream %s: %r" %
                             (o.name, self.z.name, what, msg))
            self.assertEqual(got, expected, "%s decodes %s's stream differently %s" % (o.name, self.z.name, what))

    # --- CRC-32 ---------------------------------------------------------------------------------------------------

    def test_crc32_matches_python(self):
        data = sample.mixed(1 << 20, seed=11)
        for n in list(range(0, 300)) + [1023, 1024, 1025, 4095, 4096, 4097, 65539, 1 << 20]:
            for start in (0, 0xFFFFFFFF, 0x12345678):
                self.assertEqual(self.z.crc32(start, data[:n]), zlib.crc32(data[:n], start),
                                 "%s CRC-32, %d bytes, start %#x" % (self.z.name, n, start))

    def test_crc32_any_alignment_and_chaining(self):
        data = sample.noise(70000, seed=12)
        want = zlib.crc32(data)
        for off in range(32):
            self.assertEqual(self.z.crc32(0, data, off), want, "input at +%d" % off)
        for cut in (1, 15, 16, 17, 63, 4096, 33333, 69999):
            self.assertEqual(self.z.crc32(self.z.crc32(0, data[:cut]), data[cut:], cut % 7), want, "split at %d" % cut)

    def test_crc32_negative_length_reads_nothing(self):
        """A negative length is no data, so the CRC comes back unchanged (build-exam.md W1). Libraries without the fix
        read about 4 GiB past the buffer, so the calls run in a child process."""
        rc, out, err = child("""
            buf = ctypes.create_string_buffer(b"123456789", 9)
            f = L.%sCrc32
            print(f(0, buf, 9), f(0x12345678, buf, -1), f(0x9ABCDEF0, buf, -2147483648), f(7, None, 5))
        """ % self.PREFIX)
        if rc != 0:
            known(self, "zlib-crc32-negative-length")
        nine, minus_one, int_min, null = (int(x) for x in out.split())
        self.assertEqual(nine, 0xCBF43926, "%s CRC-32 of '123456789'" % self.z.name)
        self.assertEqual(minus_one, 0x12345678, "%s: a negative length must leave the CRC unchanged" % self.z.name)
        self.assertEqual(int_min, 0x9ABCDEF0, "%s: INT32_MIN must leave the CRC unchanged" % self.z.name)
        self.assertEqual(null, 0, "%s: a NULL buffer is zlib's 'initial value' idiom" % self.z.name)

    # --- stream deflate ---------------------------------------------------------------------------------------------

    def test_deflate_every_level_and_framing(self):
        for kind, gen in sample.KINDS:
            data = gen(200000, seed=21)
            for wbits in (RAW, ZLIB, GZIP):
                for level in range(-1, 10):
                    out, _ = self.z.deflate(data, level=level, wbits=wbits)
                    self.assertDecodes(out, wbits, data, "(%s, level %d, windowBits %d)" % (kind, level, wbits))

    def test_deflate_parameter_grid(self):
        data = sample.mixed(48000, seed=22)
        for frame in (-1, 1, 17):                 # raw, zlib, gzip: windowBits = frame * bits (+16 for gzip)
            for bits in (9, 12, 15):
                wbits = -bits if frame < 0 else bits + (16 if frame == 17 else 0)
                for mem in (1, 5, 9):
                    for strategy in range(5):     # default, filtered, huffman-only, RLE, fixed
                        for level in (1, 6, 9):
                            what = "(windowBits %d, memLevel %d, strategy %d, level %d)" % (wbits, mem, strategy, level)
                            out, _ = self.z.deflate(data, level=level, wbits=wbits, mem=mem, strategy=strategy)
                            self.assertDecodes(out, wbits, data, what)

    def test_deflate_small_steps_give_the_same_stream(self):
        """Input and output in 1-byte and odd-sized steps: the stream must decode, and where the algorithm only looks
        at a full lookahead it must be the one-call stream byte for byte: zlib levels 1-9, zlib-ng's deflate_fast
        (2-3) and deflate_slow (7-9). Level 0 (stored block sizes follow the output space), zlib-ng's deflate_quick
        (1, opens its block with the flush mode it sees first) and deflate_medium (4-6) legitimately depend on the
        steps (measured: zlib-ng levels 1, 5 and 6 differ on this data)."""
        small, big = sample.text(6000, seed=23), sample.mixed(120000, seed=24)
        exact = (2, 3, 7, 8, 9) if "ng" in self.z.name else range(1, 10)
        for level in range(0, 10):
            for data, steps in ((small, ((1, 1), (1, 4096), (4096, 1))), (big, ((7, 13), (4093, 65537), (65536, 5)))):
                whole, _ = self.z.deflate(data, level=level)
                for in_step, out_step in steps:
                    what = "(level %d, %d bytes, steps in %d / out %d)" % (level, len(data), in_step, out_step)
                    out, _ = self.z.deflate(data, level=level, in_step=in_step, out_step=out_step)
                    self.assertDecodes(out, ZLIB, data, what)
                    if level in exact:
                        self.assertEqual(out, whole, "%s: stream depends on the step sizes %s" % (self.z.name, what))

    def test_flush_modes(self):
        a, b = sample.text(70000, seed=25), sample.mixed(90000, seed=26)
        data, p = a + b, len(a)
        for wbits in (RAW, ZLIB, GZIP):
            for mode, name in ((Z_SYNC_FLUSH, "SYNC"), (Z_FULL_FLUSH, "FULL"), (Z_PARTIAL_FLUSH, "PARTIAL"),
                               (Z_BLOCK, "BLOCK")):
                for out_step in (0, 97):
                    what = "(Z_%s_FLUSH, windowBits %d, output step %d)" % (name, wbits, out_step)
                    out, marks = self.z.deflate(data, level=6, wbits=wbits, flushes={p: mode}, out_step=out_step)
                    self.assertDecodes(out, wbits, data, what)
                    head = out[:marks[p]]
                    if mode in (Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_PARTIAL_FLUSH):
                        d = zlib.decompressobj(wbits)
                        self.assertEqual(d.decompress(head), a, "everything before the flush is decodable %s" % what)
                    if mode in (Z_SYNC_FLUSH, Z_FULL_FLUSH):
                        self.assertEqual(head[-4:], b"\x00\x00\xff\xff", "sync marker %s" % what)
                    if mode == Z_FULL_FLUSH and wbits == RAW:
                        tail, eof, _ = py_decode(out[marks[p]:], RAW)
                        self.assertEqual((tail, eof), (b, True), "after a full flush the rest stands alone %s" % what)

    def test_invalid_flush_value_is_rejected_and_harmless(self):
        data = sample.text(20000, seed=27)
        s = self.z.L.struct.PAL_ZStream()
        self.assertEqual(self.z.fn("DeflateInit2_")(ctypes.byref(s), 6, DEFLATED, ZLIB, 8, 0), Z_OK)
        try:
            src, addr = sample.placed(data, 0)
            out = ctypes.create_string_buffer(len(data) + 1024)
            for bad in (-1, 6, 99):
                s.nextIn, s.availIn, s.nextOut, s.availOut = addr, len(data), ctypes.addressof(out), len(out)
                self.assertEqual(self.z.fn("Deflate")(ctypes.byref(s), bad), Z_STREAM_ERROR, "flush %d" % bad)
            self.assertEqual(self.z.fn("Deflate")(ctypes.byref(s), Z_FINISH), Z_STREAM_END)
            self.assertDecodes(out.raw[:len(out) - s.availOut], ZLIB, data, "(after rejected flush values)")
        finally:
            self.z.fn("DeflateEnd")(ctypes.byref(s))

    def test_unaligned_buffers(self):
        """Input and output at every offset from a 32-byte boundary: the SIMD Adler-32/CRC-32/copy paths
        (zlib-ng's NEON Adler-32 needed 32-byte alignment on the shipped linux-arm build, platforms.md 10.7)."""
        data = sample.mixed(100000, seed=28)
        for off in range(32):
            for wbits in (ZLIB, GZIP):
                out, _ = self.z.deflate(data, level=1 + off % 9, wbits=wbits, offset=off)
                self.assertDecodes(out, wbits, data, "(input at +%d)" % off)
                rc, got, left, msg = self.z.inflate(out, wbits, offset=31 - off, out_offset=off)
                self.assertEqual((rc, left, got), (Z_STREAM_END, 0, data), "inflate, buffers at +%d/+%d" % (31 - off, off))

    # --- stream inflate -----------------------------------------------------------------------------------------

    def test_inflate_python_streams_in_any_steps(self):
        for kind, gen in sample.KINDS:
            data = gen(150000, seed=31)
            for wbits, init in ((RAW, RAW), (ZLIB, ZLIB), (ZLIB, AUTO), (ZLIB, 0), (GZIP, GZIP), (GZIP, AUTO)):
                c = zlib.compressobj(6, zlib.DEFLATED, wbits)
                stream = c.compress(data) + c.flush()
                for in_step, out_step in ((0, 0), (7, 13), (4096, 65536), (65536, 3)):
                    what = "(%s, stream windowBits %d, InflateInit2_ %d, steps %d/%d)" % (kind, wbits, init, in_step, out_step)
                    rc, got, left, msg = self.z.inflate(stream, init, in_step=in_step, out_step=out_step)
                    self.assertEqual((rc, left), (Z_STREAM_END, 0), "%s %s: %r" % (self.z.name, what, msg))
                    self.assertEqual(got, data, what)
        tiny = sample.text(3000, seed=32)
        stream = zlib.compress(tiny, 9)
        rc, got, left, msg = self.z.inflate(stream, ZLIB, in_step=1, out_step=1)
        self.assertEqual((rc, left, got), (Z_STREAM_END, 0, tiny), "1-byte input and output steps")

    def test_inflate_one_call_finish(self):
        data = sample.mixed(400000, seed=33)
        rc, got, left, msg = self.z.inflate(zlib.compress(data), ZLIB, out_step=len(data), flush=Z_FINISH)
        self.assertEqual((rc, left, got), (Z_STREAM_END, 0, data))

    def test_inflate_stops_at_stream_end(self):
        data = sample.text(50000, seed=34)
        for wbits in (RAW, ZLIB, GZIP):
            c = zlib.compressobj(6, zlib.DEFLATED, wbits)
            stream = c.compress(data) + c.flush()
            for trailer in (b"X", b"TRAILER", zlib.compress(b"second stream")):
                rc, got, left, msg = self.z.inflate(stream + trailer, wbits, in_step=1000)
                self.assertEqual((rc, got, left), (Z_STREAM_END, data, len(trailer)), "windowBits %d, %d-byte trailer" % (wbits, len(trailer)))

    def test_inflate_truncated_never_reports_the_end(self):
        data = sample.mixed(20000, seed=35)
        for wbits in (RAW, ZLIB, GZIP):
            c = zlib.compressobj(9, zlib.DEFLATED, wbits)
            stream = c.compress(data) + c.flush()
            for n in list(range(0, min(len(stream), 400))) + list(range(400, len(stream), 97)):
                rc, got, left, msg = self.z.inflate(stream[:n], wbits, in_step=64)
                self.assertNotEqual(rc, Z_STREAM_END, "windowBits %d, %d of %d bytes" % (wbits, n, len(stream)))
                self.assertIn(rc, (Z_OK, Z_BUF_ERROR, Z_DATA_ERROR), "windowBits %d, %d bytes: %d" % (wbits, n, rc))
                self.assertEqual(got, data[:len(got)], "output is a prefix of the data (windowBits %d, %d bytes)" % (wbits, n))

    def test_inflate_corrupt_bits_are_caught(self):
        """Every single-bit flip of a zlib and a gzip stream: never 'stream end' with wrong data (both framings carry
        a checksum); errors come with a message."""
        data = sample.text(1200, seed=36)
        for wbits in (ZLIB, GZIP):
            c = zlib.compressobj(9, zlib.DEFLATED, wbits)
            stream = bytearray(c.compress(data) + c.flush())
            errors = 0
            for bit in range(len(stream) * 8):
                bad = bytearray(stream)
                bad[bit // 8] ^= 1 << (bit % 8)
                rc, got, left, msg = self.z.inflate(bytes(bad), wbits)
                if rc == Z_STREAM_END:
                    # a gzip header flag bit (e.g. FTEXT) or a header byte zlib ignores (mtime, XFL, OS) doesn't matter
                    self.assertEqual(got, data, "windowBits %d: bit %d flipped, wrong data reported as complete" % (wbits, bit))
                elif rc == Z_DATA_ERROR:
                    errors += 1
                    self.assertTrue(msg, "windowBits %d: bit %d: Z_DATA_ERROR without a message" % (wbits, bit))
                else:
                    self.assertIn(rc, (Z_OK, Z_BUF_ERROR, Z_NEED_DICT), "windowBits %d: bit %d: %d" % (wbits, bit, rc))
            self.assertGreater(errors, len(stream) * 4, "most flips must be detected (windowBits %d)" % wbits)

    def test_inflate_needs_dictionary(self):
        zdict = sample.text(4000, seed=37)
        c = zlib.compressobj(6, zlib.DEFLATED, ZLIB, 8, 0, zdict=zdict)
        stream = c.compress(zdict[:3000]) + c.flush()
        rc, got, left, msg = self.z.inflate(stream, ZLIB)
        self.assertEqual((rc, got), (Z_NEED_DICT, b""), "no SetDictionary export: the stream is reported as needing one")
        rc, got, used = self.z.uncompress(stream, 8000, ZLIB, "3")
        self.assertEqual(rc, Z_DATA_ERROR, "uncompress maps Z_NEED_DICT to Z_DATA_ERROR")

    def test_init_rejects_invalid_parameters(self):
        f, end = self.z.fn("DeflateInit2_"), self.z.fn("DeflateEnd")
        for level, method, wbits, mem, strategy in ((10, 8, 15, 8, 0), (-2, 8, 15, 8, 0), (6, 7, 15, 8, 0),
                                                    (6, 8, 7, 8, 0), (6, 8, 32, 8, 0), (6, 8, -16, 8, 0),
                                                    (6, 8, -7, 8, 0), (6, 8, 15, 0, 0), (6, 8, 15, 10, 0),
                                                    (6, 8, 15, 8, 5), (6, 8, 15, 8, -1)):
            s = self.z.L.struct.PAL_ZStream()
            args = (level, method, wbits, mem, strategy)
            self.assertEqual(f(ctypes.byref(s), *args), Z_STREAM_ERROR, "DeflateInit2_%r" % (args,))
            self.assertTrue(s.internalState, "the PAL keeps its z_stream after a failed init: End must free it")
            end(ctypes.byref(s))
            self.assertFalse(s.internalState, "DeflateEnd frees the PAL's z_stream even after a failed init")
        f, end = self.z.fn("InflateInit2_"), self.z.fn("InflateEnd")
        for wbits in (7, -7, -16, 48, 64):
            s = self.z.L.struct.PAL_ZStream()
            self.assertEqual(f(ctypes.byref(s), wbits), Z_STREAM_ERROR, "InflateInit2_(%d)" % wbits)
            end(ctypes.byref(s))
            self.assertFalse(s.internalState)
        rc, out, n = self.z.compress(b"abc", 10, (15, 8, 0))
        self.assertEqual(rc, Z_STREAM_ERROR, "Compress3 level 10")
        rc, out, n = self.z.compress(b"abc", 6, (15, 0, 0))
        self.assertEqual(rc, Z_STREAM_ERROR, "Compress3 memLevel 0")
        rc, out, used = self.z.uncompress(zlib.compress(b"abc"), 10, 7, "3")
        self.assertEqual(rc, Z_STREAM_ERROR, "Uncompress3 windowBits 7")

    def test_calls_without_a_z_stream_return_an_error(self):
        """When Init's calloc fails, the stream has no z_stream (internalState NULL), and GrindCore.net still calls
        DeflateEnd/InflateEnd on it when its handle is released (memory.md M10). A zeroed stream is that state. Every
        call must return Z_STREAM_ERROR, as zlib's own do for a NULL stream, and leave it as it was. Libraries without
        the fix write through NULL, so the calls run in a child process."""
        rc, out, err = child("""
            s = L.struct.PAL_ZStream()
            calls = (("Deflate", (4,)), ("DeflateEnd", ()), ("Inflate", (0,)), ("InflateEnd", ()))
            print(" ".join(str(getattr(L, "%s" + n)(ctypes.byref(s), *a)) for n, a in calls), 1 if s.internalState else 0)
        """ % self.PREFIX)
        if rc != 0:
            known(self, "zlib-null-z-stream")
        got = [int(x) for x in out.split()]
        self.assertEqual(got, [Z_STREAM_ERROR] * 4 + [0], "%s: Deflate, DeflateEnd, Inflate, InflateEnd on a stream "
                         "without a z_stream, then whether one appeared" % self.z.name)

    def test_init_refuses_a_stream_that_has_its_z_stream(self):
        """DeflateInit2_ and InflateInit2_ refuse a stream that still has its z_stream: Init would replace it, and the
        old one would leak (memory.md M10). End sets it back to NULL, and then the stream can be initialised again. On
        a library without the check the second init replaces the z_stream: the known defect (the first one leaks)."""
        for init, args, end in (("DeflateInit2_", (6, DEFLATED, 15, 8, 0), "DeflateEnd"),
                                ("InflateInit2_", (15,), "InflateEnd")):
            with self.subTest(init=init):
                f, e = self.z.fn(init), self.z.fn(end)
                s = self.z.L.struct.PAL_ZStream()
                self.assertEqual(f(ctypes.byref(s), *args), Z_OK)
                first = s.internalState
                second = f(ctypes.byref(s), *args)
                if second == Z_OK and s.internalState != first:
                    e(ctypes.byref(s))
                    known(self, "pal-create-overwrite")
                self.assertEqual(second, Z_STREAM_ERROR)
                self.assertEqual(s.internalState, first, "a refused init leaves the stream as it was")
                self.assertEqual(e(ctypes.byref(s)), Z_OK)
                self.assertFalse(s.internalState)
                self.assertEqual(f(ctypes.byref(s), *args), Z_OK, "after End the stream can be initialised again")
                e(ctypes.byref(s))

    def test_init_with_a_null_stream_is_an_error(self):
        """DeflateInit2_ and InflateInit2_ return Z_STREAM_ERROR for a NULL stream. Only an assert, compiled out of a
        release build, used to check it, and the init wrote through NULL, so the calls run in a child process."""
        rc, out, err = child("""
            print(L.%sDeflateInit2_(None, 6, 8, 15, 8, 0), L.%sInflateInit2_(None, 15))
        """ % (self.PREFIX, self.PREFIX))
        if rc != 0:
            known(self, "zlib-init-null-stream")
        self.assertEqual([int(x) for x in out.split()], [Z_STREAM_ERROR] * 2)

    # --- one-shot wrappers and Nanook's compress3/uncompress3 -------------------------------------------------------

    def test_one_shot_compress_equals_the_stream(self):
        """Compress = Compress2(-1) = Compress3(-1, 15, 8, default) = the stream with the same parameters, byte for
        byte; Compress3 with other parameters = the stream with those parameters (the hook is compress2 with
        deflateInit2's arguments)."""
        data = sample.mixed(250000, seed=41)
        rc, plain, _ = self.z.compress(data)
        self.assertEqual(rc, Z_OK)
        self.assertEqual(self.z.compress(data, -1)[1], plain, "Compress2(-1)")
        self.assertEqual(self.z.compress(data, -1, (15, 8, 0))[1], plain, "Compress3(-1, 15, 8, 0)")
        self.assertEqual(self.z.deflate(data, level=-1)[0], plain, "stream deflate, same parameters")
        self.assertDecodes(plain, ZLIB, data, "(Compress)")
        for level, wbits, mem, strategy in ((0, 15, 8, 0), (1, 9, 1, 0), (5, RAW, 9, 1), (9, GZIP, 8, 2),
                                            (6, -9, 4, 3), (3, 25, 7, 4), (9, 15, 9, 0)):
            what = "(Compress3 level %d, windowBits %d, memLevel %d, strategy %d)" % (level, wbits, mem, strategy)
            rc, out, n = self.z.compress(data, level, (wbits, mem, strategy))
            self.assertEqual(rc, Z_OK, what)
            self.assertEqual(n, len(out))
            self.assertEqual(out, self.z.deflate(data, level, wbits, mem, strategy)[0], "stream disagrees %s" % what)
            self.assertDecodes(out, wbits, data, what)
            if (wbits, mem, strategy) == (15, 8, 0):
                self.assertEqual(out, self.z.compress(data, level)[1], "Compress2 disagrees %s" % what)

    def test_one_shot_compress_output_space(self):
        for data in (b"", b"a", sample.text(100000, seed=42), sample.noise(100000, seed=43)):
            rc, out, n = self.z.compress(data, 6)
            self.assertEqual(rc, Z_OK)
            self.assertDecodes(out, ZLIB, data, "(%d bytes)" % len(data))
            rc2, out2, n2 = self.z.compress(data, 6, capacity=n)
            self.assertEqual((rc2, out2), (Z_OK, out), "exactly enough room")
            rc3, out3, n3 = self.z.compress(data, 6, capacity=n - 1)
            self.assertEqual(rc3, Z_BUF_ERROR, "one byte short (%d bytes)" % len(data))
            self.assertLessEqual(n3, n - 1, "reported length stays within the buffer")

    def test_one_shot_uncompress(self):
        data = sample.mixed(200000, seed=44)
        stream = zlib.compress(data, 7)
        self.assertEqual(self.z.uncompress(stream, len(data))[:2], (Z_OK, data), "exact room")
        self.assertEqual(self.z.uncompress(stream, len(data) + 999)[:2], (Z_OK, data), "more room")
        rc, got, _ = self.z.uncompress(stream, len(data) - 1)
        self.assertEqual(rc, Z_BUF_ERROR, "one byte short")
        self.assertEqual(got, data[:len(got)])
        # truncated: with spare room it's an incomplete stream; with the output exactly full zlib can't tell that
        # from "out of room" and says Z_BUF_ERROR (uncompr.c's documented rule)
        self.assertEqual(self.z.uncompress(stream[:-1], len(data) + 1)[0], Z_DATA_ERROR, "truncated")
        self.assertEqual(self.z.uncompress(stream[:-1], len(data))[0], Z_BUF_ERROR, "truncated, output exactly full")
        self.assertEqual(self.z.uncompress(stream[:len(stream) // 2], len(data))[0], Z_DATA_ERROR, "cut in half")
        self.assertEqual(self.z.uncompress(b"\x78\x9c\x03\x00\x00\x00\x00\x01", 0)[0], Z_OK, "empty stream into 0 bytes")
        rc, got, used = self.z.uncompress(stream + b"TRAILER", len(data), variant="2")
        self.assertEqual((rc, got, used), (Z_OK, data, len(stream)), "Uncompress2 reports the input consumed")
        for wbits in (RAW, GZIP):
            c = zlib.compressobj(6, zlib.DEFLATED, wbits)
            s2 = c.compress(data) + c.flush()
            self.assertEqual(self.z.uncompress(s2 + b"!", len(data), wbits, "3"), (Z_OK, data, len(s2)), "Uncompress3(%d)" % wbits)
            self.assertEqual(self.z.uncompress(s2, len(data), AUTO if wbits == GZIP else wbits, "3")[:2], (Z_OK, data))
            self.assertEqual(self.z.uncompress(s2, len(data), ZLIB, "3")[0], Z_DATA_ERROR, "wrong framing (%d)" % wbits)
        self.assertEqual(self.z.uncompress(stream, len(data), ZLIB, "3"), self.z.uncompress(stream, len(data), variant="2"),
                         "Uncompress3(15) = Uncompress2")

    def test_output_matches_official(self):
        """Byte-identical to the official release built by ref/build_zlib_ref.py: Compress2 at every level, and the
        stream deflate for each framing and window, memory level and strategy.
        Where this Python's zlib is the same algorithm, it's checked too, as a build of its own: zlib 1.3.1 (e.g. Debian
        13), or zlib-ng 2.2.1, or 2.2.4 (CPython 3.14's Windows builds) on 64-bit only. On 32-bit, 2.2.1 and 2.2.4 take
        different match-finder paths (2.2.1's 4-byte UNALIGNED_OK compare, 2.2.4's OPTIMAL_CMP rewrite): measured on
        win-x86, levels 3-4 differ by a few bytes, both valid."""
        if not self.ref:
            gctest.need(self, "zlib", "no %s" % self.z.name)
        version, ng = zlib.ZLIB_RUNTIME_VERSION, getattr(zlib, "ZLIBNG_VERSION", None)
        wide = sys.maxsize > 2 ** 32
        same_python = version == "1.3.1" if self.z.name == "zlib 1.3.1" else (ng == "2.2.1" or (ng == "2.2.4" and wide))
        for kind, gen in sample.KINDS:
            data = gen(300000, seed=45)
            for level in [-1] + list(range(10)):
                with self.subTest(kind=kind, level=level):
                    rc, out, _ = self.z.compress(data, level)
                    self.assertEqual((rc, out), self.ref.compress(data, level), "Compress2 differs from official")
                    if same_python and level != 0:   # level 0's stored blocks follow Python's output buffer size
                        self.assertEqual(out, zlib.compress(data, level), "differs from Python's zlib")
        data = sample.mixed(120000, seed=46)
        for wbits in (RAW, -9, ZLIB, 10, GZIP, 25):
            for mem in (1, 8, 9):
                for strategy in range(5):   # default, filtered, Huffman only, RLE, fixed
                    for level in (1, 4, 6, 9):
                        with self.subTest(wbits=wbits, mem=mem, strategy=strategy, level=level):
                            ref_rc, ref_out = self.ref.deflate(data, level, wbits, mem, strategy)
                            self.assertEqual(ref_rc, Z_STREAM_END)
                            self.assertEqual(self.z.deflate(data, level, wbits, mem, strategy)[0], ref_out,
                                             "deflate differs from official")


class ZLib131(_ZlibTests, unittest.TestCase):
    NAME, PREFIX = LIBS[0]


_NG_STATE = []


def zlibng_state():
    """Whether zlib-ng can run in this process: ("ok" | "known" | "broken", detail). The shipped linux-arm build dies
    (SIGBUS) in zlib-ng's NEON Adler-32 on any zlib-framed stream (platforms.md 10.7), so this is found out once, in
    a child process, before anything in this process calls it."""
    if not _NG_STATE:
        rc, out, err = child("""
            import zlib
            data = bytes(range(256)) * 512
            dst = ctypes.create_string_buffer(len(data) + 1024)
            n = ctypes.c_uint32(len(dst))
            assert L.DN9_ZLibNg_v2_2_1_Compress2(dst, ctypes.byref(n), data, len(data), 1) == 0
            assert zlib.decompress(dst.raw[:n.value]) == data
            print("ok")
        """)
        if rc == 0 and "ok" in out:
            _NG_STATE.append(("ok", ""))
        elif gcnative.target() == "linux-arm" and rc in (-7, 135):
            _NG_STATE.append(("known", "rc %r" % rc))
        else:
            _NG_STATE.append(("broken", "rc %r\n%s%s" % (rc, out, err[-2000:])))
    return _NG_STATE[0]


def _arm32_probe(test):
    state, detail = zlibng_state()
    if state == "known":
        known(test, "zlibng-arm32-neon-align")
    elif state == "broken":
        test.fail("zlib-ng failed in a child process: " + detail)


class ZLibNg221(_ZlibTests, unittest.TestCase):
    NAME, PREFIX = LIBS[1]
    probe = staticmethod(_arm32_probe)
