#pragma once

#include <string>

#include "Engine/GameAppState.hpp"

namespace OneGame::Engine
{
struct EmptySceneData
{
};

template <typename TData = EmptySceneData>
class PresenterBase
{
   public:
    virtual ~PresenterBase() = default;

    virtual void Initialize(const TData& data, PresentationContext& pc) {}
    virtual void Enter(const TData& data, PresentationContext& pc) {}
    virtual void Exit(const TData& data, PresentationContext& pc) {}
    virtual void Shutdown(const TData& data, PresentationContext& pc) {}
    virtual void Present(const TData& data, PresentationContext& pc, FrameOutputData& fd) {}
};

template <typename TData = EmptySceneData>
class ControllerBase
{
   public:
    virtual ~ControllerBase() = default;

    virtual void Initialize(TData& data, AppContext& context) {}
    virtual void Enter(TData& data, AppContext& context) {}
    virtual void Exit(TData& data, AppContext& context) {}
    virtual void Shutdown(TData& data, AppContext& context) {}
    virtual void Update(TData& data, AppContext& context, const FrameInputData& fc) {}
};

class IServerScene
{
   public:
    virtual ~IServerScene() = default;
    virtual void Initialize(AppContext& context) = 0;
    virtual void Enter(AppContext& context) = 0;
    virtual void Exit(AppContext& context) = 0;
    virtual void Shutdown(AppContext& context) = 0;
    virtual void Update(AppContext& context, const FrameInputData& fc) = 0;
};

template <typename TData, typename Controller>
class ServerScene : IServerScene
{
   public:
    virtual ~ServerScene() = default;

    void Initialize(AppContext& context) override { m_sceneController.Initialize(m_sceneData, context); }

    void Enter(AppContext& context) override { m_sceneController.Initialize(m_sceneData, context); }

    void Exit(AppContext& context) override { m_sceneController.Initialize(m_sceneData, context); }

    void Shutdown(AppContext& context) override { m_sceneController.Initialize(m_sceneData, context); }

    void Update(AppContext& context, const FrameInputData& fc) override
    {
        m_sceneController.Update(m_sceneData, context, fc);
    }

   private:
    Controller m_sceneController;
    TData m_sceneData;
};

class ClientSceneBase
{
   public:
    virtual void Initialize(PresentationContext& context) {}
    virtual void Enter(PresentationContext& context) {}
    virtual void Exit(PresentationContext& context) {}
    virtual void Shutdown(PresentationContext& context) {}
    virtual void Update(PresentationContext& context, const FrameInputData& fi, FrameOutputData& fo) {}
};

template <typename TData, typename Controller, typename Presenter>
class ClientScene : ClientSceneBase
{
   public:
    virtual ~ClientScene() = default;

    void Initialize(PresentationContext& context) override
    {
        m_sceneController.Initialize(m_sceneData, context.appCtx);
        m_scenePresenter.Initialize(m_sceneData, context);
    }

    void Enter(PresentationContext& context) override
    {
        m_sceneController.Initialize(m_sceneData, context.appCtx);
        m_scenePresenter.Initialize(m_sceneData, context);
    }

    void Exit(PresentationContext& context) override
    {
        m_sceneController.Initialize(m_sceneData, context.appCtx);
        m_scenePresenter.Initialize(m_sceneData, context);
    }

    void Shutdown(PresentationContext& context) override
    {
        m_sceneController.Initialize(m_sceneData, context.appCtx);
        m_scenePresenter.Initialize(m_sceneData, context);
    }

    void Update(PresentationContext& context, const FrameInputData& fi, FrameOutputData& fo) override
    {
        m_sceneController.Update(m_sceneData, context.appCtx, fi);
        m_scenePresenter.Update(m_sceneData, context, fo);
    }

   private:
    Controller m_sceneController;
    Presenter m_scenePresenter;
    TData m_sceneData;
};
}  // namespace OneGame::Engine
