![version](https://img.shields.io/badge/version-16%2B-8331AE)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-apple-window-title-bar)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-apple-window-title-bar/total)

# 4d-plugin-apple-window-title-bar

This plugin lets a 4D window's content view display underneath its title bar, giving the window a borderless, unified look instead of a separate opaque title bar strip. It works by driving Cocoa's `NSWindow` API directly — setting the window's style mask to `NSWindowStyleMaskFullSizeContentView` (combined with `NSWindowStyleMaskBorderless` and `NSWindowStyleMaskUnifiedTitleAndToolbar`), then making the title bar transparent (`setTitlebarAppearsTransparent:`) and the window background clear. It exposes a single command and has no return value — it mutates the target window in place.

**Known issue:** on 4D v19, the title bar becomes transparent as expected, but the content view does not extend to fill the space behind it, leaving a visual gap. This is an upstream/version-specific rendering behavior, not something you can work around from the 4D side — see [4d-plugin-window-style](https://github.com/miyako/4d-plugin-window-style) for an alternative on v19+.

| Command | Returns | Purpose |
|---|---|---|
| [HIDE WINDOW TITLE BAR](#hide-window-title-bar) | (none) | Makes the target window's title bar transparent and lets its content extend behind it. |

**Platforms:** macOS only (Intel and Apple Silicon). There is no Windows implementation.

---

## Requirements & platform notes

- Requires macOS 10.10 (Yosemite) or later — the plugin only acts if the window responds to `setTitlebarAppearsTransparent:`, which is when Cocoa introduced full-size-content-view windows. On any earlier system, the command silently does nothing.
- Takes exactly one mandatory parameter (the window reference); there is no optional form.
- The command is idempotent — if the title bar is already transparent, calling it again is a safe no-op rather than re-applying (or breaking) the style.
- There is no companion command to reverse the effect (no "SHOW WINDOW TITLE BAR"). Once applied, undoing it requires closing/reopening the window or restoring the style mask yourself via another means.
- For v17 and earlier, move `manifest.json` into `Contents` per the plugin's packaging instructions for that version.

---

## HIDE WINDOW TITLE BAR

### Syntax

```4d
HIDE WINDOW TITLE BAR (window)
```

| Parameter | Type | Description |
|---|---|---|
| `window` | Longint | The window reference of the target window (the value returned by 4D's window-opening commands, e.g. `Open window`). Mandatory — the command reads it unconditionally. |
| Result | — | No return value. |

### Description

The command resolves `window` to the underlying native window and, if it responds to the transparent-title-bar API:

- Applies `NSWindowStyleMaskBorderless | NSWindowStyleMaskUnifiedTitleAndToolbar | NSWindowStyleMaskFullSizeContentView` on top of the window's existing style mask (existing style flags are preserved, not replaced).
- Sets the window to non-opaque with a clear background and a transparent title bar, so the content view shows through into what was previously the title bar area.

If `window` doesn't resolve to a real window (an invalid/stale reference), the command silently does nothing — no 4D error is raised. Likewise, if the title bar is already transparent, the command exits without reapplying anything.

### Example

```4d
$wRef:=Open window(50;50;400;300;Movable window with title bar)
HIDE WINDOW TITLE BAR($wRef)
```

*Illustrative — the plugin ships no working test method for this command (`TEST.4dm` is an empty stub in this project), so this example is built from standard 4D windowing usage rather than quoted from a provided sample.*

```4d
// Apply to every open window
ARRAY LONGINT($wRefs;0)
WINDOW LIST($wRefs)
For ($i;1;Size of array($wRefs))
	HIDE WINDOW TITLE BAR($wRefs{$i})
End for
```

---

## Error handling & troubleshooting

- **Nothing visibly happens.** Either the window reference didn't resolve to a real window, or the OS is older than 10.10 and doesn't support transparent title bars — both cases fail silently with no 4D error.
- **Calling it twice has no extra effect.** The plugin checks `titlebarAppearsTransparent` before acting, so a second call on the same window is a no-op, not a re-application.
- **On v19, a visible gap appears behind the title bar.** This is a known, version-specific issue (per the project's own README) where the content view doesn't extend to fill the transparent area; it isn't something this command can currently correct.
- **There's no way to undo the effect via this plugin.** Plan for that at the point you call the command — e.g. only call it on windows you're comfortable leaving in this style for their lifetime.

---

## Quick reference

```4d
$wRef:=Open window(50;50;400;300;My window)
HIDE WINDOW TITLE BAR($wRef)
```

## Manual source versioning and releases

**Bump version** and **Build** are independent, manual Actions. Bump offers
`patch`, `minor`, and `major`; Build has **no typed inputs**, only GitHub's ref
selector. Both check out the immutable selected `github.sha`. Bump accepts only
branches and produces one source-metadata commit with the Copilot coauthor trailer.
It never creates a tag, release, build, or workflow dispatch. Select the resulting
branch commit explicitly when running Build.

The existing authority is **coordinated source metadata**, not tags or prebuilt
bundles. The original committed Bump header at
[`bb39ecacdd433cc1986f4a87c6ce254eab416d6a`](https://github.com/miyako/4d-plugin-apple-window-title-bar/commit/bb39ecacdd433cc1986f4a87c6ce254eab416d6a)
explicitly promises to update `MARKETING_VERSION/CURRENT_PROJECT_VERSION AND
Info.plist` (`CFBundleShortVersionString/CFBundleVersion`). That explicit contract
overrides its buggy marketing-first implementation. Rename-follow history finds
that workflow's introduction at the same commit, with no later policy change.
The existing six marketing assignments are **1.1.2** at intake; only the six stale
`CURRENT_PROJECT_VERSION` assignments and the two stale localized version tokens
are repaired to that existing value. This migration does **not** increment it.
`Info.plist` retains `$(MARKETING_VERSION)` and its absent optional
`CFBundleVersion` remains absent.

The source allowlist is exactly:

| Source | Contract |
|---|---|
| `Apple window title bar.xcodeproj/project.pbxproj` | Six assignments each for marketing/current; strict coordinated `X.Y.Z`, with supported marketing references preserved |
| `Info.plist` | Required short version; optional existing build version; supported `$(...)` / `${...}` references preserved |
| `English.lproj/InfoPlist.strings` | Short version and the version token in the existing copyright/display string |

Validation rejects leading zeros, suffixes, drift, unresolved/cyclic references,
duplicate plist/localization keys (including quoted or inline keys), and extra
or noneditable project assignments before any writes. Comments and quoted-string
noise are not assignments. Rewrites preserve BOM, individual LF/CR/CRLF endings
(including mixed files), modes, references, and unrelated bytes.
Prebuilt bundle metadata/localization, native binaries, dependencies, archives,
manifests, constants, and 4D data/schema/localization counters are excluded.

Bump requires a clean checkout and the selected branch's exact remote SHA both
before and after preparation. Its mandatory executable pre-push guard checks the
server-advertised old SHA/ref, literal new commit SHA and HEAD, exactly one direct
parent, and exactly one advertised update. Rewinds, deletions, advances, and even
a concurrent identical-result update abort. The nonforce, explicit branch push
uses both `push.followTags=false` and `--no-follow-tags`.

### Build and publication safety

The actual original workflow is **macOS only**, despite its old Windows header.
No Windows job, MSBuild toolset change, dependency build, branch restriction, or
new architecture-count/i386 restriction is introduced. It retains the `Default`
target configuration, `arm64 x86_64`, `ONLY_ACTIVE_ARCH=NO`, deployment override
`11.0` (the unchanged project settings are `11.5`), Cocoa linkage, existing signing
commands, DMG stapling, and all six existing secret **names**:
`APPLE_DEVELOPER_ID_CERTIFICATE`, `APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD`,
`KEYCHAIN_PASSWORD`, `NOTARYTOOL_APPLE_ID`, `NOTARYTOOL_TEAM_ID`,
`NOTARYTOOL_PASSWORD`.

Build selectively invalidates the exact generated Mac executable, built plist,
built English localization, signature, and known Default/product intermediates.
It preserves Windows64, QuickLook, resources, and dependencies; deletion failures
abort. An explicit `CONFIGURATION_BUILD_DIR` targets `test/Plugins`, preventing
Default configuration output from landing in a sibling `Default` directory while
packaging a stale root bundle. No project setting or signing override is changed.
Xcode's actual Resources phase copies `English.lproj/InfoPlist.strings`;
its CopyFiles phase uses `dstSubfolderSpec = 7` for `manifest.json` and
`constants.xlf` (bundle `Contents/Resources`, not `Contents` root).
The checked-in destination localization is **not source** and is left unchanged
by this migration and Bump. Build must rebuild it and validate it against source,
including Xcode UTF-16 or binary-plist output. The built plist must also resolve
to the source version. Required `arm64` and `x86_64` slices must match complete
tokens, not `arm64e` / `x86_64h`. Additional slices remain permitted, as before.
Both notarization submissions must exit successfully **and** return `Accepted`
JSON before stapling and publication.

All branches share the constant `release-publication` lock with cancellation
disabled. Before native work, and again after all build/sign/notary/staple gates,
Build checks the exact remote tag and authenticated, fully paginated release
lists, including drafts. HTTP/auth/network failures, malformed responses, and
later-page failures abort, never mean "available."
Publication is create-only: POST a new `refs/tags/vX.Y.Z` at the selected SHA,
verify the remote SHA, then POST a separate new public, non-prerelease release
with explicit tag/name/target/body/draft flags. As before, notes/body are empty.
Assets are uploaded only to that newly returned release ID. There is no reuse,
PATCH, DELETE, clobber, retry, rollback, or combined release-with-assets command.

**Manual recovery:** any failure after creation retains the tag, release, and
already uploaded assets. Read the failed step, inspect the remote tag's exact SHA,
all draft/published releases for that tag, the returned release ID, and asset
state. Do not rerun Build expecting it to reuse or overwrite partial publication.
A maintainer must explicitly decide how to resolve retained partial state outside
these workflows, or source-bump to a new version and build that commit.
Concurrent tag creation (even at the same SHA) is a hard failure, not success.

### Offline verification and pinned prior art

Run `actionlint` and
`PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s .github/tests -v`.
The full suite also runs unchanged after a genuine guarded helper bump in a
disposable local Git/bare-origin copy. Fixture versions derive from source.
Tests exercise real local branch advance/rewind/delete/same-result races after
the final read, missing/nonexecutable initial/late hooks, parent/update counts,
source-only commits, trailers, no-follow-tags, metadata bytes/exclusions,
publication pagination/drafts/errors/races/payloads/partial retention, and literal
native workflow blocks with a closed PATH of stubs. No actual native build,
signing, notarization, 4D execution, Actions dispatch, or production publication
is needed. No Windows workflow block exists here to execute or stub.

Adapted from the full exact committed workflows, helpers, and tests at:

- [native-encoding-detection@c42ca565857f674bde5ea6ed3435c87375eda4a7](https://github.com/miyako/4d-plugin-native-encoding-detection/tree/c42ca565857f674bde5ea6ed3435c87375eda4a7/.github)
- [file-information@19848973601acfd3a2b5070230fa40cdc7e4edb2](https://github.com/miyako/4d-plugin-file-information/tree/19848973601acfd3a2b5070230fa40cdc7e4edb2/.github)
- [common-string-algorithm@2606a68201866ec9fcbb38b51462d79215f504ef](https://github.com/miyako/4d-plugin-common-string-algorithm/tree/2606a68201866ec9fcbb38b51462d79215f504ef/.github)
- [window-button@3f14facb9aaaaff0e22eeb8e07531163680c3a8a](https://github.com/miyako/4d-plugin-window-button/tree/3f14facb9aaaaff0e22eeb8e07531163680c3a8a/.github)
- [raw-picture-data@8144537fdc8dcc3d0c053dfe97aff0a8c0c3a6f9](https://github.com/miyako/4d-plugin-raw-picture-data/tree/8144537fdc8dcc3d0c053dfe97aff0a8c0c3a6f9/.github) (lexical duplicate correction)
- [PRNG@522f62e1e9f388be0f3f226c1d4c39362dad2491](https://github.com/miyako/4d-plugin-PRNG/tree/522f62e1e9f388be0f3f226c1d4c39362dad2491/.github) (comment-safe scoped rewrite)

The coordinated policy above is repository-specific; marketing-only/fallback
policies in other projects must not be used to infer coordination of independent
build numbers.
