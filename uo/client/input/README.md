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

## Wiring in the game scene

```cpp
// once
router.attach(gameLayer);                       // fixed priority 10: gumps see clicks first
router.now = [] { return uo::time::ms(); };     // the scene's clock, if not steady_clock

// every frame, in this order (GameScene.Update)
if (auto req = autoWalker.tick(playerMotion.endPosition().direction, walker.canRequestStep(now)))
    if (walk(req->direction, req->run).kind == WalkResult::Kind::Rejected) autoWalker.stop();

if (auto intent = router.movementIntent(playerScreenCenter, autoWalker.active())) {
    if (intent->cancelAutoWalk) autoWalker.stop();
    walk(intent->direction, intent->run);
}

for (auto& m : mobiles) advanceMotion(m.motion, m.isPlayer ? &walker : nullptr, m.mounted, now, frameMs);
```

`walk()` builds a `PlayerWalkState` from the player's `MobileMotion::endPosition()` and flags,
calls `Walker::walk`, and on `Sent` sends `result.packet` and pushes
`{result.x, result.y, result.z, direction, run}` onto the player's motion queue (on `Coalesced`
it only sets the facing). The pathfinder's `ITileSource` is the world's tile list for (x, y):
land with its stretched corner heights, statics, items and multis with their tiledata flags,
and mobiles.

Packets: 0x22 goes to `Walker::confirm` (send `net::out::resync()` when it says so), 0x21 to
`Walker::deny` plus clearing the player's motion and snapping it, 0x97 to `walk()`, 0xBF/1 and
0xBF/2 to `Walker::fastWalk()`; `MovementSystem::install` hooks all of these on `uo::world::PacketHandlers`. `Targeting::install` takes over 0x6C so a server cancel is echoed; 0x99 stays with the world handler.

Escape cancels the target cursor, else a cancellable auto-walk. Right double-click on the world
starts `AutoWalker::start` toward the clicked tile (distance 0; a blocked goal becomes 1).
