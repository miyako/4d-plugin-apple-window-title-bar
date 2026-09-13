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
