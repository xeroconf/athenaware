#pragma once

//---------------------------------------------------------------------------

class MermaidESPFeature : public scaffold::Feature 
{
public:

    MermaidESPFeature(std::shared_ptr<scaffold::Messenger> InMessenger);

private:

    struct ContextData
    {
    };

    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(EFeatureResetReason InResetReason) override;
    bool CanExecute() override;
    void OnConditionChange(bool bConditionChange) override;
    void OnDiscard(float DeltaTime) override;
    void OnExecute(float DeltaTime) override;
    void PostTick(float DeltaTime) override;
    void Tick(float DeltaTime) override;

    void OnActorAggregationUpdate(const ActorAggregationUpdateEvent& Event);

    void AddMermaid(AMermaid* InMermaid);
    void RemoveMermaid(AMermaid* InMermaid);

public:

private:

    std::map<AMermaid*, ContextData> Mermaids;

    scaffold::SubscriptionHandle AggregationUpdateHandle;

    std::shared_ptr<scaffold::Messenger> Dispatcher;

};