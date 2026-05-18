# ifel-test

Small C++ + Slint 1.10 reproducer for debugging nested
`slint::invoke_from_event_loop`.

## Flow

Pressing a button:

1. C++ spawns a `std::thread`.
2. The thread sleeps for 2 seconds.
3. The thread calls `slint::invoke_from_event_loop` with the **outer** lambda.
4. The outer lambda (on the UI thread) invokes the Slint-declared callback
   `nested_step(string, int)`.
5. The C++ handler bound to `nested_step` (via
   `ComponentInstance::set_callback`) itself calls
   `slint::invoke_from_event_loop` with the **inner** lambda — the nested case
   under investigation.
6. The inner lambda updates the window's status text via
   `ComponentInstance::set_property`.

Every stage prints `std::this_thread::get_id()` to stdout so you can confirm
the thread hops.

## Runtime-loaded UI

`ui/main.slint` is **not** compiled into the binary. The C++ uses
`slint::interpreter::ComponentCompiler::build_from_path` to parse and
instantiate the UI at runtime. The default `.slint` path is baked in via the
`SLINT_FILE_PATH` compile definition; pass an alternative path as `argv[1]` to
override it:

```sh
./build/ifel-test                       # uses the baked-in default
./build/ifel-test path/to/other.slint   # override
```

## Build

```sh
cmake -B build -S .
cmake --build build -j
./build/ifel-test
```

CMake first tries `find_package(Slint 1.10)` and uses an installed Slint if
one is on `CMAKE_PREFIX_PATH`. If not, it falls back to building Slint 1.10
from source via `FetchContent` (~2 min the first time). The fallback path
pins `SLINT_FEATURE_BACKEND_WINIT=ON` and `SLINT_FEATURE_BACKEND_QT=OFF` so
behaviour stays identical across machines.

### Rust 1.95+ compatibility

`.cargo/config.toml` at the repo root demotes the
`dangerous_implicit_autorefs` lint to a warning. The lint became
deny-by-default in Rust 1.95 and trips Slint 1.10's `slint-interpreter`
crate. Cargo discovers the config by walking upward from the workspace dir
under `build/_deps/slint-src/` — only relevant for the FetchContent fallback
path.
