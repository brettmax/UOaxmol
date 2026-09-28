# uo/client/input

Axmol glue for walking and targeting. The logic is engine-free in uocore; this folder only
routes Axmol events into it.

| Piece | Where | Ported from (ClassicUO) |
|---|---|---|
| Walk requests, 0x21/0x22 acks, fast-walk keys, turn coalescing | `uo/movement/Walker` | `WalkerManager`, `PlayerMobile.Walk` |
| Walkability and z, A* auto-walk | `uo/movement/Pathfinder`, `AutoWalker` | `Pathfinder` |
| Step interpolation for every mobile | `uo/movement/MobileMotion` | `Mobile.ProcessSteps`, `MovementSpeed` |
| Mouse and arrow walk intent | `uo/movement/MovementInput` | `GameSceneInputHandler`, `GameScene.Update` |
| Target clicks and 0x6C answers over `World::target` | `uo/world/Targeting` | `TargetManager` |
| Axmol pointer/keyboard listeners | `InputRouter` here | `GameSceneInputHandler`, `Input/Mouse` |

## Wiring in the app shell

`GameClient` owns a `WorldTileSource`, `MovementSystem` and `Targeting`, recreates them with the
world on connect, and installs their packet hooks. `walkHandler` and `playerTeleportedHandler`
route to the movement system. `WorldScene` owns the `InputRouter` and runs, every frame:

```cpp
gc.update(dt);  // network first
gc.updateMovement(router.movementIntent(playerScreenPoint(), movement.autoWalker().active()), dt);
// updateMovement: set the facet if it changed, tiles.rebuild(), then MovementSystem::update,
// which runs auto-walk, the input intent, and every mobile's step advance (GameScene.Update order).
```

Mobiles are drawn at their tile plus `MovementSystem::motion(serial)`'s pixel offset. Left click
targets the entity (or land tile) under the pointer while a cursor is up, else single-clicks it;
left double-click uses the entity. Until gumps claim their own clicks, the whole window counts as
the world, and statics are not pickable until the renderer can hit-test them.

Packets: 0x22 goes to `Walker::confirm` (send `net::out::resync()` when it says so), 0x21 to
`Walker::deny` plus clearing the player's motion and snapping it, 0x97 to `walk()`, 0xBF/1 and
0xBF/2 to `Walker::fastWalk()`; `MovementSystem::install` hooks all of these on `uo::world::PacketHandlers`. `Targeting::install` takes over 0x6C so a server cancel is echoed; 0x99 stays with the world handler.

Escape cancels the target cursor, else a cancellable auto-walk, else returns to the login screen. Right double-click on the world
starts `AutoWalker::start` toward the clicked tile (distance 0; a blocked goal becomes 1).
