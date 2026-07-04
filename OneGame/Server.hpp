
class DebugClient : public Scene<PresentationContext, const FrameInputData, FrameOutputData>
{
public:
    DebugScene3() : m_gameRenderer(m_gameWorld) {}
    virtual void Initialize(PresentationContext& context) override;
    virtual void Enter(PresentationContext& context) override;
    virtual void Exit(PresentationContext& context) override;
    virtual void Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut) override;
private:
    ECS::GameWorld m_gameWorld;
    ECS::GameRenderer m_gameRenderer;
};