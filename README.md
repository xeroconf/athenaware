# ⛵ Athenaware
Most features from the Athenaware (Prismarine) project. Not all features were included as they contain code I wish not to share, the feature is not patched or I consider the feature unimportant for release.

> [!IMPORTANT]
> This repository contains feature implementations only and cannot be compiled in its current state. It is provided solely for reference and educational purposes.

> [!NOTE]
> Athenaware is built using my personal cheat framework/architecture, [Scaffold](https://github.com/xeroconf/scaffold).


## 🧠 About this Project
### ✨ Code Style
I wanted to keep the code style consistent with that of Unreal Engine 4, hence the excessive use of pascal case.

### 🧩 Features
All the feature implementations in this repository implement an [abstract class](https://github.com/xeroconf/scaffold/blob/master/Core/Feature/Feature.h) and communicate with each other by a shared [message dispatcher](https://github.com/xeroconf/messenger) provided by the [parent manager](https://github.com/xeroconf/scaffold/blob/master/Core/Domain/Domain.h). Some features retrieve the feature instance from the parent manager directly, but this is bad practice on my part and was done to speed up release.

### 🧑‍🚀 Actor Service
The Actor Service was introduced as a solution to avoid iterating through the entire actor list every frame.
Instead of scanning all actors, we hooked into key lifecycle events of AActor, such as `BeginPlay` and `EndPlay`.

By listening to these events, we could selectively track only the actors we care about.
Relevant actors were added to an internal list when they spawned and removed when they were destroyed. Additionally, `ActorAggregationUpdateEvent` was fired to all listening features.

This approach drastically reduced the number of actors we needed to process each frame, since our internal list contained only the specific actors we were interested in, rather than every actor in the world. This significantly increased performance and is a major factor as to why Athenaware has minimal impact on framerate.

The main caveat was handling actors that already existed in the world before all features could receive the event. The workaround was to iterate over the actor list once when each feature started.

### 🖼️ Draw Service
The Draw Service managed all draw lists and safely swapped them for rendering on the RHI thread. These draw lists allowed precise control over the rendering order of items on the screen using `DrawService::PushLayer`. For example, main UI elements could be ensured to appear on top of other content.
