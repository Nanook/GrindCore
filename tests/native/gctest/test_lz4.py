"""LZ4 1.10.0: blocks and streams (SZ_Lz4_v1_10_0_*) and frames (SZ_Lz4F_v1_10_0_*).

GrindCore vendors official LZ4 1.10.0 unmodified: lib/ is byte-identical to the release tarball (other-codecs.md 4.1).
The references are:
  - official LZ4 built unmodified from the release tarball by ref/build_lz4_ref.py (set GC_REF_LZ4 to that folder).
    GrindCore's output must match it byte for byte for the same calls, since LZ4's output is fully determined by the
    version, the entry point, the parameters and the calls;
  - independent block and frame decoders written from LZ4's format descriptions (oracle_lz4), on every platform.
Corrupt-input runs and the probes of known PAL defects that can crash run in child processes."""
import ctypes
import json
import random
import unittest

import gctest
from gctest import sample
from gctest import lz4_util as Z
from gctest.oracle_lz4 import LZ4FormatError, decompress_block, decompress_frames
from gctest.test_hashes import child, known

P = "SZ_Lz4_v1_10_0_"
PF = "SZ_Lz4F_v1_10_0_"


def corpus():
    return [("empty", b""), ("1 byte", b"x")] + [("%s %d" % (k, n), f(n, seed=n)) for k, f in sample.KINDS
                                                 for n in (1000, 70000)] + [("mixed 300000", sample.mixed(300000))]


class Lz4Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.L = gctest.LIB
        cls.z = Z.Lz4(cls.L)
        cls.ref = Z.reference()

    def need_ref(self):
        if not self.ref:
            gctest.need(self, "lz4")

    def fn(self, name):
        return getattr(self.L, name)

    def need(self, *names):
        """Skip where an export is missing: the shipped Linux and macOS binaries lack the five dllexport-only LZ4
        functions (KNOWN_SYMBOL_GAPS; entrypoints.c fixed on audit/native-fixes)."""
        for name in names:
            if not self.L.has(P + name):
                self.skipTest("%s%s not exported by this library (known symbol gap)" % (P, name))

    # ------------------------------------------------------------------ blocks

    def test_block_round_trip_fast(self):
        for name, data in corpus():
            for accel in (1, 2, 8, 65537, 0, -3):     # LZ4 treats accel <= 0 as 1 and clamps at 65537
                with self.subTest(data=name, accel=accel):
                    r, block = self.z.compress_fast(data, accel)
                    self.assertGreater(r, 0)
                    self.assertEqual(decompress_block(block), data, "independent decoder")
                    self.assertEqual(self.z.decompress(block, len(data))[1], data, "GrindCore's decoder")

    def test_block_output_matches_official_fast(self):
        self.need_ref()
        for name, data in corpus():
            for accel in (1, 2, 8, 65537, 0):
                with self.subTest(data=name, accel=accel):
                    self.assertEqual(self.z.compress_fast(data, accel), self.ref.compress_fast(data, accel))

    def test_block_hc_round_trip_and_official(self):
        """LZ4_compress_HC: levels <= 0 mean the default (9), levels above 12 are clamped to 12."""
        for name, data in corpus():
            for level in (1, 2, 3, 9, 10, 12, 13, 0, -1):
                if len(data) > 70000 and level not in (3, 9, 12):
                    continue
                with self.subTest(data=name, level=level):
                    r, block = self.z.compress_hc(data, level)
                    self.assertGreater(r, 0)
                    self.assertEqual(decompress_block(block), data)
                    if self.ref:
                        self.assertEqual((r, block), self.ref.compress_hc(data, level))
        self.assertEqual(self.z.compress_hc(sample.text(20000), 13), self.z.compress_hc(sample.text(20000), 12))

    def test_hc_extstate_and_destsize(self):
        for name, data in corpus()[2:8]:
            for level in (3, 9, 12):
                with self.subTest(data=name, level=level):
                    self.assertEqual(self.z.compress_hc_ext(data, level), self.z.compress_hc(data, level))
                    target = max(len(self.z.compress_hc(data, level)[1]) // 2, 16)
                    r, used, block = self.z.compress_hc_destsize(data, target, level)
                    self.assertTrue(0 < r <= target, r)
                    self.assertEqual(decompress_block(block), data[:used])
                    if self.ref:
                        self.assertEqual((r, used, block), self.ref.compress_hc_destsize(data, target, level))

    def test_block_capacities(self):
        """LZ4 writes nothing useful into too little room and reports failure; any success is the full block, as with
        official LZ4. The fast path returns LZ4's own 0 for failure, the HC path the PAL's COMPRESSFAIL
        (other-codecs.md 4.1): both are unambiguous, since no LZ4 block is empty."""
        data = sample.text(20000)
        fast, hc = self.z.compress_fast(data)[1], self.z.compress_hc(data, 9)[1]
        paths = (("fast", fast, lambda c: self.z.compress_fast(data, 1, capacity=c), (0,),
                  lambda c: self.ref.compress_fast(data, 1, capacity=c)),
                 ("hc", hc, lambda c: self.z.compress_hc(data, 9, capacity=c), (Z.COMPRESSFAIL,),
                  lambda c: self.ref.compress_hc(data, 9, capacity=c)))
        for which, full, call, fail, ref_call in paths:
            for cap in (0, 1, len(full) // 2, len(full) - 1, len(full), len(full) + 1, Z.bound(len(data))):
                with self.subTest(path=which, capacity=cap):
                    r, out = call(cap)
                    if cap < len(full):
                        self.assertIn(r, fail)
                    elif cap >= Z.bound(len(data)):
                        self.assertEqual(out, full)
                    else:
                        self.assertTrue(out == full or r in fail, r)
                    if self.ref:
                        rr, rout = ref_call(cap)
                        self.assertEqual((out, r > 0), (rout, rr > 0))
        block = fast
        self.assertEqual(self.z.decompress(block, len(data))[1], data)
        self.assertLess(self.z.decompress(block, len(data) - 1)[0], 0, "one byte too little room must fail")

    def test_decoder_never_writes_past_capacity(self):
        """LZ4_decompress_safe's guarantee: for any input it writes at most dstCapacity bytes. Guard bytes after the
        capacity must survive every call."""
        data = sample.mixed(30000)
        block, src = self.z.compress_fast(data)[1], None
        src = Z._Buf(block)
        for cap in (0, 1, 100, 29999, 30000):
            guard = b"\xA5" * 64
            dst = ctypes.create_string_buffer(cap + 64)
            ctypes.memmove(ctypes.addressof(dst) + cap, guard, 64)
            s = self.z.stream()
            try:
                r = self.fn(P + "DecompressSafeContinue")(ctypes.byref(s), src.addr, dst, len(block), cap)
            finally:
                self.z.end(s)
            self.assertEqual(dst.raw[cap:cap + 64], guard, "capacity %d: wrote past the end" % cap)
            self.assertEqual(r, 30000 if cap >= 30000 else Z.DECOMPRESSFAIL)

    # ------------------------------------------------------------------ streams and dictionaries

    def blocks(self, data, size):
        return [data[i:i + size] for i in range(0, len(data), size)]

    def test_linked_blocks(self):
        """Blocks compressed one after another on one stream refer back into earlier blocks: they decode only in
        order, each right after the previous block's output."""
        data = sample.mixed(400000)
        for size in (4096, 65536):
            with self.subTest(block=size):
                enc = self.z.linked_encoder()
                try:
                    blocks = [enc.compress(b)[1] for b in self.blocks(data, size)]
                finally:
                    enc.close()
                if self.ref:
                    renc = self.ref.linked_encoder()
                    try:
                        self.assertEqual([renc.compress(b)[1] for b in self.blocks(data, size)], blocks)
                    finally:
                        renc.close()
                dec, out = self.z.linked_decoder(), b""
                try:
                    for b in blocks:
                        out += dec.decompress(b, size)[1]
                finally:
                    dec.close()
                self.assertEqual(out, data)
                oracle = b""
                for b in blocks:
                    oracle += decompress_block(b, size, prefix=oracle)
                self.assertEqual(oracle, data)
                refers_back = [k for k, b in enumerate(blocks[1:], 1) if self.z.decompress(b, size)[1] is None]
                self.assertTrue(refers_back, "no block refers back into an earlier one: not linked")

    def test_dictionary(self):
        """LoadDict, then blocks that refer into the dictionary: DecompressUsingDict and the oracle decode them, and
        official LZ4 gives the same blocks."""
        self.need("DecompressUsingDict")
        dictionary = sample.text(32768, seed=21)
        records = [sample.text(3000, seed=s) for s in (22, 23, 24)]
        enc = self.z.linked_encoder(dictionary=dictionary)
        try:
            first = enc.compress(records[0])[1]
        finally:
            enc.close()
        plain = self.z.compress_fast(records[0])[1]
        self.assertLess(len(first), len(plain), "the dictionary didn't help")
        self.assertEqual(self.z.decompress_using_dict(first, 3000, dictionary)[1], records[0])
        self.assertEqual(decompress_block(first, prefix=dictionary), records[0])
        if self.ref:
            renc = self.ref.linked_encoder(dictionary=dictionary)
            try:
                self.assertEqual(renc.compress(records[0])[1], first)
            finally:
                renc.close()

    def test_using_dict_empty_block(self):
        """An empty input compresses to a 1-byte block, which decodes to 0 bytes. DecompressUsingDict must report 0,
        not a failure."""
        self.need("DecompressUsingDict")
        dictionary = sample.text(4096, seed=31)
        enc = self.z.linked_encoder(dictionary=dictionary)
        try:
            r, block = enc.compress(b"")
        finally:
            enc.close()
        self.assertEqual((r, block), (1, b"\x00"))
        got = self.z.decompress_using_dict(block, 16, dictionary)
        if got[0] == Z.DECOMPRESSFAIL:
            known(self, "lz4-usingdict-empty")
        self.assertEqual(got, (0, b""))

    def test_attach_dict(self):
        self.need("AttachDict", "DecompressUsingDict")
        dictionary, data = sample.text(20000, seed=41), sample.text(5000, seed=42)
        ds, s = self.z.stream(), self.z.stream()
        dbuf, src = Z._Buf(dictionary), Z._Buf(data)
        try:
            self.assertEqual(self.fn(P + "LoadDict")(ctypes.byref(ds), dbuf.addr, len(dictionary)), Z.OK)
            self.assertEqual(self.fn(P + "AttachDict")(ctypes.byref(s), ctypes.byref(ds)), Z.OK)
            dst = ctypes.create_string_buffer(Z.bound(len(data)))
            r = self.fn(P + "CompressFastContinue")(ctypes.byref(s), src.addr, dst, len(data), len(dst), 1)
            self.assertGreater(r, 0)
            block = dst.raw[:r]
        finally:
            self.z.end(s)
            self.z.end(ds)
        self.assertEqual(decompress_block(block, prefix=dictionary), data)
        self.assertEqual(self.z.decompress_using_dict(block, len(data), dictionary)[1], data)
        if self.ref:
            d = self.ref.dll
            rds, rs = d.LZ4_createStream(), d.LZ4_createStream()
            try:
                d.LZ4_loadDict(rds, dbuf.addr, len(dictionary))
                d.LZ4_attach_dictionary(rs, rds)
                dst = ctypes.create_string_buffer(Z.bound(len(data)))
                rr = d.LZ4_compress_fast_continue(rs, src.addr, dst, len(data), len(dst), 1)
                self.assertEqual(dst.raw[:rr], block)
            finally:
                d.LZ4_freeStream(rs)
                d.LZ4_freeStream(rds)

    def test_save_dict(self):
        """SaveDict copies the stream's history (up to 64 KB) out, so the next block can refer to it after the input
        buffer is gone: the saved bytes are the input's last bytes."""
        data = sample.mixed(100000)
        enc = self.z.linked_encoder()
        try:
            enc.compress(data)
            buf = ctypes.create_string_buffer(65536)
            n = self.fn(P + "SaveDict")(ctypes.byref(enc.s), buf, 65536)
        finally:
            enc.close()
        self.assertEqual(n, 65536)
        self.assertEqual(buf.raw[:n], data[-n:])

    def test_decompress_partial_using_dict(self):
        self.need("DecompressPartialUsingDict")
        dictionary, data = sample.text(8192, seed=51), sample.text(9000, seed=52)
        enc = self.z.linked_encoder(dictionary=dictionary)
        try:
            block = enc.compress(data)[1]
        finally:
            enc.close()
        for target in (1, 100, 4096, 8999, 9000, 20000):
            with self.subTest(target=target):
                s = self.z.stream()
                src, dct = Z._Buf(block), Z._Buf(dictionary)
                dst = ctypes.create_string_buffer(20000)
                try:
                    r = self.fn(P + "DecompressPartialUsingDict")(ctypes.byref(s), src.addr, dst, len(block),
                                                                  target, 20000, dct.addr, len(dictionary))
                finally:
                    self.z.end(s)
                self.assertEqual(r, min(target, len(data)))
                self.assertEqual(dst.raw[:r], data[:r])

    def test_corrupt_blocks(self):
        """Every truncation and a bit flip in every byte of a block: GrindCore's decoders return an error or at most
        the capacity, never crash, and never write past the capacity. Runs in a child."""
        code = """
            import ctypes, json, random
            from gctest import sample, lz4_util as Z
            z = Z.Lz4(L)
            data = sample.mixed(6000, seed=61)
            dictionary = sample.text(4096, seed=62)
            plain = z.compress_fast(data)[1]
            enc = z.linked_encoder(dictionary=dictionary)
            withdict = enc.compress(data)[1]
            enc.close()
            guard = b"\\x5A" * 32
            bad, results = [], {"ok": 0, "error": 0, "short": 0}
            def run(block, use_dict):
                dst = ctypes.create_string_buffer(6000 + 32)
                ctypes.memmove(ctypes.addressof(dst) + 6000, guard, 32)
                s = z.stream()
                src = Z._Buf(block)
                if use_dict:
                    dct = Z._Buf(dictionary)
                    r = L.SZ_Lz4_v1_10_0_DecompressUsingDict(ctypes.byref(s), src.addr, dst, len(block), 6000,
                                                             dct.addr, len(dictionary))
                else:
                    r = L.SZ_Lz4_v1_10_0_DecompressSafeContinue(ctypes.byref(s), src.addr, dst, len(block), 6000)
                z.end(s)
                if dst.raw[6000:6032] != guard or r > 6000:
                    bad.append(len(block))
                results["error" if r < 0 else "ok" if dst.raw[:r] == data and r == 6000 else "short"] += 1
            pairs = [(plain, False)] + ([(withdict, True)] if L.has("SZ_Lz4_v1_10_0_DecompressUsingDict") else [])
            for block, use_dict in pairs:
                for n in range(len(block)):
                    run(block[:n], use_dict)
                rnd = random.Random(63)
                for i in range(len(block)):
                    b = bytearray(block)
                    b[i] ^= 1 << rnd.randrange(8)
                    run(bytes(b), use_dict)
            print(json.dumps({"bad": bad[:5], "results": results}))
        """
        rc, out, err = child(code, timeout=300)
        self.assertEqual(rc, 0, "decoder crashed on corrupt input: %s" % err[-400:])
        got = json.loads(out.strip().splitlines()[-1])
        self.assertEqual(got["bad"], [], "a decoder wrote past its capacity")

    def test_decode_state_is_independent_of_compression(self):
        """The PAL's Stream serves both directions, but Init allocates an LZ4_stream_t (the compressor's state) and
        DecompressSafeContinue uses it as an LZ4_streamDecode_t. On a fresh (zeroed) stream that works by accident.
        After compressing or LoadDict on the same stream, the decoder takes hash-table entries for its history (an
        external dictionary at a wild address), which only a block that refers back reads. So: the second block of a
        linked pair, which a decoder without the first block's output must reject, as a fresh stream does. On a used
        stream it has to be rejected the same way, not decoded from wild memory. Runs in a child."""
        code = """
            import ctypes, json
            from gctest import sample, lz4_util as Z
            z = Z.Lz4(L)
            first, second, other = sample.text(5000, seed=71), sample.text(5000, seed=73), sample.mixed(20000, seed=72)
            enc = z.linked_encoder()
            enc.compress(first)
            block = enc.compress(second)[1]
            enc.close()
            got = {"fresh": z.decompress(block, 5000)[0]}
            for how in ("after compression", "after LoadDict"):
                s = z.stream()
                src = Z._Buf(other)
                if how == "after compression":
                    tmp = ctypes.create_string_buffer(Z.bound(len(other)))
                    L.SZ_Lz4_v1_10_0_CompressFastContinue(ctypes.byref(s), src.addr, tmp, len(other), len(tmp), 1)
                else:
                    L.SZ_Lz4_v1_10_0_LoadDict(ctypes.byref(s), src.addr, len(other))
                b = Z._Buf(block)
                dst = ctypes.create_string_buffer(5000)
                print(json.dumps(got), flush=True)
                got[how] = L.SZ_Lz4_v1_10_0_DecompressSafeContinue(ctypes.byref(s), b.addr, dst, len(block), 5000)
                z.end(s)
            print(json.dumps(got), flush=True)
        """
        rc, out, err = child(code, timeout=60)
        got = json.loads(out.strip().splitlines()[-1]) if out.strip() else {}
        self.assertLess(got.get("fresh", -1), 0, "the probe block must refer back (a fresh decoder rejects it)")
        if rc != 0 or any(v >= 0 for k, v in got.items() if k != "fresh"):
            known(self, "lz4-stream-decode-state")
        self.assertEqual(sorted(got), ["after LoadDict", "after compression", "fresh"])

    def test_transfer_state_copies(self):
        """TransferStateToPalLZ4Stream hands one stream's compression state to another stream. The receiving stream
        must continue exactly where the source was (the same next block), and ending both streams must free each
        state once. The shipped PAL adopted the pointer, so both Ends freed the same state. Runs in a child."""
        code = """
            import ctypes, json
            from gctest import sample, lz4_util as Z
            z = Z.Lz4(L)
            first, second = sample.text(8000, seed=75), sample.text(8000, seed=76)
            a, b = z.stream(), L.struct.SZ_Lz4_v1_10_0_Stream()
            f, s = Z._Buf(first), Z._Buf(second)
            tmp = ctypes.create_string_buffer(Z.bound(8000))
            L.SZ_Lz4_v1_10_0_CompressFastContinue(ctypes.byref(a), f.addr, tmp, 8000, len(tmp), 1)
            L.SZ_Lz4_v1_10_0_TransferStateToPalLZ4Stream(L.SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(ctypes.byref(a)),
                                                         ctypes.byref(b))
            outs = []
            for st in (a, b):     # a's next block, then b's from the state a had before it; a is ended in between
                dst = ctypes.create_string_buffer(Z.bound(8000))
                r = L.SZ_Lz4_v1_10_0_CompressFastContinue(ctypes.byref(st), s.addr, dst, 8000, len(dst), 1)
                outs.append(dst.raw[:r])
                L.SZ_Lz4_v1_10_0_End(ctypes.byref(st))
            for k in range(200):
                p = z.frame_encoder(Z.prefs(z.Prefs))
                p.encode(sample.mixed(5000, seed=k), 2000)
                p.close()
            print(json.dumps({"same next block": outs[0] == outs[1] and len(outs[0]) > 0}))
        """
        rc, out, err = child(code, timeout=120)
        # adopting: b compresses from the state a already advanced and freed (the wrong block, or a crash), and the
        # second End frees it again (a crash where the heap notices)
        if rc != 0 or json.loads(out.strip().splitlines()[-1]) != {"same next block": True}:
            known(self, "lz4-transfer-adopts")

    def test_reset_and_flush_start_an_independent_block(self):
        """ResetStream and Flush both call LZ4_resetStream_fast on the stream's compressor, so the next block on that
        stream has no history: it must decode on its own, where without either it refers back to the previous block.
        The output is official LZ4's for the same calls, where present. On a Stream without state (never initialised,
        or after End) Flush returns ERROR and ResetStream does nothing. GetCurrentLZ4Stream returns the state's
        LZ4_stream_t, or NULL without one."""
        self.need("ResetStream", "Flush")
        L, data = self.L, sample.text(20000, seed=91)
        for how in ("neither", "ResetStream", "Flush"):
            with self.subTest(reset=how):
                enc = self.z.linked_encoder()
                try:
                    enc.compress(data)
                    if how == "ResetStream":
                        L.SZ_Lz4_v1_10_0_ResetStream(ctypes.byref(enc.s))
                    elif how == "Flush":
                        self.assertEqual(L.SZ_Lz4_v1_10_0_Flush(ctypes.byref(enc.s)), Z.OK)
                    r, block = enc.compress(data)
                finally:
                    enc.close()
                self.assertGreater(r, 0)
                if how == "neither":        # the same data again: it's one match back into the first block
                    with self.assertRaises(LZ4FormatError):
                        decompress_block(block)
                    continue
                self.assertEqual(decompress_block(block), data, "independent decoder, no history")
                if self.ref:
                    official = self.ref.linked_encoder()
                    try:
                        official.compress(data)
                        self.ref.dll.LZ4_resetStream_fast(official.s)
                        self.assertEqual((r, block), official.compress(data), "official LZ4, same calls")
                    finally:
                        official.close()
        s = L.struct.SZ_Lz4_v1_10_0_Stream()
        self.assertEqual(L.SZ_Lz4_v1_10_0_Flush(ctypes.byref(s)), Z.ERROR)
        L.SZ_Lz4_v1_10_0_ResetStream(ctypes.byref(s))
        self.assertIsNone(L.SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(ctypes.byref(s)))
        s = self.z.stream()
        self.assertEqual(L.SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(ctypes.byref(s)), s.internalState)
        self.z.end(s)
        self.assertIsNone(L.SZ_Lz4_v1_10_0_GetCurrentLZ4Stream(ctypes.byref(s)), "End clears the state")

    def test_stream_functions_check_their_state(self):
        """After End (or before Init) a Stream's state is NULL. The stream functions must return an error then, as
        Flush and ResetStream do, rather than pass NULL to LZ4. Runs in a child."""
        code = """
            import ctypes, json
            from gctest import lz4_util as Z
            s = L.struct.SZ_Lz4_v1_10_0_Stream()
            src, dst = ctypes.create_string_buffer(b"data" * 100), ctypes.create_string_buffer(1024)
            got = {}
            for name, args in (("CompressFastContinue", (src, dst, 400, 1024, 1)),
                               ("DecompressSafeContinue", (src, dst, 10, 1024)),
                               ("LoadDict", (src, 400)), ("SaveDict", (dst, 1024))):
                got[name] = getattr(L, "SZ_Lz4_v1_10_0_" + name)(ctypes.byref(s), *args)
                print(json.dumps(got), flush=True)
        """
        rc, out, err = child(code, timeout=60)
        if rc != 0:
            known(self, "lz4-stream-null-state")
        got = json.loads(out.strip().splitlines()[-1])
        for name, r in got.items():
            self.assertLess(r, 0, name)

    def test_init_and_create_refuse_a_struct_in_use(self):
        """Init and the LZ4F context creates refuse a struct that still holds its state: they would replace it, and the
        old one would leak (memory.md M10). End or free sets it back to NULL, and then the struct can be used again. On
        a library without the check the second call replaces the state: the known defect (the first one leaks)."""
        L = self.L
        for struct_name, create, release in (
                ("SZ_Lz4_v1_10_0_Stream", P + "Init", P + "End"),
                ("SZ_Lz4F_v1_10_0_CompressionContext", PF + "CreateCompressionContext", PF + "FreeCompressionContext"),
                ("SZ_Lz4F_v1_10_0_DecompressionContext", PF + "CreateDecompressionContext",
                 PF + "FreeDecompressionContext")):
            with self.subTest(create=create):
                s, f, free = getattr(L.struct, struct_name)(), getattr(L, create), getattr(L, release)
                self.assertEqual(f(ctypes.byref(s)), Z.OK)
                first = s.internalState
                self.assertTrue(first)
                second = f(ctypes.byref(s))
                if second == Z.OK and s.internalState != first:
                    free(ctypes.byref(s))
                    known(self, "pal-create-overwrite")
                self.assertEqual(second, Z.ERROR)
                self.assertEqual(s.internalState, first, "a refused call leaves the struct as it was")
                free(ctypes.byref(s))
                self.assertFalse(s.internalState)
                self.assertEqual(f(ctypes.byref(s)), Z.OK, "after End or the free the struct can be used again")
                free(ctypes.byref(s))

    def test_compress_hc_stream(self):
        """SZ_Lz4F_v1_10_0_CompressHC_Stream takes an LZ4F compression context, whose state is an LZ4F_cctx, and uses
        it as an LZ4_streamHC_t (about 256 KB), resetting and compressing into it. It must not write outside the
        context. Runs in a child, with heap activity afterwards to surface corruption."""
        if not self.L.has(PF + "CompressHC_Stream"):
            self.skipTest("CompressHC_Stream isn't exported by this library")
        code = """
            import ctypes, json
            from gctest import sample, lz4_util as Z
            z = Z.Lz4(L)
            ctx = L.struct.SZ_Lz4F_v1_10_0_CompressionContext()
            L.SZ_Lz4F_v1_10_0_CreateCompressionContext(ctypes.byref(ctx))
            data = sample.text(20000, seed=81)
            src, dst = Z._Buf(data), ctypes.create_string_buffer(Z.bound(20000))
            r = L.SZ_Lz4F_v1_10_0_CompressHC_Stream(ctypes.byref(ctx), dst, len(dst), src.addr, len(data), 9, None)
            L.SZ_Lz4F_v1_10_0_FreeCompressionContext(ctypes.byref(ctx))
            for k in range(50):
                p = z.frame_encoder(Z.prefs(z.Prefs, level=k % 4))
                frame = p.encode(sample.mixed(20000, seed=k), 7000)
                p.close()
            print(json.dumps({"r": r}))
        """
        rc, out, err = child(code, timeout=120)
        if rc != 0:
            known(self, "lz4f-comprehc-stream")
        r = json.loads(out.strip().splitlines()[-1])["r"]
        self.assertTrue(r < 0 or r > 0, r)

    def test_null_arguments(self):
        """The PAL's own checks: NULL pointers are an error (SZ_Lz4_v1_10_0_ERROR, -1), never a size. Every call here
        returns before LZ4 is reached."""
        n, buf = None, ctypes.create_string_buffer(64)
        s = self.z.stream()
        try:
            S = ctypes.byref(s)
            for name, args in (("CompressFastContinue", (n, buf, buf, 10, 64, 1)),
                               ("CompressFastContinue", (S, n, buf, 10, 64, 1)),
                               ("CompressFastContinue", (S, buf, n, 10, 64, 1)),
                               ("DecompressSafeContinue", (n, buf, buf, 10, 64)),
                               ("DecompressSafeContinue", (S, n, buf, 10, 64)),
                               ("CompressHC", (n, buf, 10, 64, 9)), ("CompressHC", (buf, n, 10, 64, 9)),
                               ("CompressHC_ExtState", (n, buf, buf, 10, 64, 9)),
                               ("LoadDict", (n, buf, 10)), ("LoadDict", (S, n, 10)), ("SaveDict", (S, n, 10)),
                               ("AttachDict", (n, S)), ("AttachDict", (S, n)),
                               ("DecompressUsingDict", (S, buf, buf, 10, 64, n, 10)),
                               ("Init", (n,))):
                if not self.L.has(P + name):
                    continue
                with self.subTest(fn=name, args=[a is None for a in args]):
                    self.assertEqual(self.fn(P + name)(*args), Z.ERROR)
        finally:
            self.z.end(s)

    # ------------------------------------------------------------------ frames

    PLANS = [dict(level=0), dict(level=3), dict(level=9, block_mode=Z.INDEPENDENT), dict(level=12),
             dict(level=-1), dict(level=1, block_size=Z.BLOCK_256K, content_checksum=1),
             dict(level=0, block_size=Z.BLOCK_1M, block_checksum=1, content_checksum=1),
             dict(level=4, block_size=Z.BLOCK_4M, block_mode=Z.INDEPENDENT, content_size=1),
             dict(level=0, auto_flush=1, block_checksum=1), dict(level=10, favor_dec_speed=1)]

    def frame(self, data, plan, step=1 << 20, flush_at=(), which=None):
        p = dict(plan)
        if p.pop("content_size", 0):
            p["content_size"] = len(data)
        enc = (which or self.z).frame_encoder(Z.prefs(self.z.Prefs, **p))
        try:
            return enc.encode(data, step, flush_at)
        finally:
            enc.close()

    def test_frame_round_trip(self):
        data = sample.mixed(300000, seed=91)
        for plan in self.PLANS:
            for step, flush_at in ((1 << 20, ()), (5000, {70000, 250001})):
                with self.subTest(plan=plan, step=step):
                    frame = self.frame(data, plan, step, flush_at)
                    out, infos = decompress_frames(frame)
                    self.assertEqual(out, data, "independent frame decoder")
                    info = infos[0]
                    self.assertEqual(info["independent"], plan.get("block_mode") == Z.INDEPENDENT)
                    self.assertEqual(info["content_checksum"], bool(plan.get("content_checksum")))
                    self.assertEqual(info["block_checksum"], bool(plan.get("block_checksum")))
                    self.assertEqual(info["content_size"], len(data) if plan.get("content_size") else None)
                    for in_step, out_cap in ((1 << 20, 1 << 20), (997, 4096), (65536, 777)):
                        dec = self.z.frame_decoder()
                        try:
                            got, hint, err = dec.decode(frame, in_step, out_cap)
                        finally:
                            dec.close()
                        self.assertEqual((got == data, hint, err), (True, 0, 0), "in %d, out %d" % (in_step, out_cap))
        # byte-at-a-time input and one byte of room per call, on a small frame: every decoder state boundary
        small = sample.mixed(20000, seed=97)
        for plan in (self.PLANS[0], self.PLANS[6]):
            frame = self.frame(small, plan, 3000, {9000})
            dec = self.z.frame_decoder()
            try:
                self.assertEqual(dec.decode(frame, 1, 1, max_calls=1 << 22), (small, 0, 0))
            finally:
                dec.close()

    def test_frame_output_matches_official(self):
        self.need_ref()
        data = sample.mixed(300000, seed=92)
        for plan in self.PLANS:
            for step, flush_at in ((1 << 20, ()), (5000, {70000, 250001}), (65536, ())):
                with self.subTest(plan=plan, step=step):
                    self.assertEqual(self.frame(data, plan, step, flush_at),
                                     self.frame(data, plan, step, flush_at, which=self.ref))

    def test_frame_bounds_match_official(self):
        for plan in self.PLANS[:4] + [dict(level=0, block_size=Z.BLOCK_4M, block_checksum=1, content_checksum=1)]:
            p = Z.prefs(self.z.Prefs, **{k: v for k, v in plan.items() if k != "content_size"})
            for n in (0, 1, 65535, 65536, 1 << 20, 5 << 20):
                with self.subTest(plan=plan, n=n):
                    b = self.z.frame_bound(n, p)
                    frame = self.frame(sample.noise(n, seed=n), plan) if n <= 65536 else None
                    if frame is not None:
                        self.assertLessEqual(len(frame), b)
                    if self.ref:
                        self.assertEqual(b, self.ref.dll.LZ4F_compressFrameBound(n, ctypes.byref(p)))

    def test_frame_empty(self):
        frame = self.frame(b"", dict(level=0, content_checksum=1))
        self.assertEqual(decompress_frames(frame)[0], b"")
        dec = self.z.frame_decoder()
        try:
            self.assertEqual(dec.decode(frame), (b"", 0, 0))
        finally:
            dec.close()

    def test_frame_info(self):
        data = sample.text(123456, seed=93)
        frame = self.frame(data, dict(level=0, block_size=Z.BLOCK_256K, content_size=1, content_checksum=1))
        dec = self.z.frame_decoder()
        try:
            fi, src, n = self.z.FrameInfo(), Z._Buf(frame), ctypes.c_size_t(len(frame))
            hint = self.fn(PF + "GetFrameInfo")(ctypes.byref(dec.ctx), ctypes.byref(fi), src.addr, ctypes.byref(n))
            self.assertFalse(Z.lz4f_is_error(hint))
            self.assertEqual((fi.blockSizeID, fi.contentSize, fi.contentChecksumFlag), (Z.BLOCK_256K, len(data), 1))
            self.assertEqual(n.value, 15, "header: magic, FLG, BD, 8-byte content size, checksum")
        finally:
            dec.close()

    def test_frame_corrupt_input(self):
        """With both checksums on, every truncation and a bit flip in every byte must give an error (or the exact
        data): never different data, never a crash or a hang. Runs in a child."""
        code = """
            import json, random
            from gctest import sample, lz4_util as Z
            z = Z.Lz4(L)
            data = sample.mixed(9000, seed=94)
            enc = z.frame_encoder(Z.prefs(z.Prefs, level=0, block_checksum=1, content_checksum=1))
            frame = enc.encode(data, 3000)
            enc.close()
            dec = z.frame_decoder()
            res = {"error": 0, "exact": 0, "incomplete": 0, "wrong": []}
            rnd = random.Random(95)
            cases = [frame[:n] for n in range(len(frame))]
            for i in range(len(frame)):
                b = bytearray(frame)
                b[i] ^= 1 << rnd.randrange(8)
                cases.append(bytes(b))
            for k, c in enumerate(cases):
                dec.reset()
                out, hint, err = dec.decode(c, 997, 4096)
                if err:
                    res["error"] += 1
                elif hint != 0:
                    res["incomplete"] += 1 if data.startswith(out) else 0
                    if not data.startswith(out):
                        res["wrong"].append(k)
                elif out == data:
                    res["exact"] += 1
                else:
                    res["wrong"].append(k)
            dec.close()
            print(json.dumps(res))
        """
        rc, out, err = child(code, timeout=300)
        self.assertEqual(rc, 0, "frame decoder crashed: %s" % err[-400:])
        got = json.loads(out.strip().splitlines()[-1])
        self.assertEqual(got["wrong"], [], "corrupt frames decoded to different data")

    def test_frame_decoder_reset_after_error(self):
        good = self.frame(sample.text(50000, seed=96), dict(level=0, content_checksum=1))
        bad = bytearray(good)
        bad[len(bad) // 2] ^= 0x10
        dec = self.z.frame_decoder()
        try:
            self.assertNotEqual(dec.decode(bytes(bad))[2], 0)
            dec.reset()
            self.assertEqual(dec.decode(good), (sample.text(50000, seed=96), 0, 0))
        finally:
            dec.close()

    def test_frame_null_arguments(self):
        """NULL arguments give the PAL's -1 as size_t: SIZE_MAX, which LZ4F_isError accepts. A NULL source with size 0
        (how .NET pins an empty array) is rejected too, which is an error, not a false success."""
        n, buf = None, ctypes.create_string_buffer(64)
        enc = self.z.frame_encoder(Z.prefs(self.z.Prefs))
        dec = self.z.frame_decoder()
        try:
            C, D = ctypes.byref(enc.ctx), ctypes.byref(dec.ctx)
            sz = ctypes.c_size_t(64)
            for name, args in (("CompressBegin", (n, buf, 64, n)), ("CompressBegin", (C, n, 64, n)),
                               ("CompressUpdate", (C, buf, 64, n, 0, n)), ("CompressUpdate", (C, n, 64, buf, 10, n)),
                               ("Flush", (C, n, 64, n)), ("CompressEnd", (C, n, 64, n)),
                               ("Decompress", (D, n, ctypes.byref(sz), buf, ctypes.byref(sz), n)),
                               ("GetFrameInfo", (D, n, buf, ctypes.byref(sz)))):
                with self.subTest(fn=name, args=[a is None for a in args]):
                    self.assertTrue(Z.lz4f_is_error(self.fn(PF + name)(*args)))
            self.assertEqual(self.fn(PF + "CreateCompressionContext")(n), Z.ERROR)
        finally:
            enc.close()
            dec.close()

    def test_frame_error_helpers(self):
        """IsError, GetErrorName and GetErrorCode, exported on audit/native-fixes (absent from the shipped binaries,
        where this skips): LZ4F's own classifier, name and code for a size_t result, where GrindCore.net uses a signed
        cast that can't see an error when size_t is 32-bit (abi-verification.md 4.6). On both sides of the threshold
        for this RID's size_t, against lz4_util's copy of LZ4F_isError; then real failures by code and name, the PAL's
        argument error among them (SIZE_MAX, ERROR_GENERIC)."""
        for name in ("IsError", "GetErrorName", "GetErrorCode"):
            if not self.L.has(PF + name):
                self.skipTest("%s%s not exported by this library (exported on audit/native-fixes)" % (PF, name))
        is_error, error_name, error_code = (self.fn(PF + n) for n in ("IsError", "GetErrorName", "GetErrorCode"))
        top, m = 1 << Z.SIZE_T_BITS, Z.LZ4F_ERROR_MAXCODE
        for v in (0, 1, 19, top // 2, top - m - 1, top - m, top - m + 1, top - 11, top - 1):
            with self.subTest(value=hex(v)):
                self.assertEqual(bool(is_error(v)), Z.lz4f_is_error(v))
                self.assertEqual(error_code(v), Z.lz4f_error_code(v) if Z.lz4f_is_error(v) else 0)
                if not Z.lz4f_is_error(v):
                    self.assertEqual(error_name(v), b"Unspecified error code")
        small = ctypes.create_string_buffer(4)
        enc, dec = self.z.frame_encoder(Z.prefs(self.z.Prefs)), self.z.frame_decoder()
        try:
            begin = self.fn(PF + "CompressBegin")
            real = [("a NULL context", begin(None, small, 4, None), 1, b"ERROR_GENERIC"),
                    ("4 bytes for a frame header", begin(ctypes.byref(enc.ctx), small, 4, None), 11,
                     b"ERROR_dstMaxSize_tooSmall"),
                    ("no frame magic", dec.decode(b"not an LZ4 frame")[1], 13, b"ERROR_frameType_unknown")]
        finally:
            enc.close()
            dec.close()
        for what, r, code, name in real:
            with self.subTest(what):
                self.assertTrue(is_error(r))
                self.assertEqual((error_code(r), error_name(r)), (code, name))
