#include "headset_adapters.h"

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include <vector>

namespace {

class SteelSeriesHeadsetAdapter final : public IHeadsetMuteAdapter {
public:
    ~SteelSeriesHeadsetAdapter() override { Disconnect(); }

    PCWSTR Id() const override { return L"steelseries-nova-pro"; }
    PCWSTR DisplayName() const override {
        return L"SteelSeries device state";
    }

    bool TryConnect() override {
        Disconnect();
        GUID hidGuid{};
        HidD_GetHidGuid(&hidGuid);
        HDEVINFO devices = SetupDiGetClassDevsW(
            &hidGuid, nullptr, nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (devices == INVALID_HANDLE_VALUE) return false;

        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        for (DWORD index = 0; SetupDiEnumDeviceInterfaces(
                 devices, nullptr, &hidGuid, index, &interfaceData);
             index++) {
            DWORD required = 0;
            SetupDiGetDeviceInterfaceDetailW(
                devices, &interfaceData, nullptr, 0, &required, nullptr);
            if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
                continue;
            std::vector<BYTE> storage(required);
            auto* detail =
                reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(
                    storage.data());
            detail->cbSize = sizeof(*detail);
            if (!SetupDiGetDeviceInterfaceDetailW(
                    devices, &interfaceData, detail, required, nullptr,
                    nullptr))
                continue;

            HANDLE candidate = CreateFileW(
                detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED, nullptr);
            if (candidate == INVALID_HANDLE_VALUE) continue;

            HIDD_ATTRIBUTES attributes{};
            attributes.Size = sizeof(attributes);
            PHIDP_PREPARSED_DATA preparsed = nullptr;
            HIDP_CAPS caps{};
            bool matches =
                HidD_GetAttributes(candidate, &attributes) &&
                attributes.VendorID == 0x1038 &&
                IsSupportedProduct(attributes.ProductID) &&
                HidD_GetPreparsedData(candidate, &preparsed) &&
                HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS &&
                caps.UsagePage == 0xFFC0 &&
                caps.InputReportByteLength == 64 &&
                caps.OutputReportByteLength == 64;
            if (preparsed) HidD_FreePreparsedData(preparsed);
            if (matches) {
                device_ = candidate;
                productId_ = attributes.ProductID;
                break;
            }
            CloseHandle(candidate);
        }
        SetupDiDestroyDeviceInfoList(devices);
        return device_ != INVALID_HANDLE_VALUE;
    }

    bool Poll(HeadsetAdapterObservation& observation) override {
        observation = {};
        if (device_ == INVALID_HANDLE_VALUE) return false;

        BYTE request[64]{};
        request[0] = 0x06;
        request[1] = 0xB0;
        DWORD written = 0;
        if (!RunIo(true, request, sizeof(request), 250, &written) ||
            written != sizeof(request))
            return false;

        for (int attempt = 0; attempt < 6; attempt++) {
            BYTE response[64]{};
            DWORD read = 0;
            if (!RunIo(false, response, sizeof(response), 125, &read))
                return false;
            if (read < 16 || response[0] != 0x06 || response[1] != 0xB0)
                continue;
            if (response[9] > 1) return false;
            observation.available = response[15] == 0x08;
            observation.stateKnown = observation.available;
            observation.muted = response[9] == 1;
            observation.deviceName = L"SteelSeries Arctis Nova Pro Wireless";
            wchar_t detail[64];
            wsprintfW(detail, L"VID 1038 / PID %04X", productId_);
            observation.detail = detail;
            return true;
        }
        return false;
    }

    void Disconnect() override {
        if (device_ != INVALID_HANDLE_VALUE) CloseHandle(device_);
        device_ = INVALID_HANDLE_VALUE;
        productId_ = 0;
    }

private:
    static bool IsSupportedProduct(USHORT productId) {
        return productId == 0x12E0 || productId == 0x12E5 ||
               productId == 0x225D;
    }

    bool RunIo(bool write, BYTE* buffer, DWORD length, DWORD timeout,
               DWORD* transferred) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!overlapped.hEvent) return false;
        DWORD bytes = 0;
        BOOL started = write ? WriteFile(device_, buffer, length, &bytes,
                                         &overlapped)
                             : ReadFile(device_, buffer, length, &bytes,
                                        &overlapped);
        bool success = started != FALSE;
        if (!started && GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(overlapped.hEvent, timeout) ==
                WAIT_OBJECT_0) {
                success = GetOverlappedResult(device_, &overlapped, &bytes,
                                              FALSE) != FALSE;
            } else {
                CancelIoEx(device_, &overlapped);
                WaitForSingleObject(overlapped.hEvent, INFINITE);
                success = false;
            }
        }
        CloseHandle(overlapped.hEvent);
        if (transferred) *transferred = bytes;
        return success;
    }

    HANDLE device_ = INVALID_HANDLE_VALUE;
    USHORT productId_ = 0;
};

}  // namespace

std::unique_ptr<IHeadsetMuteAdapter> CreateSteelSeriesHeadsetAdapter() {
    return std::make_unique<SteelSeriesHeadsetAdapter>();
}

const std::vector<VendorAdapterSlot>& GetVendorAdapterSlots() {
    static const std::vector<VendorAdapterSlot> slots = {
        {0x1038, L"SteelSeries", L"steelseries-nova-pro", true},
        {0x046D, L"Logitech", L"logitech", false},
        {0x0B0E, L"Jabra", L"jabra", false},
        {0x047F, L"Poly / Plantronics", L"poly", false},
        {0x1B1C, L"Corsair", L"corsair", false},
    };
    return slots;
}

const VendorAdapterSlot* FindVendorAdapterSlot(USHORT vendorId) {
    for (const auto& slot : GetVendorAdapterSlots()) {
        if (slot.vendorId == vendorId) return &slot;
    }
    return nullptr;
}
