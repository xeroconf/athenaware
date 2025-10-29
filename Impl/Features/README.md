# 🧩 Features
Each feature in this folder is a self-contained module that implements a specific piece of functionality.
Follow the guidelines below to ensure features remain consistent, modular, and easy to maintain.

## Guidelines
### 1. Inheritance
All features **must inherit from** `scaffold::Feature` as it provides virtual functions that **must be overridden** by each feature.
This ensures proper lifecycle management (initialization, tick, shutdown, etc.). [More Information](https://github.com/xeroconf/scaffold).

```cpp
class ExampleFeature : public scaffold::Feature
{
    bool OnStart() override;
    bool OnEnd() override;
    void OnReset(FeatureResetReason reason) override;
    bool CanExecute() override;
    void OnConditionChange(bool newCondition) override;
    void OnDiscard(float deltaTime) override;
    void OnExecute(float deltaTime) override;
    void OnPostTick(float deltaTime) override;
    void OnTick(float deltaTime) override;
};
```

### 2. Constructor
Each feature must define its own constructor, and it must call the `scaffold::Feature` base constructor, passing in the feature name and any user defined flags:
```cpp
ExampleFeature::ExampleFeature() : 
  scaffold::Feature("Example", CUSTOM_FEATURE_FLAG_1 | CUSTOM_FEATURE_FLAG_2)
{};
```
> [!TIP]
> Feature flags can be used to categorize features for management. For example, we may choose to start all features with a specific flag set.


### 3. Dependency Injection
All external dependencies that are owned by the domain (e.g. messengers, services, configuration, etc.) must be provided via dependency injection.
Do not globally access dependencies or the domain from within the feature.
```cpp
ExampleFeature::ExampleFeature(aufority::Dispatcher* dispatcher, IService* service) : 
  scaffold::Feature("Example").
  m_eventDispatcher(dispatcher), 
  m_service(service)
{};
```

### 4. Communication
Features **must only communicate through a shared [messenger](https://github.com/xeroconf/messenger) instance**.
Direct cross-feature interaction or hard references are not allowed.
This maintains loose coupling and clear message-based coordination.


### 5. Cleanup
When disabled or destroyed, a feature must fully clean up after itself:
- Free all dynamically allocated memory
- Clear internal lists, containers, and caches
- Reset variables to default states

- Unregister any event or messenger subscriptions
