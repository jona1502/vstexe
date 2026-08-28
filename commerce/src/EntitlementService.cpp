#include <inputrack/EntitlementService.h>

#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
#include <windows.h>
#include <shobjidl_core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>
#endif

namespace inputrack {
namespace {
class DevelopmentEntitlement final : public EntitlementService {
public:
    bool isPro() const noexcept override { return true; }
    bool isBusy() const noexcept override { return false; }
    void refresh(void*, Callback callback) override { callback({true, {}}); }
    void purchase(void*, Callback callback) override { callback({true, "Pro is enabled in this development build."}); }
};

#if INPUTRACK_STORE_BUILD && JUCE_WINDOWS
class StoreEntitlement final : public EntitlementService, private juce::Thread {
public:
    StoreEntitlement() : Thread("Microsoft Store entitlement")
    {
        const auto override = juce::SystemStats::getEnvironmentVariable("INPUTRACK_PRO", {});
        developmentOverride = override == "1" || override.equalsIgnoreCase("true");
        pro.store(developmentOverride);
    }

    ~StoreEntitlement() override
    {
        signalThreadShouldExit();
        stopThread(10000);
    }

    bool isPro() const noexcept override { return pro.load(); }
    bool isBusy() const noexcept override { return isThreadRunning(); }

    void refresh(void* ownerWindow, Callback next) override
    {
        if (developmentOverride) {
            next({true, {}});
            return;
        }
        start(Job::refresh, ownerWindow, std::move(next));
    }

    void purchase(void* ownerWindow, Callback next) override
    {
        if (developmentOverride) {
            next({true, "Pro is enabled by INPUTRACK_PRO."});
            return;
        }
        start(Job::purchase, ownerWindow, std::move(next));
    }

private:
    enum class Job { none, refresh, purchase };

    void start(Job requested, void* ownerWindow, Callback next)
    {
        if (isThreadRunning()) {
            next({pro.load(), "A Microsoft Store request is already running."});
            return;
        }
        job = requested;
        window = static_cast<HWND>(ownerWindow);
        callback = std::move(next);
        startThread();
    }

    void finish(EntitlementResult result)
    {
        if (result.pro) pro.store(true);
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
            const auto token = winrt::to_hstring(INPUTRACK_PRO_OFFER_TOKEN);
            if (job == Job::purchase) {
                const auto result = context.RequestPurchaseByInAppOfferTokenAsync(token).get();
                using Status = winrt::Windows::Services::Store::StorePurchaseStatus;
                if (result.Status() == Status::Succeeded || result.Status() == Status::AlreadyPurchased)
                    finish({true, "InputRack Pro is ready."});
                else if (result.Status() == Status::NotPurchased)
                    finish({pro.load(), "The purchase was cancelled."});
                else
                    finish({pro.load(), "The Microsoft Store could not complete the purchase."});
                return;
            }

            const auto tokens = winrt::single_threaded_vector<winrt::hstring>();
            tokens.Append(token);
            const auto query = context.GetAssociatedStoreProductsByInAppOfferTokenAsync(tokens).get();
            if (query.ExtendedError().value < 0) {
                finish({pro.load(), "The Microsoft Store is temporarily unavailable."});
                return;
            }
            auto owned = false;
            for (const auto& entry : query.Products())
                if (entry.Value().IsInUserCollection()) owned = true;
            finish({owned || pro.load(), owned ? "InputRack Pro purchase restored." : juce::String{}});
        } catch (const winrt::hresult_error& error) {
            finish({pro.load(), "Microsoft Store error 0x" + juce::String::toHexString(error.code().value)});
        }
    }

    std::atomic<bool> pro{};
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
