"""ctypes access to GrindCore's native library, built from the generated spec (_spec.py). Python 3.6+, stdlib only.

    lib = gcnative.load("path/to/libGrindCore.so")      # or GrindCore.dll / libGrindCore.dylib
    lib.SZ_blake3_version()                              # prototypes applied; every call is recorded
    st = lib.struct.CSha256()                            # structs as ctypes classes, laid out by this platform's ABI

Every export is __cdecl on Windows (FUNCTIONCALLINGCONVENCTION) and plain C elsewhere, so CDLL fits everywhere.
"""
import ctypes
import os
import platform
import sys
import sysconfig

from . import _spec

_SCALARS = {
    "void": None, "i8": ctypes.c_int8, "u8": ctypes.c_uint8, "i16": ctypes.c_int16, "u16": ctypes.c_uint16,
    "i32": ctypes.c_int32, "u32": ctypes.c_uint32, "i64": ctypes.c_int64, "u64": ctypes.c_uint64,
    "long": ctypes.c_long, "ulong": ctypes.c_ulong, "f32": ctypes.c_float, "f64": ctypes.c_double, "size": ctypes.c_size_t, "ssize": ctypes.c_ssize_t,
    "ptr": ctypes.c_void_p, "cstr": ctypes.c_char_p,
}


# Exports added or changed in the source after the code map (and so _spec.py) was generated. Rebuilding the code map
# from a tree that has them makes these redundant; the generated entry wins only once it matches.
OVERRIDES = {
    # GrindCore audit/native-fixes: implemented, with srcSize now in/out and an acceleration argument
    "SZ_Lz4_v1_10_0_CompressPartial": ("i32", ["ptr", "ptr", "ptr", "ptr", "i32", "i32"]),
    # audit/native-fixes: void -> int32 status (argument checks, audit/hashes.md 6.5). On older libraries the value
    # read back is whatever the register held, so tests must not trust it there.
    "SZ_SHA3_Init": ("i32", ["ptr", "u32"]),
    "SZ_Sha1_PrepareBlock": ("i32", ["ptr", "ptr", "u32"]),
    # audit/native-fixes: the error helpers (abi-verification.md 4.6): zstd's error code, LZ4F's classifier, name and
    # code, and fast-lzma2's own three, which were compiled in but not in entrypoints.c
    "SZ_ZStd_v1_5_2_GetErrorCode": ("i32", ["size"]),
    "SZ_ZStd_v1_5_7_GetErrorCode": ("i32", ["size"]),
    "SZ_Lz4F_v1_10_0_IsError": ("u32", ["size"]),
    "SZ_Lz4F_v1_10_0_GetErrorName": ("cstr", ["size"]),
    "SZ_Lz4F_v1_10_0_GetErrorCode": ("i32", ["size"]),
    "FL2_isError": ("u32", ["size"]),
    "FL2_isTimedOut": ("u32", ["size"]),
    "FL2_getErrorName": ("cstr", ["size"]),
    # audit/native-fixes: block compression with zstd's frame flags (content size, checksum, dictionary ID), zstd.md 2.3
    "SZ_ZStd_v1_5_2_CompressBlockAdvanced": ("size", ["ptr", "ptr", "size", "ptr", "size", "i32", "i32", "i32", "i32"]),
    "SZ_ZStd_v1_5_7_CompressBlockAdvanced": ("size", ["ptr", "ptr", "size", "ptr", "size", "i32", "i32", "i32", "i32"]),
    "SZ_ZStd_v1_5_2_CompressBlockWithDictAdvanced": ("size", ["ptr", "ptr", "ptr", "size", "ptr", "size", "i32", "i32", "i32"]),
    "SZ_ZStd_v1_5_7_CompressBlockWithDictAdvanced": ("size", ["ptr", "ptr", "ptr", "size", "ptr", "size", "i32", "i32", "i32"]),
}
EXPORTS = dict(_spec.EXPORTS)
EXPORTS.update(OVERRIDES)


class MissingExport(AttributeError):
    pass


def target():
    """This process's RID, e.g. win-x64, linux-arm, osx-arm64 (the library must match it)."""
    bits = 8 * ctypes.sizeof(ctypes.c_void_p)
    m = platform.machine().lower()
    if sys.platform.startswith("win"):
        # platform.machine() is the host CPU: x64 Python emulated on Windows ARM64 says ARM64. The interpreter's own
        # build (win-amd64, win32, win-arm64) is this process's architecture.
        m = {"win-amd64": "amd64", "win32": "x86", "win-arm64": "arm64", "win-arm32": "arm"}.get(
            sysconfig.get_platform(), m)
    if m in ("amd64", "x86_64", "x64", "i386", "i686", "x86"):
        arch = "x64" if bits == 64 else "x86"
    elif m in ("arm64", "aarch64", "armv8l", "armv7l", "armv6l", "arm"):
        arch = "arm64" if bits == 64 else "arm"
    else:
        arch = m
    osname = "win" if sys.platform.startswith("win") else "osx" if sys.platform == "darwin" else "linux"
    return "%s-%s" % (osname, arch)


class _Structs(object):
    """Lazily built ctypes classes for _spec.STRUCTS."""

    def __init__(self):
        self._classes = {}

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        if name not in _spec.STRUCTS:
            raise AttributeError("no struct %s in the spec" % name)
        return self._get(name)

    def _get(self, name):
        if name in self._classes:
            return self._classes[name]
        kind, fields, _ = _spec.STRUCTS[name]
        cls = type(str(name), (ctypes.Union if kind == "union" else ctypes.Structure,), {})
        self._classes[name] = cls  # registered before fields, so nested references resolve
        cls._fields_ = [(str(f), self._ctype(t, dims)) for f, t, dims in fields]
        return cls

    def _ctype(self, code, dims):
        t = self._get(code.split(":", 1)[1]) if code.startswith(("struct:", "union:")) else _SCALARS[code]
        for d in reversed(dims):
            t = t * d
        return t

    def names(self):
        return sorted(_spec.STRUCTS)


CALLS_ENV = "GCNATIVE_CALLS"


def read_calls(log, path):
    """The exports that libraries loaded from `path` recorded in a GCNATIVE_CALLS file (see Library)."""
    path, names = os.path.abspath(path), set()
    try:
        with open(log) as f:
            for line in f:
                p, _, name = line.rstrip("\n").rpartition("\t")
                if p == path:
                    names.add(name)
    except OSError:
        pass
    return names


class Library(object):
    """With GCNATIVE_CALLS naming a file in the environment, each export's first call is also appended to that file
    (library path, tab, name), before the call is made: the suite's child processes report their calls that way, even
    ones that crash."""

    def __init__(self, path):
        self.path = os.path.abspath(path)
        self.dll = ctypes.CDLL(self.path)
        self.struct = _Structs()
        self.called = set()
        self._funcs = {}
        self._calls_log = os.environ.get(CALLS_ENV)

    def has(self, name):
        try:
            getattr(self.dll, name)
            return True
        except AttributeError:
            return False

    def __getattr__(self, name):
        if name.startswith("_") or name not in EXPORTS:
            raise AttributeError(name)
        f = self._funcs.get(name)
        if f is None:
            try:
                raw = getattr(self.dll, name)
            except AttributeError:
                raise MissingExport("%s is not exported by %s" % (name, os.path.basename(self.path)))
            ret, args = EXPORTS[name]
            raw.restype = _SCALARS[ret]
            raw.argtypes = [_SCALARS[a] if a in _SCALARS else self.struct._ctype(a, []) for a in args]
            called, log, path = self.called, self._calls_log, self.path

            def f(*a, **kw):
                if name not in called:
                    called.add(name)
                    if log:
                        with open(log, "a") as fh:
                            fh.write("%s\t%s\n" % (path, name))
                return raw(*a, **kw)
            f.__name__ = str(name)
            f.raw = raw
            self._funcs[name] = f
        return f

    # --- checks that need no knowledge of the codecs ---

    def symbol_report(self):
        """Per symbol: expected exported here? actually exported? Windows exports entrypoints.c + dllexport,
        Linux/macOS exactly entrypoints.c (everything else is localized at link time)."""
        win = target().startswith("win")
        rows = []
        for name, tags in sorted(_spec.SYMBOLS.items()):
            expected = "entrypoints" in tags or (win and "dllexport" in tags)
            rows.append((name, tags, expected, self.has(name)))
        return rows

    def layout_report(self):
        """ctypes' layout of every spec struct vs the compiler's for this target (_spec.LAYOUTS):
        [(name, (size, offsets, widths) here, (size, offsets, widths) per the compiler, or None)]."""
        t = target()
        rows = []
        for name in self.struct.names():
            cls = getattr(self.struct, name)
            fields = [getattr(cls, f) for f, _, _ in _spec.STRUCTS[name][1]]
            got = (ctypes.sizeof(cls), [f.offset for f in fields], [f.size for f in fields])
            want = _spec.LAYOUTS.get(name, {}).get(t)
            rows.append((name, got, (want[0], want[2], want[3]) if want else None))
        return rows

    def alignment(self, name):
        """Alignment the C side requires (e.g. CBlake2sp: 64, MY_ALIGN), which ctypes objects don't get by themselves:
        this target's, or the largest any target has where this one isn't known."""
        cls = getattr(self.struct, name)
        known = _spec.LAYOUTS.get(name, {})
        aligns = [known[target()][1]] if target() in known else [v[1] for v in known.values()]
        return max(aligns + [ctypes.alignment(cls)])

    def alloc(self, name):
        """A zeroed instance of a spec struct at an address with the alignment the C side requires."""
        cls = getattr(self.struct, name)
        align = self.alignment(name)
        buf = ctypes.create_string_buffer(ctypes.sizeof(cls) + align)
        obj = cls.from_buffer(buf, (-ctypes.addressof(buf)) % align)  # from_buffer keeps buf alive
        assert ctypes.addressof(obj) % align == 0
        return obj


def load(path):
    return Library(path)
