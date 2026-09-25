// ==WindhawkMod==
// @id              sleek-tabs
// @name            Sleek Tabs
// @description     File Explorer tab tweaks: folder name instead of full path, and jump to newly opened tabs (each independently toggleable)
// @version         1.1.0
// @author          SleekGPX
// @github          https://github.com/SleekGPX/sleek-windhawk-explorer-tabs
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# SleekTabs

Two independent File Explorer tab tweaks. Each has its own setting and can be
turned off without affecting the other.

## Show folder name instead of full path

Windows 11's native File Explorer tabs can get stuck showing the full folder
path (e.g. `C:\Users\Name\Documents\Projects`) instead of just the folder
name (`Projects`) — most noticeably on tabs opened in the background via
middle-click, or via "Open in new tab" from the context menu.

This hooks the internal call File Explorer uses to set a tab's title text
(`CExplorerFrame::SetFrameTitle`) and, whenever the incoming text looks like
a literal filesystem path, rewrites it down to just its last path component
before Explorer displays it.

Virtual/special locations (This PC, Search, Quick access, a drive's friendly
name, etc.) don't contain a path separator and pass through unchanged, since
Explorer already gives those their correct display name.

## Jump to newly opened tabs

By default, a tab opened via middle-click (or "Open in new tab") opens in
the background without switching to it. This hooks the tab-creation calls
(`CExplorerFrame::AddTab` / `AddTabAtIndex`) and, once the new tab exists,
immediately switches to it via the same `SelectTab` call Explorer itself
uses for e.g. the "+" new-tab button.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- fixTabTitles: true
  $name: Show folder name instead of full path
  $description: >-
    Rewrites a tab's title down to just the folder name whenever Explorer
    tries to set it to a full filesystem path
- selectNewTabs: true
  $name: Jump to newly opened tabs
  $description: >-
    Switches to a tab as soon as it's created (e.g. via middle-click or
    "Open in new tab"), instead of leaving it open in the background
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>

#include <string>
#include <string_view>
#include <vector>

struct {
    bool fixTabTitles;
    bool selectNewTabs;
} g_settings;

// -----------------------------------------------------------------------
// Feature 1: show folder name instead of full path
// -----------------------------------------------------------------------

std::wstring ComputeDisplayTitle(PCWSTR title) {
    std::wstring_view sv(title);

    // Trim a single trailing separator, e.g. "C:\" or "D:\Foo\".
    while (sv.size() > 1 && (sv.back() == L'\\' || sv.back() == L'/')) {
        sv.remove_suffix(1);
    }

    size_t pos = sv.find_last_of(L"\\/");
    if (pos == std::wstring_view::npos || pos + 1 >= sv.size()) {
        // No separator (already a plain display name) or nothing left after
        // it (a bare drive/share root) - leave as-is.
        return std::wstring(sv);
    }

    return std::wstring(sv.substr(pos + 1));
}

using CExplorerFrame_SetFrameTitle_t = void(WINAPI*)(void* pThis,
                                                     PCWSTR title,
                                                     GUID tabId);
CExplorerFrame_SetFrameTitle_t CExplorerFrame_SetFrameTitle_Original;
void WINAPI CExplorerFrame_SetFrameTitle_Hook(void* pThis,
                                              PCWSTR title,
                                              GUID tabId) {
    Wh_Log(L"SetFrameTitle: %s", title ? title : L"(null)");

    if (g_settings.fixTabTitles && title && *title) {
        std::wstring newTitle = ComputeDisplayTitle(title);
        if (newTitle != title) {
            Wh_Log(L"Rewriting to: %s", newTitle.c_str());
            CExplorerFrame_SetFrameTitle_Original(pThis, newTitle.c_str(),
                                                  tabId);
            return;
        }
    }

    CExplorerFrame_SetFrameTitle_Original(pThis, title, tabId);
}

// -----------------------------------------------------------------------
// Feature 2: jump to newly opened tabs
// -----------------------------------------------------------------------

using CExplorerFrame_SelectTab_t = HRESULT(WINAPI*)(void* pThis, GUID tabId);
CExplorerFrame_SelectTab_t CExplorerFrame_SelectTab_Original;

// Selecting the new tab immediately, inside AddTab's own call, doesn't stick:
// Explorer's own tab-creation flow runs more setup right after AddTab returns
// (finishing the view's async navigation, etc.), and that appears to put
// focus back on the previous tab. Deferring the SelectTab call to the next
// message loop tick, after that settles, is what actually makes it stick.
struct PendingSelect {
    void* pThis;
    GUID tabId;
};
std::vector<PendingSelect> g_pendingSelects;
constexpr UINT_PTR kSelectTabTimerId = 0xBEEF;
constexpr UINT kSelectTabDelayMs = 150;

void CALLBACK SelectTabTimerProc(HWND, UINT, UINT_PTR, DWORD) {
    KillTimer(nullptr, kSelectTabTimerId);

    std::vector<PendingSelect> pending = std::move(g_pendingSelects);
    g_pendingSelects.clear();

    for (const auto& p : pending) {
        if (CExplorerFrame_SelectTab_Original) {
            Wh_Log(L"Deferred SelectTab firing");
            CExplorerFrame_SelectTab_Original(p.pThis, p.tabId);
        }
    }
}

void ScheduleSelectTab(void* pThis, GUID tabId) {
    g_pendingSelects.push_back({pThis, tabId});
    SetTimer(nullptr, kSelectTabTimerId, kSelectTabDelayMs, SelectTabTimerProc);
}

using CExplorerFrame_AddTab_t = HRESULT(WINAPI*)(void* pThis,
                                                 const void* pidl,
                                                 int entryPoint,
                                                 GUID* outTabId);
CExplorerFrame_AddTab_t CExplorerFrame_AddTab_Original;
HRESULT WINAPI CExplorerFrame_AddTab_Hook(void* pThis,
                                          const void* pidl,
                                          int entryPoint,
                                          GUID* outTabId) {
    HRESULT hr =
        CExplorerFrame_AddTab_Original(pThis, pidl, entryPoint, outTabId);

    Wh_Log(L"AddTab: hr=0x%08X", hr);

    if (g_settings.selectNewTabs && SUCCEEDED(hr) && outTabId) {
        ScheduleSelectTab(pThis, *outTabId);
    }

    return hr;
}

using CExplorerFrame_AddTabAtIndex_t = HRESULT(WINAPI*)(void* pThis,
                                                        const void* pidl,
                                                        int entryPoint,
                                                        int index,
                                                        GUID* outTabId);
CExplorerFrame_AddTabAtIndex_t CExplorerFrame_AddTabAtIndex_Original;
HRESULT WINAPI CExplorerFrame_AddTabAtIndex_Hook(void* pThis,
                                                 const void* pidl,
                                                 int entryPoint,
                                                 int index,
                                                 GUID* outTabId) {
    HRESULT hr = CExplorerFrame_AddTabAtIndex_Original(pThis, pidl, entryPoint,
                                                       index, outTabId);

    Wh_Log(L"AddTabAtIndex: hr=0x%08X", hr);

    if (g_settings.selectNewTabs && SUCCEEDED(hr) && outTabId) {
        ScheduleSelectTab(pThis, *outTabId);
    }

    return hr;
}

bool HookExplorerFrameSymbols() {
    HMODULE module = LoadLibrary(L"explorerframe.dll");
    if (!module) {
        Wh_Log(L"Couldn't load explorerframe.dll");
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK explorerFrameDllHooks[] = {
        {
            {LR"(public: void __cdecl CExplorerFrame::SetFrameTitle(unsigned short const *,struct _GUID))"},
            &CExplorerFrame_SetFrameTitle_Original,
            CExplorerFrame_SetFrameTitle_Hook,
        },
        {
            {LR"(public: virtual long __cdecl CExplorerFrame::SelectTab(struct _GUID))"},
            &CExplorerFrame_SelectTab_Original,
        },
        {
            {LR"(public: virtual long __cdecl CExplorerFrame::AddTab(struct _ITEMIDLIST_ABSOLUTE const *,enum NewTabEntryPoint,struct _GUID *))"},
            &CExplorerFrame_AddTab_Original,
            CExplorerFrame_AddTab_Hook,
        },
        {
            {LR"(public: virtual long __cdecl CExplorerFrame::AddTabAtIndex(struct _ITEMIDLIST_ABSOLUTE const *,enum NewTabEntryPoint,int,struct _GUID *))"},
            &CExplorerFrame_AddTabAtIndex_Original,
            CExplorerFrame_AddTabAtIndex_Hook,
            true,  // optional - a less common path, not required for the mod to be useful
        },
    };

    return HookSymbols(module, explorerFrameDllHooks,
                       ARRAYSIZE(explorerFrameDllHooks));
}

void LoadSettings() {
    g_settings.fixTabTitles = Wh_GetIntSetting(L"fixTabTitles");
    g_settings.selectNewTabs = Wh_GetIntSetting(L"selectNewTabs");
}

BOOL Wh_ModInit() {
    Wh_Log(L">");

    LoadSettings();

    if (!HookExplorerFrameSymbols()) {
        Wh_Log(L"Error hooking explorer frame symbols");
        return FALSE;
    }

    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L">");
}

void Wh_ModSettingsChanged() {
    Wh_Log(L">");
    LoadSettings();
}
