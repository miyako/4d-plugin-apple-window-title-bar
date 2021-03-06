![version](https://img.shields.io/badge/version-16%2B-8331AE)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-apple-window-title-bar)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-apple-window-title-bar/total)

**Note**: for v17 and earlier, move `manifest.json` to `Contents`

for v19 consider [4d-plugin-window-style](https://github.com/miyako/4d-plugin-window-style)

# 4d-plugin-apple-window-title-bar
Make window content view display behind title bar

<img width="300" alt="screenshot" src="https://user-images.githubusercontent.com/1725068/40612708-9c5f764e-62b6-11e8-9050-33201bcdc68b.png">

## Syntax

```
HIDE WINDOW TITLE BAR (window)
```

Parameter|Type|Description
------------|------------|----
window|LONGINT|

## Related projects

other ways to customise a window on Mac

* https://github.com/miyako/4d-plugin-custom-window

* https://github.com/miyako/4d-plugin-window-icon

* https://github.com/miyako/4d-plugin-window-button

## Remarks

broken in v19. the window title bar becomes transparent but the background does not extend behind it.

<img width="247" alt="スクリーンショット 2021-07-16 7 21 19" src="https://user-images.githubusercontent.com/1725068/125865429-a387c198-0c77-491d-92bc-ba48aa7694a6.png">

