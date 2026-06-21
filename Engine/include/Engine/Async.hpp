#pragma once
#include <bit>
#include <tuple>
#include <functional>
#include <optional>
#include <mutex>
#include <queue>
#include "Engine/ResourcePool.hpp"

namespace OneGame::Engine
{
    template <typename Signature, size_t MaxCaptureSize = 24>
    class FixedSizeFunction;

    template <size_t MaxSize, typename RetTy, typename... Args>
    class FixedSizeFunction<RetTy(Args...), MaxSize> {
        alignas(std::max_align_t) std::byte storage[MaxSize];

        RetTy(*thunk_ptr)(void*, Args...) = nullptr;
    public:
        template <typename F>
        explicit FixedSizeFunction(F&& fn)
        {
            using DecayedF = std::decay_t<F>;

            static_assert(sizeof(DecayedF) <= MaxSize, "Lambda capture size exceeded");

            std::construct_at(reinterpret_cast<DecayedF*>(storage), std::forward<F>(fn));

            thunk_ptr = [](void* buffer, Args... args) {
                return (*static_cast<DecayedF*>(buffer))(std::forward<Args>(args)...);
            };
        }

        RetTy operator()(Args... args) const
        {
            return thunk_ptr((void*)(storage), std::forward<Args>(args)...);
        }
    };

    enum class AsyncObjects
    {
        Future,
        Callback,
    };

    using FutureHandle = ResourceHandle<AsyncObjects::Future>;
    using CallbackHandle = ResourceHandle<AsyncObjects::Callback>;
    
    template<typename T>
    class Future;

    class AsyncDispatcher
    {
        struct CallbackData
        {
            FixedSizeFunction<uint64_t(uint64_t)> fn;
            CallbackHandle sibling;
            FutureHandle next;
        };

        struct FutureData
        {
            uint64_t value;
            bool ready;
            CallbackHandle callback;
        };

    public:
        template<typename T>
        Future<T> create_future()
        {
            auto handle = m_futures.Create();
            return Future<T>(handle, this);
        }

        template<typename F>
        void add_callback(FutureHandle handle, F&& callback, FutureHandle next = {})
        {
            CallbackHandle sibling{};
            auto fData = m_futures.Get(handle);
            assert(!fData->ready);
            if (fData->callback.IsValid())
            {
                sibling = fData->callback;
            }
            auto cHandle = m_callbacks.Create(FixedSizeFunction<uint64_t(uint64_t)>(callback), sibling, next);
            fData->callback = cHandle;
        }

        void post_future(FutureHandle handle, uint64_t val)
        {
            auto data = m_futures.Get(handle);
            data->value = val;
            data->ready = true;
            m_completedFutures.push(handle);
        }

        void process_callbacks()
        {
            while (!m_completedFutures.empty())
            {
                auto futureHandle = std::move(m_completedFutures.front());
                m_completedFutures.pop();
                auto fData = m_futures.Get(futureHandle);

                assert(fData->ready);
                auto callbackHandle = fData->callback;
                while (callbackHandle.IsValid())
                {
                    auto cData = m_callbacks.Get(callbackHandle);
                    uint64_t res = cData->fn(fData->value);
                    m_callbacks.Destroy(callbackHandle);
                    callbackHandle = cData->sibling;
                    if (cData->next.IsValid())
                    {
                        post_future(cData->next, res);
                    }
                }
                m_futures.Destroy(futureHandle);
            }
        }

    private:
        std::mutex mutex;
        std::queue<FutureHandle> m_completedFutures;
        ResourcePool<AsyncObjects::Future, FutureData> m_futures;
        ResourcePool<AsyncObjects::Callback, CallbackData> m_callbacks;
    };

    struct VoidRet
    {
    };

    template<typename T>
    class Future
    {
    public:
        Future(FutureHandle h, AsyncDispatcher* d)
            : handle(h), dispatcher(d)
        {
            static_assert(sizeof(T) <= sizeof(uint64_t));
        }

        template<typename F>
        auto then(F&& func)
        {
            using Ret = decltype(func(std::declval<T>()));

            if constexpr (!std::is_void_v<Ret>)
            {
                auto next = dispatcher->create_future<Ret>();

                dispatcher->add_callback(
                    handle,
                    [=](uint64_t raw) -> uint64_t
                    {
                        Ret result;
                        if constexpr (!std::is_same_v<T, VoidRet>)
                        {
                            T value = std::bit_cast<T>(raw);
                            result = func(value);
                        }
                        else
                        {
                            result = func();
                        }
                        return std::bit_cast<uint64_t>(result);
                    },
                    next.handle
                );

                return next;
            }
            else
            {
                auto next = dispatcher->create_future<VoidRet>();
                dispatcher->add_callback(
                    handle,
                    [=](uint64_t raw) -> uint64_t
                    {
                        if constexpr (!std::is_same_v<T, VoidRet>)
                        {
                            T value = std::bit_cast<T>(raw);
                            func(value);
                        }
                        else
                        {
                            func();
                        }
                        return 0;
                    },
                    next.handle
                );

                return next;
            }
        }

        void post(T val)
        {
            dispatcher->post_future(handle, std::bit_cast<uint64_t>(val));
        }

        FutureHandle handle;
    private:
        AsyncDispatcher* dispatcher;
    };

}
