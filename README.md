# ⛵ Athenaware
Most features from the Athenaware (Prismarine) project. Not all features were included as they contain code I wish not to share or I consider it unimportant for release.

Overtime I'll continue to populate this repository with features.

> [!IMPORTANT]
> This repository contains feature implementations only and cannot be compiled in its current state. It is provided solely for reference and educational purposes.

> [!NOTE]
> Athenaware is built on an old version of my cheat framework, [Scaffold](https://github.com/xeroconf/scaffold).


## About this Project
### Code Style
I chose to keep the code style consistent with Unreal Engine 4, hence the extensive use of PascalCase.

### Features
All the feature implementations in this repository implement the feature [abstract class](https://github.com/xeroconf/scaffold/blob/master/Core/Feature/Feature.h) and communicate with each other using a shared [message dispatcher](https://github.com/xeroconf/messenger) provided by the [managing domain](https://github.com/xeroconf/scaffold/blob/master/Core/Domain/Domain.h). Some features retrieve the feature instance from the owning [feature manager](https://github.com/xeroconf/scaffold/blob/master/Core/Feature/FeatureManager.h) directly, but this is bad practice on my part and was done to speed up release.

### Actor Service
The Actor Service was introduced as a solution to avoid iterating through the entire actor list every frame.
Instead of scanning all actors, we hooked into key lifecycle events of AActor, such as `BeginPlay` and `EndPlay`.

By listening to these events, we could selectively track only the actors we care about.
Relevant actors were added to an internal list when they spawned and removed when they were destroyed. Additionally, `ActorAggregationUpdateEvent` was fired to all listening features. The goal was to allow each feature to independently track the actors it needs by handling the relevant actor lifecycle event(s).

This approach drastically reduced the number of actors we needed to process each frame, since our internal list contained only the specific actors we were interested in, rather than every actor in the world. This significantly increased performance and is a major factor as to why Athenaware has minimal impact on framerate.

The main caveat was handling actors that already existed in the world before all features could receive the event. The workaround was to iterate over the actor list once when each feature started.

### Draw Service
The Draw Service managed all draw lists and safely swapped them for rendering on the RHI thread. These draw lists allowed precise control over the rendering order of items on the screen using `DrawService::PushLayer`. For example, main UI elements could be ensured to appear on top of other content.

### Session Service
You may find references to the Session Service in some features (e.g., `GetSession()`, `IsInSession()`). The Session Service managed the state of the current server game world and provided helper functions for sending RPCs, managing action states, and more. It was also responsible for starting and stopping features that were specific to being in the world (e.g. ESP).

![Screenshot of Athenaware](https://cdn.discordapp.com/attachments/1070231600979259392/1253985203253411972/SoTGame_8HMZRvCKxp.jpg?ex=68fa8b7a&is=68f939fa&hm=59235a31601c98d3fce5840d5c1288592570281627ced0ee02c5200f21287ea6)
