# Sleek WinTabs

Part of the [Sleek](https://github.com/SleekGPX) tab-tooling line — this one
targets Windows 11 File Explorer's native tabs specifically (as opposed to
tabs in other apps, which is why this repo is named for Explorer rather than
just "tabs").

A [Windhawk](https://windhawk.net/) mod for Windows 11's native File Explorer
tabs. Two independent tweaks, each with its own on/off setting:

- **Show folder name instead of full path** — Explorer tabs can get stuck
  showing the full path (`C:\Users\Name\Documents\Projects`) instead of just
  the folder name (`Projects`), most noticeably on tabs opened in the
  background via middle-click or "Open in new tab." This rewrites the title
  down to the last path component before it's displayed.
- **Jump to newly opened tabs** — by default, a tab opened via middle-click
  (or "Open in new tab") opens silently in the background. This switches to
  it as soon as it's created, the same way Explorer's own "+" new-tab button
  does.

Both are on by default and can be toggled independently in the mod's
settings.

## Install

1. Install [Windhawk](https://windhawk.net/) if you don't have it.
2. In the Windhawk app, choose to create/paste a new custom mod.
3. Paste in the contents of [`sleekwintabs.wh.cpp`](sleekwintabs.wh.cpp).
4. Save and enable it.

## How it works

Both features hook internal (undocumented) functions in
`explorerframe.dll` — `CExplorerFrame::SetFrameTitle`, `AddTab`,
`AddTabAtIndex`, and `SelectTab` — resolved by symbol name against
Microsoft's public debug symbols, the same mechanism Windhawk uses for all
of its mods. This means the mod is tied to the internal shape of File
Explorer on your Windows build; if a future Windows update changes these
functions, Windhawk will simply fail to find the symbol and log an error —
Explorer keeps running normally, nothing crashes. If that happens, open an
issue here and it can be re-checked against the new build.

Built and tested against Windows 11 build 26100.9278.

## License

MIT — see [LICENSE](LICENSE).
