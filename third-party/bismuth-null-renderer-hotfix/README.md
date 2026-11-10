# Bismuth null-renderer hotfix (source)

This folder publishes the source-level fix and a guarded binary patcher for the exact Bismuth Windows x64 build supplied with the crash report.

## Target

- Geometry Dash: Windows x64 2.2081
- Geode: 5.11.2
- Bismuth: 0.1.0-alpha (the supplied DLL layout, not every build carrying that version string)

## Crash diagnosis

The crash stack points to `ObjectBatchNode::draw()` followed by `ObjectBatch::draw()`. During layer teardown, the renderer may already be gone while a batch node is still visited. The old code dereferenced `Renderer::get()` without checking it.

## Source fix

Apply `bismuth-null-renderer-hotfix.patch` to the Bismuth source tree. The fixed draw method keeps the returned `Ref<Renderer>` alive for the call and returns early when it is empty:

```cpp
void ObjectBatchNode::draw() {
    auto renderer = Renderer::get();
    if (!renderer)
        return;

    renderer->getObjectBatch().draw(layerId);
}
```

## Binary patcher

`make_bismuth_hotfix.py` applies a small x64 trampoline to the exact DLL layout from the supplied crash-report build. It checks the call bytes and unused code-cave bytes before modifying anything; it exits instead of patching if those checks do not match.

Example (Python 3, standard library only):

```powershell
py make_bismuth_hotfix.py --input "iliashdz.bismuth.geode" --output "bismuth-null-renderer-hotfix.geode"
```

**Do not use the binary patcher on another Bismuth build.** A different compiler build or DLL layout may have different code addresses. For other source revisions, apply the source-level fix and rebuild instead.

## Validation and limits

The supplied hotfix archive was checked for ZIP integrity and the patch bytes were verified against the expected call site/code cave. It has not been runtime-tested inside Geometry Dash. Bismuth is experimental and may still have unrelated renderer crashes.

The prebuilt `.geode` is not stored in this Git branch; this branch publishes the patch source and reproducible patcher. The prebuilt file's SHA-256 is:

`5a7f6c076f9c73107320ff8844ba8cc8a937670e625be2f6d0f9736e2d29061e`

Upstream project: https://github.com/iliasHDZ/Bismuth

License: GPL-3.0-or-later; see `LICENSE.txt`.
