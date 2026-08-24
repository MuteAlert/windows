#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

enum class HeadsetDetectionMethod {
    Unsupported,
    WindowsHardwareMute,
    StandardHidButton,
    SteelSeriesDeviceState,
};

enum class HeadsetConfidence {
    None,
    Medium,
    High,
};

struct HeadsetAdapterObservation {
    bool available = false;
    bool stateKnown = false;
    bool muted = false;
    std::wstring deviceName;
    std::wstring detail;
};

// Vendor integrations implement this interface without changing the generic
// Core Audio or standards-based HID providers in the main application.
class IHeadsetMuteAdapter {
public:
    virtual ~IHeadsetMuteAdapter() = default;
    virtual PCWSTR Id() const = 0;
    virtual PCWSTR DisplayName() const = 0;
    virtual bool TryConnect() = 0;
    virtual bool Poll(HeadsetAdapterObservation& observation) = 0;
    virtual void Disconnect() = 0;
};

struct VendorAdapterSlot {
    USHORT vendorId;
    PCWSTR vendorName;
    PCWSTR adapterId;
    bool implemented;
};

std::unique_ptr<IHeadsetMuteAdapter> CreateSteelSeriesHeadsetAdapter();
const std::vector<VendorAdapterSlot>& GetVendorAdapterSlots();
const VendorAdapterSlot* FindVendorAdapterSlot(USHORT vendorId);
