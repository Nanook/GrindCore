"""zstd 1.5.2 and 1.5.7 (SZ_ZStd_v1_5_2_* / SZ_ZStd_v1_5_7_*), plain frames and the seekable format.

GrindCore vendors both official releases (lib/ identical after its renames) and each release's
contrib/seekable_format (identical apart from two build-only edits; other-codecs.md 4.8.1). The references are:
  - official zstd plus its seekable contrib, built unmodified from the release tarballs by ref/build_zstd_ref.py:
    set GC_REF_ZSTD to that folder. GrindCore's output must match it byte for byte for the same call sequence,
    since zstd's output is fully determined by the version, the parameters and the calls;
  - Python's compression.zstd (3.14+) where present: an independent decoder, and a byte-for-byte reference when it
    was built with the same zstd version.
Without either, the tests still check round trips, the seekable layouts each release defines, and the PAL's argument
and error handling. Corrupt-input runs happen in a child process, so a crash there is a finding, not a dead run."""
import ctypes
import json
import random
import struct
import sys
import unittest

import gctest
from gctest import sample
from gctest import zstd_util as Z
from gctest.test_hashes import child, known

try:
    from compression import zstd as pyzstd      # Python 3.14+
except ImportError:
    pyzstd = None


def py_version():
    return ".".join(map(str, pyzstd.zstd_version_info[:3])) if pyzstd else None


_STDCALL = {}


def callback_convention():
    """True when the library's seekable read/seek callbacks are __stdcall: GrindCore's win-x86 build compiles C with
    /Gz, so the vendored callback typedefs take that default there (hub row 17). Only 32-bit Windows has two
    conventions. Probed in child processes, since the wrong one corrupts the stack; cached per library."""
    if sys.platform != "win32" or ctypes.sizeof(ctypes.c_void_p) != 4:
        return False
    if gctest.LIB.path not in _STDCALL:
        code = """
            from gctest import sample, zstd_util as Z
            Z.CALLBACK_STDCALL = %r
            z = Z.ZStd(L, "1.5.7")
            data = sample.text(9000)
            s = z.seekable(Z.encode(z.seekable_encoder(3, 1, 4000), data, 5000), callbacks=True)
            print("ok" if not z.is_error(s.init) and s.read(0, 9000)[1] == data else "wrong")
        """
        works = [conv for conv in (True, False) if child(code % conv, timeout=60)[1].strip().endswith("ok")]
        _STDCALL[gctest.LIB.path] = works == [True]
    return _STDCALL[gctest.LIB.path]


def corpus():
    return [("empty", b""), ("1 byte", b"x")] + [("%s %d" % (k, n), f(n, seed=n)) for k, f in sample.KINDS
                                                 for n in (1000, 70000)] + [("mixed 300000", sample.mixed(300000))]


class _ZStdTests(object):
    VERSION = None

    @classmethod
    def setUpClass(cls):
        cls.L = gctest.LIB
        cls.z = Z.ZStd(cls.L, cls.VERSION)
        cls.ref = Z.reference(cls.VERSION)
        Z.CALLBACK_STDCALL = callback_convention()

    def need_ref(self):
        if not self.ref:
            gctest.need(self, "zstd", "no %s" % self.VERSION)

    def py_decode(self, stream):
        return pyzstd.decompress(stream) if pyzstd else None

    # ------------------------------------------------------------------ one-shot

    def test_block_round_trip_every_kind_and_level(self):
        for name, data in corpus():
            for level in (1, 3, 9, 19) if len(data) <= 70000 else (1, 3):
                with self.subTest(data=name, level=level):
                    r, stream = self.z.compress_block(data, level)
                    self.assertFalse(self.z.is_error(r), self.z.error_name(r))
                    self.assertEqual(stream[:4], Z.MAGIC)
                    self.assertEqual(self.z.decompress_block(stream, len(data))[1], data)
                    if pyzstd:
                        self.assertEqual(self.py_decode(stream), data)

    def test_block_output_matches_official(self):
        """Byte-identical to official zstd built from the release tarball, at every level; and where this Python's
        compression.zstd is the same zstd version (3.14+ bundles 1.5.7), to that independent build too."""
        self.need_ref()
        same_python = pyzstd is not None and py_version() == self.VERSION
        for name, data in corpus():
            for level in (1, 2, 3, 5, 9, 15, 19, 22) if len(data) <= 70000 else (1, 3, 9):
                with self.subTest(data=name, level=level):
                    out = self.z.compress_block(data, level)[1]
                    self.assertEqual(out, self.ref.compress_block(data, level)[1])
                    if same_python:
                        self.assertEqual(out, pyzstd.compress(data, level), "differs from Python's compression.zstd")

    def test_block_capacities(self):
        """zstd compresses into the caller's buffer and needs some headroom, so a buffer exactly the size of the
        result can fail (dstSize_tooSmall); ZSTD_compressBound always suffices. Whatever happens, it's an error or
        the full frame, and it's what official zstd does with the same capacity."""
        data = sample.text(20000)
        r, stream = self.z.compress_block(data, 3)
        for cap in (0, 1, len(stream) // 2, len(stream) - 1, len(stream), len(stream) + 8, len(stream) + 64,
                    Z.bound(len(data))):
            with self.subTest(capacity=cap):
                r, out = self.z.compress_block(data, 3, capacity=cap)
                self.assertTrue(self.z.is_error(r) or out == stream, "capacity %d: %d" % (cap, r))
                if cap < len(stream):
                    self.assertTrue(self.z.is_error(r))
                if self.ref:
                    rr, rout = self.ref.compress_block(data, 3, capacity=cap)
                    self.assertEqual((self.z.is_error(r), out), (self.ref.is_error(rr), rout))
        self.assertEqual(self.z.decompress_block(stream, len(data))[1], data)
        r = self.z.decompress_block(stream, len(data) - 1)[0]
        self.assertTrue(self.z.is_error(r), "decoding into one byte too few must be an error, got %d" % r)

    def test_error_codes(self):
        """GetErrorCode, exported on audit/native-fixes (absent from the shipped binaries, where this skips): zstd's
        ZSTD_ErrorCode for a result, so a caller can tell a too-small buffer from corrupt data (zstd.md, ZStdBlock's
        mapResult). 0 for anything that isn't an error and n for (size_t)-n, on both sides of IsError's threshold for
        this RID's size_t (maxCode 120 in both versions); then real failures, with official zstd's names for them."""
        if not self.L.has(self.z.P + "GetErrorCode"):
            self.skipTest("%sGetErrorCode not exported by this library (exported on audit/native-fixes)" % self.z.P)
        error_code = self.z.fn("GetErrorCode")
        top = 1 << (8 * ctypes.sizeof(ctypes.c_size_t))
        for v in (0, 1, 70, top // 2, top - 121, top - 120, top - 119, top - 70, top - 1):
            with self.subTest(value=hex(v)):
                self.assertEqual(self.z.is_error(v), v > top - 120)
                self.assertEqual(error_code(v), top - v if self.z.is_error(v) else 0)
        stream = self.z.compress_block(sample.text(20000), 3)[1]
        real = [("one byte too few", self.z.decompress_block(stream, 19999)[0], 70),           # dstSize_tooSmall
                ("no frame magic", self.z.decompress_block(b"not a zstd frame", 100)[0], 10),  # prefix_unknown
                ("the PAL's argument error", self.z.fn("CompressBlock")(None, None, 0, None, 0, 3), 1)]  # GENERIC
        for what, r, code in real:
            with self.subTest(what):
                self.assertTrue(self.z.is_error(r))
                self.assertEqual(error_code(r), code)
                if self.ref:
                    self.assertEqual(self.z.error_name(r), self.ref.error_name(r))

    def _contexts(self):
        """A compression and a decompression context, and a raw-content dictionary for each, freed by cleanups."""
        c, d = self.z.cctx(), self.z.dctx()
        self.addCleanup(self.z.free_cctx, c)
        self.addCleanup(self.z.free_dctx, d)
        content = sample.text(4000, seed=5)
        (rc1, cd), (rc2, dd) = self.z.cdict(content, 3), self.z.ddict(content)
        self.assertEqual((rc1, rc2), (0, 0))
        self.addCleanup(self.z.fn("FreeCompressionDict"), ctypes.byref(cd))
        self.addCleanup(self.z.fn("FreeDecompressionDict"), ctypes.byref(dd))
        return c, d, cd, dd

    def test_null_arguments(self):
        """The PAL's own checks (zstd.md 2.2): a NULL context or dictionary, or a NULL buffer with a nonzero size, must
        be an error IsError recognises. The shipped PAL returned 0 from the block functions and DecompressStream: a
        valid decoded size (and "frame complete" from DecompressStream), so bad arguments looked like success with
        empty output. Every call here returns before the PAL touches anything, on any build."""
        c, d, cd, dd = self._contexts()
        n, buf = None, ctypes.create_string_buffer(64)
        i, o = ctypes.c_int64(), ctypes.c_int64()
        C, D, CD, DD, I, O = (ctypes.byref(x) for x in (c, d, cd, dd, i, o))
        calls = [("CompressBlock", (n, buf, 64, buf, 10, 3)), ("CompressBlock", (C, n, 64, buf, 10, 3)),
                 ("CompressBlock", (C, buf, 64, n, 10, 3)),
                 ("DecompressBlock", (n, buf, 64, buf, 10)), ("DecompressBlock", (D, n, 64, buf, 10)),
                 ("DecompressBlock", (D, buf, 64, n, 10)),
                 ("CompressBlockWithDict", (n, CD, buf, 64, buf, 10)), ("CompressBlockWithDict", (C, n, buf, 64, buf, 10)),
                 ("CompressBlockWithDict", (C, CD, n, 64, buf, 10)), ("CompressBlockWithDict", (C, CD, buf, 64, n, 10)),
                 ("DecompressBlockWithDict", (n, DD, buf, 64, buf, 10)),
                 ("DecompressBlockWithDict", (D, n, buf, 64, buf, 10)),
                 ("DecompressBlockWithDict", (D, DD, n, 64, buf, 10)),
                 ("DecompressBlockWithDict", (D, DD, buf, 64, n, 10)),
                 ("DecompressStream", (n, buf, 64, buf, 10, I, O)), ("DecompressStream", (D, n, 64, buf, 10, I, O)),
                 ("DecompressStream", (D, buf, 64, n, 10, I, O))]
        calls += [(name, args) for name in ("CompressStream", "FlushStream", "EndStream")
                  for args in ((n, buf, 64, buf, 10, I, O), (C, n, 64, buf, 10, I, O), (C, buf, 64, n, 10, I, O))]
        got = [(k, name, self.z.fn(name)(*args)) for k, (name, args) in enumerate(calls)]
        if any(v == 0 for _, _, v in got):
            known(self, "zstd-null-args-zero")
        for k, name, v in got:
            self.assertTrue(self.z.is_error(v), "case %d, %s: returned %d" % (k, name, v))

    def test_null_buffer_with_size_zero_is_an_empty_buffer(self):
        """.NET pins an empty array as NULL. With size 0 that's a valid empty buffer, and zstd's API allows NULL there,
        so the PAL must pass it through: each call is made on fresh contexts twice, with NULL and with a real
        zero-length buffer, and must give the same results and output. The shipped PAL rejected NULL, answering 0
        (success, nothing written; "frame complete" from DecompressStream) or an error, where zstd would compress an
        empty frame, report the destination too small, or keep draining a frame's buffered output."""
        text = sample.text(3000, seed=9)
        c, d, cd, dd = self._contexts()
        frame, empty = self.z.compress_block(text, 3)[1], self.z.compress_block(b"", 3)[1]
        dict_frame = self.z.compress_block(text, 3, cdict=cd)[1]
        keep = []

        def buf(data):
            keep.append(ctypes.create_string_buffer(data, max(len(data), 1)))
            return keep[-1]

        def run(zero):
            """(case, result, output) for each call, with `zero` as the pointer of every zero-length buffer."""
            c, d, cd, dd = self._contexts()
            C, D, CD, DD = (ctypes.byref(x) for x in (c, d, cd, dd))
            i, o = ctypes.c_int64(), ctypes.c_int64()
            I, O = ctypes.byref(i), ctypes.byref(o)
            dst, res = ctypes.create_string_buffer(8192), []

            def block(case, name, *args):
                r = self.z.fn(name)(*args)
                res.append((case, r, None if self.z.is_error(r) else ctypes.string_at(dst, r)))

            block("compress an empty source", "CompressBlock", C, dst, 8192, zero, 0, 3)
            block("compress into no room", "CompressBlock", C, zero, 0, buf(text), len(text), 3)
            block("decompress an empty frame into no room", "DecompressBlock", D, zero, 0, buf(empty), len(empty))
            block("decompress a frame into no room", "DecompressBlock", D, zero, 0, buf(frame), len(frame))
            block("decompress an empty source", "DecompressBlock", D, dst, 8192, zero, 0)
            block("compress an empty source, dictionary", "CompressBlockWithDict", C, CD, dst, 8192, zero, 0)
            block("decompress into no room, dictionary", "DecompressBlockWithDict", D, DD, zero, 0, buf(dict_frame),
                  len(dict_frame))
            # a whole frame into 64 bytes of room, then zero-length input until it's drained: the rest of the frame
            # comes out of zstd's own buffer. zstd holds back the frame's last input byte (its "hostage byte") until
            # the output is flushed, then returns 1 for it, so the unconsumed tail goes in last.
            f, d2 = self.z.fn("DecompressStream"), self.z.dctx()
            self.addCleanup(self.z.free_dctx, d2)
            out, idle = b"", 0
            r = f(ctypes.byref(d2), dst, 64, buf(frame), len(frame), I, O)
            used = i.value
            for _ in range(len(text)):
                out += ctypes.string_at(dst, o.value)
                idle = idle + 1 if o.value == 0 else 0
                if r == 0 or self.z.is_error(r) or idle > 3:
                    break
                r = f(ctypes.byref(d2), dst, 64, zero, 0, I, O)
            drained = r
            if r != 0 and not self.z.is_error(r) and used < len(frame):
                r = f(ctypes.byref(d2), dst, 64, buf(frame[used:]), len(frame) - used, I, O)
                out += ctypes.string_at(dst, o.value)
            res.append(("drain with empty input", (used, drained, r), out))
            # finish a stream with no new input
            c2 = self.z.cctx(level=3)
            self.addCleanup(self.z.free_cctx, c2)
            r = self.z.fn("EndStream")(ctypes.byref(c2), dst, 8192, zero, 0, I, O)
            res.append(("end a stream with empty input", r, ctypes.string_at(dst, o.value)))
            return res

        with_null, with_buffer = run(None), run(buf(b""))
        self.assertEqual((with_buffer[-2][1][2], with_buffer[-2][2]), (0, text), "reference run: draining the frame")
        if with_null != with_buffer:
            known(self, "zstd-null-args-zero")
        for a, b in zip(with_null, with_buffer):
            self.assertEqual(a, b, a[0])

    def test_null_size_pointers(self):
        """DecompressStream needs both size pointers and the compressors need outSize; CompressStream needs inSize, but
        FlushStream and EndStream accept a NULL one (their input is usually empty). The shipped PAL wrote through a
        NULL inSize or outSize in DecompressStream, and a NULL inSize in FlushStream/EndStream: a crash, so this runs
        in a child process."""
        code = """
            import ctypes, json
            from gctest import zstd_util as Z
            z = Z.ZStd(L, %r)
            data = b"size pointers " * 400
            frame = z.compress_block(data, 3)[1]
            dst, src, empty = (ctypes.create_string_buffer(x) for x in (1 << 16, frame, 1))
            d, c, i, o = z.dctx(), z.cctx(level=3), ctypes.c_int64(), ctypes.c_int64()
            D, C, I, O, got = ctypes.byref(d), ctypes.byref(c), ctypes.byref(i), ctypes.byref(o), {}
            err = lambda name, *a: z.is_error(z.fn(name)(*a))
            got["decompress, NULL inSize"] = err("DecompressStream", D, dst, 1 << 16, src, len(frame), None, O)
            got["decompress, NULL outSize"] = err("DecompressStream", D, dst, 1 << 16, src, len(frame), I, None)
            got["compress, NULL inSize"] = err("CompressStream", C, dst, 1 << 16, src, 10, None, O)
            got["compress, NULL outSize"] = err("CompressStream", C, dst, 1 << 16, src, 10, I, None)
            z.fn("CompressStream")(C, dst, 1 << 16, ctypes.create_string_buffer(data), len(data), I, O)
            out, pos = ctypes.string_at(dst, o.value), i.value
            got["flush, NULL inSize"] = err("FlushStream", C, dst, 1 << 16, empty, 0, None, O)
            out += ctypes.string_at(dst, o.value)
            r = z.fn("EndStream")(C, dst, 1 << 16, empty, 0, None, O)
            got["end, NULL inSize"] = (z.is_error(r), r)
            out += ctypes.string_at(dst, o.value)
            got["round trip"] = pos == len(data) and z.decompress_block(out, len(data))[1] == data
            z.free_dctx(d)
            z.free_cctx(c)
            print(json.dumps(got))
        """ % self.VERSION
        rc, out, err = child(code, timeout=60)
        if rc != 0:
            known(self, "zstd-null-size-pointers")
        self.assertEqual(json.loads(out.strip().splitlines()[-1]),
                         {"decompress, NULL inSize": True, "decompress, NULL outSize": True,
                          "compress, NULL inSize": True, "compress, NULL outSize": True,
                          "flush, NULL inSize": False, "end, NULL inSize": [False, 0], "round trip": True})

    # ------------------------------------------------------------------ streaming

    def test_stream_round_trip_steps_and_buffers(self):
        data = sample.mixed(150000)
        for step, out_cap in ((1, 1 << 17), (13, 7), (4096, 1), (1 << 17, 1 << 17), (len(data), 64)):
            d = data if step > 1 else data[:6000]
            with self.subTest(step=step, out=out_cap):
                stream = Z.encode(self.z.stream_encoder(level=3), d, step, out_cap=out_cap)
                for in_step, dcap in ((1, 1 << 17), (7, 1), (len(stream) or 1, 1 << 17)):
                    out, last = Z.drain(self.z.stream_decoder(), stream, in_step, out_cap=dcap,
                                        max_idle=4 * len(stream) + 64)
                    self.assertEqual((out, last), (d, 0))

    def test_stream_output_matches_official(self):
        self.need_ref()
        data = sample.mixed(400000)
        plans = [dict(step=len(data)), dict(step=7000, flush_at={70000, 140000, 400000}), dict(step=1 << 16, out_cap=33),
                 dict(step=4096, workers=2), dict(step=50000, workers=3, job=512 << 10)]
        for plan in plans:
            for level in (1, 3, 12):
                p = dict(plan)
                kw = {k: p.pop(k) for k in ("workers", "job") if k in p}
                with self.subTest(plan=plan, level=level):
                    a = Z.encode(self.z.stream_encoder(level=level, **kw), data, **p)
                    b = Z.encode(self.ref.stream_encoder(level=level, **kw), data, **p)
                    self.assertEqual(a, b)

    def test_stream_output_is_the_same_for_any_worker_count(self):
        """zstd's multithreaded output depends on the job size, never on how many workers run the jobs."""
        data = sample.mixed(3 << 20)
        outs = {w: Z.encode(self.z.stream_encoder(level=3, workers=w, job=512 << 10), data, 1 << 20)
                for w in (1, 2, 4)}
        self.assertEqual(outs[1], outs[2])
        self.assertEqual(outs[1], outs[4])
        self.assertEqual(Z.drain(self.z.stream_decoder(), outs[1], 1 << 20)[0], data)

    def test_flush_makes_everything_so_far_decodable(self):
        data = sample.text(200000)
        cuts = (1, 5000, 65536, 131072, 199999)
        for cut in cuts:
            with self.subTest(flush_at=cut):
                enc = self.z.stream_encoder(level=5)
                try:
                    part = Z.drive(enc, data[:cut], 3000, flush_at={cut}, finish=False)
                finally:
                    enc.close()
                out, last = Z.drain(self.z.stream_decoder(), part, 1 << 20)
                self.assertEqual(out, data[:cut])
                self.assertNotEqual(last, 0, "the frame isn't finished, so the decoder must still want input")

    def test_set_block_size_is_target_c_block_size(self):
        """SetBlockSize sets ZSTD_c_targetCBlockSize (zstd.md 2.2), whose id differs between the two releases."""
        data = sample.mixed(300000)
        plain = Z.encode(self.z.stream_encoder(level=3), data, 1 << 20)
        small = Z.encode(self.z.stream_encoder(level=3, block=1340), data, 1 << 20)
        self.assertNotEqual(plain, small, "SetBlockSize changed nothing")
        self.assertEqual(Z.drain(self.z.stream_decoder(), small, 1 << 20)[0], data)
        if self.ref:
            self.assertEqual(small, Z.encode(self.ref.stream_encoder(level=3, block=1340), data, 1 << 20))

    def test_setters_report_the_value_set(self):
        """The setters return ZSTD_CCtx_setParameter's result as int32: the value set, or -(error code); the PAL's
        own range checks return -1. GrindCore.net ignores all four results (ZStdEncoder.cs:50-55)."""
        c = self.z.cctx()
        try:
            f = lambda name, v: self.z.fn(name)(ctypes.byref(c), v)
            self.assertEqual(f("SetCompressionLevel", 7), 7)
            self.assertEqual(f("SetCompressionLevel", 0), 3, "0 selects ZSTD_CLEVEL_DEFAULT, and that's reported")
            self.assertEqual(f("SetCompressionLevel", -1), -1, "negative levels are refused by the PAL (a gap)")
            self.assertEqual(f("SetCompressionLevel", 23), -1)
            self.assertEqual(f("SetNbWorkers", 2), 2)
            self.assertEqual(f("SetNbWorkers", -1), -1)
            clamped = f("SetNbWorkers", 100000)
            self.assertTrue(0 < clamped < 100000, "zstd clamps the worker count to its maximum, got %d" % clamped)
        finally:
            self.z.free_cctx(c)

    def test_level_zero_means_the_default(self):
        data = sample.text(50000)
        self.assertEqual(Z.encode(self.z.stream_encoder(level=0), data, 1 << 20),
                         Z.encode(self.z.stream_encoder(level=3), data, 1 << 20))

    def test_decoder_reads_concatenated_and_skippable_frames(self):
        a, b = sample.text(30000), sample.noise(3000)
        fa, fb = self.z.compress_block(a, 3)[1], self.z.compress_block(b, 3)[1]
        skip = ctypes.create_string_buffer(64)
        n = self.z.fn("WriteSkippableFrame")(skip, 64, b"metadata", 8, 5)
        self.assertEqual(n, 16)
        stream = fa + skip.raw[:n] + fb
        for step in (1, 4096, len(stream)):
            self.assertEqual(Z.drain(self.z.stream_decoder(), stream, step, max_idle=4 * len(stream))[0], a + b)

    def test_corrupt_stream_is_reported_as_an_error(self):
        """The native side reports corruption with an error code; R04's hang is GrindCore.net discarding it."""
        good = self.z.compress_block(sample.text(50000), 3)[1]
        reserved = bytearray(good)
        reserved[4] |= 0x08                     # Frame_Header_Descriptor's reserved bit must be 0
        for bad in (bytes(reserved), b"\x28\xb5\x2f\xfe" + good[4:]):
            with self.assertRaises(Z.ZErr):
                Z.drain(self.z.stream_decoder(), bad, 1 << 20)
        with self.assertRaises(Z.ZErr):
            Z.drain(self.z.stream_decoder(), b"not a zstd frame at all", 1 << 20)

    def test_corrupt_and_truncated_input_never_crashes(self):
        """Every truncation and a bit flip in every byte of two frames, through the block and stream decoders, in a
        child process. Frames without a checksum can decode to wrong data after a flip; only crashes, hangs and
        overlong output count."""
        code = """
            import json
            from gctest import sample, zstd_util as Z
            z = Z.ZStd(L, %r)
            counts = {"frames": 0, "errors": 0, "decoded": 0}
            for data in (sample.text(3000), sample.mixed(20000)):
                stream = z.compress_block(data, 3)[1]
                cases = [stream[:i] for i in range(len(stream))]
                for i in range(len(stream)):
                    b = bytearray(stream); b[i] ^= 1 << (i %% 8); cases.append(bytes(b))
                for c in cases:
                    counts["frames"] += 1
                    r, out = z.decompress_block(c, len(data))
                    counts["errors" if z.is_error(r) else "decoded"] += 1
                    assert out is None or len(out) <= len(data)
                    try:
                        out, last = Z.drain(z.stream_decoder(), c, 1 << 20, max_idle=16)
                        assert len(out) <= len(data)
                    except Z.ZErr:
                        pass
            print(json.dumps(counts))
        """ % self.VERSION
        rc, out, err = child(code, timeout=600)
        self.assertEqual(rc, 0, "child: %r %s" % (rc, err[-2000:]))
        self.assertGreater(json.loads(out.strip().splitlines()[-1])["errors"], 0)

    # ------------------------------------------------------------------ dictionaries

    def dictionary(self):
        return sample.text(32000, seed=99)

    def test_dictionary_round_trip_and_official_output(self):
        content, data = self.dictionary(), sample.text(20000, seed=100)
        for window_log in (0, 20):
            with self.subTest(window_log=window_log):
                rc, cd = self.z.cdict(content, 5, window_log)
                self.assertEqual(rc, 0)
                rc, dd = self.z.ddict(content)
                self.assertEqual(rc, 0)
                try:
                    stream = self.z.compress_block(data, 0, cdict=cd)[1]
                    self.assertLess(len(stream), len(self.z.compress_block(data, 5)[1]), "the dictionary didn't help")
                    self.assertEqual(self.z.decompress_block(stream, len(data), ddict=dd)[1], data)
                    if self.ref:
                        rd = self.ref.make_cdict(content, 5, window_log)
                        self.assertEqual(stream, self.ref.compress_block(data, 0, cdict=rd)[1])
                        self.ref.freeCDict(rd)
                    streamed = Z.encode(self.z.stream_encoder(cdict=cd), data, 5000)
                    self.assertEqual(Z.drain(self.z.stream_decoder(ddict=dd), streamed, 5000)[0], data)
                    if self.ref:
                        rd = self.ref.make_cdict(content, 5, window_log)
                        self.assertEqual(streamed, Z.encode(self.ref.stream_encoder(cdict=rd), data, 5000))
                        self.ref.freeCDict(rd)
                finally:
                    self.z.fn("FreeCompressionDict")(ctypes.byref(cd))
                    self.z.fn("FreeDecompressionDict")(ctypes.byref(dd))

    def test_dictionary_arguments(self):
        self.assertEqual(self.z.cdict(b"", 3)[0], -1)
        self.assertEqual(self.z.ddict(b"")[0], -1)
        d = self.z.struct("CompressionDict")
        self.assertEqual(self.z.fn("CreateCompressionDict")(ctypes.byref(d), None, 10, 3, 0), -1)

    def test_create_refuses_a_struct_in_use(self):
        """Every create refuses a struct that still holds its object: it would replace it, and the old one would leak
        (memory.md M10). The free sets the field back to NULL, and then the struct can be created into again. On a
        library without the check the second create replaces the object: the known defect (the first one leaks)."""
        content = self.dictionary()
        for struct_name, create, args, free in (
                ("CompressionContext", "CreateCompressionContext", (), "FreeCompressionContext"),
                ("DecompressionContext", "CreateDecompressionContext", (), "FreeDecompressionContext"),
                ("CompressionDict", "CreateCompressionDict", (content, len(content), 3, 0), "FreeCompressionDict"),
                ("CompressionDict", "CreateCompressionDict", (content, len(content), 3, 20), "FreeCompressionDict"),
                ("DecompressionDict", "CreateDecompressionDict", (content, len(content)), "FreeDecompressionDict"),
                ("SeekableCStream", "Seekable_CreateCStream", (), "Seekable_FreeCStream"),
                ("Seekable", "Seekable_Create", (), "Seekable_Free")):
            with self.subTest(create=create, args=args[2:]):
                s, f, release = self.z.struct(struct_name), self.z.fn(create), self.z.fn(free)
                self.assertEqual(f(ctypes.byref(s), *args), 0)
                first = bytes(s)
                second = f(ctypes.byref(s), *args)
                if second == 0 and bytes(s) != first:
                    release(ctypes.byref(s))
                    known(self, "pal-create-overwrite")
                self.assertEqual(second, -1)
                self.assertEqual(bytes(s), first, "a refused create leaves the struct as it was")
                release(ctypes.byref(s))
                self.assertFalse(any(bytes(s)), "the free sets the struct back to NULL")
                self.assertEqual(f(ctypes.byref(s), *args), 0, "after the free the struct can be created into again")
                release(ctypes.byref(s))

    # ------------------------------------------------------------------ skippable frames, sizes

    def test_skippable_frames(self):
        for variant in range(16):
            buf = ctypes.create_string_buffer(64)
            n = self.z.fn("WriteSkippableFrame")(buf, 64, b"hello", 5, variant)
            self.assertEqual(n, 13)
            frame = buf.raw[:n]
            self.assertEqual(struct.unpack("<I", frame[:4])[0], 0x184D2A50 + variant)
            self.assertEqual(self.z.fn("IsSkippableFrame")(frame, n), 1)
            out, var = ctypes.create_string_buffer(16), ctypes.c_uint(99)
            self.assertEqual(self.z.fn("ReadSkippableFrame")(out, 16, ctypes.byref(var), frame, n), 5)
            self.assertEqual((out.raw[:5], var.value), (b"hello", variant))
            self.assertTrue(self.z.is_error(self.z.fn("ReadSkippableFrame")(out, 4, None, frame, n)), "dst too small")
        buf = ctypes.create_string_buffer(64)
        self.assertTrue(self.z.is_error(self.z.fn("WriteSkippableFrame")(buf, 64, b"x", 1, 16)), "variant 16")
        frame = self.z.compress_block(b"abc", 1)[1]
        self.assertEqual(self.z.fn("IsSkippableFrame")(frame, len(frame)), 0)

    def test_stream_buffer_sizes(self):
        i, o = self.z.fn("CStreamInSize")(), self.z.fn("CStreamOutSize")()
        self.assertEqual(i, 128 << 10)
        if self.ref:
            self.assertEqual((i, o), (self.ref.CStreamInSize(), self.ref.CStreamOutSize()))
        self.assertGreaterEqual(o, Z.bound(128 << 10))

    # ------------------------------------------------------------------ seekable

    def seek_encode(self, data, level=3, checksum=1, frame=4096, step=5000, **kw):
        return Z.encode(self.z.seekable_encoder(level, checksum, frame), data, step, **kw)

    def expected_frames(self, n, frame):
        """GrindCore's seekable encoders, 1.5.7 included, end the stream without an empty last frame: empty input
        gives 0 frames and an exact multiple of the frame size no trailing frame. Official 1.5.7 (1.5.4+, PR #3346)
        writes one there; GrindCore's 1.5.7 keeps 1.5.2's endStream check on purpose, since GrindCore.net's own
        tests expect no empty frames (other-codecs.md 4.8.1)."""
        return n // frame + (1 if n % frame else 0)

    def test_seekable_round_trip_and_layout(self):
        for n in (0, 1, 4095, 4096, 4097, 12288, 12289, 100000):
            for checksum in (0, 1):
                data = sample.mixed(n, seed=n)
                with self.subTest(n=n, checksum=checksum):
                    stream = self.seek_encode(data, checksum=checksum)
                    s = self.z.seekable(stream)
                    try:
                        self.assertFalse(self.z.is_error(s.init), self.z.error_name(s.init))
                        frames = s.frames()
                        self.assertEqual(frames, self.expected_frames(n, 4096))
                        self.assertEqual(s.size(), n)
                        self.assertEqual(s.read(0, n)[1], data)
                        if n == 0:          # just the seek table, a skippable frame (see expected_frames)
                            self.assertEqual(stream[:4], struct.pack("<I", Z.SKIPPABLE_SEEK_TABLE))
                        footer = stream[-9:]
                        self.assertEqual(struct.unpack("<I", footer[5:])[0], Z.SEEK_TABLE_FOOTER_MAGIC)
                        self.assertEqual(struct.unpack("<I", footer[:4])[0], frames)
                        self.assertEqual(footer[4] >> 7, checksum)
                    finally:
                        s.close()

    def test_seekable_end_frame_sets_frame_boundaries(self):
        """Seekable_EndFrame (GrindCore.net's ZStdSeekableEncoder.EndFrame) ends the frame being written, and the next
        one starts there. Between the caller's ends, frames still end at the maximum frame size, inside
        CompressStream, as soon as they reach it. endFrame logs a frame even when it's empty (zstdseek_compress.c
        endFrame, the same in both releases), so an EndFrame straight after one of those writes an empty frame,
        recorded with size 0. EndStream doesn't (GrindCore's endStream rule, expected_frames). Each frame decodes on
        its own to its part of the data."""
        data, frame = sample.mixed(50000, seed=31), 4096

        def sizes(ends):
            out, start = [], 0
            for cut in sorted(ends) + [len(data)]:
                full, rest = divmod(cut - start, frame)
                out += [frame] * full + ([rest] if rest or cut != len(data) else [])
                start = cut
            return out

        for ends in ((), (1000,), (1000, 5000, 20000), (4096, 8192), (49000,)):
            with self.subTest(end_frame_at=ends):
                # drive() ends frames on step boundaries only, so the step divides every end
                stream = self.seek_encode(data, frame=frame, step=8, end_frame_at=set(ends))
                n, desc = struct.unpack("<IB", stream[-9:-4])
                size = 12 if desc >> 7 else 8
                table = stream[-9 - n * size:-9]
                got = [struct.unpack("<I", table[k * size + 4:k * size + 8])[0] for k in range(n)]
                self.assertEqual(got, sizes(ends), "decompressed size per frame, from the seek table")
                s = self.z.seekable(stream)
                try:
                    self.assertEqual(s.frames(), n)
                    pos = 0
                    for k, want in enumerate(got):
                        self.assertEqual(s.frame(k, want), (want, data[pos:pos + want]), "frame %d" % k)
                        pos += want
                finally:
                    s.close()

    def test_seekable_output_matches_official(self):
        self.need_ref()
        for n, frame, step, ends in ((0, 4096, 5000, ()), (1, 4096, 5000, ()), (4096, 4096, 5000, ()),
                                     (12288, 4096, 1000, ()), (12289, 4096, 1, ()), (100000, 0, 30000, ()),
                                     (50000, 1000, 777, (5000, 20000)), (65536, 65536, 1 << 20, (100,))):
            for level, checksum in ((1, 0), (3, 1), (9, 1)):
                data = sample.mixed(n, seed=n + level)
                with self.subTest(n=n, frame=frame, level=level, checksum=checksum, end_frame_at=ends):
                    a = Z.encode(self.z.seekable_encoder(level, checksum, frame), data, step, end_frame_at=set(ends))
                    b = Z.encode(self.ref.seekable_encoder(level, checksum, frame), data, step, end_frame_at=set(ends))
                    if self.VERSION == "1.5.7":         # GrindCore's endStream rule: see expected_frames
                        b = Z.drop_trailing_empty_frame(b)
                    self.assertEqual(a, b)
                    self.assertEqual(self.ref.seekable_read_all(a)[1], data, "official decoder")

    def test_seekable_random_access(self):
        data = sample.mixed(250000, seed=5)
        stream = self.seek_encode(data, frame=10000, step=1 << 20)
        s = self.z.seekable(stream)
        try:
            frames = s.frames()
            self.assertEqual(frames, 25)
            f = lambda name, *a: s.f(name, *a)
            coff = 0
            for i in range(frames):
                start, size = f("GetFrameDecompressedOffset", i), f("GetFrameDecompressedSize", i)
                self.assertEqual((start, size), (i * 10000, 10000 if i < 25 else 0))
                self.assertEqual(f("GetFrameCompressedOffset", i), coff)
                coff += f("GetFrameCompressedSize", i)
                if size:
                    self.assertEqual(f("OffsetToFrameIndex", start + size - 1), i)
                    self.assertEqual(s.frame(i, size)[1], data[start:start + size])
            self.assertEqual(f("OffsetToFrameIndex", len(data)), frames)
            self.assertEqual(f("GetFrameCompressedOffset", frames), Z.FRAMEINDEX_TOOLARGE)
            self.assertEqual(f("GetFrameDecompressedOffset", frames), Z.FRAMEINDEX_TOOLARGE)
            self.assertTrue(self.z.is_error(f("GetFrameCompressedSize", frames)))
            self.assertTrue(self.z.is_error(s.frame(frames, 1 << 20)[0]))
            self.assertTrue(self.z.is_error(s.frame(0, 9999)[0]), "frame 0 into 9,999 bytes")
            r = random.Random(3)
            for _ in range(300):
                off = r.randrange(len(data))
                n = r.randrange(1, 40000)
                got = s.read(off, n)[1]
                self.assertEqual(got, data[off:off + n], "offset %d length %d" % (off, n))
            self.assertEqual(s.read(len(data), 64), (0, b""), "a read at the very end")
        finally:
            s.close()

    def test_seekable_read_past_the_end(self):
        """Newly found, upstream (both releases, and zstd's dev branch): ZSTD_seekable_decompress clamps with
        `len = eos - offset`, which wraps when offset is past the end. Nothing is written, but the return value is a
        near-2^64 "length": within ~120 bytes of the end it reads as some unrelated error, further out as success.
        GrindCore.net guards against it (ZStdSeekableDecoder.cs:165, ZStdSeekableStream.cs:188).
        Fixed: a read starting at the end gives 0 bytes (as upstream), and one starting beyond it gives an error
        (frameIndex_tooLarge), which is what upstream's own tests for #2335 expect (they pass upstream only because
        their read starts 2 bytes past the end, where the wrapped value happens to read as an error)."""
        data = sample.text(10000)
        s = self.z.seekable(self.seek_encode(data))
        try:
            dst = ctypes.create_string_buffer(64)
            results = {gap: s.f("Decompress", dst, 64, len(data) + gap) for gap in (1, 200, 10000)}
            if any(v > 64 and not self.z.is_error(v) for v in results.values()):
                known(self, "zstd-seekable-read-past-end")
            for gap, v in results.items():
                self.assertTrue(self.z.is_error(v), "gap %d: %d is not an error" % (gap, v))
            self.assertEqual(s.f("Decompress", dst, 64, len(data)), 0, "a read starting at the end")
        finally:
            s.close()
        # upstream's seekable_tests.c tests 2 and 3 (#2335): a seek table with no frames, read 2 bytes in
        for raw in (b"^*M\x18\x09" + bytes(7) + b"\x03\xb1\xea\x92\x8f",
                    b"\x28\xb5\x2f\xfd\x00\x32\x91\x00\x00\x00^*M\x18\x09" + bytes(8) + b"\xb1\xea\x92\x8f"):
            s = self.z.seekable(raw)
            try:
                self.assertFalse(self.z.is_error(s.init))
                dst = ctypes.create_string_buffer(400)
                self.assertTrue(self.z.is_error(s.f("Decompress", dst, 400, 2)), "upstream #2335 case")
            finally:
                s.close()

    def test_zero_frame_file_is_readable(self):
        """1.5.2 writes just the seek table for empty input: a valid file with no zstd frame. Every decoder must
        read it as empty."""
        zero = Z.encode(Z.ZStd(self.L, "1.5.2").seekable_encoder(3, 1, 4096), b"", 5000)
        self.assertEqual(len(zero), 17)
        s = self.z.seekable(zero)
        try:
            self.assertFalse(self.z.is_error(s.init))
            self.assertEqual((s.frames(), s.size(), s.read(0, 10)), (0, 0, (0, b"")))
        finally:
            s.close()
        self.assertEqual(self.z.decompress_block(zero, 16), (0, b""))
        self.assertEqual(Z.drain(self.z.stream_decoder(), zero, 5), (b"", 0))
        if pyzstd:
            self.assertEqual(pyzstd.decompress(zero), b"")

    def test_seekable_files_cross_versions(self):
        """The format didn't change between the releases, so each version reads the other's files."""
        data = sample.mixed(60000, seed=8)
        other = Z.ZStd(self.L, "1.5.2" if self.VERSION == "1.5.7" else "1.5.7")
        for n in (0, 8192, 60000):
            stream = Z.encode(other.seekable_encoder(3, 1, 4096), data[:n], 5000)
            s = self.z.seekable(stream)
            try:
                self.assertFalse(self.z.is_error(s.init))
                self.assertEqual(s.read(0, n)[1], data[:n])
            finally:
                s.close()

    def test_seekable_frame_size_limit(self):
        limit = Z.MAX_FRAME[self.VERSION]
        for size, ok in ((0, True), (1, True), (limit, True), (limit + 1, False)):
            enc = self.z.seekable_encoder(3, 0, size)
            try:
                self.assertEqual(not self.z.is_error(enc.init_result), ok, "maxFrameSize %d" % size)
            finally:
                enc.close()

    def patched(self, stream, frame, csize=None, dsize=None):
        """`stream` with seek-table entry `frame`'s compressed and/or decompressed size replaced."""
        n, desc = struct.unpack("<IB", stream[-9:-4])
        size = 12 if desc >> 7 else 8
        at = len(stream) - 9 - n * size + frame * size
        out = bytearray(stream)
        if csize is not None:
            struct.pack_into("<I", out, at, csize)
        if dsize is not None:
            struct.pack_into("<I", out, at + 4, dsize)
        return bytes(out)

    def test_seekable_table_checks(self):
        """GrindCore's PAL checks the seek table zstd has just loaded (audit/other-codecs.md 4.8.2), the same way on
        every RID. Each check is tried just inside its limit (accepted) and just outside (rejected), through
        InitBuff and through callbacks (GrindCore.net's stream decoder):
          G1: no entry size within zstd's error-code range below 4 GiB: 32-bit GetFrame*Size would return such a size
              as an error code. The limit, 2^32 - ZSTD_error_maxCode, is worked out from IsError;
          G2: no frame claiming more data than its compressed bytes can hold, 128 KiB per 4 bytes (an RLE block);
          G3: InitBuff only, where the source's size is known: the frames end before the seek table.
        A table that fails gives corruption_detected, and the context then refuses every call."""
        bits = 8 * ctypes.sizeof(ctypes.c_size_t)
        k = next(k for k in range(1, 1 << 16) if not self.z.is_error((1 << bits) - k))   # = ZSTD_error_maxCode
        limit = (1 << 32) - k
        stream = self.seek_encode(sample.text(12000, seed=3), frame=4000)                  # 3 frames, checksums
        n = struct.unpack("<I", stream[-9:-5])[0]
        c1 = struct.unpack_from("<I", stream, len(stream) - 9 - n * 12 + 12)[0]           # frame 1's compressed size
        big_c = 4 * ((1 << 32) // (128 << 10))      # just enough compressed bytes for G2 to allow 4 GiB
        cases = [  # (label, compressed size, decompressed size, accepted by InitBuff, accepted through callbacks)
            ("as written", None, None, True, True),
            ("G1, compressed size at the limit", limit, None, False, True),        # InitBuff: G3
            ("G1, compressed size one past", limit + 1, None, False, False),
            ("G1, decompressed size at the limit", big_c, limit, False, True),     # InitBuff: G3
            ("G1, decompressed size one past", big_c, limit + 1, False, False),
            ("G2, at 128 KiB per 4 bytes", None, c1 // 4 * (128 << 10), True, True),
            ("G2, one past", None, c1 // 4 * (128 << 10) + 1, False, False),
            ("G3, frames end a byte before the table", c1 - 1, None, True, True),
            ("G3, frames run a byte into the table", c1 + 1, None, False, True),
        ]
        got, want = {}, {}
        for label, cs, ds, buff_ok, cb_ok in cases:
            raw = self.patched(stream, 1, cs, ds)
            for callbacks, ok in ((False, buff_ok), (True, cb_ok)):
                key = (label, "callbacks" if callbacks else "InitBuff")
                s = self.z.seekable(raw, callbacks=callbacks)
                try:
                    accepted = not self.z.is_error(s.init)
                    if not accepted:    # zstd's loader checks no sizes, so this is the PAL: then nothing works
                        dst = ctypes.create_string_buffer(64)
                        after = (self.z.error_name(s.init), s.frames(), self.z.is_error(s.f("Decompress", dst, 64, 0)))
                        self.assertEqual(after, ("Data corruption detected", 0, True), key)
                finally:
                    s.close()
                got[key], want[key] = accepted, ok
        guard_only = [key for key in want if not want[key] and key[0] != "as written"]
        if all(got[key] for key in guard_only):
            known(self, "zstd-seekable-table-unchecked")
        self.assertEqual(got, want)

    def test_seekable_beyond_4_gib(self):
        """Offsets past 2^32: the format's offsets are 64-bit, its entries' sizes 32-bit, and on 32-bit RIDs this is
        where a truncated offset would show. 4.5 GiB of zeros with each MiB starting with its own 8-byte offset, at
        the release's largest frame, read around and past 4 GiB (about 200 KB compressed; 5 s per version on the
        PC)."""
        mib, total = 1 << 20, 9 << 29
        enc = self.z.seekable_encoder(1, 1, Z.MAX_FRAME[self.VERSION])
        src, cap = ctypes.create_string_buffer(mib), 1 << 17
        dst, out = ctypes.create_string_buffer(cap), bytearray()
        try:
            for m in range(total // mib):
                struct.pack_into("<Q", src, 0, m * mib)
                pos = 0
                while pos < mib:
                    r, i, o = enc.call(Z.CONTINUE, dst, cap, ctypes.addressof(src) + pos, mib - pos)
                    self.assertFalse(self.z.is_error(r), self.z.error_name(r) if self.z.is_error(r) else "")
                    out += dst.raw[:o]
                    pos += i
            while True:
                r, _, o = enc.call(Z.END, dst, cap, 0, 0)
                self.assertFalse(self.z.is_error(r))
                out += dst.raw[:o]
                if r == 0:
                    break
        finally:
            enc.close()

        def expect(off, n):
            want = bytearray(n)
            for m in range(off // mib, (off + n - 1) // mib + 1):
                for j, b in enumerate(struct.pack("<Q", m * mib)):
                    if 0 <= m * mib + j - off < n:
                        want[m * mib + j - off] = b
            return bytes(want)

        frame = Z.MAX_FRAME[self.VERSION]
        s = self.z.seekable(bytes(out))
        try:
            self.assertFalse(self.z.is_error(s.init), self.z.error_name(s.init))
            self.assertEqual((s.frames(), s.size()), (total // frame + (1 if total % frame else 0), total))
            self.assertEqual(s.f("GetFrameDecompressedOffset", s.frames() - 1), (s.frames() - 1) * frame)
            self.assertEqual(s.f("OffsetToFrameIndex", (1 << 32) + 5), (1 << 32) // frame)
            for off, n in (((1 << 32) - 4, 12), (1 << 32, 16), ((1 << 32) + 3 * mib - 3, 11), (total - 16, 16)):
                self.assertEqual(s.read(off, n), (n, expect(off, n)), "read at %#x" % off)
            self.assertEqual(s.read(total - 8, 100), (8, expect(total - 8, 8)), "the clamped read at the end")
        finally:
            s.close()
        print("  %s: 4.5 GiB in %d frames, %d bytes compressed; reads past 4 GiB correct" % (
            self.VERSION, total // frame + (1 if total % frame else 0), len(out)))

    def test_seekable_rejects_oversized_frame_count(self):
        """Upstream (open PRs #4685, #4717): loadSeekTable never bounds the footer's Number_Of_Frames by
        ZSTD_SEEKABLE_MAXFRAMES, and computes the table size in 32 bits. A count whose size wraps to the real table's
        (0x40000003 for three checksummed entries: one bit away from 3) passes every check; the entry array is then
        sizeof(entry) * (count + 1) bytes: about 26 GB on 64-bit, which Linux hands out and the fill loop writes
        until the process is killed, and 80-96 bytes on 32-bit, where the loop is a heap overflow.

        Detected safely first, with #4717's own vector (0x08000001 frames, whose size doesn't wrap onto a real
        table): a fixed loader says corruption_detected, an unfixed one fails later with an I/O error. The wrapping
        vector runs only on a loader that passed that, and in a child process."""
        probe = struct.pack("<IIIBI", Z.SKIPPABLE_SEEK_TABLE, 9, 0x08000001, 0, Z.SEEK_TABLE_FOOTER_MAGIC)
        s = self.z.seekable(probe)
        try:
            self.assertTrue(self.z.is_error(s.init))
            name = self.z.error_name(s.init)
        finally:
            s.close()
        if "corruption" not in name.lower():
            known(self, "zstd-seekable-numframes-overflow")
        code = """
            import struct
            from gctest import sample, zstd_util as Z
            z = Z.ZStd(L, %r)
            good = Z.encode(z.seekable_encoder(3, 1, 4000), sample.text(12000), 5000)
            n = struct.unpack("<I", good[-9:-5])[0]
            bad = good[:-9] + struct.pack("<I", n | 0x40000000) + good[-5:]
            s = z.seekable(bad)
            print("error" if z.is_error(s.init) else "accepted", z.error_name(s.init))
            s.close()
        """ % self.VERSION
        rc, out, err = child(code, timeout=120)
        self.assertEqual(rc, 0, "child: %r %s" % (rc, err[-1500:]))
        self.assertTrue(out.startswith("error"), out)

    def test_seekable_frame_size_index_bounds(self):
        """Upstream (open PR #4758): getFrameDecompressedSize checks `frameIndex > tableLen` where its compressed
        twin checks `>=`, so index == frame count reads entries[count + 1], one past the table. In a child process."""
        code = """
            from gctest import sample, zstd_util as Z
            z = Z.ZStd(L, %r)
            s = z.seekable(Z.encode(z.seekable_encoder(3, 1, 4000), sample.text(10000), 5000))
            n = s.frames()
            print(z.is_error(s.f("GetFrameCompressedSize", n)), z.is_error(s.f("GetFrameDecompressedSize", n)),
                  z.is_error(s.f("GetFrameDecompressedSize", n + 1)))
            s.close()
        """ % self.VERSION
        rc, out, err = child(code, timeout=120)
        self.assertEqual(rc, 0, "child: %r %s" % (rc, err[-1500:]))
        compressed, at_count, past = out.split()
        self.assertEqual((compressed, past), ("True", "True"))
        if at_count != "True":
            known(self, "zstd-seekable-framesize-overread")

    def test_seekable_callback_source_reads_the_same(self):
        """Seekable_InitAdvanced with read/seek callbacks (GrindCore.net's stream-based decoder) against InitBuff."""
        data = sample.mixed(90000, seed=12)
        stream = self.seek_encode(data, frame=7000, step=1 << 20)
        a, b = self.z.seekable(stream), self.z.seekable(stream, callbacks=True)
        try:
            self.assertFalse(self.z.is_error(b.init), self.z.error_name(b.init))
            self.assertEqual((b.frames(), b.size()), (a.frames(), a.size()))
            self.assertEqual(b.read(0, len(data))[1], data)
            r = random.Random(4)
            for _ in range(100):
                off, n = r.randrange(len(data)), r.randrange(1, 20000)
                self.assertEqual(b.read(off, n), a.read(off, n), "offset %d length %d" % (off, n))
            self.assertEqual(b.frame(3, 7000)[1], data[21000:28000])
        finally:
            a.close()
            b.close()

    def test_seekable_frame_size_disagreeing_with_the_table(self):
        """A seek table whose Decompressed_Size for a frame is wrong (the table has no checksum of its own). Newly
        found, upstream: when the frame decodes shorter than the table says, the decoder picks the same frame again
        and re-decodes it without end from a callback source (a buffer source gives up after reading more than the
        buffer); when it decodes longer, its extra bytes overwrite the next frame's in the output, even with
        checksums on. Either must be corruption_detected. One child process per case, since a hang is the defect."""
        results = {}
        for checksum in (1, 0):
            for delta in (100, -100):
                for callbacks in (False, True):
                    code = """
                        import struct
                        from gctest import sample, zstd_util as Z
                        z = Z.ZStd(L, %r)
                        Z.CALLBACK_STDCALL = %r
                        FRAME = 4000
                        data = sample.text(3 * FRAME + 500, seed=4)
                        good = Z.encode(z.seekable_encoder(3, %d, FRAME), data, 5000)
                        s = z.seekable(good); nf = s.frames()
                        table = s.f("GetFrameCompressedOffset", nf - 1) + s.f("GetFrameCompressedSize", nf - 1); s.close()
                        bad = bytearray(good)
                        struct.pack_into("<I", bad, table + 8 + 4, FRAME + %d)      # frame 0's Decompressed_Size
                        s = z.seekable(bytes(bad), callbacks=%r)
                        r, got = s.read(0, len(data))
                        print("error" if z.is_error(r) else ("same data" if got == data else "different data"))
                        s.close()
                    """ % (self.VERSION, Z.CALLBACK_STDCALL, checksum, delta, callbacks)
                    rc, out, err = child(code, timeout=30)
                    key = "checksums %s, frame 0 %+d, %s" % ("on" if checksum else "off", delta,
                                                            "callbacks" if callbacks else "buffer")
                    results[key] = "hang" if rc == "timeout" else (out.strip() if rc == 0 else "crash %r" % rc)
        if any(v != "error" for v in results.values()):
            known(self, "zstd-seekable-frame-size-mismatch")
        self.assertEqual(set(results.values()), {"error"}, results)

    def test_seekable_decoder_rejects_other_input(self):
        for name, stream in (("empty", b""), ("plain frame", self.z.compress_block(sample.text(5000), 3)[1]),
                             ("short", b"\x5e\x2a\x4d\x18\x09\x00\x00\x00")):
            s = self.z.seekable(stream)
            try:
                self.assertTrue(self.z.is_error(s.init), name)
            finally:
                s.close()

    def test_seekable_checksums_catch_corruption(self):
        """With checksums, a bit flip in a frame must give an error or the right data, never wrong data: through a
        whole-file read and through DecompressFrame. (The seek table itself has no checksum, so flips there can
        legitimately move frame boundaries; they're counted apart.) In a child process."""
        code = """
            import json
            from gctest import sample, zstd_util as Z
            z = Z.ZStd(L, %r)
            FRAME = 4000
            data = sample.mixed(3 * FRAME, seed=2)
            stream = Z.encode(z.seekable_encoder(3, 1, FRAME), data, 5000)
            s = z.seekable(stream); nf = s.frames()
            offs = [s.f("GetFrameCompressedOffset", k) for k in range(nf)]
            table = offs[-1] + s.f("GetFrameCompressedSize", nf - 1); s.close()
            counts = {"cases": 0, "errors": 0, "correct": 0, "wrong_whole": [], "wrong_frame": []}
            # frames only: seek-table flips have their own tests, and one flip in Number_Of_Frames can make an
            # unfixed decoder allocate and fill gigabytes (test_seekable_rejects_oversized_frame_count)
            for i in range(table):
                for bit in (0, 3, 6):
                    b = bytearray(stream); b[i] ^= 1 << bit
                    s = z.seekable(bytes(b)); counts["cases"] += 1
                    if z.is_error(s.init):
                        counts["errors"] += 1
                    else:
                        r, out = s.read(0, len(data))
                        if z.is_error(r):
                            counts["errors"] += 1
                        elif out == data:
                            counts["correct"] += 1
                        else:
                            counts["wrong_whole"].append(i)
                        f = max(k for k in range(nf) if offs[k] <= i)
                        r, out = s.frame(f, FRAME)
                        if not z.is_error(r) and out != data[f * FRAME:(f + 1) * FRAME]:
                            counts["wrong_frame"].append(i)
                        if f < 2:                       # a read running one byte into the next frame
                            r, out = s.read(f * FRAME, FRAME + 1)
                            if not z.is_error(r) and out != data[f * FRAME:(f + 1) * FRAME + 1]:
                                counts["wrong_frame"].append(i)
                    s.close()
            print(json.dumps(counts))
        """ % self.VERSION
        rc, out, err = child(code, timeout=900)
        self.assertEqual(rc, 0, "child: %r %s" % (rc, err[-2000:]))
        counts = json.loads(out.strip().splitlines()[-1])
        self.assertGreater(counts["errors"], counts["cases"] // 2, counts)
        if counts["wrong_whole"] or counts["wrong_frame"]:
            known(self, "zstd-seekable-checksum-gaps")


class ZStd157(_ZStdTests, unittest.TestCase):
    VERSION = "1.5.7"


class ZStd152(_ZStdTests, unittest.TestCase):
    VERSION = "1.5.2"
