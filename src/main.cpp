#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <endpointvolume.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>
#include <gdiplus.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <setupapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <UIAutomation.h>

#include "headset_adapters.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <commdlg.h>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
MIDL_INTERFACE("c02216f6-8c67-4b5b-9d00-d008e73e0064")
IAudioMeterInformation : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* peak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT count,
                                                            float* peaks) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD* mask) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IAudioMeterInformation, 0xc02216f6, 0x8c67, 0x4b5b, 0x9d,
                0x00, 0xd0, 0x08, 0xe7, 0x3e, 0x00, 0x64)
#endif
#endif

using namespace Gdiplus;

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    ComPtr(ComPtr&& other) noexcept : value_(other.value_) {
        other.value_ = nullptr;
    }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            value_ = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }
    T* get() const { return value_; }
    T* operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
    T** put() {
        reset();
        return &value_;
    }
    void** put_void() {
        reset();
        return reinterpret_cast<void**>(&value_);
    }
    void reset() {
        if (value_) value_->Release();
        value_ = nullptr;
    }
    void copy_from(T* value) {
        reset();
        value_ = value;
        if (value_) value_->AddRef();
    }

private:
    T* value_ = nullptr;
};

static constexpr wchar_t kAppName[] = L"Microphone Activity Widget";
static constexpr wchar_t kWindowClass[] =
    L"MicrophoneActivityWidget.Standalone.Window";
static constexpr UINT kTrayCallback = WM_APP + 1;
static constexpr UINT kStateChanged = WM_APP + 2;
static constexpr UINT kWheelMessage = WM_APP + 3;
static constexpr UINT kShowSettingsMessage = WM_APP + 4;
static constexpr UINT_PTR kStandardHidTimer = 1;
static constexpr UINT kMicIconId = 1;
static constexpr UINT kCallIconId = 2;
static constexpr int kAppIconResourceId = 101;
static const GUID kMicIconGuid = {0x16ccb84d, 0xbab1, 0x47ef,
                                  {0x85, 0x62, 0xe1, 0x54, 0xeb, 0xd9, 0x31,
                                   0x1a}};
static const GUID kCallIconGuid = {0x69c59ac8, 0x27b4, 0x4d4a,
                                   {0xa0, 0xec, 0x79, 0xd9, 0x2c, 0x15, 0xab,
                                    0x8b}};

static void Log(PCWSTR format, ...) {
    wchar_t message[1024];
    va_list args;
    va_start(args, format);
    StringCchVPrintfW(message, ARRAYSIZE(message), format, args);
    va_end(args);
    OutputDebugStringW(L"[Microphone Activity Widget] ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

struct Settings {
    ERole deviceRole = eConsole;
    int volumeStep = 2;
    bool forceVolume = false;
    int forcedVolume = 100;
    int updateInterval = 50;
    int peakSensitivity = 150;
    bool showCallStateIcon = true;
    bool startWithWindows = false;

    std::wstring headsetMode = L"full";
    bool headsetSyncWindows = true;
    bool headsetSyncCalls = true;
    int headsetPollInterval = 500;

    bool slackWarning = true;
    bool slackAudioCue = true;
    bool slackToggle = true;
    std::wstring slackMutedText = L"unmute";
    std::wstring slackCallText = L"leave";
    int slackThreshold = 8;
    int slackDelay = 500;

    bool teamsWarning = true;
    bool teamsAudioCue = true;
    bool teamsToggle = true;
    std::wstring teamsMutedText = L"unmute";
    std::wstring teamsCallText = L"hang up|leave";
    int teamsThreshold = 8;
    int teamsDelay = 500;

    bool zoomWarning = true;
    bool zoomAudioCue = true;
    bool zoomToggle = true;
    std::wstring zoomMutedText = L"unmute";
    std::wstring zoomCallText = L"leave|end";
    int zoomThreshold = 8;
    int zoomDelay = 500;
};

static Settings g_settings;
static std::wstring g_settingsPath;
static HINSTANCE g_instance;
static HWND g_mainWindow;
static HWND g_settingsWindow;
static HHOOK g_mouseHook;
static HFONT g_uiFont;
static ULONG_PTR g_gdiplusToken;
static HICON g_micIcon;
static HICON g_callIcon;
static HICON g_appIconLarge;
static HICON g_appIconSmall;
static bool g_micIconAdded;
static bool g_callIconAdded;
static bool g_previousWarning;
static UINT g_taskbarCreated;
static HANDLE g_singleInstance;

static std::atomic<bool> g_exiting{false};
static std::atomic<int> g_audioRole{eConsole};
static std::atomic<int> g_updateInterval{50};
static std::atomic<int> g_peakSensitivity{150};
static std::atomic<bool> g_forceVolume{false};
static std::atomic<int> g_forcedVolume{100};
static std::atomic<bool> g_audioAvailable{false};
static std::atomic<bool> g_audioMuted{false};
static std::atomic<int> g_audioVolume{0};
static std::atomic<float> g_audioPeak{0.0f};
static std::atomic<float> g_audioLinearPeak{0.0f};
static std::atomic<int> g_pendingVolumeNotches{0};
static std::atomic<int> g_pendingVolumeSet{-1};
static std::atomic<unsigned> g_pendingMuteToggles{0};
static std::atomic<int> g_pendingMuteSet{-1};
static std::atomic<int> g_pendingSlackCommand{-1};
static std::atomic<int> g_pendingTeamsCommand{-1};
static std::atomic<int> g_pendingZoomCommand{-1};
static std::atomic<bool> g_slackActive{false};
static std::atomic<bool> g_slackMuted{false};
static std::atomic<bool> g_slackWarning{false};
static std::atomic<HWND> g_slackWindow{nullptr};
static std::atomic<bool> g_teamsActive{false};
static std::atomic<bool> g_teamsMuted{false};
static std::atomic<bool> g_teamsWarning{false};
static std::atomic<HWND> g_teamsWindow{nullptr};
static std::atomic<bool> g_zoomActive{false};
static std::atomic<bool> g_zoomMuted{false};
static std::atomic<bool> g_zoomKnown{false};
static std::atomic<bool> g_zoomWarning{false};
static std::atomic<HWND> g_zoomWindow{nullptr};
static SRWLOCK g_deviceNameLock = SRWLOCK_INIT;
static std::wstring g_deviceName = L"No microphone available";

struct HeadsetStatus {
    bool detected = false;
    bool stateKnown = false;
    bool muted = false;
    HeadsetDetectionMethod method = HeadsetDetectionMethod::Unsupported;
    HeadsetConfidence confidence = HeadsetConfidence::None;
    std::wstring deviceName;
    std::wstring detail;
};

static SRWLOCK g_headsetStatusLock = SRWLOCK_INIT;
static HeadsetStatus g_headsetStatus;
static bool g_windowsHardwareMuteSupported;
static bool g_windowsHardwareMuted;
static std::wstring g_windowsHardwareDevice;
static bool g_standardHidDetected;
static std::wstring g_standardHidDevice;
static std::wstring g_standardHidDetail;
static bool g_steelSeriesDetected;
static bool g_steelSeriesMuted;
static std::wstring g_steelSeriesDevice;
static std::wstring g_steelSeriesDetail;

static SRWLOCK g_diagnosticLock = SRWLOCK_INIT;
static std::vector<std::wstring> g_diagnosticEvents;
static ULONGLONG g_diagnosticStartTime;

static HANDLE g_stopEvent;
static HANDLE g_wakeEvent;
static HANDLE g_audioThread;
static HANDLE g_callThread;
static HANDLE g_headsetThread;

static std::wstring ReadIniString(PCWSTR section, PCWSTR key,
                                  PCWSTR fallback) {
    wchar_t buffer[512];
    GetPrivateProfileStringW(section, key, fallback, buffer,
                             ARRAYSIZE(buffer), g_settingsPath.c_str());
    return buffer;
}

static bool ReadIniBool(PCWSTR section, PCWSTR key, bool fallback) {
    return GetPrivateProfileIntW(section, key, fallback ? 1 : 0,
                                 g_settingsPath.c_str()) != 0;
}

static int ReadIniInt(PCWSTR section, PCWSTR key, int fallback, int low,
                      int high) {
    return std::clamp(static_cast<int>(GetPrivateProfileIntW(
                          section, key, fallback, g_settingsPath.c_str())),
                      low, high);
}

static void WriteIniString(PCWSTR section, PCWSTR key,
                           const std::wstring& value) {
    WritePrivateProfileStringW(section, key, value.c_str(),
                               g_settingsPath.c_str());
}

static void WriteIniInt(PCWSTR section, PCWSTR key, int value) {
    WriteIniString(section, key, std::to_wstring(value));
}

static void WriteIniBool(PCWSTR section, PCWSTR key, bool value) {
    WriteIniInt(section, key, value ? 1 : 0);
}

static void LoadSettings() {
    Settings defaults;
    std::wstring role = ReadIniString(L"General", L"DeviceRole", L"console");
    g_settings.deviceRole = role == L"communications" ? eCommunications
                            : role == L"multimedia"    ? eMultimedia
                                                       : eConsole;
    g_settings.volumeStep = ReadIniInt(L"General", L"VolumeStep",
                                       defaults.volumeStep, 1, 20);
    g_settings.forceVolume = ReadIniBool(
        L"General", L"ForceVolume", defaults.forceVolume);
    g_settings.forcedVolume = ReadIniInt(
        L"General", L"ForcedVolume", defaults.forcedVolume, 0, 100);
    g_settings.updateInterval = ReadIniInt(
        L"General", L"UpdateInterval", defaults.updateInterval, 25, 500);
    g_settings.peakSensitivity = ReadIniInt(
        L"General", L"PeakSensitivity", defaults.peakSensitivity, 25, 500);
    g_settings.showCallStateIcon = ReadIniBool(
        L"General", L"ShowCallIcon", defaults.showCallStateIcon);
    g_settings.startWithWindows = ReadIniBool(
        L"General", L"StartWithWindows", defaults.startWithWindows);

    auto loadApp = [&](PCWSTR section, bool& warning, bool& cue, bool& toggle,
                       std::wstring& mutedText, std::wstring& callText,
                       int& threshold, int& delay, const std::wstring& dMuted,
                       const std::wstring& dCall) {
        warning = ReadIniBool(section, L"Warning", true);
        cue = ReadIniBool(section, L"AudioCue", true);
        toggle = ReadIniBool(section, L"Toggle", true);
        mutedText = ReadIniString(section, L"MutedText", dMuted.c_str());
        callText = ReadIniString(section, L"CallText", dCall.c_str());
        threshold = ReadIniInt(section, L"Threshold", 8, 1, 100);
        delay = ReadIniInt(section, L"Delay", 500, 100, 3000);
    };
    loadApp(L"Slack", g_settings.slackWarning, g_settings.slackAudioCue,
            g_settings.slackToggle, g_settings.slackMutedText,
            g_settings.slackCallText, g_settings.slackThreshold,
            g_settings.slackDelay, defaults.slackMutedText,
            defaults.slackCallText);
    loadApp(L"Teams", g_settings.teamsWarning, g_settings.teamsAudioCue,
            g_settings.teamsToggle, g_settings.teamsMutedText,
            g_settings.teamsCallText, g_settings.teamsThreshold,
            g_settings.teamsDelay, defaults.teamsMutedText,
            defaults.teamsCallText);
    loadApp(L"Zoom", g_settings.zoomWarning, g_settings.zoomAudioCue,
            g_settings.zoomToggle, g_settings.zoomMutedText,
            g_settings.zoomCallText, g_settings.zoomThreshold,
            g_settings.zoomDelay, defaults.zoomMutedText,
            defaults.zoomCallText);
    g_settings.headsetMode =
        ReadIniString(L"Headset", L"Mode", defaults.headsetMode.c_str());
    g_settings.headsetSyncWindows = ReadIniBool(
        L"Headset", L"SyncWindows", defaults.headsetSyncWindows);
    g_settings.headsetSyncCalls = ReadIniBool(
        L"Headset", L"SyncCalls", defaults.headsetSyncCalls);
    g_settings.headsetPollInterval = ReadIniInt(
        L"Headset", L"PollInterval", defaults.headsetPollInterval, 200,
        2000);
    g_audioRole.store(g_settings.deviceRole);
    g_updateInterval.store(g_settings.updateInterval);
    g_peakSensitivity.store(g_settings.peakSensitivity);
    g_forceVolume.store(g_settings.forceVolume);
    g_forcedVolume.store(g_settings.forcedVolume);
}

static void SaveSettings() {
    PCWSTR role = g_settings.deviceRole == eCommunications
                      ? L"communications"
                  : g_settings.deviceRole == eMultimedia ? L"multimedia"
                                                         : L"console";
    WriteIniString(L"General", L"DeviceRole", role);
    WriteIniInt(L"General", L"VolumeStep", g_settings.volumeStep);
    WriteIniBool(L"General", L"ForceVolume", g_settings.forceVolume);
    WriteIniInt(L"General", L"ForcedVolume", g_settings.forcedVolume);
    WriteIniInt(L"General", L"UpdateInterval", g_settings.updateInterval);
    WriteIniInt(L"General", L"PeakSensitivity", g_settings.peakSensitivity);
    WriteIniBool(L"General", L"ShowCallIcon",
                 g_settings.showCallStateIcon);
    WriteIniBool(L"General", L"StartWithWindows",
                 g_settings.startWithWindows);
    auto saveApp = [&](PCWSTR section, bool warning, bool cue, bool toggle,
                       const std::wstring& mutedText,
                       const std::wstring& callText, int threshold, int delay) {
        WriteIniBool(section, L"Warning", warning);
        WriteIniBool(section, L"AudioCue", cue);
        WriteIniBool(section, L"Toggle", toggle);
        WriteIniString(section, L"MutedText", mutedText);
        WriteIniString(section, L"CallText", callText);
        WriteIniInt(section, L"Threshold", threshold);
        WriteIniInt(section, L"Delay", delay);
    };
    saveApp(L"Slack", g_settings.slackWarning, g_settings.slackAudioCue,
            g_settings.slackToggle, g_settings.slackMutedText,
            g_settings.slackCallText, g_settings.slackThreshold,
            g_settings.slackDelay);
    saveApp(L"Teams", g_settings.teamsWarning, g_settings.teamsAudioCue,
            g_settings.teamsToggle, g_settings.teamsMutedText,
            g_settings.teamsCallText, g_settings.teamsThreshold,
            g_settings.teamsDelay);
    saveApp(L"Zoom", g_settings.zoomWarning, g_settings.zoomAudioCue,
            g_settings.zoomToggle, g_settings.zoomMutedText,
            g_settings.zoomCallText, g_settings.zoomThreshold,
            g_settings.zoomDelay);
    WriteIniString(L"Headset", L"Mode", g_settings.headsetMode);
    WriteIniBool(L"Headset", L"SyncWindows",
                 g_settings.headsetSyncWindows);
    WriteIniBool(L"Headset", L"SyncCalls", g_settings.headsetSyncCalls);
    WriteIniInt(L"Headset", L"PollInterval",
                g_settings.headsetPollInterval);
}

static void ApplyStartupSetting() {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr) != ERROR_SUCCESS) {
        return;
    }
    if (g_settings.startWithWindows) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        std::wstring command = L"\"" + std::wstring(path) + L"\"";
        RegSetValueExW(key, kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(command.c_str()),
                       static_cast<DWORD>((command.size() + 1) *
                                          sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }
    RegCloseKey(key);
}

static void SetDeviceName(std::wstring name) {
    AcquireSRWLockExclusive(&g_deviceNameLock);
    g_deviceName = std::move(name);
    ReleaseSRWLockExclusive(&g_deviceNameLock);
}

static std::wstring GetDeviceName() {
    AcquireSRWLockShared(&g_deviceNameLock);
    std::wstring result = g_deviceName;
    ReleaseSRWLockShared(&g_deviceNameLock);
    return result;
}

static PCWSTR HeadsetMethodName(HeadsetDetectionMethod method) {
    switch (method) {
        case HeadsetDetectionMethod::WindowsHardwareMute:
            return L"Windows hardware mute";
        case HeadsetDetectionMethod::StandardHidButton:
            return L"Standard HID mute button";
        case HeadsetDetectionMethod::SteelSeriesDeviceState:
            return L"SteelSeries device state";
        default:
            return L"Unsupported/no observable state";
    }
}

static PCWSTR HeadsetConfidenceName(HeadsetConfidence confidence) {
    switch (confidence) {
        case HeadsetConfidence::High:
            return L"High — a latched mute state is observable";
        case HeadsetConfidence::Medium:
            return L"Medium — button events are observable; switch state is not";
        default:
            return L"None — no physical mute signal is observable";
    }
}

static HeadsetStatus GetHeadsetStatus() {
    AcquireSRWLockShared(&g_headsetStatusLock);
    HeadsetStatus result = g_headsetStatus;
    ReleaseSRWLockShared(&g_headsetStatusLock);
    return result;
}

static bool SameHeadsetStatus(const HeadsetStatus& left,
                              const HeadsetStatus& right) {
    return left.detected == right.detected &&
           left.stateKnown == right.stateKnown && left.muted == right.muted &&
           left.method == right.method &&
           left.confidence == right.confidence &&
           left.deviceName == right.deviceName && left.detail == right.detail;
}

static void RecomputeHeadsetStatus() {
    bool changed = false;
    AcquireSRWLockExclusive(&g_headsetStatusLock);
    HeadsetStatus next;
    if (g_steelSeriesDetected) {
        next.detected = true;
        next.stateKnown = true;
        next.muted = g_steelSeriesMuted;
        next.method = HeadsetDetectionMethod::SteelSeriesDeviceState;
        next.confidence = HeadsetConfidence::High;
        next.deviceName = g_steelSeriesDevice;
        next.detail = g_steelSeriesDetail;
    } else if (g_standardHidDetected) {
        next.detected = true;
        next.stateKnown = false;
        next.method = HeadsetDetectionMethod::StandardHidButton;
        next.confidence = HeadsetConfidence::Medium;
        next.deviceName = g_standardHidDevice;
        next.detail = g_standardHidDetail;
    } else if (g_windowsHardwareMuteSupported) {
        next.detected = true;
        next.stateKnown = true;
        next.muted = g_windowsHardwareMuted;
        next.method = HeadsetDetectionMethod::WindowsHardwareMute;
        next.confidence = HeadsetConfidence::High;
        next.deviceName = g_windowsHardwareDevice;
        next.detail = L"The active capture driver exposes hardware mute.";
    } else {
        next.detail =
            L"Silence is never treated as proof that a headset is muted.";
    }
    changed = !SameHeadsetStatus(g_headsetStatus, next);
    g_headsetStatus = std::move(next);
    ReleaseSRWLockExclusive(&g_headsetStatusLock);
    if (changed && g_mainWindow)
        PostMessageW(g_mainWindow, kStateChanged, 0, 0);
}

static void UpdateWindowsHardwareSource(bool supported, bool muted,
                                        const std::wstring& deviceName) {
    AcquireSRWLockExclusive(&g_headsetStatusLock);
    g_windowsHardwareMuteSupported = supported;
    g_windowsHardwareMuted = muted;
    g_windowsHardwareDevice = supported ? deviceName : L"";
    ReleaseSRWLockExclusive(&g_headsetStatusLock);
    RecomputeHeadsetStatus();
}

static void UpdateStandardHidSource(bool detected,
                                    const std::wstring& deviceName,
                                    const std::wstring& detail) {
    AcquireSRWLockExclusive(&g_headsetStatusLock);
    g_standardHidDetected = detected;
    g_standardHidDevice = detected ? deviceName : L"";
    g_standardHidDetail = detected ? detail : L"";
    ReleaseSRWLockExclusive(&g_headsetStatusLock);
    RecomputeHeadsetStatus();
}

static void UpdateSteelSeriesSource(bool detected, bool muted,
                                    const std::wstring& deviceName,
                                    const std::wstring& detail) {
    AcquireSRWLockExclusive(&g_headsetStatusLock);
    g_steelSeriesDetected = detected;
    g_steelSeriesMuted = muted;
    g_steelSeriesDevice = detected ? deviceName : L"";
    g_steelSeriesDetail = detected ? detail : L"";
    ReleaseSRWLockExclusive(&g_headsetStatusLock);
    RecomputeHeadsetStatus();
}

static void RecordDiagnosticEvent(const std::wstring& event) {
    ULONGLONG elapsed = GetTickCount64() - g_diagnosticStartTime;
    std::wstring line = L"+" + std::to_wstring(elapsed) + L" ms: " + event;
    AcquireSRWLockExclusive(&g_diagnosticLock);
    if (g_diagnosticEvents.size() >= 256) g_diagnosticEvents.erase(
        g_diagnosticEvents.begin(), g_diagnosticEvents.begin() + 64);
    g_diagnosticEvents.push_back(std::move(line));
    ReleaseSRWLockExclusive(&g_diagnosticLock);
}

static void NotifyMain() {
    if (g_mainWindow) PostMessageW(g_mainWindow, kStateChanged, 0, 0);
}

static void QueueVolume(int notches) {
    if (!notches) return;
    if (g_forceVolume.load()) {
        int target = std::clamp(
            g_forcedVolume.load() + notches * g_settings.volumeStep, 0, 100);
        g_forcedVolume.store(target);
        g_settings.forcedVolume = target;
        g_pendingVolumeSet.store(target);
        WriteIniInt(L"General", L"ForcedVolume", target);
    } else {
        g_pendingVolumeNotches.fetch_add(notches);
    }
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

static void QueueWindowsToggle() {
    g_pendingMuteToggles.fetch_add(1);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

static void QueueWindowsMute(bool muted) {
    g_pendingMuteSet.store(muted ? 1 : 0);
    if (g_wakeEvent) SetEvent(g_wakeEvent);
}

static constexpr int kCallNone = -1;
static constexpr int kCallUnmute = 0;
static constexpr int kCallMute = 1;
static constexpr int kCallToggle = 2;

static bool QueueCallToggles() {
    bool queued = false;
    if (g_settings.slackToggle && g_slackActive.load()) {
        g_pendingSlackCommand.store(kCallToggle);
        queued = true;
    }
    if (g_settings.teamsToggle && g_teamsActive.load()) {
        g_pendingTeamsCommand.store(kCallToggle);
        queued = true;
    }
    if (g_settings.zoomToggle && g_zoomActive.load() && g_zoomKnown.load()) {
        g_pendingZoomCommand.store(kCallToggle);
        queued = true;
    }
    return queued;
}

static void QueueCallMuteState(bool muted) {
    int command = muted ? kCallMute : kCallUnmute;
    if (g_slackActive.load() && g_slackMuted.load() != muted)
        g_pendingSlackCommand.store(command);
    if (g_teamsActive.load() && g_teamsMuted.load() != muted)
        g_pendingTeamsCommand.store(command);
    if (g_zoomActive.load() && g_zoomMuted.load() != muted)
        g_pendingZoomCommand.store(command);
}

static int SelectedCall() {
    if (g_slackActive.load() && g_slackMuted.load()) return 0;
    if (g_teamsActive.load() && g_teamsMuted.load()) return 1;
    if (g_zoomActive.load() && g_zoomMuted.load()) return 2;
    if (g_slackActive.load()) return 0;
    if (g_teamsActive.load()) return 1;
    if (g_zoomActive.load()) return 2;
    return -1;
}

static bool FocusSelectedCall() {
    int selected = SelectedCall();
    HWND window = selected == 0   ? g_slackWindow.load()
                  : selected == 1 ? g_teamsWindow.load()
                  : selected == 2 ? g_zoomWindow.load()
                                  : nullptr;
    if (!window || !IsWindow(window)) return false;
    if (IsIconic(window)) ShowWindowAsync(window, SW_RESTORE);
    else if (!IsWindowVisible(window)) ShowWindowAsync(window, SW_SHOW);
    BringWindowToTop(window);
    SetForegroundWindow(window);
    return true;
}

struct AudioEndpoint {
    ComPtr<IMMDevice> device;
    ComPtr<IAudioMeterInformation> meter;
    ComPtr<IAudioEndpointVolume> volume;
    std::wstring id;
    std::wstring name;
    bool hardwareMute = false;
    void reset() {
        volume.reset();
        meter.reset();
        device.reset();
        id.clear();
        name.clear();
        hardwareMute = false;
    }
};

static std::wstring DeviceFriendlyName(IMMDevice* device) {
    ComPtr<IPropertyStore> properties;
    if (FAILED(device->OpenPropertyStore(STGM_READ, properties.put())))
        return L"Microphone";
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring result = L"Microphone";
    if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) &&
        value.vt == VT_LPWSTR && value.pwszVal)
        result = value.pwszVal;
    PropVariantClear(&value);
    return result;
}

static bool OpenDefaultEndpoint(IMMDeviceEnumerator* enumerator,
                                AudioEndpoint& endpoint) {
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(
            eCapture, static_cast<ERole>(g_audioRole.load()), device.put()))) {
        endpoint.reset();
        return false;
    }
    LPWSTR rawId = nullptr;
    if (FAILED(device->GetId(&rawId)) || !rawId) {
        endpoint.reset();
        return false;
    }
    std::wstring id = rawId;
    CoTaskMemFree(rawId);
    if (endpoint.device && endpoint.id == id) return true;

    AudioEndpoint replacement;
    replacement.device = std::move(device);
    replacement.id = std::move(id);
    replacement.name = DeviceFriendlyName(replacement.device.get());
    if (FAILED(replacement.device->Activate(
            __uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr,
            replacement.meter.put_void())) ||
        FAILED(replacement.device->Activate(
            __uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
            replacement.volume.put_void()))) {
        endpoint.reset();
        return false;
    }
    DWORD hardwareSupport = 0;
    replacement.hardwareMute =
        SUCCEEDED(replacement.volume->QueryHardwareSupport(
            &hardwareSupport)) &&
        (hardwareSupport & ENDPOINT_HARDWARE_SUPPORT_MUTE) != 0;
    endpoint = std::move(replacement);
    Log(L"Following microphone: %ls (hardware mute: %ls)",
        endpoint.name.c_str(), endpoint.hardwareMute ? L"yes" : L"no");
    return true;
}

static void PublishUnavailableAudio() {
    g_audioAvailable.store(false);
    g_audioMuted.store(false);
    g_audioVolume.store(0);
    g_audioPeak.store(0);
    g_audioLinearPeak.store(0);
    g_slackWarning.store(false);
    g_teamsWarning.store(false);
    g_zoomWarning.store(false);
    SetDeviceName(L"No microphone available");
    UpdateWindowsHardwareSource(false, false, L"");
    NotifyMain();
}

static DWORD WINAPI AudioThreadProc(void*) {
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized)) {
        PublishUnavailableAudio();
        return 0;
    }
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                enumerator.put_void()))) {
        PublishUnavailableAudio();
        CoUninitialize();
        return 0;
    }
    AudioEndpoint endpoint;
    float smoothed = 0;
    ULONGLONG lastEndpointCheck = 0;
    ULONGLONG lastVolumeForce = 0;
    std::wstring observedEndpointId;
    bool observedMuteKnown = false;
    bool observedMuted = false;
    HANDLE waits[] = {g_stopEvent, g_wakeEvent};
    for (;;) {
        DWORD wait = WaitForMultipleObjects(
            ARRAYSIZE(waits), waits, FALSE,
            static_cast<DWORD>(g_updateInterval.load()));
        if (wait == WAIT_OBJECT_0 || g_exiting.load()) break;
        ULONGLONG now = GetTickCount64();
        if (!lastEndpointCheck || now - lastEndpointCheck >= 1000) {
            lastEndpointCheck = now;
            if (!OpenDefaultEndpoint(enumerator.get(), endpoint)) {
                observedEndpointId.clear();
                observedMuteKnown = false;
                PublishUnavailableAudio();
                continue;
            }
            if (observedEndpointId != endpoint.id) {
                observedEndpointId = endpoint.id;
                observedMuteKnown = false;
            }
        }
        if (!endpoint.device) continue;

        int volumeSet = g_pendingVolumeSet.exchange(-1);
        if (volumeSet >= 0) {
            endpoint.volume->SetMasterVolumeLevelScalar(
                volumeSet / 100.0f, nullptr);
            lastVolumeForce = now;
        }
        int notches = g_pendingVolumeNotches.exchange(0);
        if (notches) {
            float current = 0;
            if (SUCCEEDED(endpoint.volume->GetMasterVolumeLevelScalar(
                    &current))) {
                float next = std::clamp(
                    current + notches * g_settings.volumeStep / 100.0f,
                    0.0f, 1.0f);
                endpoint.volume->SetMasterVolumeLevelScalar(next, nullptr);
            }
        }
        int muteSet = g_pendingMuteSet.exchange(-1);
        if (muteSet >= 0) {
            g_pendingMuteToggles.exchange(0);
            endpoint.volume->SetMute(muteSet != 0, nullptr);
        } else if (g_pendingMuteToggles.exchange(0) & 1U) {
            BOOL muted = FALSE;
            if (SUCCEEDED(endpoint.volume->GetMute(&muted)))
                endpoint.volume->SetMute(!muted, nullptr);
        }

        float peak = 0;
        float volume = 0;
        BOOL muted = FALSE;
        if (FAILED(endpoint.meter->GetPeakValue(&peak)) ||
            FAILED(endpoint.volume->GetMasterVolumeLevelScalar(&volume)) ||
            FAILED(endpoint.volume->GetMute(&muted))) {
            endpoint.reset();
            observedEndpointId.clear();
            observedMuteKnown = false;
            PublishUnavailableAudio();
            continue;
        }
        if (g_forceVolume.load()) {
            float targetVolume = g_forcedVolume.load() / 100.0f;
            if (std::fabs(volume - targetVolume) > 0.01f &&
                (!lastVolumeForce || now - lastVolumeForce >= 250)) {
                if (SUCCEEDED(endpoint.volume->SetMasterVolumeLevelScalar(
                        targetVolume, nullptr))) {
                    volume = targetVolume;
                    lastVolumeForce = now;
                }
            }
        }
        float scaled = std::clamp(
            peak * g_peakSensitivity.load() / 100.0f, 0.0f, 1.0f);
        float display = 0;
        if (scaled > 0.001f) {
            float db = 20.0f * std::log10(scaled);
            display = std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
        }
        smoothed += (display - smoothed) * (display > smoothed ? 0.65f : 0.18f);
        if (smoothed < 0.005f) smoothed = 0;
        g_audioAvailable.store(true);
        g_audioMuted.store(muted != FALSE);
        g_audioVolume.store(static_cast<int>(std::lround(volume * 100)));
        g_audioPeak.store(muted ? 0 : smoothed);
        g_audioLinearPeak.store(muted ? 0 : scaled);
        SetDeviceName(endpoint.name);
        UpdateWindowsHardwareSource(endpoint.hardwareMute, muted != FALSE,
                                    endpoint.name);
        if (endpoint.hardwareMute) {
            bool changed = observedMuteKnown &&
                           observedMuted != (muted != FALSE);
            bool initialMuted = !observedMuteKnown && muted != FALSE;
            bool syncMute = g_settings.headsetMode == L"full" ||
                            g_settings.headsetMode == L"muteOnly";
            bool syncUnmute = g_settings.headsetMode == L"full";
            if (g_settings.headsetSyncCalls &&
                ((muted && (changed || initialMuted) && syncMute) ||
                 (!muted && changed && syncUnmute))) {
                QueueCallMuteState(muted != FALSE);
                RecordDiagnosticEvent(
                    std::wstring(L"Windows hardware mute changed to ") +
                    (muted ? L"muted" : L"unmuted"));
            }
        }
        observedMuteKnown = true;
        observedMuted = muted != FALSE;
        NotifyMain();
    }
    endpoint.reset();
    enumerator.reset();
    CoUninitialize();
    return 0;
}

enum class CallApp { Slack, Teams, Zoom };
enum class CallState { NotInCall, Unknown, Unmuted, Muted };

static std::wstring Lowercase(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    });
    return text;
}

static PCWSTR CallName(CallApp app) {
    return app == CallApp::Slack ? L"Slack"
           : app == CallApp::Teams ? L"Microsoft Teams"
                                   : L"Zoom";
}

static bool IsCallWindow(HWND window, CallApp app) {
    if (!IsWindowVisible(window)) return false;
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 processId);
    if (!process) return false;
    wchar_t path[32768];
    DWORD length = ARRAYSIZE(path);
    bool matches = false;
    if (QueryFullProcessImageNameW(process, 0, path, &length)) {
        PCWSTR file = path;
        for (DWORD i = 0; i < length; i++)
            if (path[i] == L'\\' || path[i] == L'/') file = path + i + 1;
        if (app == CallApp::Slack)
            matches = _wcsicmp(file, L"slack.exe") == 0;
        else if (app == CallApp::Teams)
            matches = _wcsicmp(file, L"ms-teams.exe") == 0 ||
                      _wcsicmp(file, L"teams.exe") == 0;
        else
            matches = _wcsicmp(file, L"zoom.exe") == 0 ||
                      _wcsicmp(file, L"cpthost.exe") == 0;
    }
    CloseHandle(process);
    return matches;
}

static bool ZoomMeetingHostRunning() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"cpthost.exe") != 0) continue;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                         FALSE, entry.th32ProcessID);
            if (!process) continue;
            wchar_t path[32768];
            DWORD length = ARRAYSIZE(path);
            if (QueryFullProcessImageNameW(process, 0, path, &length) &&
                Lowercase(std::wstring(path, length)).find(L"\\zoom\\") !=
                    std::wstring::npos)
                found = true;
            CloseHandle(process);
            if (found) break;
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

static bool RequestForeground(HWND window) {
    window = GetAncestor(window, GA_ROOT);
    if (!window || !IsWindow(window)) return false;
    DWORD current = GetCurrentThreadId();
    DWORD target = GetWindowThreadProcessId(window, nullptr);
    HWND oldForeground = GetForegroundWindow();
    DWORD foreground = oldForeground
                           ? GetWindowThreadProcessId(oldForeground, nullptr)
                           : 0;
    bool attachedTarget = target && target != current &&
                          AttachThreadInput(current, target, TRUE);
    bool attachedForeground = foreground && foreground != current &&
                              foreground != target &&
                              AttachThreadInput(current, foreground, TRUE);
    if (IsIconic(window)) ShowWindowAsync(window, SW_RESTORE);
    BringWindowToTop(window);
    SetForegroundWindow(window);
    SetFocus(window);
    DWORD wantedProcess = 0;
    DWORD actualProcess = 0;
    GetWindowThreadProcessId(window, &wantedProcess);
    if (HWND actual = GetForegroundWindow())
        GetWindowThreadProcessId(actual, &actualProcess);
    if (attachedForeground) AttachThreadInput(current, foreground, FALSE);
    if (attachedTarget) AttachThreadInput(current, target, FALSE);
    return wantedProcess && wantedProcess == actualProcess;
}

static bool SendZoomShortcut(HWND callWindow) {
    if (!callWindow || !IsWindow(callWindow) ||
        !IsCallWindow(callWindow, CallApp::Zoom))
        return false;
    HWND zoom = GetAncestor(callWindow, GA_ROOT);
    HWND previous = GetForegroundWindow();
    if (!RequestForeground(zoom)) return false;
    Sleep(20);
    INPUT input[4]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = VK_MENU;
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = L'A';
    input[2].type = INPUT_KEYBOARD;
    input[2].ki.wVk = L'A';
    input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].type = INPUT_KEYBOARD;
    input[3].ki.wVk = VK_MENU;
    input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    UINT sent = SendInput(ARRAYSIZE(input), input, sizeof(INPUT));
    Sleep(40);
    if (previous && previous != zoom && IsWindow(previous))
        RequestForeground(previous);
    return sent == ARRAYSIZE(input);
}

static CallState ApplyCommand(CallState state, int command) {
    if (command == kCallMute) return CallState::Muted;
    if (command == kCallUnmute) return CallState::Unmuted;
    if (command == kCallToggle) {
        if (state == CallState::Muted) return CallState::Unmuted;
        if (state == CallState::Unmuted) return CallState::Muted;
    }
    return state;
}

static bool ContainsToken(const std::wstring& name,
                          const std::wstring& configured) {
    size_t start = 0;
    while (start <= configured.size()) {
        size_t end = configured.find(L'|', start);
        std::wstring token = configured.substr(
            start, end == std::wstring::npos ? end : end - start);
        size_t first = token.find_first_not_of(L" \t");
        size_t last = token.find_last_not_of(L" \t");
        if (first != std::wstring::npos) {
            token = token.substr(first, last - first + 1);
            size_t match = 0;
            while (!token.empty() &&
                   (match = name.find(token, match)) != std::wstring::npos) {
                bool left = match == 0 || !std::iswalnum(name[match - 1]);
                size_t after = match + token.size();
                bool right = after == name.size() ||
                             !std::iswalnum(name[after]);
                if (left && right) return true;
                match++;
            }
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return false;
}

struct WindowSearch {
    CallApp app;
    std::vector<HWND>* windows;
};

static CallState ReadCallState(IUIAutomation* automation, CallApp app,
                               int command, HWND* activeWindow) {
    if (activeWindow) *activeWindow = nullptr;
    std::vector<HWND> windows;
    WindowSearch search{app, &windows};
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto* search = reinterpret_cast<WindowSearch*>(parameter);
            if (IsCallWindow(window, search->app))
                search->windows->push_back(window);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    if (!automation || windows.empty()) return CallState::NotInCall;
    if (activeWindow && app == CallApp::Zoom) *activeWindow = windows.front();

    VARIANT type;
    VariantInit(&type);
    type.vt = VT_I4;
    type.lVal = UIA_ButtonControlTypeId;
    ComPtr<IUIAutomationCondition> condition;
    if (FAILED(automation->CreatePropertyCondition(
            UIA_ControlTypePropertyId, type, condition.put())))
        return CallState::NotInCall;
    const std::wstring* mutedSetting =
        app == CallApp::Slack ? &g_settings.slackMutedText
        : app == CallApp::Teams ? &g_settings.teamsMutedText
                                : &g_settings.zoomMutedText;
    const std::wstring* callSetting =
        app == CallApp::Slack ? &g_settings.slackCallText
        : app == CallApp::Teams ? &g_settings.teamsCallText
                                : &g_settings.zoomCallText;
    std::wstring mutedText = Lowercase(*mutedSetting);
    std::wstring callText = Lowercase(*callSetting);
    for (HWND window : windows) {
        ComPtr<IUIAutomationElement> root;
        if (FAILED(automation->ElementFromHandle(window, root.put())) || !root)
            continue;
        ComPtr<IUIAutomationElementArray> buttons;
        if (FAILED(root->FindAll(TreeScope_Descendants, condition.get(),
                                 buttons.put())) ||
            !buttons)
            continue;
        bool marker = false;
        bool hasUnmute = false;
        bool hasMute = false;
        ComPtr<IUIAutomationElement> unmuteButton;
        ComPtr<IUIAutomationElement> muteButton;
        int count = 0;
        buttons->get_Length(&count);
        for (int index = 0; index < count; index++) {
            ComPtr<IUIAutomationElement> button;
            if (FAILED(buttons->GetElement(index, button.put())) || !button)
                continue;
            BOOL offscreen = TRUE;
            if (FAILED(button->get_CurrentIsOffscreen(&offscreen)) ||
                (offscreen && app != CallApp::Zoom))
                continue;
            BSTR raw = nullptr;
            if (FAILED(button->get_CurrentName(&raw)) || !raw) continue;
            std::wstring name = Lowercase(raw);
            SysFreeString(raw);
            if (ContainsToken(name, callText)) marker = true;
            if (ContainsToken(name, mutedText)) {
                hasUnmute = true;
                unmuteButton.copy_from(button.get());
            } else if (ContainsToken(name, L"mute")) {
                hasMute = true;
                muteButton.copy_from(button.get());
            }
        }
        if (!marker || (!hasUnmute && !hasMute)) continue;
        if (activeWindow) *activeWindow = window;
        CallState current = hasUnmute ? CallState::Muted
                                     : CallState::Unmuted;
        IUIAutomationElement* action =
            hasUnmute ? unmuteButton.get() : muteButton.get();
        bool invoke = command == kCallToggle ||
                      (command == kCallMute && current == CallState::Unmuted) ||
                      (command == kCallUnmute && current == CallState::Muted);
        if (invoke && action) {
            ComPtr<IUIAutomationInvokePattern> pattern;
            if (SUCCEEDED(action->GetCurrentPatternAs(
                    UIA_InvokePatternId, IID_IUIAutomationInvokePattern,
                    pattern.put_void())) &&
                pattern && SUCCEEDED(pattern->Invoke())) {
                Log(L"%ls mute action invoked", CallName(app));
                return current == CallState::Muted ? CallState::Unmuted
                                                   : CallState::Muted;
            }
        }
        return current;
    }
    return CallState::NotInCall;
}

static DWORD WINAPI CallThreadProc(void*) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 0;
    ComPtr<IUIAutomation> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                CLSCTX_INPROC_SERVER, IID_IUIAutomation,
                                automation.put_void()))) {
        CoUninitialize();
        return 0;
    }
    ULONGLONG lastSlack = 0, lastTeams = 0, lastZoom = 0;
    ULONGLONG slackSpeaking = 0, slackUntil = 0;
    ULONGLONG teamsSpeaking = 0, teamsUntil = 0;
    ULONGLONG zoomSpeaking = 0, zoomUntil = 0;
    CallState slack = CallState::NotInCall;
    CallState teams = CallState::NotInCall;
    CallState zoom = CallState::NotInCall;
    int deferredZoom = kCallNone;
    bool headsetCalls = g_settings.headsetSyncCalls &&
                        (g_settings.headsetMode == L"full" ||
                         g_settings.headsetMode == L"muteOnly");
    bool monitorSlack = g_settings.showCallStateIcon ||
                        g_settings.slackWarning || g_settings.slackToggle ||
                        headsetCalls;
    bool monitorTeams = g_settings.showCallStateIcon ||
                        g_settings.teamsWarning || g_settings.teamsToggle ||
                        headsetCalls;
    bool monitorZoom = g_settings.showCallStateIcon ||
                       g_settings.zoomWarning || g_settings.zoomToggle ||
                       headsetCalls;

    while (WaitForSingleObject(g_stopEvent, 50) == WAIT_TIMEOUT &&
           !g_exiting.load()) {
        ULONGLONG now = GetTickCount64();
        int slackCommand = g_pendingSlackCommand.exchange(kCallNone);
        if (!lastSlack || now - lastSlack >= 750 ||
            slackCommand != kCallNone) {
            HWND window = nullptr;
            slack = monitorSlack
                        ? ReadCallState(automation.get(), CallApp::Slack,
                                        slackCommand, &window)
                        : CallState::NotInCall;
            lastSlack = now = GetTickCount64();
            bool active = slack != CallState::NotInCall;
            bool muted = slack == CallState::Muted;
            bool changed = g_slackActive.exchange(active) != active;
            changed = (g_slackMuted.exchange(muted) != muted) || changed;
            g_slackWindow.store(window);
            if (changed) NotifyMain();
        }
        int teamsCommand = g_pendingTeamsCommand.exchange(kCallNone);
        if (!lastTeams || now - lastTeams >= 750 ||
            teamsCommand != kCallNone) {
            HWND window = nullptr;
            teams = monitorTeams
                        ? ReadCallState(automation.get(), CallApp::Teams,
                                        teamsCommand, &window)
                        : CallState::NotInCall;
            lastTeams = now = GetTickCount64();
            bool active = teams != CallState::NotInCall;
            bool muted = teams == CallState::Muted;
            bool changed = g_teamsActive.exchange(active) != active;
            changed = (g_teamsMuted.exchange(muted) != muted) || changed;
            g_teamsWindow.store(window);
            if (changed) NotifyMain();
        }
        int zoomCommand = g_pendingZoomCommand.exchange(kCallNone);
        if (zoomCommand != kCallNone) deferredZoom = zoomCommand;
        if (!lastZoom || now - lastZoom >= 750 || zoomCommand != kCallNone) {
            HWND window = nullptr;
            CallState detected =
                monitorZoom
                    ? ReadCallState(automation.get(), CallApp::Zoom,
                                    deferredZoom, &window)
                    : CallState::NotInCall;
            bool host = monitorZoom && ZoomMeetingHostRunning();
            if (detected == CallState::NotInCall && host) {
                if (zoom == CallState::NotInCall) zoom = CallState::Unknown;
                HWND previous = g_zoomWindow.load();
                if (!window && IsWindow(previous)) window = previous;
                if (deferredZoom != kCallNone && SendZoomShortcut(window)) {
                    zoom = ApplyCommand(zoom, deferredZoom);
                    deferredZoom = kCallNone;
                }
            } else {
                zoom = detected;
                deferredZoom = kCallNone;
            }
            lastZoom = now = GetTickCount64();
            bool active = zoom != CallState::NotInCall;
            bool muted = zoom == CallState::Muted;
            bool known = zoom == CallState::Muted ||
                         zoom == CallState::Unmuted;
            bool changed = g_zoomActive.exchange(active) != active;
            changed = (g_zoomMuted.exchange(muted) != muted) || changed;
            changed = (g_zoomKnown.exchange(known) != known) || changed;
            g_zoomWindow.store(window);
            if (changed) NotifyMain();
        }

        bool windowsCanHear = g_audioAvailable.load() &&
                              !g_audioMuted.load();
        float peak = g_audioLinearPeak.load();
        bool cue = false;
        bool changed = false;
        auto warning = [&](CallState state, bool enabled, bool audioCue,
                           int threshold, int delay, ULONGLONG& speaking,
                           ULONGLONG& until, std::atomic<bool>& published) {
            bool muted = state == CallState::Muted;
            if (!muted || !windowsCanHear) {
                speaking = 0;
                until = 0;
            } else if (peak >= threshold / 100.0f) {
                if (!speaking) speaking = now;
                if (now - speaking >= static_cast<ULONGLONG>(delay))
                    until = now + 2000;
            } else {
                speaking = 0;
            }
            bool active = enabled && muted && windowsCanHear && now < until;
            bool old = published.exchange(active);
            if (old != active) {
                changed = true;
                if (active && audioCue) cue = true;
            }
        };
        warning(slack, g_settings.slackWarning, g_settings.slackAudioCue,
                g_settings.slackThreshold, g_settings.slackDelay,
                slackSpeaking, slackUntil, g_slackWarning);
        warning(teams, g_settings.teamsWarning, g_settings.teamsAudioCue,
                g_settings.teamsThreshold, g_settings.teamsDelay,
                teamsSpeaking, teamsUntil, g_teamsWarning);
        warning(zoom, g_settings.zoomWarning, g_settings.zoomAudioCue,
                g_settings.zoomThreshold, g_settings.zoomDelay,
                zoomSpeaking, zoomUntil, g_zoomWarning);
        if (cue) MessageBeep(MB_ICONEXCLAMATION);
        if (changed) NotifyMain();
    }
    g_slackActive.store(false);
    g_slackMuted.store(false);
    g_slackWarning.store(false);
    g_slackWindow.store(nullptr);
    g_teamsActive.store(false);
    g_teamsMuted.store(false);
    g_teamsWarning.store(false);
    g_teamsWindow.store(nullptr);
    g_zoomActive.store(false);
    g_zoomMuted.store(false);
    g_zoomKnown.store(false);
    g_zoomWarning.store(false);
    g_zoomWindow.store(nullptr);
    NotifyMain();
    automation.reset();
    CoUninitialize();
    return 0;
}

static constexpr USHORT kHidUsagePageGeneric = 0x01;
static constexpr USHORT kHidUsageSystemControl = 0x80;
static constexpr USHORT kHidUsageSystemMicrophoneMute = 0xA9;
static constexpr USHORT kHidUsagePageTelephony = 0x0B;
static constexpr USHORT kHidUsagePhoneMute = 0x2F;
static constexpr USHORT kHidUsageCallMuteToggle = 0xE1;
static constexpr unsigned kHidSystemMuteMask = 1;
static constexpr unsigned kHidPhoneMuteMask = 2;
static constexpr unsigned kHidCallMuteMask = 4;

struct StandardHidMetadata {
    USHORT vendorId = 0;
    USHORT productId = 0;
    USHORT usagePage = 0;
    USHORT usage = 0;
    USHORT inputReportLength = 0;
    USHORT outputReportLength = 0;
    USHORT featureReportLength = 0;
    bool systemMicrophoneMute = false;
    bool phoneMute = false;
    bool callMuteToggle = false;
    std::wstring manufacturer;
    std::wstring product;

    bool HasStandardMuteUsage() const {
        return systemMicrophoneMute || phoneMute || callMuteToggle;
    }
};

struct StandardHidRuntime {
    std::unordered_map<unsigned, unsigned> activeMasks;
    std::unordered_map<unsigned, std::vector<BYTE>> previousReports;
};

struct PendingStandardHidAction {
    bool valid = false;
    bool beforeAudioAvailable = false;
    bool beforeAudioMuted = false;
    std::optional<bool> beforeCallMuted;
    std::optional<bool> explicitTargetMuted;
    unsigned usageMask = 0;
};

static std::unordered_map<HANDLE, StandardHidRuntime> g_standardHidRuntime;
static PendingStandardHidAction g_pendingStandardHidAction;

static std::wstring Hex4(USHORT value) {
    wchar_t text[5];
    StringCchPrintfW(text, ARRAYSIZE(text), L"%04X", value);
    return text;
}

static bool GetRawHidPreparsedData(HANDLE device, std::vector<BYTE>& storage,
                                   HIDP_CAPS& caps) {
    UINT size = 0;
    GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &size);
    if (!size) return false;
    storage.resize(size);
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, storage.data(),
                               &size) == static_cast<UINT>(-1))
        return false;
    auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(storage.data());
    return HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS;
}

static bool ButtonCapsContains(const HIDP_BUTTON_CAPS& caps, USHORT page,
                               USHORT usage) {
    if (caps.UsagePage != page) return false;
    return caps.IsRange
               ? usage >= caps.Range.UsageMin && usage <= caps.Range.UsageMax
               : caps.NotRange.Usage == usage;
}

static bool ValueCapsContains(const HIDP_VALUE_CAPS& caps, USHORT page,
                              USHORT usage) {
    if (caps.UsagePage != page) return false;
    return caps.IsRange
               ? usage >= caps.Range.UsageMin && usage <= caps.Range.UsageMax
               : caps.NotRange.Usage == usage;
}

static bool InputCapsContainUsage(PHIDP_PREPARSED_DATA preparsed,
                                  const HIDP_CAPS& caps, USHORT page,
                                  USHORT usage) {
    USHORT buttonCount = caps.NumberInputButtonCaps;
    if (buttonCount) {
        std::vector<HIDP_BUTTON_CAPS> buttons(buttonCount);
        if (HidP_GetButtonCaps(HidP_Input, buttons.data(), &buttonCount,
                               preparsed) == HIDP_STATUS_SUCCESS) {
            for (USHORT index = 0; index < buttonCount; index++) {
                if (ButtonCapsContains(buttons[index], page, usage))
                    return true;
            }
        }
    }
    USHORT valueCount = caps.NumberInputValueCaps;
    if (valueCount) {
        std::vector<HIDP_VALUE_CAPS> values(valueCount);
        if (HidP_GetValueCaps(HidP_Input, values.data(), &valueCount,
                              preparsed) == HIDP_STATUS_SUCCESS) {
            for (USHORT index = 0; index < valueCount; index++) {
                if (ValueCapsContains(values[index], page, usage))
                    return true;
            }
        }
    }
    return false;
}

static std::wstring RawHidString(HANDLE rawDevice, bool product) {
    UINT characters = 0;
    GetRawInputDeviceInfoW(rawDevice, RIDI_DEVICENAME, nullptr, &characters);
    if (!characters) return L"";
    std::vector<wchar_t> path(characters + 1);
    if (GetRawInputDeviceInfoW(rawDevice, RIDI_DEVICENAME, path.data(),
                               &characters) == static_cast<UINT>(-1))
        return L"";
    HANDLE device = CreateFileW(path.data(), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (device == INVALID_HANDLE_VALUE) return L"";
    wchar_t text[256]{};
    BOOLEAN read = product ? HidD_GetProductString(device, text, sizeof(text))
                           : HidD_GetManufacturerString(device, text,
                                                       sizeof(text));
    CloseHandle(device);
    return read ? text : L"";
}

static bool ReadStandardHidMetadata(HANDLE device,
                                    StandardHidMetadata& metadata,
                                    std::vector<BYTE>* preparsedStorage =
                                        nullptr) {
    RID_DEVICE_INFO info{};
    info.cbSize = sizeof(info);
    UINT infoSize = sizeof(info);
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &info, &infoSize) ==
            static_cast<UINT>(-1) ||
        info.dwType != RIM_TYPEHID)
        return false;

    std::vector<BYTE> localStorage;
    std::vector<BYTE>& storage = preparsedStorage ? *preparsedStorage
                                                  : localStorage;
    HIDP_CAPS caps{};
    if (!GetRawHidPreparsedData(device, storage, caps)) return false;
    auto* preparsed = reinterpret_cast<PHIDP_PREPARSED_DATA>(storage.data());

    metadata = {};
    metadata.vendorId = static_cast<USHORT>(info.hid.dwVendorId);
    metadata.productId = static_cast<USHORT>(info.hid.dwProductId);
    metadata.usagePage = info.hid.usUsagePage;
    metadata.usage = info.hid.usUsage;
    metadata.inputReportLength = caps.InputReportByteLength;
    metadata.outputReportLength = caps.OutputReportByteLength;
    metadata.featureReportLength = caps.FeatureReportByteLength;
    metadata.systemMicrophoneMute = InputCapsContainUsage(
        preparsed, caps, kHidUsagePageGeneric,
        kHidUsageSystemMicrophoneMute);
    metadata.phoneMute = InputCapsContainUsage(
        preparsed, caps, kHidUsagePageTelephony, kHidUsagePhoneMute);
    metadata.callMuteToggle = InputCapsContainUsage(
        preparsed, caps, kHidUsagePageTelephony, kHidUsageCallMuteToggle);
    metadata.manufacturer = RawHidString(device, false);
    metadata.product = RawHidString(device, true);
    return true;
}

static bool UsageAsserted(PHIDP_PREPARSED_DATA preparsed, USHORT page,
                          USHORT usage, const BYTE* report,
                          ULONG reportLength) {
    std::vector<USAGE> usages(128);
    ULONG usageCount = static_cast<ULONG>(usages.size());
    NTSTATUS status = HidP_GetUsages(
        HidP_Input, page, 0, usages.data(), &usageCount, preparsed,
        reinterpret_cast<PCHAR>(const_cast<BYTE*>(report)), reportLength);
    if (status == HIDP_STATUS_SUCCESS) {
        for (ULONG index = 0; index < usageCount; index++) {
            if (usages[index] == usage) return true;
        }
    }
    ULONG value = 0;
    return HidP_GetUsageValue(
               HidP_Input, page, 0, usage, &value, preparsed,
               reinterpret_cast<PCHAR>(const_cast<BYTE*>(report)),
               reportLength) == HIDP_STATUS_SUCCESS &&
           value != 0;
}

static unsigned StandardMuteMask(PHIDP_PREPARSED_DATA preparsed,
                                 const StandardHidMetadata& metadata,
                                 const BYTE* report, ULONG reportLength) {
    unsigned mask = 0;
    if (metadata.systemMicrophoneMute &&
        UsageAsserted(preparsed, kHidUsagePageGeneric,
                      kHidUsageSystemMicrophoneMute, report, reportLength))
        mask |= kHidSystemMuteMask;
    if (metadata.phoneMute &&
        UsageAsserted(preparsed, kHidUsagePageTelephony, kHidUsagePhoneMute,
                      report, reportLength))
        mask |= kHidPhoneMuteMask;
    if (metadata.callMuteToggle &&
        UsageAsserted(preparsed, kHidUsagePageTelephony,
                      kHidUsageCallMuteToggle, report, reportLength))
        mask |= kHidCallMuteMask;
    return mask;
}

static std::optional<bool> CurrentCallMuteState() {
    int selected = SelectedCall();
    if (selected == 0) return g_slackMuted.load();
    if (selected == 1) return g_teamsMuted.load();
    if (selected == 2 && g_zoomKnown.load()) return g_zoomMuted.load();
    return std::nullopt;
}

static void BeginStandardHidAction(
    unsigned usageMask,
    std::optional<bool> explicitTargetMuted = std::nullopt) {
    g_pendingStandardHidAction.valid = true;
    g_pendingStandardHidAction.beforeAudioAvailable =
        g_audioAvailable.load();
    g_pendingStandardHidAction.beforeAudioMuted = g_audioMuted.load();
    g_pendingStandardHidAction.beforeCallMuted = CurrentCallMuteState();
    g_pendingStandardHidAction.explicitTargetMuted = explicitTargetMuted;
    g_pendingStandardHidAction.usageMask = usageMask;
    KillTimer(g_mainWindow, kStandardHidTimer);
    SetTimer(g_mainWindow, kStandardHidTimer, 300, nullptr);
}

static void ResolveStandardHidAction() {
    PendingStandardHidAction action = g_pendingStandardHidAction;
    g_pendingStandardHidAction = {};
    if (!action.valid || g_settings.headsetMode == L"off" ||
        g_settings.headsetMode == L"statusOnly")
        return;

    bool currentAudioAvailable = g_audioAvailable.load();
    bool currentAudioMuted = g_audioMuted.load();
    std::optional<bool> currentCallMuted = CurrentCallMuteState();
    bool audioChanged = action.beforeAudioAvailable && currentAudioAvailable &&
                        action.beforeAudioMuted != currentAudioMuted;
    bool callChanged = action.beforeCallMuted && currentCallMuted &&
                       *action.beforeCallMuted != *currentCallMuted;
    bool systemFirst = (action.usageMask & kHidSystemMuteMask) != 0;
    bool targetMuted = false;
    if (action.explicitTargetMuted)
        targetMuted = *action.explicitTargetMuted;
    else if (systemFirst && audioChanged)
        targetMuted = currentAudioMuted;
    else if (!systemFirst && callChanged)
        targetMuted = *currentCallMuted;
    else if (audioChanged)
        targetMuted = currentAudioMuted;
    else if (callChanged)
        targetMuted = *currentCallMuted;
    else if (systemFirst || !action.beforeCallMuted)
        targetMuted = !action.beforeAudioMuted;
    else
        targetMuted = !*action.beforeCallMuted;

    bool maySynchronize = targetMuted || g_settings.headsetMode == L"full";
    if (!maySynchronize) return;
    if (g_settings.headsetSyncWindows &&
        (!currentAudioAvailable || currentAudioMuted != targetMuted))
        QueueWindowsMute(targetMuted);
    if (g_settings.headsetSyncCalls)
        QueueCallMuteState(targetMuted);
    RecordDiagnosticEvent(
        std::wstring(L"Standard HID action resolved to ") +
        (targetMuted ? L"muted" : L"unmuted"));
}

static std::wstring StandardUsageDescription(
    const StandardHidMetadata& metadata) {
    std::wstring result;
    auto append = [&](bool present, PCWSTR name) {
        if (!present) return;
        if (!result.empty()) result += L", ";
        result += name;
    };
    append(metadata.systemMicrophoneMute, L"System Microphone Mute");
    append(metadata.phoneMute, L"Phone Mute");
    append(metadata.callMuteToggle, L"Call Mute Toggle");
    return result;
}

static void RefreshStandardHidDevices() {
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) ==
            static_cast<UINT>(-1) ||
        !count) {
        UpdateStandardHidSource(false, L"", L"");
        return;
    }
    std::vector<RAWINPUTDEVICELIST> devices(count);
    if (GetRawInputDeviceList(devices.data(), &count,
                              sizeof(RAWINPUTDEVICELIST)) ==
        static_cast<UINT>(-1)) {
        UpdateStandardHidSource(false, L"", L"");
        return;
    }
    for (UINT index = 0; index < count; index++) {
        if (devices[index].dwType != RIM_TYPEHID) continue;
        StandardHidMetadata metadata;
        if (!ReadStandardHidMetadata(devices[index].hDevice, metadata) ||
            !metadata.HasStandardMuteUsage())
            continue;
        std::wstring name = metadata.product.empty()
                                ? L"HID headset VID " + Hex4(metadata.vendorId) +
                                      L" / PID " + Hex4(metadata.productId)
                                : metadata.product;
        UpdateStandardHidSource(true, name,
                                StandardUsageDescription(metadata));
        return;
    }
    UpdateStandardHidSource(false, L"", L"");
}

static void RecordSanitizedReportChange(
    const StandardHidMetadata& metadata, StandardHidRuntime& runtime,
    const BYTE* report, DWORD reportLength, unsigned reportKey,
    unsigned muteMask) {
    auto& previous = runtime.previousReports[reportKey];
    std::wstring event = L"HID VID " + Hex4(metadata.vendorId) + L" / PID " +
                         Hex4(metadata.productId) + L", report " +
                         std::to_wstring(reportKey) + L", length " +
                         std::to_wstring(reportLength);
    if (previous.size() == reportLength) {
        std::wstring offsets;
        for (DWORD index = 0; index < reportLength; index++) {
            if (previous[index] == report[index]) continue;
            if (!offsets.empty()) offsets += L",";
            offsets += std::to_wstring(index);
        }
        if (offsets.empty()) return;
        event += L", changed byte offsets [" + offsets + L"]";
    } else {
        event += L", initial sanitized report";
    }
    if (muteMask) event += L", standard mute usage asserted";
    previous.assign(report, report + reportLength);
    RecordDiagnosticEvent(event);
}

static void ProcessStandardHidRawInput(HRAWINPUT inputHandle) {
    UINT size = 0;
    GetRawInputData(inputHandle, RID_INPUT, nullptr, &size,
                    sizeof(RAWINPUTHEADER));
    if (!size) return;
    std::vector<BYTE> inputStorage(size);
    if (GetRawInputData(inputHandle, RID_INPUT, inputStorage.data(), &size,
                        sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        return;
    auto* input = reinterpret_cast<RAWINPUT*>(inputStorage.data());
    if (input->header.dwType != RIM_TYPEHID) return;

    std::vector<BYTE> preparsedStorage;
    StandardHidMetadata metadata;
    if (!ReadStandardHidMetadata(input->header.hDevice, metadata,
                                 &preparsedStorage) ||
        !metadata.HasStandardMuteUsage())
        return;
    auto* preparsed =
        reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedStorage.data());
    auto& runtime = g_standardHidRuntime[input->header.hDevice];
    for (DWORD index = 0; index < input->data.hid.dwCount; index++) {
        const BYTE* report = input->data.hid.bRawData +
                             index * input->data.hid.dwSizeHid;
        DWORD reportLength = input->data.hid.dwSizeHid;
        unsigned reportKey = reportLength ? report[0] : 0;
        unsigned mask = StandardMuteMask(preparsed, metadata, report,
                                         reportLength);
        RecordSanitizedReportChange(metadata, runtime, report, reportLength,
                                    reportKey, mask);
        unsigned previousMask = runtime.activeMasks[reportKey];
        unsigned asserted =
            (mask & ~previousMask) &
            (kHidSystemMuteMask | kHidCallMuteMask);
        unsigned phoneChanged =
            (mask ^ previousMask) & kHidPhoneMuteMask;
        runtime.activeMasks[reportKey] = mask;
        if (!asserted && !phoneChanged) continue;
        std::wstring name = metadata.product.empty()
                                ? L"HID headset VID " + Hex4(metadata.vendorId) +
                                      L" / PID " + Hex4(metadata.productId)
                                : metadata.product;
        UpdateStandardHidSource(true, name,
                                StandardUsageDescription(metadata));
        RecordDiagnosticEvent(L"Standard HID mute control changed: " +
                              StandardUsageDescription(metadata));
        std::optional<bool> explicitTarget;
        if (phoneChanged)
            explicitTarget = (mask & kHidPhoneMuteMask) != 0;
        BeginStandardHidAction(asserted | phoneChanged, explicitTarget);
    }
}

static bool RegisterStandardHidInput() {
    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage = kHidUsagePageGeneric;
    devices[0].usUsage = kHidUsageSystemControl;
    devices[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    devices[0].hwndTarget = g_mainWindow;
    devices[1].usUsagePage = kHidUsagePageTelephony;
    devices[1].usUsage = 0;
    devices[1].dwFlags =
        RIDEV_INPUTSINK | RIDEV_DEVNOTIFY | RIDEV_PAGEONLY;
    devices[1].hwndTarget = g_mainWindow;
    return RegisterRawInputDevices(devices, ARRAYSIZE(devices),
                                   sizeof(devices[0])) != FALSE;
}

static DWORD WINAPI HeadsetThreadProc(void*) {
    auto adapter = CreateSteelSeriesHeadsetAdapter();
    bool connected = false;
    bool known = false;
    bool previousMuted = false;
    while (WaitForSingleObject(g_stopEvent, 0) == WAIT_TIMEOUT &&
           !g_exiting.load()) {
        if (!connected) {
            connected = adapter->TryConnect();
            if (!connected) {
                UpdateSteelSeriesSource(false, false, L"", L"");
                if (WaitForSingleObject(g_stopEvent, 2000) != WAIT_TIMEOUT)
                    break;
                continue;
            }
            RecordDiagnosticEvent(L"SteelSeries vendor adapter connected");
            known = false;
        }
        HeadsetAdapterObservation observation;
        if (!adapter->Poll(observation)) {
            adapter->Disconnect();
            connected = false;
            known = false;
            UpdateSteelSeriesSource(false, false, L"", L"");
            RecordDiagnosticEvent(
                L"SteelSeries vendor adapter disconnected after read failure");
            if (WaitForSingleObject(g_stopEvent, 500) != WAIT_TIMEOUT) break;
            continue;
        }
        if (!observation.available || !observation.stateKnown) {
            known = false;
            UpdateSteelSeriesSource(false, false, L"", L"");
            if (WaitForSingleObject(g_stopEvent,
                                    g_settings.headsetPollInterval) !=
                WAIT_TIMEOUT)
                break;
            continue;
        }
        bool stateChanged = !known || observation.muted != previousMuted;
        UpdateSteelSeriesSource(true, observation.muted,
                                observation.deviceName, observation.detail);
        if (stateChanged) {
            RecordDiagnosticEvent(
                std::wstring(L"SteelSeries physical state: ") +
                (observation.muted ? L"muted" : L"unmuted"));
        }
        bool syncMute = g_settings.headsetMode == L"full" ||
                        g_settings.headsetMode == L"muteOnly";
        bool syncUnmute = g_settings.headsetMode == L"full";
        if (observation.muted && syncMute) {
            if (g_settings.headsetSyncWindows && !g_audioMuted.load())
                QueueWindowsMute(true);
            if (g_settings.headsetSyncCalls) QueueCallMuteState(true);
        } else if (!observation.muted && syncUnmute && known &&
                   previousMuted) {
            if (g_settings.headsetSyncWindows) QueueWindowsMute(false);
            if (g_settings.headsetSyncCalls) QueueCallMuteState(false);
        }
        known = true;
        previousMuted = observation.muted;
        if (WaitForSingleObject(g_stopEvent,
                                g_settings.headsetPollInterval) != WAIT_TIMEOUT)
            break;
    }
    adapter->Disconnect();
    UpdateSteelSeriesSource(false, false, L"", L"");
    return 0;
}

static bool StartWorkers() {
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_stopEvent || !g_wakeEvent) {
        if (g_stopEvent) CloseHandle(g_stopEvent);
        if (g_wakeEvent) CloseHandle(g_wakeEvent);
        g_stopEvent = g_wakeEvent = nullptr;
        return false;
    }
    g_audioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0,
                                 nullptr);
    bool headsetCalls = g_settings.headsetSyncCalls &&
                        (g_settings.headsetMode == L"full" ||
                         g_settings.headsetMode == L"muteOnly");
    if (g_settings.showCallStateIcon || g_settings.slackWarning ||
        g_settings.slackToggle || g_settings.teamsWarning ||
        g_settings.teamsToggle || g_settings.zoomWarning ||
        g_settings.zoomToggle || headsetCalls)
        g_callThread = CreateThread(nullptr, 0, CallThreadProc, nullptr, 0,
                                    nullptr);
    if (g_settings.headsetMode != L"off")
        g_headsetThread = CreateThread(nullptr, 0, HeadsetThreadProc, nullptr,
                                       0, nullptr);
    return g_audioThread != nullptr;
}

static void StopWorkers() {
    if (g_stopEvent) SetEvent(g_stopEvent);
    HANDLE threads[] = {g_headsetThread, g_callThread, g_audioThread};
    for (HANDLE thread : threads) {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }
    if (g_stopEvent) CloseHandle(g_stopEvent);
    if (g_wakeEvent) CloseHandle(g_wakeEvent);
    g_headsetThread = g_callThread = g_audioThread = nullptr;
    g_stopEvent = g_wakeEvent = nullptr;
}

static void AddRoundedRect(GraphicsPath& path, RectF rectangle, REAL radius) {
    REAL diameter = radius * 2;
    path.AddArc(rectangle.X, rectangle.Y, diameter, diameter, 180, 90);
    path.AddArc(rectangle.GetRight() - diameter, rectangle.Y, diameter,
                diameter, 270, 90);
    path.AddArc(rectangle.GetRight() - diameter,
                rectangle.GetBottom() - diameter, diameter, diameter, 0, 90);
    path.AddArc(rectangle.X, rectangle.GetBottom() - diameter, diameter,
                diameter, 90, 90);
    path.CloseFigure();
}

static HICON CreateMicrophoneIcon() {
    Bitmap bitmap(32, 32, PixelFormat32bppARGB);
    Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));
    bool available = g_audioAvailable.load();
    HeadsetStatus headset = GetHeadsetStatus();
    bool muted = g_audioMuted.load() ||
                 (headset.stateKnown && headset.muted);
    float peak = muted ? 0 : std::clamp(g_audioPeak.load(), 0.0f, 1.0f);
    GraphicsPath body;
    AddRoundedRect(body, RectF(10, 3, 12, 18), 6);
    SolidBrush base(available ? Color(255, 112, 112, 112)
                              : Color(255, 75, 75, 75));
    graphics.FillPath(&base, &body);
    if (peak > 0) {
        graphics.SetClip(&body);
        SolidBrush level(Color(255, 75, 210, 125));
        REAL height = 18 * peak;
        graphics.FillRectangle(&level, 10.0f, 21.0f - height, 12.0f, height);
        graphics.ResetClip();
    }
    Pen outline(Color(255, 225, 225, 225), 1.5f);
    graphics.DrawPath(&outline, &body);
    Pen stand(Color(255, 225, 225, 225), 2.0f);
    stand.SetStartCap(LineCapRound);
    stand.SetEndCap(LineCapRound);
    graphics.DrawArc(&stand, RectF(6, 9, 20, 17), 0, 180);
    graphics.DrawLine(&stand, 16, 25, 16, 29);
    graphics.DrawLine(&stand, 11, 29, 21, 29);
    if (muted) {
        Pen slash(Color(255, 239, 68, 68), 3.0f);
        slash.SetStartCap(LineCapRound);
        slash.SetEndCap(LineCapRound);
        graphics.DrawLine(&slash, 6, 5, 26, 27);
    }
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    return icon;
}

static void DrawSlackLogo(Graphics& graphics) {
    Pen cyan(Color(255, 54, 197, 240), 4.2f);
    Pen green(Color(255, 46, 182, 125), 4.2f);
    Pen yellow(Color(255, 236, 178, 46), 4.2f);
    Pen red(Color(255, 224, 30, 90), 4.2f);
    for (Pen* pen : {&cyan, &green, &yellow, &red}) {
        pen->SetStartCap(LineCapRound);
        pen->SetEndCap(LineCapRound);
    }
    graphics.DrawLine(&cyan, 12, 6, 12, 17);
    graphics.DrawLine(&green, 17, 12, 26, 12);
    graphics.DrawLine(&yellow, 20, 15, 20, 26);
    graphics.DrawLine(&red, 6, 20, 15, 20);
}

static void DrawTeamsLogo(Graphics& graphics) {
    SolidBrush purple(Color(255, 91, 95, 199));
    graphics.FillEllipse(&purple, 19, 5, 8, 8);
    GraphicsPath body;
    AddRoundedRect(body, RectF(11, 10, 17, 16), 3);
    graphics.FillPath(&purple, &body);
    SolidBrush front(Color(255, 98, 100, 210));
    GraphicsPath tile;
    AddRoundedRect(tile, RectF(4, 8, 15, 16), 2.5f);
    graphics.FillPath(&front, &tile);
    FontFamily family(L"Segoe UI");
    Font font(&family, 12, FontStyleBold, UnitPixel);
    SolidBrush white(Color(255, 255, 255, 255));
    graphics.DrawString(L"T", 1, &font, PointF(7, 9), &white);
}

static void DrawZoomLogo(Graphics& graphics) {
    SolidBrush blue(Color(255, 45, 140, 255));
    graphics.FillEllipse(&blue, 3, 3, 26, 26);
    SolidBrush white(Color(255, 255, 255, 255));
    GraphicsPath camera;
    AddRoundedRect(camera, RectF(8, 10, 11, 11), 2);
    graphics.FillPath(&white, &camera);
    PointF points[] = {{19, 13}, {25, 10}, {25, 21}, {19, 18}};
    graphics.FillPolygon(&white, points, ARRAYSIZE(points));
}

static HICON CreateCallIcon() {
    Bitmap bitmap(32, 32, PixelFormat32bppARGB);
    Graphics graphics(&bitmap);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));
    int selected = SelectedCall();
    if (selected == 0) DrawSlackLogo(graphics);
    else if (selected == 1) DrawTeamsLogo(graphics);
    else DrawZoomLogo(graphics);
    bool muted = selected == 0   ? g_slackMuted.load()
                 : selected == 1 ? g_teamsMuted.load()
                                 : g_zoomMuted.load();
    bool known = selected != 2 || g_zoomKnown.load();
    if (muted) {
        Pen slash(Color(255, 239, 68, 68), 3.0f);
        slash.SetStartCap(LineCapRound);
        slash.SetEndCap(LineCapRound);
        graphics.DrawLine(&slash, 5, 5, 27, 27);
    } else if (!known) {
        FontFamily family(L"Segoe UI");
        Font font(&family, 12, FontStyleBold, UnitPixel);
        SolidBrush white(Color(255, 255, 255, 255));
        SolidBrush dark(Color(230, 35, 35, 35));
        graphics.FillEllipse(&dark, 20, 20, 11, 11);
        graphics.DrawString(L"?", 1, &font, PointF(22, 19), &white);
    }
    int activeCount = static_cast<int>(g_slackActive.load()) +
                      static_cast<int>(g_teamsActive.load()) +
                      static_cast<int>(g_zoomActive.load());
    if (activeCount > 1) {
        SolidBrush dark(Color(240, 35, 35, 35));
        SolidBrush white(Color(255, 255, 255, 255));
        graphics.FillEllipse(&dark, 21, 21, 10, 10);
        Pen plus(Color(255, 255, 255, 255), 1.5f);
        graphics.DrawLine(&plus, 23, 26, 29, 26);
        graphics.DrawLine(&plus, 26, 23, 26, 29);
    }
    HICON icon = nullptr;
    bitmap.GetHICON(&icon);
    return icon;
}

static std::wstring CallList(bool withStates) {
    std::wstring result;
    auto append = [&](bool active, bool known, bool muted, PCWSTR name) {
        if (!active) return;
        if (!result.empty()) result += withStates ? L"\n" : L", ";
        result += name;
        if (withStates) {
            result += L": ";
            result += !known ? L"Unknown" : muted ? L"Muted" : L"Unmuted";
        }
    };
    append(g_slackActive.load(), true, g_slackMuted.load(), L"Slack");
    append(g_teamsActive.load(), true, g_teamsMuted.load(), L"Teams");
    append(g_zoomActive.load(), g_zoomKnown.load(), g_zoomMuted.load(),
           L"Zoom");
    return result;
}

static std::wstring MicTooltip() {
    std::wstring suffix = L"\n" + std::to_wstring(g_audioVolume.load()) + L"%";
    if (g_forceVolume.load())
        suffix += L" | Lock " + std::to_wstring(g_forcedVolume.load()) + L"%";
    if (!g_audioAvailable.load()) suffix += L" | Unavailable";
    else if (g_audioMuted.load()) suffix += L" | Win muted";
    HeadsetStatus headset = GetHeadsetStatus();
    if (headset.detected) {
        suffix += L" | Headset ";
        suffix += headset.stateKnown
                      ? (headset.muted ? L"muted" : L"unmuted")
                      : L"button detected";
    }
    suffix += L"\nLeft: Win mute | Wheel: volume";
    suffix += !CallList(false).empty()
                  ? L"\nRight: Toggle call | Middle: settings"
                  : L"\nMiddle: settings";

    // NOTIFYICONDATA::szTip holds at most 127 visible characters. Preserve
    // every control instruction and shorten only the device name if needed.
    constexpr size_t maxTooltipLength = 127;
    std::wstring device = GetDeviceName();
    size_t availableForDevice =
        suffix.size() < maxTooltipLength ? maxTooltipLength - suffix.size() : 0;
    if (device.size() > availableForDevice) {
        if (availableForDevice > 1) {
            device.resize(availableForDevice - 1);
            device += L"…";
        } else {
            device.clear();
        }
    }
    return device + suffix;
}

static std::wstring CallTooltip() {
    std::wstring text = L"Call microphone\n" + CallList(true);
    text += L"\nLeft: focus call | Right: mute/unmute | Middle: Settings";
    return text;
}

static void CopyTooltip(wchar_t* destination, size_t count,
                        const std::wstring& text) {
    StringCchCopyNW(destination, count, text.c_str(), count - 1);
}

static NOTIFYICONDATAW BaseNotifyData(UINT id, const GUID& guid) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = g_mainWindow;
    data.uID = id;
    data.guidItem = guid;
    return data;
}

static bool AddTrayIcon(UINT id, const GUID& guid, HICON icon,
                        const std::wstring& tooltip) {
    NOTIFYICONDATAW data = BaseNotifyData(id, guid);
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
    data.uCallbackMessage = kTrayCallback;
    data.hIcon = icon;
    CopyTooltip(data.szTip, ARRAYSIZE(data.szTip), tooltip);
    if (!Shell_NotifyIconW(NIM_ADD, &data)) return false;
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

static void RemoveTrayIcon(UINT id, const GUID& guid) {
    NOTIFYICONDATAW data = BaseNotifyData(id, guid);
    data.uFlags = NIF_GUID;
    Shell_NotifyIconW(NIM_DELETE, &data);
}

static void UpdateTrayIcons(bool forceAdd = false) {
    HICON mic = CreateMicrophoneIcon();
    if (!g_micIconAdded || forceAdd) {
        if (forceAdd && g_micIconAdded) RemoveTrayIcon(kMicIconId, kMicIconGuid);
        g_micIconAdded = AddTrayIcon(kMicIconId, kMicIconGuid, mic,
                                     MicTooltip());
    } else {
        NOTIFYICONDATAW data = BaseNotifyData(kMicIconId, kMicIconGuid);
        data.uFlags = NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
        data.hIcon = mic;
        CopyTooltip(data.szTip, ARRAYSIZE(data.szTip), MicTooltip());
        Shell_NotifyIconW(NIM_MODIFY, &data);
    }
    if (g_micIcon) DestroyIcon(g_micIcon);
    g_micIcon = mic;

    bool showCall = g_settings.showCallStateIcon && SelectedCall() >= 0;
    if (showCall) {
        HICON call = CreateCallIcon();
        if (!g_callIconAdded || forceAdd) {
            if (forceAdd && g_callIconAdded)
                RemoveTrayIcon(kCallIconId, kCallIconGuid);
            g_callIconAdded = AddTrayIcon(kCallIconId, kCallIconGuid, call,
                                          CallTooltip());
        } else {
            NOTIFYICONDATAW data = BaseNotifyData(kCallIconId, kCallIconGuid);
            data.uFlags = NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
            data.hIcon = call;
            CopyTooltip(data.szTip, ARRAYSIZE(data.szTip), CallTooltip());
            Shell_NotifyIconW(NIM_MODIFY, &data);
        }
        if (g_callIcon) DestroyIcon(g_callIcon);
        g_callIcon = call;
    } else if (g_callIconAdded) {
        RemoveTrayIcon(kCallIconId, kCallIconGuid);
        g_callIconAdded = false;
        if (g_callIcon) DestroyIcon(g_callIcon);
        g_callIcon = nullptr;
    }
}

static void ShowWarningBalloon() {
    std::wstring apps;
    if (g_slackWarning.load()) apps += L"Slack";
    if (g_teamsWarning.load()) {
        if (!apps.empty()) apps += L", ";
        apps += L"Microsoft Teams";
    }
    if (g_zoomWarning.load()) {
        if (!apps.empty()) apps += L", ";
        apps += L"Zoom";
    }
    if (apps.empty()) return;
    NOTIFYICONDATAW data = BaseNotifyData(kMicIconId, kMicIconGuid);
    data.uFlags = NIF_INFO | NIF_GUID;
    StringCchCopyW(data.szInfoTitle, ARRAYSIZE(data.szInfoTitle),
                   L"Your call microphone is muted");
    std::wstring body = L"You're speaking, but " + apps +
                        (apps.find(L", ") == std::wstring::npos
                             ? L" is muted."
                             : L" are muted.");
    StringCchCopyW(data.szInfo, ARRAYSIZE(data.szInfo), body.c_str());
    data.dwInfoFlags = NIIF_WARNING | NIIF_RESPECT_QUIET_TIME;
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

static std::wstring BuildHeadsetDiagnostics() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    std::wstringstream output;
    output << L"Microphone Activity Widget — headset diagnostics\r\n";
    output << L"Generated: " << now.wYear << L"-";
    output.width(2);
    output.fill(L'0');
    output << now.wMonth << L"-";
    output.width(2);
    output << now.wDay << L" ";
    output.width(2);
    output << now.wHour << L":";
    output.width(2);
    output << now.wMinute << L":";
    output.width(2);
    output << now.wSecond << L"\r\n";
    output << L"Privacy: device paths, serial numbers, and raw HID values are "
              L"not included.\r\n\r\n";

    HeadsetStatus status = GetHeadsetStatus();
    output << L"Current detection\r\n";
    output << L"  Method: " << HeadsetMethodName(status.method) << L"\r\n";
    output << L"  Confidence: "
           << HeadsetConfidenceName(status.confidence) << L"\r\n";
    output << L"  Device: "
           << (status.deviceName.empty() ? L"(none)" : status.deviceName)
           << L"\r\n";
    output << L"  State: "
           << (!status.stateKnown ? L"unknown"
                                  : status.muted ? L"muted" : L"unmuted")
           << L"\r\n";
    output << L"  Detail: " << status.detail << L"\r\n\r\n";

    output << L"Synchronization settings\r\n";
    output << L"  Mode: " << g_settings.headsetMode << L"\r\n";
    output << L"  Windows: "
           << (g_settings.headsetSyncWindows ? L"enabled" : L"disabled")
           << L"\r\n";
    output << L"  Calls: "
           << (g_settings.headsetSyncCalls ? L"enabled" : L"disabled")
           << L"\r\n\r\n";

    output << L"Vendor adapter registry\r\n";
    for (const auto& slot : GetVendorAdapterSlots()) {
        output << L"  VID " << Hex4(slot.vendorId) << L": "
               << slot.vendorName << L" — "
               << (slot.implemented ? L"implemented" : L"extension slot")
               << L" (" << slot.adapterId << L")\r\n";
    }
    output << L"  Other vendors can implement IHeadsetMuteAdapter.\r\n\r\n";

    output << L"Sanitized HID descriptor summaries\r\n";
    UINT count = 0;
    if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) !=
            static_cast<UINT>(-1) &&
        count) {
        std::vector<RAWINPUTDEVICELIST> devices(count);
        if (GetRawInputDeviceList(devices.data(), &count,
                                  sizeof(RAWINPUTDEVICELIST)) !=
            static_cast<UINT>(-1)) {
            unsigned written = 0;
            for (UINT index = 0; index < count; index++) {
                if (devices[index].dwType != RIM_TYPEHID) continue;
                StandardHidMetadata metadata;
                if (!ReadStandardHidMetadata(devices[index].hDevice,
                                             metadata))
                    continue;
                const VendorAdapterSlot* slot =
                    FindVendorAdapterSlot(metadata.vendorId);
                output << L"  [" << ++written << L"] VID "
                       << Hex4(metadata.vendorId) << L" / PID "
                       << Hex4(metadata.productId) << L"\r\n";
                output << L"      Manufacturer: "
                       << (metadata.manufacturer.empty()
                               ? L"(not reported)"
                               : metadata.manufacturer)
                       << L"\r\n";
                output << L"      Product: "
                       << (metadata.product.empty() ? L"(not reported)"
                                                    : metadata.product)
                       << L"\r\n";
                output << L"      Top-level usage: page 0x"
                       << Hex4(metadata.usagePage) << L", usage 0x"
                       << Hex4(metadata.usage) << L"\r\n";
                output << L"      Reports: input "
                       << metadata.inputReportLength << L", output "
                       << metadata.outputReportLength << L", feature "
                       << metadata.featureReportLength << L" bytes\r\n";
                output << L"      Standard mute usages: "
                       << (metadata.HasStandardMuteUsage()
                               ? StandardUsageDescription(metadata)
                               : L"none")
                       << L"\r\n";
                output << L"      Vendor adapter: "
                       << (slot ? slot->vendorName : L"unregistered")
                       << (slot && slot->implemented ? L" (implemented)"
                                                     : L"")
                       << L"\r\n";
            }
            if (!written) output << L"  No HID devices were readable.\r\n";
        }
    } else {
        output << L"  No HID devices were enumerated.\r\n";
    }

    output << L"\r\nSanitized report-change log\r\n";
    AcquireSRWLockShared(&g_diagnosticLock);
    if (g_diagnosticEvents.empty()) {
        output << L"  No report changes observed during this run.\r\n";
    } else {
        for (const auto& event : g_diagnosticEvents)
            output << L"  " << event << L"\r\n";
    }
    ReleaseSRWLockShared(&g_diagnosticLock);
    return output.str();
}

static bool WriteUtf8File(const std::wstring& path,
                          const std::wstring& content) {
    int required = WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                                       static_cast<int>(content.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (required <= 0) return false;
    std::vector<char> bytes(static_cast<size_t>(required));
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                        static_cast<int>(content.size()), bytes.data(),
                        required, nullptr, nullptr);
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool success = WriteFile(file, bytes.data(),
                             static_cast<DWORD>(bytes.size()), &written,
                             nullptr) != FALSE &&
                   written == bytes.size();
    CloseHandle(file);
    return success;
}

static void ExportHeadsetDiagnostics(HWND owner) {
    wchar_t path[MAX_PATH] =
        L"MicrophoneActivityWidget-headset-diagnostics.txt";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = L"Text files (*.txt)\0*.txt\0All files (*.*)\0*.*\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = ARRAYSIZE(path);
    dialog.lpstrDefExt = L"txt";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&dialog)) return;
    if (WriteUtf8File(path, BuildHeadsetDiagnostics())) {
        MessageBoxW(owner,
                    L"The sanitized headset diagnostics were exported. No "
                    L"device paths, serial numbers, or raw HID values were "
                    L"included.",
                    kAppName, MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(owner, L"The diagnostics file could not be written.",
                    kAppName, MB_OK | MB_ICONERROR);
    }
}

enum ControlId {
    IDC_TAB = 100,
    IDC_SAVE,
    IDC_CANCEL,
    IDC_DEFAULTS,
    IDC_ROLE = 200,
    IDC_VOLUME_STEP,
    IDC_FORCE_VOLUME,
    IDC_FORCED_VOLUME,
    IDC_UPDATE_INTERVAL,
    IDC_SENSITIVITY,
    IDC_SHOW_CALL_ICON,
    IDC_STARTUP,
    IDC_SLACK_WARNING = 300,
    IDC_SLACK_CUE,
    IDC_SLACK_TOGGLE,
    IDC_SLACK_MUTED_TEXT,
    IDC_SLACK_CALL_TEXT,
    IDC_SLACK_THRESHOLD,
    IDC_SLACK_DELAY,
    IDC_TEAMS_WARNING = 400,
    IDC_TEAMS_CUE,
    IDC_TEAMS_TOGGLE,
    IDC_TEAMS_MUTED_TEXT,
    IDC_TEAMS_CALL_TEXT,
    IDC_TEAMS_THRESHOLD,
    IDC_TEAMS_DELAY,
    IDC_ZOOM_WARNING = 500,
    IDC_ZOOM_CUE,
    IDC_ZOOM_TOGGLE,
    IDC_ZOOM_MUTED_TEXT,
    IDC_ZOOM_CALL_TEXT,
    IDC_ZOOM_THRESHOLD,
    IDC_ZOOM_DELAY,
    IDC_HEADSET_MODE = 600,
    IDC_HEADSET_WINDOWS,
    IDC_HEADSET_CALLS,
    IDC_HEADSET_INTERVAL,
    IDC_HEADSET_METHOD,
    IDC_HEADSET_CONFIDENCE,
    IDC_HEADSET_EXPORT,
    IDM_SETTINGS = 800,
    IDM_TOGGLE_CALLS,
    IDM_EXIT
};

static std::vector<std::pair<HWND, int>> g_pageControls;

static HWND AddControl(HWND parent, PCWSTR className, PCWSTR text, DWORD style,
                       int x, int y, int width, int height, int id, int page) {
    HWND control = CreateWindowExW(
        wcscmp(className, L"EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, className,
        text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_instance,
        nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont),
                 TRUE);
    if (page >= 0) g_pageControls.emplace_back(control, page);
    return control;
}

static void AddLabel(HWND parent, PCWSTR text, int x, int y, int width,
                     int page) {
    AddControl(parent, L"STATIC", text, SS_LEFT, x, y, width, 22, 0, page);
}

static void AddNote(HWND parent, PCWSTR text, int x, int y, int width,
                    int height, int page) {
    AddControl(parent, L"STATIC", text, SS_LEFT, x, y, width, height, 0,
               page);
}

static HWND AddEdit(HWND parent, int id, int x, int y, int width, int page) {
    return AddControl(parent, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, x,
                      y, width, 24, id, page);
}

static HWND AddCheck(HWND parent, PCWSTR text, int id, int x, int y,
                     int width, int page) {
    return AddControl(parent, L"BUTTON", text,
                      BS_AUTOCHECKBOX | WS_TABSTOP, x, y, width, 24, id,
                      page);
}

static void CreateGeneralPage(HWND window) {
    int page = 0;
    AddLabel(window, L"Default microphone role", 32, 58, 220, page);
    HWND role = AddControl(window, WC_COMBOBOXW, L"",
                           CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 270, 54,
                           270, 200, IDC_ROLE, page);
    SendMessageW(role, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"General / default input"));
    SendMessageW(role, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Communications input"));
    SendMessageW(role, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Multimedia input"));
    AddLabel(window, L"Volume per wheel notch (%)", 32, 102, 220, page);
    AddEdit(window, IDC_VOLUME_STEP, 270, 98, 90, page);
    AddLabel(window, L"Meter update interval (25–500 ms)", 32, 146, 230,
             page);
    AddEdit(window, IDC_UPDATE_INTERVAL, 270, 142, 90, page);
    AddLabel(window, L"Meter sensitivity (25–500%)", 32, 190, 220, page);
    AddEdit(window, IDC_SENSITIVITY, 270, 186, 90, page);
    AddCheck(window, L"Keep the microphone at a fixed volume",
             IDC_FORCE_VOLUME, 32, 228, 360, page);
    AddLabel(window, L"Forced microphone volume (0–100%)", 32, 270, 240,
             page);
    AddEdit(window, IDC_FORCED_VOLUME, 270, 266, 90, page);
    AddCheck(window, L"Show a separate active-call icon", IDC_SHOW_CALL_ICON,
             32, 306, 330, page);
    AddCheck(window, L"Start automatically when I sign in", IDC_STARTUP, 32,
             342, 330, page);
    AddNote(window,
            L"Left-click the microphone icon to mute Windows input. Scroll "
            L"over it to change volume; while volume lock is enabled, "
            L"scrolling also updates the forced target. Middle-click either "
            L"icon to reopen this panel.",
            32, 382, 520, 62, page);
}

static void CreateCallPage(HWND window, int page, PCWSTR appName, int base,
                           PCWSTR markerExample) {
    std::wstring title = std::wstring(appName) + L" integration";
    AddLabel(window, title.c_str(), 32, 58, 300, page);
    AddCheck(window, L"Warn when speaking while the call is muted", base, 32,
             94, 400, page);
    AddCheck(window, L"Play an audio cue when the warning begins", base + 1,
             32, 128, 400, page);
    AddCheck(window, L"Allow taskbar right-click mute/unmute", base + 2, 32,
             162, 400, page);
    AddLabel(window, L"Muted-button accessible text", 32, 210, 230, page);
    AddEdit(window, base + 3, 270, 206, 270, page);
    AddLabel(window, L"In-call marker text", 32, 254, 230, page);
    AddEdit(window, base + 4, 270, 250, 270, page);
    AddLabel(window, L"Speech threshold (1–100%)", 32, 298, 230, page);
    AddEdit(window, base + 5, 270, 294, 90, page);
    AddLabel(window, L"Speech delay (100–3000 ms)", 32, 342, 230, page);
    AddEdit(window, base + 6, 270, 338, 90, page);
    std::wstring note = L"Separate alternative labels with |. Default call "
                        L"marker: ";
    note += markerExample;
    AddNote(window, note.c_str(), 32, 392, 510, 44, page);
}

static void CreateHeadsetPage(HWND window) {
    int page = 4;
    AddLabel(window, L"Headset mute synchronization", 32, 58, 400, page);
    AddLabel(window, L"Detection method", 32, 88, 140, page);
    AddControl(window, L"STATIC", L"Unsupported/no observable state",
               SS_LEFT, 178, 88, 362, 22, IDC_HEADSET_METHOD, page);
    AddLabel(window, L"Confidence", 32, 116, 140, page);
    AddControl(window, L"STATIC", L"None", SS_LEFT, 178, 116, 362, 36,
               IDC_HEADSET_CONFIDENCE, page);
    AddLabel(window, L"Synchronization mode", 32, 158, 220, page);
    HWND mode = AddControl(window, WC_COMBOBOXW, L"",
                           CBS_DROPDOWNLIST | WS_TABSTOP, 270, 154, 270, 180,
                           IDC_HEADSET_MODE, page);
    SendMessageW(mode, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Full mute/unmute synchronization"));
    SendMessageW(mode, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Mute-only synchronization"));
    SendMessageW(mode, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Status display only"));
    SendMessageW(mode, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Disabled"));
    AddCheck(window, L"Synchronize Windows microphone", IDC_HEADSET_WINDOWS,
             32, 202, 360, page);
    AddCheck(window, L"Synchronize active Slack, Teams, and Zoom calls",
             IDC_HEADSET_CALLS, 32, 234, 440, page);
    AddLabel(window, L"Status polling interval (200–2000 ms)", 32, 274, 250,
             page);
    AddEdit(window, IDC_HEADSET_INTERVAL, 300, 270, 90, page);
    AddControl(window, L"BUTTON", L"Export headset diagnostics…",
               WS_TABSTOP, 32, 310, 220, 30, IDC_HEADSET_EXPORT, page);
    AddNote(window,
            L"Core Audio and standard HID devices are detected automatically. "
            L"SteelSeries uses a vendor adapter. Silence is never interpreted "
            L"as a physical mute state.",
            32, 358, 510, 62, page);
}

static void ShowSettingsPage(int page) {
    for (auto [control, controlPage] : g_pageControls)
        ShowWindow(control, controlPage == page ? SW_SHOW : SW_HIDE);
}

static void SetCheck(HWND window, int id, bool checked) {
    CheckDlgButton(window, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

static void SetEdit(HWND window, int id, int value) {
    SetDlgItemTextW(window, id, std::to_wstring(value).c_str());
}

static void UpdateHeadsetStatusControls(HWND window) {
    if (!window || !IsWindow(window)) return;
    HeadsetStatus status = GetHeadsetStatus();
    SetDlgItemTextW(window, IDC_HEADSET_METHOD,
                    HeadsetMethodName(status.method));
    SetDlgItemTextW(window, IDC_HEADSET_CONFIDENCE,
                    HeadsetConfidenceName(status.confidence));
}

static void LoadSettingsControls(HWND window, const Settings& settings) {
    int role = settings.deviceRole == eCommunications
                   ? 1
               : settings.deviceRole == eMultimedia ? 2
                                                    : 0;
    SendDlgItemMessageW(window, IDC_ROLE, CB_SETCURSEL, role, 0);
    SetEdit(window, IDC_VOLUME_STEP, settings.volumeStep);
    SetCheck(window, IDC_FORCE_VOLUME, settings.forceVolume);
    SetEdit(window, IDC_FORCED_VOLUME, settings.forcedVolume);
    SetEdit(window, IDC_UPDATE_INTERVAL, settings.updateInterval);
    SetEdit(window, IDC_SENSITIVITY, settings.peakSensitivity);
    SetCheck(window, IDC_SHOW_CALL_ICON, settings.showCallStateIcon);
    SetCheck(window, IDC_STARTUP, settings.startWithWindows);
    auto setApp = [&](int base, bool warning, bool cue, bool toggle,
                      const std::wstring& muted, const std::wstring& marker,
                      int threshold, int delay) {
        SetCheck(window, base, warning);
        SetCheck(window, base + 1, cue);
        SetCheck(window, base + 2, toggle);
        SetDlgItemTextW(window, base + 3, muted.c_str());
        SetDlgItemTextW(window, base + 4, marker.c_str());
        SetEdit(window, base + 5, threshold);
        SetEdit(window, base + 6, delay);
    };
    setApp(IDC_SLACK_WARNING, settings.slackWarning, settings.slackAudioCue,
           settings.slackToggle, settings.slackMutedText,
           settings.slackCallText, settings.slackThreshold,
           settings.slackDelay);
    setApp(IDC_TEAMS_WARNING, settings.teamsWarning, settings.teamsAudioCue,
           settings.teamsToggle, settings.teamsMutedText,
           settings.teamsCallText, settings.teamsThreshold,
           settings.teamsDelay);
    setApp(IDC_ZOOM_WARNING, settings.zoomWarning, settings.zoomAudioCue,
           settings.zoomToggle, settings.zoomMutedText,
           settings.zoomCallText, settings.zoomThreshold,
           settings.zoomDelay);
    int mode = settings.headsetMode == L"muteOnly" ? 1
               : settings.headsetMode == L"statusOnly" ? 2
               : settings.headsetMode == L"off"        ? 3
                                                        : 0;
    SendDlgItemMessageW(window, IDC_HEADSET_MODE, CB_SETCURSEL, mode, 0);
    SetCheck(window, IDC_HEADSET_WINDOWS, settings.headsetSyncWindows);
    SetCheck(window, IDC_HEADSET_CALLS, settings.headsetSyncCalls);
    SetEdit(window, IDC_HEADSET_INTERVAL, settings.headsetPollInterval);
    UpdateHeadsetStatusControls(window);
}

static std::wstring GetEditText(HWND window, int id, PCWSTR fallback) {
    wchar_t buffer[512];
    GetDlgItemTextW(window, id, buffer, ARRAYSIZE(buffer));
    return buffer[0] ? buffer : fallback;
}

static int GetEditInt(HWND window, int id, int fallback, int low, int high) {
    BOOL translated = FALSE;
    UINT value = GetDlgItemInt(window, id, &translated, FALSE);
    return translated ? std::clamp(static_cast<int>(value), low, high)
                      : fallback;
}

static Settings ReadSettingsControls(HWND window) {
    Settings settings;
    int role = static_cast<int>(SendDlgItemMessageW(
        window, IDC_ROLE, CB_GETCURSEL, 0, 0));
    settings.deviceRole = role == 1 ? eCommunications
                          : role == 2 ? eMultimedia
                                      : eConsole;
    settings.volumeStep = GetEditInt(window, IDC_VOLUME_STEP, 2, 1, 20);
    settings.forceVolume =
        IsDlgButtonChecked(window, IDC_FORCE_VOLUME) == BST_CHECKED;
    settings.forcedVolume =
        GetEditInt(window, IDC_FORCED_VOLUME, 100, 0, 100);
    settings.updateInterval =
        GetEditInt(window, IDC_UPDATE_INTERVAL, 50, 25, 500);
    settings.peakSensitivity =
        GetEditInt(window, IDC_SENSITIVITY, 150, 25, 500);
    settings.showCallStateIcon =
        IsDlgButtonChecked(window, IDC_SHOW_CALL_ICON) == BST_CHECKED;
    settings.startWithWindows =
        IsDlgButtonChecked(window, IDC_STARTUP) == BST_CHECKED;
    auto readApp = [&](int base, bool& warning, bool& cue, bool& toggle,
                       std::wstring& muted, std::wstring& marker,
                       int& threshold, int& delay, PCWSTR defaultMuted,
                       PCWSTR defaultMarker) {
        warning = IsDlgButtonChecked(window, base) == BST_CHECKED;
        cue = IsDlgButtonChecked(window, base + 1) == BST_CHECKED;
        toggle = IsDlgButtonChecked(window, base + 2) == BST_CHECKED;
        muted = GetEditText(window, base + 3, defaultMuted);
        marker = GetEditText(window, base + 4, defaultMarker);
        threshold = GetEditInt(window, base + 5, 8, 1, 100);
        delay = GetEditInt(window, base + 6, 500, 100, 3000);
    };
    readApp(IDC_SLACK_WARNING, settings.slackWarning,
            settings.slackAudioCue, settings.slackToggle,
            settings.slackMutedText, settings.slackCallText,
            settings.slackThreshold, settings.slackDelay, L"unmute", L"leave");
    readApp(IDC_TEAMS_WARNING, settings.teamsWarning,
            settings.teamsAudioCue, settings.teamsToggle,
            settings.teamsMutedText, settings.teamsCallText,
            settings.teamsThreshold, settings.teamsDelay, L"unmute",
            L"hang up|leave");
    readApp(IDC_ZOOM_WARNING, settings.zoomWarning, settings.zoomAudioCue,
            settings.zoomToggle, settings.zoomMutedText,
            settings.zoomCallText, settings.zoomThreshold,
            settings.zoomDelay, L"unmute", L"leave|end");
    int mode = static_cast<int>(SendDlgItemMessageW(
        window, IDC_HEADSET_MODE, CB_GETCURSEL, 0, 0));
    settings.headsetMode = mode == 1 ? L"muteOnly"
                           : mode == 2 ? L"statusOnly"
                           : mode == 3 ? L"off"
                                       : L"full";
    settings.headsetSyncWindows =
        IsDlgButtonChecked(window, IDC_HEADSET_WINDOWS) == BST_CHECKED;
    settings.headsetSyncCalls =
        IsDlgButtonChecked(window, IDC_HEADSET_CALLS) == BST_CHECKED;
    settings.headsetPollInterval =
        GetEditInt(window, IDC_HEADSET_INTERVAL, 500, 200, 2000);
    return settings;
}

static LRESULT CALLBACK SettingsWindowProc(HWND window, UINT message,
                                           WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_pageControls.clear();
            HWND tab = AddControl(window, WC_TABCONTROLW, L"",
                                  WS_TABSTOP, 12, 12, 576, 450, IDC_TAB, -1);
            for (PCWSTR name : {L"General", L"Slack", L"Teams", L"Zoom",
                                L"Headset"}) {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                item.pszText = const_cast<PWSTR>(name);
                TabCtrl_InsertItem(tab, TabCtrl_GetItemCount(tab), &item);
            }
            CreateGeneralPage(window);
            CreateCallPage(window, 1, L"Slack", IDC_SLACK_WARNING, L"leave");
            CreateCallPage(window, 2, L"Microsoft Teams", IDC_TEAMS_WARNING,
                           L"hang up|leave");
            CreateCallPage(window, 3, L"Zoom", IDC_ZOOM_WARNING,
                           L"leave|end");
            CreateHeadsetPage(window);
            AddControl(window, L"BUTTON", L"Restore defaults", WS_TABSTOP,
                       12, 474, 130, 30, IDC_DEFAULTS, -1);
            AddControl(window, L"BUTTON", L"Save", BS_DEFPUSHBUTTON | WS_TABSTOP,
                       420, 474, 80, 30, IDC_SAVE, -1);
            AddControl(window, L"BUTTON", L"Cancel", WS_TABSTOP, 508, 474, 80,
                       30, IDC_CANCEL, -1);
            LoadSettingsControls(window, g_settings);
            ShowSettingsPage(0);
            return 0;
        }
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header->idFrom == IDC_TAB && header->code == TCN_SELCHANGE) {
                int page = TabCtrl_GetCurSel(header->hwndFrom);
                ShowSettingsPage(page);
            }
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_SAVE: {
                    Settings updated = ReadSettingsControls(window);
                    StopWorkers();
                    g_settings = std::move(updated);
                    g_audioRole.store(g_settings.deviceRole);
                    g_updateInterval.store(g_settings.updateInterval);
                    g_peakSensitivity.store(g_settings.peakSensitivity);
                    g_forceVolume.store(g_settings.forceVolume);
                    g_forcedVolume.store(g_settings.forcedVolume);
                    g_pendingVolumeNotches.store(0);
                    g_pendingVolumeSet.store(
                        g_settings.forceVolume ? g_settings.forcedVolume : -1);
                    SaveSettings();
                    ApplyStartupSetting();
                    StartWorkers();
                    UpdateTrayIcons();
                    DestroyWindow(window);
                    return 0;
                }
                case IDC_DEFAULTS:
                    LoadSettingsControls(window, Settings{});
                    return 0;
                case IDC_HEADSET_EXPORT:
                    ExportHeadsetDiagnostics(window);
                    return 0;
                case IDC_CANCEL:
                    DestroyWindow(window);
                    return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            g_settingsWindow = nullptr;
            g_pageControls.clear();
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static void ShowSettings() {
    if (g_settingsWindow && IsWindow(g_settingsWindow)) {
        ShowWindow(g_settingsWindow, SW_RESTORE);
        SetForegroundWindow(g_settingsWindow);
        return;
    }
    g_settingsWindow = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_CONTROLPARENT,
        L"MicrophoneActivityWidget.Settings",
        L"Settings — Microphone Activity Widget",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 616, 550, nullptr, nullptr, g_instance,
        nullptr);
    ShowWindow(g_settingsWindow, SW_SHOW);
    UpdateWindow(g_settingsWindow);
}

static void ShowTrayMenu(POINT point) {
    HMENU menu = CreatePopupMenu();
    if (!CallList(false).empty()) {
        AppendMenuW(menu, MF_STRING, IDM_TOGGLE_CALLS,
                    L"Mute/unmute active call(s)");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"Settings…");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");

    RECT iconRectangle{point.x, point.y, point.x + 1, point.y + 1};
    NOTIFYICONIDENTIFIER identifier{};
    identifier.cbSize = sizeof(identifier);
    identifier.hWnd = g_mainWindow;
    identifier.uID = kMicIconId;
    identifier.guidItem = kMicIconGuid;
    Shell_NotifyIconGetRect(&identifier, &iconRectangle);

    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_VERTICAL;
    APPBARDATA taskbar{};
    taskbar.cbSize = sizeof(taskbar);
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &taskbar)) {
        switch (taskbar.uEdge) {
            case ABE_TOP:
                point = {iconRectangle.left, iconRectangle.bottom};
                flags |= TPM_TOPALIGN | TPM_LEFTALIGN;
                break;
            case ABE_LEFT:
                point = {iconRectangle.right, iconRectangle.top};
                flags |= TPM_TOPALIGN | TPM_LEFTALIGN;
                break;
            case ABE_RIGHT:
                point = {iconRectangle.left, iconRectangle.top};
                flags |= TPM_TOPALIGN | TPM_RIGHTALIGN;
                break;
            case ABE_BOTTOM:
            default:
                point = {iconRectangle.left, iconRectangle.top};
                flags |= TPM_BOTTOMALIGN | TPM_LEFTALIGN;
                break;
        }
    }

    // The Windows 11 taskbar and overflow panel are topmost surfaces. Give the
    // menu a temporarily visible topmost tool-window owner so it stays above
    // both, then hide the owner immediately after the menu closes.
    SetWindowPos(g_mainWindow, HWND_TOPMOST, point.x, point.y, 1, 1,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetForegroundWindow(g_mainWindow);
    TPMPARAMS parameters{};
    parameters.cbSize = sizeof(parameters);
    parameters.rcExclude = iconRectangle;
    UINT command = TrackPopupMenuEx(menu, flags, point.x, point.y,
                                    g_mainWindow, &parameters);
    PostMessageW(g_mainWindow, WM_NULL, 0, 0);
    ShowWindow(g_mainWindow, SW_HIDE);
    SetWindowPos(g_mainWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    DestroyMenu(menu);
    if (command) PostMessageW(g_mainWindow, WM_COMMAND, command, 0);
    NOTIFYICONDATAW data = BaseNotifyData(kMicIconId, kMicIconGuid);
    data.uFlags = NIF_GUID;
    Shell_NotifyIconW(NIM_SETFOCUS, &data);
}

static void HandleTrayEvent(WPARAM wParam, LPARAM lParam) {
    UINT event = LOWORD(lParam);
    UINT iconId = HIWORD(lParam);
    static DWORD lastSelectTime = 0;
    static UINT lastSelectIcon = 0;
    if (event == NIN_SELECT || event == WM_LBUTTONUP) {
        DWORD now = GetTickCount();
        if (iconId == lastSelectIcon && now - lastSelectTime < 180) return;
        lastSelectIcon = iconId;
        lastSelectTime = now;
        if (iconId == kMicIconId) QueueWindowsToggle();
        else if (iconId == kCallIconId) FocusSelectedCall();
        return;
    }
    if (event == WM_MBUTTONUP) {
        ShowSettings();
        return;
    }
    if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
        if (iconId == kCallIconId) {
            QueueCallToggles();
            return;
        }
        if (iconId == kMicIconId && !(GetKeyState(VK_CONTROL) & 0x8000) &&
            QueueCallToggles())
            return;
        POINT point{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};
        if (point.x == -1 && point.y == -1) GetCursorPos(&point);
        ShowTrayMenu(point);
        return;
    }
    if (event == NIN_BALLOONUSERCLICK) ShowSettings();
}

static LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam,
                                      LPARAM lParam) {
    if (code == HC_ACTION && wParam == WM_MOUSEWHEEL && g_micIconAdded) {
        auto* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        NOTIFYICONIDENTIFIER identifier{};
        identifier.cbSize = sizeof(identifier);
        identifier.hWnd = g_mainWindow;
        identifier.uID = kMicIconId;
        identifier.guidItem = kMicIconGuid;
        RECT rectangle{};
        if (SUCCEEDED(Shell_NotifyIconGetRect(&identifier, &rectangle)) &&
            PtInRect(&rectangle, mouse->pt)) {
            int delta = GET_WHEEL_DELTA_WPARAM(mouse->mouseData);
            int notches = delta / WHEEL_DELTA;
            if (notches)
                PostMessageW(g_mainWindow, kWheelMessage, notches, 0);
        }
    }
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

static LRESULT CALLBACK MainWindowProc(HWND window, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    if (g_taskbarCreated && message == g_taskbarCreated) {
        g_micIconAdded = false;
        g_callIconAdded = false;
        UpdateTrayIcons(true);
        return 0;
    }
    switch (message) {
        case kTrayCallback:
            HandleTrayEvent(wParam, lParam);
            return 0;
        case kStateChanged: {
            UpdateTrayIcons();
            UpdateHeadsetStatusControls(g_settingsWindow);
            bool warning = g_slackWarning.load() || g_teamsWarning.load() ||
                           g_zoomWarning.load();
            if (warning && !g_previousWarning) ShowWarningBalloon();
            g_previousWarning = warning;
            return 0;
        }
        case WM_INPUT:
            ProcessStandardHidRawInput(
                reinterpret_cast<HRAWINPUT>(lParam));
            return DefWindowProcW(window, message, wParam, lParam);
        case WM_INPUT_DEVICE_CHANGE:
            g_standardHidRuntime.clear();
            RefreshStandardHidDevices();
            return 0;
        case WM_TIMER:
            if (wParam == kStandardHidTimer) {
                KillTimer(window, kStandardHidTimer);
                ResolveStandardHidAction();
                return 0;
            }
            break;
        case kWheelMessage:
            QueueVolume(static_cast<int>(wParam));
            return 0;
        case kShowSettingsMessage:
            ShowSettings();
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_SETTINGS:
                    ShowSettings();
                    return 0;
                case IDM_TOGGLE_CALLS:
                    QueueCallToggles();
                    return 0;
                case IDM_EXIT:
                    DestroyWindow(window);
                    return 0;
            }
            break;
        case WM_QUERYENDSESSION:
            return TRUE;
        case WM_ENDSESSION:
            if (wParam) DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

static bool InitializeSettingsPath() {
    wchar_t localAppData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                nullptr, SHGFP_TYPE_CURRENT, localAppData)))
        return false;
    std::wstring directory = std::wstring(localAppData) +
                             L"\\MicrophoneActivityWidget";
    if (!CreateDirectoryW(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    g_settingsPath = directory + L"\\settings.ini";
    return true;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    g_instance = instance;
    g_diagnosticStartTime = GetTickCount64();
    g_singleInstance = CreateMutexW(
        nullptr, FALSE, L"Local\\MicrophoneActivityWidget.Standalone.Instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(kWindowClass, nullptr))
            PostMessageW(existing, kShowSettingsMessage, 0, 0);
        if (g_singleInstance) CloseHandle(g_singleInstance);
        return 0;
    }
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_TAB_CLASSES};
    InitCommonControlsEx(&controls);
    GdiplusStartupInput gdiplusInput;
    if (GdiplusStartup(&g_gdiplusToken, &gdiplusInput, nullptr) != Ok)
        return 1;
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                          0);
    g_uiFont = CreateFontIndirectW(&metrics.lfMessageFont);
    if (!InitializeSettingsPath()) return 1;
    LoadSettings();

    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.hInstance = instance;
    mainClass.lpszClassName = kWindowClass;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    g_appIconLarge = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(kAppIconResourceId), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
        LR_DEFAULTCOLOR));
    g_appIconSmall = static_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(kAppIconResourceId), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    mainClass.hIcon = g_appIconLarge;
    mainClass.hIconSm = g_appIconSmall;
    RegisterClassExW(&mainClass);
    WNDCLASSEXW settingsClass{};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.lpfnWndProc = SettingsWindowProc;
    settingsClass.hInstance = instance;
    settingsClass.lpszClassName = L"MicrophoneActivityWidget.Settings";
    settingsClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    settingsClass.hIcon = g_appIconLarge;
    settingsClass.hIconSm = g_appIconSmall;
    RegisterClassExW(&settingsClass);
    g_mainWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                                   kWindowClass, kAppName, WS_POPUP, 0, 0, 1,
                                   1, nullptr, nullptr, instance, nullptr);
    if (!g_mainWindow) return 1;
    if (!RegisterStandardHidInput())
        RecordDiagnosticEvent(L"Standard HID raw-input registration failed");
    RefreshStandardHidDevices();
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    UpdateTrayIcons(true);
    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, instance, 0);
    if (!StartWorkers()) {
        MessageBoxW(nullptr, L"The microphone monitoring thread could not start.",
                    kAppName, MB_ICONERROR);
    }

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!g_settingsWindow || !IsDialogMessageW(g_settingsWindow, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    g_exiting.store(true);
    StopWorkers();
    if (g_mouseHook) UnhookWindowsHookEx(g_mouseHook);
    if (g_callIconAdded) RemoveTrayIcon(kCallIconId, kCallIconGuid);
    if (g_micIconAdded) RemoveTrayIcon(kMicIconId, kMicIconGuid);
    if (g_callIcon) DestroyIcon(g_callIcon);
    if (g_micIcon) DestroyIcon(g_micIcon);
    if (g_appIconSmall) DestroyIcon(g_appIconSmall);
    if (g_appIconLarge) DestroyIcon(g_appIconLarge);
    if (g_uiFont) DeleteObject(g_uiFont);
    GdiplusShutdown(g_gdiplusToken);
    if (g_singleInstance) CloseHandle(g_singleInstance);
    return static_cast<int>(message.wParam);
}
