# Native test suite (ctypes, no .NET)

Tests GrindCore's C library directly: all 341 exports, every codec, on all 11 RIDs. It was written for the 2026 audit.
The plan, the results and the findings are in the audit's notes, the `grindcore-workspace` repo (the
`audit/<topic>.md` references in the code are to that repo). They aren't needed to run anything here.

```
python -m gctest --lib <libGrindCore.so | GrindCore.dll | libGrindCore.dylib> [-v] [--refs DIR] [--rid RID]
       [-k pattern,...] [--without input,...] [--json out.json]
```

Run it from this folder, or run `gctest/__main__.py` by its path from anywhere.
- **`python`** in these commands means whichever command runs Python 3.6+ on your machine: `python3`, `python` or
  `py -3`. Check with `--version`. On Windows, `python3` may be the Microsoft Store's stub rather than a Python, and
  Python is often installed without being on PATH (look in `C:\Python3*` or `%LOCALAPPDATA%\Programs\Python`). On
  CentOS 7, `python` is Python 2. The workspace's host scripts find it the same way (`hosts/find-python.sh`).
- **`-k`** takes a comma-separated list and can be repeated; tests whose id contains any of the patterns run (e.g.
  `-k zstd,lzma2_flush` while working on those).
- **`--refs DIR`** finds every reference (below) for the RID: `DIR/out-<rid>/` and `DIR/tb/`, laid out as the
  builders' `--out` writes them (the workspace's `tools/native-test/ref`). A `GC_REF_*` already set wins.
- **`--rid`** names the RID where the process can't tell: `linux-x64-legacy` and `linux-arm64-legacy` look like
  `linux-x64` and `linux-arm64` at run time.
- **A run fails if a test went without an input it needs** (a reference, Brotli's test streams, a Python module:
  `gctest.INPUTS`), whether the test skipped or only left a check out. The exceptions are inputs that can't exist
  for the RID (`gctest.unavailable()`: 7-Zip's assembler decoder off x86-64 Windows and arm64) and those named by
  `--without` (comma-separated; `all` for any). So a reference a run forgot can't pass as a skip.
- **The end of every run lists the skips by kind:** known defects, missing inputs, and any other, with its reason.

## Design rules

- **Standard library only, Python 3.6+** (CentOS 7 and Ubuntu 18.04 ship 3.6). The Python must have the library's
  bitness. Nothing needs installing: Linux RIDs can run in their build images, and on Windows an unpacked
  embeddable Python will do (`.python/` here is git-ignored for that).
- **Portable by design:** every test runs on every RID and against any build configuration of the library.
  - Internal layouts are worked out at run time: a field is found by how it behaves, or a struct definition is
    laid out with ctypes.
  - Debug info, PDBs and disassembly may confirm an answer but are never an input.
  - When something can't be determined, the test skips with a stated reason.
- **Independent oracles:**
  - Python's `zlib`, `bz2`, `lzma` and `hashlib`;
  - official libraries built from pinned sources (below);
  - pure-Python decoders written from the format specs (`oracle_lz4.py`, `oracle_hashes.py`).

  Self-consistency alone isn't trusted. A golden-value test would have missed the BZip2 `FLUSH`/`FINISH` bug, which
  showed up as an error, not wrong bytes.
- **Every test runs on every RID.** A skip is for a known defect, or for something that can't apply to that build
  (an export the shipped binaries lack, a sanitizer build), never for a missing input or a platform the test
  wasn't written for. Inputs go through `gctest.need()` (skips) or `gctest.missing()` (a check left out), never a
  bare `skipTest` or a silent `if ... is not None`.
- **Known defects:** `gctest.KNOWN_DEFECTS` plus `known()`.
  - A library that still has a documented defect (e.g. a release built before the fix) reports it as a skip; a
    fixed library must pass.
  - Probes that crash or hang run in `child()` / `spawn()` subprocesses.
  - `gctest/KNOWN_SYMBOL_GAPS` lists the export-table gaps. If one disappears or a new one appears, the test fails.

## Layout

- `gcnative/` loads the library:
  - prototypes for every export, and `ctypes.Structure` classes for the structs callers allocate;
  - call recording (export coverage), `symbol_report()` and `layout_report()`. The runner ends with
    `export coverage: N of M exports present here were called`, and names the rest when there are 20 or fewer.
    Child processes count too: the runner sets `GCNATIVE_CALLS` to a file, and every library loaded with that set
    appends each export's first call to it before making the call, so a probe that crashes still counts;
  - `alloc()` for structs that need more alignment than ctypes gives (`CBlake2sp`: 64).
- `gcnative/_spec.py` is **generated**; don't edit it. `python gen_bindings.py [--db codemap.db]` writes it from the
  code map (the workspace's `tools/codemap`; by default its `out/codemap.db`, with GrindCore checked out as the
  workspace's submodule) and from `layouts/` (below). `gcnative.OVERRIDES` carries prototypes that changed or were
  added on the audit branch after the code map was made (it predates `ec43222`, 2026-09-26). Running the suite needs
  only the committed `_spec.py`; regenerating it needs the code map.
  - Symbols = `entrypoints.c` ∪ dllexport ∪ GrindCore.net's P/Invokes, each tagged with its source.
  - Types come from the win-x64 and win-x86 maps: whatever differs in width is `size_t`. C's `long` is a type of its
    own (`long`/`ulong`, ctypes' `c_long`): 4 bytes on Windows and 32-bit targets, 8 on 64-bit Linux and macOS
    (`FL2_dictBuffer.size`). Enums declared through a typedef are listed by hand (`ENUM_TYPEDEFS`), and any other
    unknown type stops the generator.
  - Anonymous unions have no record in the code map; `gen_bindings.py` lists them (`ANONYMOUS`), and the layout
    check verifies them.
  - **Layouts per RID:** `layouts/<rid>.json`, one per RID, from `probe_layouts.py` (below). `test_abi`'s
    `test_struct_layouts_match_the_build` compares ctypes' size, field offsets and field widths for every spec struct
    with them, on every RID.
- `gctest/test_*.py` are the tests, and `gctest/*_util.py` drive each codec identically through GrindCore or a
  reference (`zstd_util.drive()`, `brotli_util.Api`, `lzmadec_util.Api`).
- `loadso.c` loads `libGrindCore.so` as a plain C program would (no `-lm`, no `-lpthread`, `RTLD_NOW`):
  `cc -o loadso loadso.c -ldl && ./loadso libGrindCore.so`.
- Other tools:
  - `leakcheck.py`: Valgrind, or `leaks` on macOS;
  - `determinism.py`: hashes compressed output to compare builds and RIDs;
  - `probe_layouts.py`: struct layouts as GrindCore.build compiles them, per RID (below);
  - `bench_*.py`, speed;
  - `fl2_timeout_stress.py`;
  - `arch_limits.py`: the architecture limitations (dictionary limits by pointer width, LZ4's fast-mode levels) as
    lines to diff between RIDs. `--official` compares with official 7-Zip, and `--big` runs the large-input cases;
    `gctest/test_arch_limits.py` pins the cheap facts on every RID;
  - `ref/`: the reference builders, `upstream_tests.py` (below), and `lzma_dict_harness.c`, official 7-Zip's LZMA
    with no Python, for 32-bit measurements with 4 GB of address space (build commands in its header);
  - `fuzz/`: libFuzzer harnesses for the Fast-LZMA2 and 7-Zip decoders, and `fuzz.py`, which builds and runs them
    (below).

## Running it per RID

The audit ran it with the workspace's `tools/native-build` scripts (`hosts/native-tests.sh`), which do the steps
below, keep the references, and run each Linux RID in the image GrindCore.build builds it in.

| RID | Where | How |
|---|---|---|
| linux-* (all 6) | the RID's build environment (GrindCore.build's Dockerfile, or the CI runner) on a host of that architecture | `python3 -B -m gctest -v --refs <refs> --rid <rid> --lib <tree>/artifacts/bin/<rid>/native/libGrindCore.so`, after `loadso.c` |
| osx-arm64, osx-x64 | a Mac | osx-arm64: `/usr/bin/python3`. osx-x64: an x86_64 Python ([python-build-standalone](https://github.com/astral-sh/python-build-standalone)) under `arch -x86_64`, so Rosetta |
| win-x64 | Windows x64 | `python -B gctest\__main__.py -v --refs <refs> --lib …\win-x64\native\GrindCore.dll` |
| win-x86 | Windows x64 | python.org's 32-bit *embeddable* zip, unpacked: `<python>\python.exe -B gctest\__main__.py -v --refs <refs> --lib …` |
| win-arm64 | Windows on ARM | the ARM64 embeddable zip, as for win-x86. win-x64 also runs there under Prism with an ordinary x64 Python |

- **Embeddable Pythons:** `-m gctest` fails with "No module named gctest", because their `._pth` file drops the
  working directory. Run `gctest\__main__.py` instead.
- **Memory caps:** some corruption sweeps allocate a lot when a defect is present. Run Linux containers with `-m 4g`
  (`-m 2560m --memory-swap 2560m` on a 4 GB Raspberry Pi). Never flip seek-table bits in ad-hoc sweeps: on unfixed
  zstd seekable loaders, one bit makes them allocate about 26 GB.
- **Sanitizer builds** (`-fsanitize=address` or `=undefined`): run the suite as usual, with the sanitizer's runtime
  preloaded into Python (`LD_PRELOAD`, plus `PYTHONMALLOC=malloc` and `ASAN_OPTIONS=detect_leaks=0` for ASan).
  - Two tests skip under ASan, saying why: an address-space limit, and a leak test that measures memory, which ASan's
    quarantine inflates. They use `gctest.address_sanitizer()`.
  - Crash probes in child processes recognise a sanitizer's `DEADLYSIGNAL` exit.
  - The workspace's `tools/native-build` does all of this with `SAN=1 hosts/native-tests.sh`.

## Struct layouts per RID (`probe_layouts.py`)

`layouts/<rid>.json` hold each spec struct's layout as GrindCore.build compiles it for that RID: size, alignment, and
every field's offset and width. `probe_layouts.py` measures them from the compile commands GrindCore.build's own
CMake writes (`<tree>/artifacts/obj/native/<rid>/compile_commands.json`). For each C unit that includes a spec
struct's header, it compiles a probe with that unit's exact command: the unit's preprocessor lines, then a table of
`sizeof`/`offsetof` values. It reads the table back out of the object file. Nothing is linked or run, and nothing is
written into the build. All units that see a struct must agree.

- **This is a check, not a build step.** The build never runs it or reads `layouts/`. It confirms that the bindings
  match what GrindCore.build's compilers and flags produce on every RID (MSVC's `/Zp8`, i386's 4-byte alignment of
  64-bit fields, `long`'s width). If a flag ever changes a layout, the fix belongs in GrindCore.build's CMake or in
  the code.
- **Rerun it** when a spec struct's header changes, or GrindCore.build's compiler or flags change. Run it for each
  RID in that RID's build environment, with the tree at the path it was built at (the commands hold absolute paths):

  ```
  python probe_layouts.py --cc-json <tree>/artifacts/obj/native/<rid>/compile_commands.json --rid <rid>
  python gen_bindings.py
  ```

  On Windows it sets up MSVC from the commands' own `cl.exe` (that Visual Studio, toolset version and host/target
  pair). The workspace's `tools/native-build/hosts/layout-probe.sh` runs it for each RID in its build image.
- `gen_bindings.py` stops if two RIDs that share a run-time name (`linux-x64` and `linux-x64-legacy`) disagree. It
  also stops if the win-x64 or win-x86 numbers differ from clang's in the code map.

## References (official libraries, built from pinned sources)

Every RID has all of them (2026-10-17). `--refs` sets the variables; each can also be set by hand.

| Variable | Builder | What |
|---|---|---|
| `GC_REF_ZLIB` (folder) | `ref/build_zlib_ref.py` | zlib 1.3.1 and zlib-ng 2.2.1 (its own CMake, with GrindCore's options: compat mode, no ARMv6/RVV) |
| `GC_REF_ZSTD` (folder) | `ref/build_zstd_ref.py` | zstd 1.5.2 and 1.5.7 plus `contrib/seekable_format`, from the SHA-256-checked tarballs |
| `GC_REF_LZ4` (folder) | `ref/build_lz4_ref.py` | LZ4 1.10.0 |
| `GC_REF_BROTLI` (folder, or several with `os.pathsep`) | `ref/build_brotli_ref.py` | Brotli 1.1.0 and its 44 test streams. `--fixtures-only` gives the streams without a library |
| `GC_REF_FL2` (folder) | `ref/build_fl2_ref.py` | Fast-LZMA2 1.0.1 and master `967306d3`; `--asm` adds the x86-64 assembler decoder |
| `GC_REF_7ZIP` (file) | `ref/build_7zip_ref.py` | 7-Zip 25.01's LZMA/LZMA2; `--asm` adds the assembler decoder (win-x64, arm64) |

- Each builder writes to `--out ref/out-<rid>` (git-ignored) and downloads its archive, checking the SHA-256.
  `build_7zip_ref.py --cflags=...` adds compiler options, e.g. `--cflags=-DLZMA_LOG_BSR`.
  `--tarballs <folder>` (and `--src <folder>/7zip-25.01`) use archives already on disk, for hosts without network.
- **Build the references for each architecture, in the RID's own build environment.** Brotli's Q10/Q11 output
  depends on the compiler for some inputs, so its builder (and Fast-LZMA2's, to match) picks the compiler as
  GrindCore.build's `init-compiler.sh` does: the newest `clang-N`.
- **linux-x86:** build the references with `--cflags="-msse2 -mfpmath=sse"`, as GrindCore.build builds linux-x86.
  x87 references differ in Brotli Q10/11 and zstd 1.5.2 L5/L9.
- **linux-arm:** `build_zlib_ref.py` builds the official zlib-ng with gcc there. Built with clang (14 through 19
  measured), the release faults on 32-byte-aligned NEON loads in its Adler-32, which GrindCore's own build avoids
  with `external/dotnet9_0/zlib-ng_arm32_neon_ld4.h`. The compiler doesn't change what deflate writes.
- **Legacy (CentOS 7):** every reference builds in the legacy images (references built elsewhere need a newer glibc).
  Brotli's, Fast-LZMA2's, zlib's and zstd's use the images' clang 7.0.1 (`/opt/rh/llvm-toolset-7.0`), which builds
  GrindCore there. zstd's needs it on the arm64 one: CentOS's gcc 4.8.5 doesn't define `__ARM_NEON`, so zstd 1.5.2
  would take its scalar row matcher, whose level-5 and level-9 output differs. For 7-Zip's on the arm64 one, pass
  `--cc /opt/rh/devtoolset-8/root/usr/bin/gcc`: gcc 4.8.5 can't compile 7-Zip's `CpuArch.c`, and devtoolset-11 is
  empty.
- **Windows:** every builder uses MSVC through vswhere (`ref/msvc.py`; `--arch x64/x86/arm64`, cross-compiling arm64 on
  an x64 host, as GrindCore's win-arm64 build is). DLLs export, through a `.def`, exactly the functions the tests bind
  (`REF_FUNCTIONS` in `gctest/zstd_util.py`, `lz4_util.py`, `brotli_util.py` and `test_zlib.py`); Fast-LZMA2's header
  and zlib-ng's CMake export their APIs themselves. Brotli's is compiled with the options GrindCore's build gives it
  (`/O2 /Ob2 /Ox /Oi /fp:precise /GL`), for the compiler-dependent Q10/Q11 output.

## The vendored libraries' own test suites (`ref/upstream_tests.py`)

Each library's release tree supplies its tests, and GrindCore's vendored copy is laid over the release's sources, so
the tests run against the code GrindCore ships, compiled with the compiler and flags of a real GrindCore build (read
from that build's `compile_commands.json`). Run it in the RID's build environment, with the build tree where it was
built (the paths in `compile_commands.json` are absolute):

```
python3 ref/upstream_tests.py --src <tree>/src/native --rid <rid> --work <dir>
        --cc-json <tree>/artifacts/obj/native/<rid>/compile_commands.json [--tarballs <folder>] [--time 60]
```

The workspace's `hosts/upstream-tests.sh` does that for each Linux RID in its build image, and natively on a Mac.

| Suite | What runs | Notes |
|---|---|---|
| zstd 1.5.7 | `fuzzer`, `zstreamtest` (`--time` seconds each) | GrindCore's `lib/` |
| zstd 1.5.7 seekable | `seekable_tests` | GrindCore's renamed API is mapped to the plain names for the test. Test 4 reports the known end-of-stream difference (other-codecs.md §4.8.1) |
| zstd 1.5.2 | `fuzzer`, `zstreamtest` | the release's own `lib/` with GrindCore's 1.5.2 defines: GrindCore's copy is renamed `ZSTD_v1_5_2_*` |
| LZ4 1.10.0 | `fuzzer`, `frametest` | |
| Brotli 1.1.0 | the round-trip and compatibility scripts | with `BROTLI_ENCODER_CLEANUP_ON_OOM` |
| zlib-ng 2.2.1 | `ctest`, with googletest when it builds | zlib-ng's own CMake, with GrindCore's options, so its probes decide as in GrindCore's build. On 32-bit, `gzip.readwrite` fails as it does in official zlib-ng with `-D_FILE_OFFSET_BITS=64` (the gz file API, which GrindCore doesn't export) |
| zlib 1.3.1 | `example`, a `minigzip` round trip | the release's `gz*` files, which GrindCore doesn't build |
| BZip2 1.0.8 | `make test` | without `BZ_NO_STDIO`, which removes the file API the command-line tool needs |
| Fast-LZMA2 1.0.1 | `fuzzer -m<threads>` | upstream's fuzzer divides by zero with its default thread count of 0 |
| BLAKE3 0.3.7 | `test.py` with the official vectors | built once per SIMD level (dispatch, SSE4.1, SSE2, portable on x86; NEON and portable on ARM64) |

7-Zip's C sources ship no test suite; the native suite compares GrindCore with official 7-Zip instead. Windows isn't
covered (the suites are make- and shell-based); the native suite covers Windows.

## Memory checking

- **Linux:** `leakcheck.py` runs the suite plus leak scenarios under Valgrind memcheck and reports only findings with a
  GrindCore frame (`--save` keeps Valgrind's full output).
  - Use Valgrind 3.22+, which means `ubuntu:24.04`: 3.18 can't read the clang-15 builds' DWARF 5.
  - On ARM32, use Debian 12; Python 3.13 (Debian 13) segfaults under Valgrind at startup.
  - `-k <codec>,...` limits it to those tests. `--refs`/`--rid` work as for the suite, so the tests that compare
    with official libraries run under Valgrind too (and with them the GrindCore code they call).
  - Valgrind doesn't follow child processes, so the corrupt-input sweeps have `--scenario` equivalents, e.g.
    `--scenario lzma_decoders`.
- **macOS:** `leaks`, run on the framework's real Python binary, because the Command Line Tools' `python3` is a
  launcher and `leaks` would see nothing.
- **When:** the codec being changed before each commit; the whole suite once per codec or phase.

## Fuzzing (`fuzz/`)

Time-boxed libFuzzer runs of a decoder under AddressSanitizer, on Linux:

```
python3 fuzz/fuzz.py fl2  --minutes 30      # Fast-LZMA2: one-shot, 2 threads, and the stream
python3 fuzz/fuzz.py lzma --minutes 20      # 7-Zip's LZMA and LZMA2 decoders: one-shot and streamed
```

- **What it builds:** each harness (`fuzz/<target>_decode.c`) is compiled with GrindCore's own copy of that
  library's sources, as GrindCore's CMake lists them, at `-O1 -g`. Same source, so the same bugs; not GrindCore's
  machine code.
- **Seeds and output:** the seeds are valid streams from Python's `lzma`. Everything goes to `fuzz/out/`
  (git-ignored): the corpus, which later runs continue from, and findings as `<target>-crash-*`, `-leak-*` and
  `-timeout-*`. Replay one with `fuzz/out/<target>_decode <file>`.
- **Where:** the linux-x64 build image has clang 15 with libFuzzer. Run the container with
  `--security-opt seccomp=unconfined`, so `fuzz.py` can turn off address randomisation (`setarch -R`); otherwise
  clang 15's ASan crashes at start in about 3 runs of 10.
- **What counts:** crashes, writes past a buffer, uses after free, leaks and hangs.
  - Reads are checked too. Before 2026-10-24, Fast-LZMA2 was fuzzed with ASan's read checks off: its decoder's
    over-read on corrupt input (since fixed) stopped every run within seconds. `--writes-only` does that for a tree
    without the fix.
  - Inputs that declare a dictionary over 16 MiB are capped, since allocating what a stream declares is the
    format, not a defect.
- The other decoders (zstd, LZ4, Brotli, zlib, bzip2) aren't here: they are fuzzed continuously upstream.
- **`fuzz/pool_race.py`** is a deterministic check of Fast-LZMA2's thread-pool wait (2026-10-24), which a fuzz run
  caught letting a multithreaded decode go on while a worker was just starting its job. The window is two memory
  loads wide, so it builds `fl2_pool_race.c` against a copy of `fl2_pool.c` with a delay in that window (the
  vendored file isn't touched), then decodes and compresses valid data with 2 threads under ASan: a pool with the
  bug crashes in both, a fixed one passes. `python3 fuzz/pool_race.py`, on Linux, as `fuzz.py`.

## Harness lessons (each one cost a debugging session)

- **ctypes:** a closure must hold the buffer *object*, not just its address, or Python frees it and the library reads
  freed memory. That access violation looks like a decoder bug. On Windows, ctypes turns a native access violation into
  `OSError: exception: access violation`, so crash probes check for that as well as a signal.
- **Calling conventions:** on win-x86, callbacks the vendored code declares without a convention are `__stdcall`
  (`/Gz`): zstd seekable's read/seek, Brotli's allocator, and 7-Zip's `ISeqInStream`/`ISeqOutStream`/
  `ICompressProgress`. Probe per library (`WINFUNCTYPE` vs `CFUNCTYPE`). The official references are built with
  MSVC's default (`/Gd`), so theirs are `__cdecl`.
- **zstd:**
  - `DecompressStream` holds back a frame's last input byte until the output is flushed, then returns 1 for it, so
    a drain loop must re-supply the unconsumed tail;
  - the setters return the value actually set (level 0 reports 3);
  - a destination exactly the compressed size can fail.
- **LZ4:** block output depends on memory layout: a block contiguous with the previous one continues it. Keep linked
  blocks in one buffer, or output differs from official.
- **Brotli:**
  - the decoder can return `NEEDS_MORE_INPUT` with output still pending, so repeat while the output buffer comes
    back full;
  - the encoder `exit()`s on a failed allocation in Brotli's default mode;
  - allocation-failure sweeps need one-off failures as well as "fail from here on". The latter let a later check
    hide the missing one.
- **Fast-LZMA2:**
  - a double-buffered stream's `FL2_flushStream` returns 0 before the output exists, so wait (`FL2_waitCStream`)
    and drain again;
  - that mode's output varies with timing, so compare it by decoding, not byte for byte;
  - `FL2_CONTENTSIZE_ERROR` is `(size_t)-1`, which is `0xFFFFFFFF` on 32-bit.
- **LZMA:** with a known size, a streaming decoder must keep being called while the status is `NEEDS_MORE_INPUT`, even
  at size 0, because the range coder needs its 5 start bytes.
- **unittest:** never `assertEqual` large `bytes`. unittest builds the full text diff before truncating it, which took
  20+ minutes on the Pi and looked like a hang. Use `self.same` / `describe`.
- **ThreadSanitizer** on a ctypes-loaded library: gcc `-fsanitize=thread`, `LD_PRELOAD` of `libtsan.so`, and
  `setarch x86_64 -R`. In Docker it also needs `--security-opt seccomp=unconfined`.
