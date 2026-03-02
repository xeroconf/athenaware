# ⛵ Athenaware
Features from the Athenaware (Prismarine) project. Not all features are included, as some are trivial (e.g., a crosshair) or still work and are potentially game-breaking.
Over time, I’ll continue to populate this repository with the remaining features.

> [!IMPORTANT]
> This repository contains feature implementations only and cannot be compiled in its current state. It is provided **solely for reference and educational purposes**.

> [!NOTE]
> Athenaware is built on an old version of [Scaffold](https://github.com/xeroconf/scaffold), a framework built for clean and modular cheat development. More information about this can be found below.

## About this Project
### Code Style
I chose to keep the code style consistent with Unreal Engine 4, hence the extensive use of PascalCase and UE4 naming conventions.

### Features
All the feature implementations in this repository implement the feature [abstract class](https://github.com/xeroconf/scaffold/blob/master/Core/Feature/Feature.h) and communicate with each other using a [message dispatcher](https://github.com/xeroconf/messenger). Some features retrieve the feature instance from the owning [feature manager](https://github.com/xeroconf/scaffold/blob/master/Core/Feature/FeatureManager.h) directly, but this is bad practice on my part and was done to speed up release.

- `Feature::OnStart` is called once when the feature is initialized or enabled.
- `Feature::OnEnd` is called once when the feature is disabled.
- `Feature::Tick` is called every engine tick and drives the feature's update cycle.
- `Feature::CanExecute` is evaluated every tick and determines whether the feature should run.
- `Feature::OnConditionChange` is called only when the result of CanExecute changes from the previous tick.
- `Feature::OnExecute` is called every tick while CanExecute returns true.
- `Feature::OnDiscard` is called every tick while CanExecute returns false.

### Actor Service
The Actor Service was introduced as a solution to avoid iterating through the entire actor list every frame.
Instead of scanning all actors, we hooked into [key lifecycle events of AActor](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-actor-lifecycle), such as `BeginPlay` and `EndPlay`.

By listening to these events, we could selectively track only the actors we care about. Relevant actors were added to an internal list when they spawned and removed when they were destroyed. Additionally, `ActorAggregationUpdateEvent` was broadcast to all listening features, allowing them to independently track the actors.

This approach drastically reduced the number of actors we needed to process each frame, since the list(s) contained only the specific actors we were interested in, rather than every actor in the world. This significantly increased performance and is a major factor as to why Athenaware has minimal impact on framerate.

### Session Service
You may find references to the Session Service in some features (e.g., `GetSession()`, `IsInSession()`). The Session Service managed the state of the current server game world and provided helper functions for sending RPCs, managing action states, and more. It was also responsible for starting and stopping features that were specific to being in the world (e.g. ESP).

