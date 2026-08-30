#include <inputrack/EntitlementService.h>
#include <inputrack/TrialPolicy.h>

#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
#include <windows.h>
#include <shobjidl_core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>
#endif

namespace inputrack {
namespace {
#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
constexpr auto trialLengthDays = 14;
constexpr wchar_t trialRegistryPath[] = L"Software\\InputRack";
constexpr wchar_t trialRegistryValue[] = L"ProTrialStartedUnix";

EntitlementResult localTrialState()
{
    unsigned long type{};
    unsigned long size = sizeof(unsigned long long);
    unsigned long long started{};
    const auto found = RegGetValueW(HKEY_CURRENT_USER, trialRegistryPath, trialRegistryValue,
                                    RRF_RT_REG_QWORD, &type, &started, &size) == ERROR_SUCCESS;
    const auto now = static_cast<unsigned long long>(juce::Time::currentTimeMillis() / 1000);
    const auto trial = evaluateTrialState(
        found ? std::optional<std::uint64_t>{started} : std::nullopt,
        now, trialLengthDays);
    return {false, trial.active, trial.available, trial.daysRemaining, {}};
}

bool beginLocalTrial()
{
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, trialRegistryPath, 0, nullptr, 0, KEY_SET_VALUE,
                        nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    const auto started = static_cast<unsigned long long>(juce::Time::currentTimeMillis() / 1000);
    const auto written = RegSetValueExW(key, trialRegistryValue, 0, REG_QWORD,
        reinterpret_cast<const BYTE*>(&started), sizeof(started)) == ERROR_SUCCESS;
    RegCloseKey(key);
    return written;
}
#endif

class DevelopmentEntitlement final : public EntitlementService {
public:
    EntitlementResult state() const noexcept override { return {true, false, false, 0, {}}; }
    bool isBusy() const noexcept override { return false; }
    void refresh(void*, Callback callback) override { callback(state()); }
    void purchase(void*, Callback callback) override
    {
        callback({true, false, false, 0, "Pro is enabled in this development build."});
    }
    void startTrial(void*, Callback callback) override { callback(state()); }
};

#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
class StoreEntitlement final : public EntitlementService, private juce::Thread {
public:
    StoreEntitlement() : Thread("Microsoft Store entitlement")
    {
        const auto override = juce::SystemStats::getEnvironmentVariable("INPUTRACK_PRO", {});
        developmentOverride = override == "1" || override.equalsIgnoreCase("true");
        const auto initial = developmentOverride
            ? EntitlementResult{true, false, false, 0, {}} : localTrialState();
        permanent.store(initial.permanent);
        trial.store(initial.trial);
        trialAvailable.store(initial.trialAvailable);
        trialDays.store(initial.trialDaysRemaining);
    }

    ~StoreEntitlement() override
    {
        signalThreadShouldExit();
        stopThread(10000);
    }

    EntitlementResult state() const noexcept override
    {
        return {permanent.load(), trial.load(), trialAvailable.load(), trialDays.load(), {}};
    }
    bool isBusy() const noexcept override { return isThreadRunning(); }

    void refresh(void* ownerWindow, Callback next) override
    {
        if (developmentOverride) {
            next(state());
            return;
        }
        start(Job::refresh, ownerWindow, std::move(next));
    }

    void purchase(void* ownerWindow, Callback next) override
    {
        if (developmentOverride) {
            next({true, false, false, 0, "Pro is enabled by INPUTRACK_PRO."});
            return;
        }
        start(Job::purchasePro, ownerWindow, std::move(next));
    }

    void startTrial(void* ownerWindow, Callback next) override
    {
        juce::ignoreUnused(ownerWindow);
        if (developmentOverride) {
            next(state());
            return;
        }
        if (isThreadRunning()) {
            auto current = state();
            current.message = "A Microsoft Store request is already running.";
            next(std::move(current));
            return;
        }
        if (state().permanent) {
            next(state());
            return;
        }
        auto current = localTrialState();
        if (!current.trialAvailable) {
            current.message = current.trial ? "Your Pro trial is already active."
                                            : "The Pro trial has already been used.";
        } else if (!beginLocalTrial()) {
            current.message = "The Pro trial could not be saved for this Windows user.";
        } else {
            current = localTrialState();
            current.message = "Your 14-day InputRack Pro trial has started.";
        }
        permanent.store(current.permanent);
        trial.store(current.trial);
        trialAvailable.store(current.trialAvailable);
        trialDays.store(current.trialDaysRemaining);
        next(std::move(current));
    }

private:
    enum class Job { none, refresh, purchasePro };

    void start(Job requested, void* ownerWindow, Callback next)
    {
        if (isThreadRunning()) {
            auto result = state();
            result.message = "A Microsoft Store request is already running.";
            next(std::move(result));
            return;
        }
        job = requested;
        window = static_cast<HWND>(ownerWindow);
        callback = std::move(next);
        startThread();
    }

    void finish(EntitlementResult result)
    {
        permanent.store(result.permanent);
        trial.store(result.trial);
        trialAvailable.store(result.trialAvailable);
        trialDays.store(result.trialDaysRemaining);
        auto next = std::move(callback);
        juce::MessageManager::callAsync([next = std::move(next), result = std::move(result)]() mutable {
            if (next) next(std::move(result));
        });
    }

    void run() override
    {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            const auto context = winrt::Windows::Services::Store::StoreContext::GetDefault();
            if (window != nullptr) {
                const auto initialise = context.as<IInitializeWithWindow>();
                winrt::check_hresult(initialise->Initialize(window));
            }
            const auto permanentToken = winrt::to_hstring(INPUTRACK_PRO_OFFER_TOKEN);
            if (job == Job::purchasePro) {
                const auto result = context.RequestPurchaseByInAppOfferTokenAsync(permanentToken).get();
                using Status = winrt::Windows::Services::Store::StorePurchaseStatus;
                auto current = state();
                if (result.Status() == Status::Succeeded || result.Status() == Status::AlreadyPurchased) {
                    current.permanent = true;
                    current.trial = false;
                    current.trialAvailable = false;
                    current.trialDaysRemaining = 0;
                    current.message = "InputRack Pro is ready.";
                } else if (result.Status() == Status::NotPurchased) {
                    current.message = "The purchase was cancelled.";
                } else {
                    current.message = "The Microsoft Store could not complete the request.";
                }
                finish(std::move(current));
                return;
            }

            const auto license = context.GetAppLicenseAsync().get();
            auto refreshed = localTrialState();
            for (const auto& entry : license.AddOnLicenses()) {
                const auto addOn = entry.Value();
                if (addOn.InAppOfferToken() == permanentToken) refreshed.permanent = true;
            }
            if (refreshed.permanent) {
                refreshed.trial = false;
                refreshed.trialAvailable = false;
                refreshed.trialDaysRemaining = 0;
                refreshed.message = "InputRack Pro purchase restored.";
            }
            else if (refreshed.trial)
                refreshed.message = "InputRack Pro trial: "
                    + juce::String(refreshed.trialDaysRemaining) + " day(s) remaining.";
            finish(std::move(refreshed));
        } catch (const winrt::hresult_error& error) {
            auto current = state();
            current.message = "Microsoft Store error 0x"
                + juce::String::toHexString(error.code().value);
            finish(std::move(current));
        }
    }

    std::atomic<bool> permanent{};
    std::atomic<bool> trial{};
    std::atomic<bool> trialAvailable{};
    std::atomic<int> trialDays{};
    bool developmentOverride{};
    Job job{Job::none};
    HWND window{};
    Callback callback;
};
#endif
}

std::unique_ptr<EntitlementService> EntitlementService::create()
{
#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
    return std::make_unique<StoreEntitlement>();
#else
    return std::make_unique<DevelopmentEntitlement>();
#endif
}
}
