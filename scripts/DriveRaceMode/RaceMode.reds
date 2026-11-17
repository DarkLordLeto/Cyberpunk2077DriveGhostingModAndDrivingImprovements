// DriveNoCollision race mode 0.4.0
// Requires redscript 0.5.32+, Codeware 1.21+ (raw key input, dynamic entities,
// reflection) and the DriveNoCollision plugin 0.32.0+ for road routes and GPS
// directions (without it the checkpoints follow straight lines).
//
// Build a circuit from map waypoints, then drive it as a time trial with the
// game's own race HUD, the game's race checkpoint posts, and GPS directions to
// the next checkpoint (the checkpoint's map marker becomes the tracked marker).
// Checkpoints are laid along the game's GPS road route between the circuit points.
// Keys (work on the map screen and in the world):
//   F6   set START to the map waypoint (or the car's position if none is placed)
//   F7   set END to the map waypoint (leave unset, or put it on START, for a loop)
//   F8   add an extra point before the END, in driving order
//   F10  remove the last thing added (extra point, then END, then START)
//   F2   clear the whole circuit
//   F11  enter race mode / leave race mode
module DriveNoCollision.Race

enum RacePhase {
  Editing = 0,
  WaitingAtStart = 1,
  Lapping = 2,
  Planning = 3,
}

public class RaceTickCallback extends DelayCallback {
  public let system: wref<RaceModeSystem>;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnRaceTick();
    };
  }
}

// Polls the road route queries while the race route is being planned.
public class RacePlanCallback extends DelayCallback {
  public let system: wref<RaceModeSystem>;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnPlanTick();
    };
  }
}

// Sends a race HUD step a moment after the previous one, as quests do.
public class RaceUICallback extends DelayCallback {
  public let system: wref<RaceModeSystem>;
  public let mode: vehicleRaceUI;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnDelayedRaceUI(this.mode);
    };
  }
}

// Fires a few seconds after a session starts to confirm the script is running.
public class RaceReadyCallback extends DelayCallback {
  public let system: wref<RaceModeSystem>;

  public func Call() -> Void {
    if IsDefined(this.system) {
      this.system.OnReady();
    };
  }
}

public class RaceModeSystem extends ScriptableSystem {
  // Circuit as the player defined it. Saved with the game, together with the
  // marker IDs, so a reload can remove markers that were saved while visible.
  private persistent let hasStart: Bool;
  private persistent let hasEnd: Bool;
  private persistent let startPoint: Vector4;
  private persistent let endPoint: Vector4;
  private persistent let extraPoints: array<Vector4>;
  private persistent let bestLap: Float;
  private persistent let editPins: array<NewMappinID>;
  private persistent let racePins: array<NewMappinID>;
  private persistent let raceWasActive: Bool;
  private persistent let savedCheckpointFact: Int32;

  // Checkpoints generated for the current race. A gate is a point the player
  // chose and must be driven through. The others are spaced along the route
  // between gates and are skipped automatically if the car goes another way.
  // Each checkpoint has a travel direction, a pass radius, and whether its
  // position came from the road route (then its height is the road surface).
  private let checkpoints: array<Vector4>;
  private let isGate: array<Bool>;
  private let checkpointDir: array<Vector4>;
  private let checkpointRadius: array<Float>;
  private let checkpointOnRoad: array<Bool>;
  private let isLoop: Bool;
  private let raceStart: Vector4;
  private let raceStartDir: Vector4;
  private let raceStartOnRoad: Bool;

  // Route planning: one GPS road route query per leg between circuit points.
  // routePoints is the whole route from the START to the finish, and
  // routeGates holds the index of each leg's end in it.
  private let legPoints: array<Vector4>;
  private let planLeg: Int32;
  private let planQuery: Int32;
  private let planLegStart: Float;
  private let routePoints: array<Vector4>;
  private let routeOnRoad: array<Bool>;
  private let routeGates: array<Int32>;
  private let roadLegs: Int32;

  private let phase: RacePhase;
  private let nextCheckpoint: Int32;
  private let lapStartTime: Float;
  private let lapCount: Int32;
  private let lastLap: Float;
  private let clockRunning: Bool;
  private let hudActive: Bool;
  private let raceVehicle: wref<VehicleObject>;
  private let heldKeys: array<EInputKey>;

  // Checkpoint posts spawned through Codeware's DynamicEntitySystem, two per
  // checkpoint, only for the next few checkpoints near the car.
  private let gateLeft: array<EntityID>;
  private let gateRight: array<EntityID>;
  private let gateSpawned: array<Bool>;
  private let startGateSpawned: Bool;
  private let startGateLeft: EntityID;
  private let startGateRight: EntityID;

  // GPS guidance: the next checkpoint's marker is made the player's tracked
  // marker through the plugin. The marker tracked before the race is restored.
  private let hasGps: Bool;
  private let gpsNoticeShown: Bool;
  private let trackedBeforeRace: NewMappinID;

  private func OnAttach() -> Void {
    this.phase = RacePhase.Editing;
    let ready: ref<RaceReadyCallback> = new RaceReadyCallback();
    // No key filter and the default run mode: every key event reaches
    // OnKeyInput, which picks out the race keys itself.
    GameInstance.GetCallbackSystem().RegisterCallback(n"Input/Key", this, n"OnKeyInput");
    this.Log("system attached, key callback registered");
    ready.system = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(ready, 8.0, false);
  }

  public func OnReady() -> Void {
    this.Log("ready message shown");
    // Posts are never saved, but remove any left over from an interrupted session.
    if NotEquals(this.phase, RacePhase.Lapping) && NotEquals(this.phase, RacePhase.WaitingAtStart) {
      GameInstance.GetDynamicEntitySystem().DeleteTagged(n"DriveRaceGate");
    };
    this.RefreshEditPins();
    this.Notify("Race mode ready: F6 start, F7 end, F8 extra point, F10 undo, F2 clear, F11 race on/off");
  }

  private func Log(text: String) -> Void {
    FTLog("[DriveRaceMode] " + text);
    ModLog(n"DriveRaceMode", text);
  }

  private func IsRaceKey(key: EInputKey) -> Bool {
    return Equals(key, EInputKey.IK_F2) || Equals(key, EInputKey.IK_F6) || Equals(key, EInputKey.IK_F7)
      || Equals(key, EInputKey.IK_F8) || Equals(key, EInputKey.IK_F10) || Equals(key, EInputKey.IK_F11);
  }

  // A save made during race mode stores the race markers and the borrowed HUD
  // counter. Put both back the way the game had them, then show the circuit.
  private func OnRestored(saveVersion: Int32, gameVersion: Int32) -> Void {
    this.phase = RacePhase.Editing;
    this.ClearPins(this.racePins);
    if this.raceWasActive {
      GameInstance.GetQuestsSystem(this.GetGameInstance()).SetFact(n"sq024_current_race_checkpoint_fact_add", this.savedCheckpointFact);
      this.raceWasActive = false;
    };
    this.RefreshEditPins();
  }

  // ---- input -------------------------------------------------------------

  private cb func OnKeyInput(evt: ref<KeyInputEvent>) {
    let key: EInputKey = evt.GetKey();
    if !this.IsRaceKey(key) {
      return;
    };
    if Equals(evt.GetAction(), EInputAction.IACT_Release) {
      ArrayRemove(this.heldKeys, key);
      return;
    };
    if NotEquals(evt.GetAction(), EInputAction.IACT_Press) || ArrayContains(this.heldKeys, key) {
      return;
    };
    ArrayPush(this.heldKeys, key);
    this.Log("key pressed: " + ToString(key));
    if Equals(key, EInputKey.IK_F11) {
      if Equals(this.phase, RacePhase.Editing) {
        this.EnterRaceMode();
      } else {
        this.LeaveRaceMode("Race mode off");
      };
      return;
    };
    if NotEquals(this.phase, RacePhase.Editing) {
      this.Notify("Leave race mode (F11) to edit the circuit");
      return;
    };
    if Equals(key, EInputKey.IK_F6) {
      this.SetStart();
    } else {
      if Equals(key, EInputKey.IK_F7) {
        this.SetEnd();
      } else {
        if Equals(key, EInputKey.IK_F8) {
          this.AddExtraPoint();
        } else {
          if Equals(key, EInputKey.IK_F10) {
            this.RemoveLast();
          } else {
            if Equals(key, EInputKey.IK_F2) {
              this.ClearCircuit();
            };
          };
        };
      };
    };
  }

  // ---- circuit editing ---------------------------------------------------

  // The player's own map waypoint if one is placed, otherwise where the player is.
  private func PickPoint(out point: Vector4, out fromWaypoint: Bool) -> Bool {
    let game: GameInstance = this.GetGameInstance();
    let mappins: ref<MappinSystem> = GameInstance.GetMappinSystem(game);
    let tracked: ref<IMappin> = mappins.GetMappin(mappins.GetManuallyTrackedMappinID());
    let all: array<ref<IMappin>>;
    let i: Int32 = 0;
    let player: ref<PlayerPuppet> = GetPlayer(game);
    fromWaypoint = true;
    if IsDefined(tracked) && Equals(tracked.GetVariant(), gamedataMappinVariant.CustomPositionVariant) {
      point = tracked.GetWorldPosition();
      return true;
    };
    mappins.GetMappins(gamemappinsMappinTargetType.Map, all);
    while i < ArraySize(all) {
      if IsDefined(all[i]) && Equals(all[i].GetVariant(), gamedataMappinVariant.CustomPositionVariant) {
        point = all[i].GetWorldPosition();
        return true;
      };
      i += 1;
    };
    fromWaypoint = false;
    if !IsDefined(player) {
      return false;
    };
    point = player.GetWorldPosition();
    return true;
  }

  private func Source(fromWaypoint: Bool) -> String {
    if fromWaypoint {
      return "map waypoint";
    };
    return "current position (no map waypoint placed)";
  }

  private func SetStart() -> Void {
    let point: Vector4;
    let fromWaypoint: Bool;
    if !this.PickPoint(point, fromWaypoint) {
      return;
    };
    this.startPoint = point;
    this.hasStart = true;
    this.bestLap = 0.0;
    this.RefreshEditPins();
    this.Notify("Race START set at " + this.Source(fromWaypoint));
  }

  private func SetEnd() -> Void {
    let point: Vector4;
    let fromWaypoint: Bool;
    if !this.hasStart {
      this.Notify("Set the START first (F6)");
      return;
    };
    if !this.PickPoint(point, fromWaypoint) {
      return;
    };
    this.endPoint = point;
    this.hasEnd = true;
    this.bestLap = 0.0;
    this.RefreshEditPins();
    if this.EndIsStart() {
      this.Notify("Race END is on the START: the circuit is a loop");
    } else {
      this.Notify("Race END set at " + this.Source(fromWaypoint));
    };
  }

  private func AddExtraPoint() -> Void {
    let point: Vector4;
    let fromWaypoint: Bool;
    if !this.hasStart {
      this.Notify("Set the START first (F6)");
      return;
    };
    if ArraySize(this.extraPoints) >= 40 {
      this.Notify("The circuit already has 40 extra points");
      return;
    };
    if !this.PickPoint(point, fromWaypoint) {
      return;
    };
    ArrayPush(this.extraPoints, point);
    this.bestLap = 0.0;
    this.RefreshEditPins();
    this.Notify("Extra point " + IntToString(ArraySize(this.extraPoints)) + " added at " + this.Source(fromWaypoint));
  }

  private func RemoveLast() -> Void {
    if ArraySize(this.extraPoints) > 0 {
      ArrayPop(this.extraPoints);
      this.Notify("Removed the last extra point (" + IntToString(ArraySize(this.extraPoints)) + " left)");
    } else {
      if this.hasEnd {
        this.hasEnd = false;
        this.Notify("Race END removed");
      } else {
        if this.hasStart {
          this.hasStart = false;
          this.Notify("Race START removed");
        } else {
          this.Notify("The circuit is already empty");
        };
      };
    };
    this.bestLap = 0.0;
    this.RefreshEditPins();
  }

  private func ClearCircuit() -> Void {
    this.hasStart = false;
    this.hasEnd = false;
    ArrayClear(this.extraPoints);
    this.bestLap = 0.0;
    this.bestLap = 0.0;
    this.RefreshEditPins();
    this.Notify("Race circuit cleared");
  }

  // A missing END, or an END within 25 m of the START, means a closed loop.
  private func EndIsStart() -> Bool {
    return !this.hasEnd || Vector4.Distance2D(this.startPoint, this.endPoint) < 25.0;
  }

  // ---- checkpoint generation ---------------------------------------------

  // The circuit points in driving order: START, extra points, then END, or the
  // START again for a loop. A loop needs somewhere to go before coming back.
  private func MakeLegPoints() -> Bool {
    let i: Int32 = 0;
    ArrayClear(this.legPoints);
    this.isLoop = this.EndIsStart();
    ArrayPush(this.legPoints, this.startPoint);
    while i < ArraySize(this.extraPoints) {
      ArrayPush(this.legPoints, this.extraPoints[i]);
      i += 1;
    };
    if this.isLoop {
      if ArraySize(this.extraPoints) == 0 {
        return false;
      };
      ArrayPush(this.legPoints, this.startPoint);
    } else {
      ArrayPush(this.legPoints, this.endPoint);
    };
    return true;
  }

  private func ClearRoute() -> Void {
    ArrayClear(this.routePoints);
    ArrayClear(this.routeOnRoad);
    ArrayClear(this.routeGates);
    this.roadLegs = 0;
  }

  private func PushRoutePoint(point: Vector4, onRoad: Bool) -> Void {
    ArrayPush(this.routePoints, point);
    ArrayPush(this.routeOnRoad, onRoad);
  }

  // Straight lines between the circuit points, used without the plugin.
  private func BuildStraightRoute() -> Void {
    let i: Int32 = 0;
    this.ClearRoute();
    while i < ArraySize(this.legPoints) {
      this.PushRoutePoint(this.legPoints[i], false);
      if i > 0 {
        ArrayPush(this.routeGates, i);
      };
      i += 1;
    };
  }

  // Adds the leg from legPoints[planLeg] to legPoints[planLeg + 1]. With a road
  // route that really connects the two points, the leg follows it and its gate
  // sits on the road where the route ends; otherwise the leg is a straight line.
  private func AddLeg(count: Int32) -> Void {
    let from: Vector4 = this.legPoints[this.planLeg];
    let to: Vector4 = this.legPoints[this.planLeg + 1];
    let first: Vector4;
    let last: Vector4;
    let point: Vector4;
    let i: Int32 = 1;
    let road: Bool = count >= 2;
    if road {
      first = this.RouteQueryPoint(0);
      last = this.RouteQueryPoint(count - 1);
      road = Vector4.Distance2D(first, from) < 60.0 && Vector4.Distance2D(last, to) < 60.0;
    };
    if !road {
      if ArraySize(this.routePoints) == 0 {
        this.PushRoutePoint(from, false);
      };
      this.PushRoutePoint(to, false);
      ArrayPush(this.routeGates, ArraySize(this.routePoints) - 1);
      this.Log("leg " + IntToString(this.planLeg + 1) + ": straight line (route status " + IntToString(count) + ")");
      return;
    };
    if ArraySize(this.routePoints) == 0 || Vector4.Distance2D(this.routePoints[ArraySize(this.routePoints) - 1], first) > 2.0 {
      this.PushRoutePoint(first, true);
    };
    // Route points can be a metre apart; every 3 m is plenty for placement.
    while i < count {
      point = this.RouteQueryPoint(i);
      if i == count - 1 || Vector4.Distance2D(point, this.routePoints[ArraySize(this.routePoints) - 1]) >= 3.0 {
        this.PushRoutePoint(point, true);
      };
      i += 1;
    };
    ArrayPush(this.routeGates, ArraySize(this.routePoints) - 1);
    this.roadLegs += 1;
    this.Log("leg " + IntToString(this.planLeg + 1) + ": road route with " + IntToString(count) + " points");
  }

  // Travel direction at a route point, from the points about 12 m either side.
  private func RouteDirection(index: Int32) -> Vector4 {
    let count: Int32 = ArraySize(this.routePoints);
    let a: Int32 = index;
    let b: Int32 = index;
    let dx: Float;
    let dy: Float;
    let length: Float;
    while a > 0 && Vector4.Distance2D(this.routePoints[a], this.routePoints[index]) < 12.0 {
      a -= 1;
    };
    while b < count - 1 && Vector4.Distance2D(this.routePoints[b], this.routePoints[index]) < 12.0 {
      b += 1;
    };
    dx = this.routePoints[b].X - this.routePoints[a].X;
    dy = this.routePoints[b].Y - this.routePoints[a].Y;
    length = SqrtF(dx * dx + dy * dy);
    if length < 0.1 {
      return new Vector4(0.0, 1.0, 0.0, 0.0);
    };
    return new Vector4(dx / length, dy / length, 0.0, 0.0);
  }

  private func AddCheckpoint(at: Vector4, dir: Vector4, gate: Bool, onRoad: Bool) -> Void {
    ArrayPush(this.checkpoints, at);
    ArrayPush(this.isGate, gate);
    ArrayPush(this.checkpointDir, dir);
    ArrayPush(this.checkpointOnRoad, onRoad);
    if gate {
      ArrayPush(this.checkpointRadius, 14.0);
    } else {
      if onRoad {
        ArrayPush(this.checkpointRadius, 25.0);
      } else {
        ArrayPush(this.checkpointRadius, 35.0);
      };
    };
  }

  // Lays checkpoints along the route: a gate at the end of every leg, and
  // evenly spaced checkpoints about every 150 m of route between gates.
  private func BuildCheckpointsFromRoute() -> Void {
    let count: Int32 = ArraySize(this.routePoints);
    let along: array<Float>;
    let i: Int32 = 1;
    let g: Int32 = 0;
    let k: Int32;
    let gate: Int32;
    let pieces: Int32;
    let cursor: Int32 = 0;
    let previous: Float = 0.0;
    let length: Float;
    let target: Float;
    let segment: Float;
    let t: Float;
    let spacing: Float = 150.0;
    ArrayClear(this.checkpoints);
    ArrayClear(this.isGate);
    ArrayClear(this.checkpointDir);
    ArrayClear(this.checkpointRadius);
    ArrayClear(this.checkpointOnRoad);
    ArrayPush(along, 0.0);
    while i < count {
      ArrayPush(along, along[i - 1] + Vector4.Distance2D(this.routePoints[i - 1], this.routePoints[i]));
      i += 1;
    };
    this.raceStart = this.routePoints[0];
    this.raceStartDir = this.RouteDirection(0);
    this.raceStartOnRoad = this.routeOnRoad[0];
    while g < ArraySize(this.routeGates) && ArraySize(this.checkpoints) < 400 {
      gate = this.routeGates[g];
      length = along[gate] - previous;
      pieces = FloorF(length / spacing) + 1;
      k = 1;
      while k < pieces && ArraySize(this.checkpoints) < 399 {
        target = previous + length * Cast<Float>(k) / Cast<Float>(pieces);
        while cursor < gate - 1 && along[cursor + 1] < target {
          cursor += 1;
        };
        segment = along[cursor + 1] - along[cursor];
        t = 0.0;
        if segment > 0.01 {
          t = ClampF((target - along[cursor]) / segment, 0.0, 1.0);
        };
        this.AddCheckpoint(Vector4.Interpolate(this.routePoints[cursor], this.routePoints[cursor + 1], t),
          this.RouteDirection(cursor), false, this.routeOnRoad[cursor] && this.routeOnRoad[cursor + 1]);
        k += 1;
      };
      this.AddCheckpoint(this.routePoints[gate], this.RouteDirection(gate), true, this.routeOnRoad[gate]);
      previous = along[gate];
      cursor = gate;
      g += 1;
    };
  }

  // ---- road route planning -------------------------------------------------

  private func RoutePlannerAvailable() -> Bool {
    return IsDefined(Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRouteStart"))
      && IsDefined(Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRoutePoll"))
      && IsDefined(Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRoutePoint"));
  }

  // The plugin functions are called through reflection so the script still
  // loads with an older plugin that does not provide them.
  private func StartRouteQuery(from: Vector4, to: Vector4) -> Int32 {
    let fn: ref<ReflectionStaticFunc> = Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRouteStart");
    let args: array<Variant>;
    if !IsDefined(fn) {
      return -1;
    };
    ArrayPush(args, ToVariant(from));
    ArrayPush(args, ToVariant(to));
    return FromVariant<Int32>(fn.Call(args));
  }

  // 0 while computing, the number of route points when ready, below 0 on failure.
  private func PollRouteQuery(id: Int32) -> Int32 {
    let fn: ref<ReflectionStaticFunc> = Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRoutePoll");
    let args: array<Variant>;
    if !IsDefined(fn) {
      return -2;
    };
    ArrayPush(args, ToVariant(id));
    return FromVariant<Int32>(fn.Call(args));
  }

  private func RouteQueryPoint(index: Int32) -> Vector4 {
    let fn: ref<ReflectionStaticFunc> = Reflection.GetGlobalFunction(n"DriveNoCollision_GpsRoutePoint");
    let args: array<Variant>;
    ArrayPush(args, ToVariant(index));
    return FromVariant<Vector4>(fn.Call(args));
  }

  private func BeginRoutePlanning() -> Void {
    this.ClearRoute();
    this.planLeg = 0;
    this.phase = RacePhase.Planning;
    this.StartPlanLeg();
    this.SchedulePlanTick();
  }

  private func StartPlanLeg() -> Void {
    this.planQuery = this.StartRouteQuery(this.legPoints[this.planLeg], this.legPoints[this.planLeg + 1]);
    this.planLegStart = this.Now();
  }

  private func SchedulePlanTick() -> Void {
    let callback: ref<RacePlanCallback> = new RacePlanCallback();
    callback.system = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, 0.1, false);
  }

  // One leg at a time: wait up to 6 s for its route, then take the next leg.
  public func OnPlanTick() -> Void {
    let player: ref<PlayerPuppet>;
    let status: Int32 = -1;
    if NotEquals(this.phase, RacePhase.Planning) {
      return;
    };
    player = GetPlayer(this.GetGameInstance());
    if !IsDefined(player) || !IsDefined(GetMountedVehicle(player)) {
      this.LeaveRaceMode("Race mode off: you left the vehicle");
      return;
    };
    if this.planQuery >= 0 {
      status = this.PollRouteQuery(this.planQuery);
      if status == 0 && this.Now() - this.planLegStart < 6.0 {
        this.SchedulePlanTick();
        return;
      };
    };
    this.AddLeg(status);
    this.planLeg += 1;
    if this.planLeg < ArraySize(this.legPoints) - 1 {
      this.StartPlanLeg();
      this.SchedulePlanTick();
      return;
    };
    this.BuildCheckpointsFromRoute();
    this.BeginRaceMode();
  }

  // ---- race mode ---------------------------------------------------------

  private func EnterRaceMode() -> Void {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<PlayerPuppet> = GetPlayer(game);
    let vehicle: wref<VehicleObject>;
    if !IsDefined(player) {
      return;
    };
    if !this.hasStart {
      this.Notify("Set a START first: place a map waypoint and press F6");
      return;
    };
    if GameInstance.GetRacingSystem(game).IsRaceInProgress() {
      this.Notify("A story race is running, race mode is unavailable");
      return;
    };
    vehicle = GetMountedVehicle(player);
    if !IsDefined(vehicle) {
      this.Notify("Get in a vehicle to enter race mode");
      return;
    };
    if !this.MakeLegPoints() {
      this.Notify("A loop needs at least one extra point (F8) besides the START");
      return;
    };
    this.raceVehicle = vehicle;
    this.ClearPins(this.editPins);
    if this.RoutePlannerAvailable() {
      this.Notify("Planning the race route along the roads...");
      this.BeginRoutePlanning();
      return;
    };
    this.BuildStraightRoute();
    this.BuildCheckpointsFromRoute();
    this.BeginRaceMode();
  }

  private func BeginRaceMode() -> Void {
    let game: GameInstance = this.GetGameInstance();
    let legs: Int32 = ArraySize(this.legPoints) - 1;
    let routeNote: String;
    this.savedCheckpointFact = GameInstance.GetQuestsSystem(game).GetFact(n"sq024_current_race_checkpoint_fact_add");
    this.lapCount = 0;
    this.lastLap = 0.0;
    this.clockRunning = false;
    this.ResetGates();
    GameInstance.GetBlackboardSystem(game).Get(GetAllBlackboardDefs().UI_ActiveVehicleData).SetInt(GetAllBlackboardDefs().UI_ActiveVehicleData.PositionInRace, 1, true);
    this.SendRaceUI(vehicleRaceUI.PreRaceSetup);
    this.SendRaceUILater(vehicleRaceUI.CountdownStart, 1.0);
    this.hudActive = true;
    this.raceWasActive = true;
    this.phase = RacePhase.WaitingAtStart;
    this.nextCheckpoint = 0;
    this.SetCheckpointFact(0);
    this.trackedBeforeRace = GameInstance.GetMappinSystem(game).GetManuallyTrackedMappinID();
    this.hasGps = IsDefined(Reflection.GetGlobalFunction(n"DriveNoCollision_TrackMappin"));
    this.RefreshRacePins();
    if !this.RoutePlannerAvailable() {
      routeNote = " Checkpoints follow straight lines: road routes and GPS directions need the DriveNoCollision plugin 0.32.0.";
    } else {
      if this.roadLegs < legs {
        routeNote = " " + IntToString(legs - this.roadLegs) + " of " + IntToString(legs) + " legs had no road route and use straight lines.";
      };
    };
    if this.isLoop {
      this.Notify("Race mode on: loop with " + IntToString(ArraySize(this.checkpoints)) + " checkpoints. Drive through the START to begin. F11 exits." + routeNote);
    } else {
      this.Notify("Race mode on: " + IntToString(ArraySize(this.checkpoints)) + " checkpoints. Drive through the START to begin. F11 exits." + routeNote);
    };
    this.Log("race route: " + IntToString(ArraySize(this.routePoints)) + " points, " + IntToString(this.roadLegs) + " of " + IntToString(legs) + " legs on roads, " + IntToString(ArraySize(this.checkpoints)) + " checkpoints");
    this.ScheduleTick();
  }

  private func LeaveRaceMode(message: String) -> Void {
    let game: GameInstance = this.GetGameInstance();
    let ui: ref<UISystem> = GameInstance.GetUISystem(game);
    let forward: ref<ForwardVehicleRaceUIEvent>;
    if this.hudActive {
      if IsDefined(this.raceVehicle) {
        if this.clockRunning {
          this.SendRaceUI(vehicleRaceUI.RaceEnd);
        };
        this.SendRaceUI(vehicleRaceUI.Disable);
      } else {
        // The vehicle is gone, so close the race HUD directly.
        forward = new ForwardVehicleRaceUIEvent();
        forward.mode = vehicleRaceUI.Disable;
        ui.QueueEvent(forward);
        ui.PopGameContext(UIGameContext.VehicleRace);
      };
      GameInstance.GetQuestsSystem(game).SetFact(n"sq024_current_race_checkpoint_fact_add", this.savedCheckpointFact);
    };
    this.hudActive = false;
    this.raceWasActive = false;
    this.clockRunning = false;
    this.phase = RacePhase.Editing;
    this.ClearGates();
    this.ClearPins(this.racePins);
    this.RestoreTracking();
    this.RefreshEditPins();
    if NotEquals(message, "") {
      if this.bestLap > 0.0 {
        this.Notify(message + ". Laps: " + IntToString(this.lapCount) + ", best " + this.FormatTime(this.bestLap));
      } else {
        this.Notify(message);
      };
    };
  }

  private func ScheduleTick() -> Void {
    let callback: ref<RaceTickCallback> = new RaceTickCallback();
    callback.system = this;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, 0.05, false);
  }

  public func OnRaceTick() -> Void {
    let game: GameInstance = this.GetGameInstance();
    let player: ref<PlayerPuppet>;
    let position: Vector4;
    let i: Int32;
    let reached: Int32 = -1;
    let radius: Float;
    if Equals(this.phase, RacePhase.Editing) {
      return;
    };
    player = GetPlayer(game);
    if !IsDefined(player) || !IsDefined(GetMountedVehicle(player)) {
      this.LeaveRaceMode("Race mode off: you left the vehicle");
      return;
    };
    position = player.GetWorldPosition();
    if Equals(this.phase, RacePhase.WaitingAtStart) {
      if Vector4.Distance2D(position, this.raceStart) < 14.0 {
        this.BeginLap();
      };
    } else {
      // Look ahead as far as the next gate. Reaching a later checkpoint counts
      // the skipped in-between ones as passed.
      i = this.nextCheckpoint;
      while i < ArraySize(this.checkpoints) {
        radius = this.checkpointRadius[i];
        if Vector4.Distance2D(position, this.checkpoints[i]) < radius {
          reached = i;
        };
        if this.isGate[i] {
          break;
        };
        i += 1;
      };
      if reached >= 0 {
        this.nextCheckpoint = reached + 1;
        this.SetCheckpointFact(this.nextCheckpoint);
        if this.nextCheckpoint >= ArraySize(this.checkpoints) {
          this.FinishLap();
        } else {
          this.RefreshRacePins();
        };
      };
    };
    if NotEquals(this.phase, RacePhase.Editing) {
      this.UpdateGates(position);
      this.ScheduleTick();
    };
  }

  private func Now() -> Float {
    return EngineTime.ToFloat(GameInstance.GetSimTime(this.GetGameInstance()));
  }

  private func BeginLap() -> Void {
    if this.clockRunning {
      this.SendRaceUI(vehicleRaceUI.RaceEnd);
    };
    this.SendRaceUI(vehicleRaceUI.RaceStart);
    this.clockRunning = true;
    this.lapStartTime = this.Now();
    this.nextCheckpoint = 0;
    this.SetCheckpointFact(0);
    this.phase = RacePhase.Lapping;
    this.RefreshRacePins();
  }

  private func FinishLap() -> Void {
    let text: String;
    this.lastLap = this.Now() - this.lapStartTime;
    this.lapCount += 1;
    text = "Lap " + IntToString(this.lapCount) + ": " + this.FormatTime(this.lastLap);
    if this.bestLap <= 0.0 || this.lastLap < this.bestLap {
      this.bestLap = this.lastLap;
      text += " (new best)";
    } else {
      text += " (best " + this.FormatTime(this.bestLap) + ")";
    };
    this.Notify(text);
    FTLog("[DriveRaceMode] " + text);
    if this.isLoop {
      // The finish is the start, so the next lap is already under way.
      this.BeginLap();
    } else {
      this.SendRaceUI(vehicleRaceUI.RaceEnd);
      this.clockRunning = false;
      this.phase = RacePhase.WaitingAtStart;
      this.nextCheckpoint = 0;
      this.RefreshRacePins();
    };
  }

  // ---- game race HUD -----------------------------------------------------

  private func SendRaceUI(mode: vehicleRaceUI) -> Void {
    let evt: ref<VehicleRaceQuestEvent> = new VehicleRaceQuestEvent();
    let ps: ref<VehicleComponentPS>;
    if !IsDefined(this.raceVehicle) {
      return;
    };
    evt.mode = mode;
    evt.maxPosition = 1;
    evt.maxCheckpoints = ArraySize(this.checkpoints);
    // VehicleComponentPS forwards the event to the race HUD and pushes or pops
    // the VehicleRace UI context, then passes it on to the vehicle for the clock.
    ps = this.raceVehicle.GetVehiclePS();
    if IsDefined(ps) {
      GameInstance.GetPersistencySystem(this.GetGameInstance()).QueuePSEvent(ps.GetID(), ps.GetClassName(), evt);
    } else {
      this.raceVehicle.QueueEvent(evt);
    };
  }

  private func SendRaceUILater(mode: vehicleRaceUI, delay: Float) -> Void {
    let callback: ref<RaceUICallback> = new RaceUICallback();
    callback.system = this;
    callback.mode = mode;
    GameInstance.GetDelaySystem(this.GetGameInstance()).DelayCallback(callback, delay, false);
  }

  public func OnDelayedRaceUI(mode: vehicleRaceUI) -> Void {
    if this.hudActive {
      this.SendRaceUI(mode);
    };
  }

  private func SetCheckpointFact(passed: Int32) -> Void {
    GameInstance.GetQuestsSystem(this.GetGameInstance()).SetFact(n"sq024_current_race_checkpoint_fact_add", passed);
  }

  // ---- checkpoint posts -------------------------------------------------

  private func ResetGates() -> Void {
    let i: Int32 = 0;
    let none: EntityID;
    this.ClearGates();
    while i < ArraySize(this.checkpoints) {
      ArrayPush(this.gateLeft, none);
      ArrayPush(this.gateRight, none);
      ArrayPush(this.gateSpawned, false);
      i += 1;
    };
  }

  private func ClearGates() -> Void {
    let none: EntityID;
    GameInstance.GetDynamicEntitySystem().DeleteTagged(n"DriveRaceGate");
    ArrayClear(this.gateLeft);
    ArrayClear(this.gateRight);
    ArrayClear(this.gateSpawned);
    this.startGateSpawned = false;
    this.startGateLeft = none;
    this.startGateRight = none;
  }

  private func DeleteGate(i: Int32) -> Void {
    let entities: ref<DynamicEntitySystem> = GameInstance.GetDynamicEntitySystem();
    entities.DeleteEntity(this.gateLeft[i]);
    entities.DeleteEntity(this.gateRight[i]);
    this.gateSpawned[i] = false;
  }

  // Keeps posts at the START and at the next three checkpoints within 350 m.
  // Posts are placed only near the car so the ground below them is loaded.
  private func UpdateGates(position: Vector4) -> Void {
    let count: Int32 = ArraySize(this.checkpoints);
    let i: Int32 = 0;
    let left: EntityID;
    let right: EntityID;
    if !this.startGateSpawned && count > 0 && Vector4.Distance2D(position, this.raceStart) < 350.0 {
      this.SpawnGate(this.raceStart, this.raceStartDir, this.raceStartOnRoad, position.Z, left, right);
      this.startGateLeft = left;
      this.startGateRight = right;
      this.startGateSpawned = true;
    };
    while i < count {
      if this.gateSpawned[i] && (i < this.nextCheckpoint || i >= this.nextCheckpoint + 3) {
        this.DeleteGate(i);
      } else {
        // On a loop the last checkpoint is the START, which has its own posts.
        if !this.gateSpawned[i] && i >= this.nextCheckpoint && i < this.nextCheckpoint + 3
          && !(this.isLoop && i == count - 1) && Vector4.Distance2D(position, this.checkpoints[i]) < 350.0 {
          this.SpawnGate(this.checkpoints[i], this.checkpointDir[i], this.checkpointOnRoad[i], position.Z, left, right);
          this.gateLeft[i] = left;
          this.gateRight[i] = right;
          this.gateSpawned[i] = true;
        };
      };
      i += 1;
    };
  }

  // Two posts 13 m apart across the direction of travel, facing the car.
  private func SpawnGate(at: Vector4, dir: Vector4, onRoad: Bool, hintZ: Float, out left: EntityID, out right: EntityID) -> Void {
    let length: Float = SqrtF(dir.X * dir.X + dir.Y * dir.Y);
    let half: Float = 6.5;
    let fx: Float = 0.0;
    let fy: Float = 1.0;
    let leftPos: Vector4;
    let rightPos: Vector4;
    let facing: Quaternion;
    if length > 0.1 {
      fx = dir.X / length;
      fy = dir.Y / length;
    };
    // With Z up, the right-hand side of travel direction (fx, fy) is (fy, -fx).
    leftPos = new Vector4(at.X - fy * half, at.Y + fx * half, at.Z, 1.0);
    rightPos = new Vector4(at.X + fy * half, at.Y - fx * half, at.Z, 1.0);
    leftPos.Z = this.GroundZ(leftPos, hintZ, onRoad);
    rightPos.Z = this.GroundZ(rightPos, hintZ, onRoad);
    facing = EulerAngles.ToQuat(Vector4.ToRotation(new Vector4(-fx, -fy, 0.0, 0.0)));
    left = this.SpawnPost(leftPos, facing, r"base\\gameplay\\devices\\street_signs\\race_checkpoint\\race_checkpoint_left.ent");
    right = this.SpawnPost(rightPos, facing, r"base\\gameplay\\devices\\street_signs\\race_checkpoint\\race_checkpoint_right.ent");
  }

  private func SpawnPost(position: Vector4, facing: Quaternion, template: ResRef) -> EntityID {
    let spec: ref<DynamicEntitySpec> = new DynamicEntitySpec();
    spec.templatePath = template;
    spec.position = position;
    spec.orientation = facing;
    spec.persistState = false;
    spec.persistSpawn = false;
    spec.alwaysSpawned = true;
    spec.active = true;
    ArrayPush(spec.tags, n"DriveRaceGate");
    return GameInstance.GetDynamicEntitySystem().CreateEntity(spec);
  }

  // Ground height under a point. A road route point already has the road's
  // height, so only a few metres around it are searched (this keeps posts off
  // bridges above and out of underpasses below). Map waypoints can carry an
  // unreliable height, so the car's height is used when they disagree.
  private func GroundZ(point: Vector4, hintZ: Float, onRoad: Bool) -> Float {
    let queries: ref<SpatialQueriesSystem> = GameInstance.GetSpatialQueriesSystem(this.GetGameInstance());
    let base: Float = point.Z;
    let top: Vector4;
    let bottom: Vector4;
    let hit: TraceResult;
    let best: Float = -100000.0;
    if onRoad {
      top = new Vector4(point.X, point.Y, base + 2.5, 1.0);
      bottom = new Vector4(point.X, point.Y, base - 6.0, 1.0);
    } else {
      if AbsF(base - hintZ) > 25.0 {
        base = hintZ;
      };
      top = new Vector4(point.X, point.Y, base + 6.0, 1.0);
      bottom = new Vector4(point.X, point.Y, base - 40.0, 1.0);
    };
    if queries.SyncRaycastByCollisionGroup(top, bottom, n"Static", hit, true, false) {
      best = hit.position.Z;
    };
    if queries.SyncRaycastByCollisionGroup(top, bottom, n"Terrain", hit, true, false) && hit.position.Z > best {
      best = hit.position.Z;
    };
    if best > -100000.0 {
      return best;
    };
    return base;
  }

  // ---- GPS guidance --------------------------------------------------------

  // Makes a marker the player's tracked marker, which is what the GPS route
  // follows. Called through reflection so the script still loads with an older
  // plugin that does not provide the function.
  private func TrackMarker(id: NewMappinID) -> Bool {
    let fn: ref<ReflectionStaticFunc> = Reflection.GetGlobalFunction(n"DriveNoCollision_TrackMappin");
    let args: array<Variant>;
    if !IsDefined(fn) || id.value == 0ul {
      return false;
    };
    ArrayPush(args, ToVariant(id.value));
    return FromVariant<Bool>(fn.Call(args));
  }

  // Tracks the first race marker: the START while waiting, otherwise the next checkpoint.
  private func TrackNextCheckpoint() -> Void {
    if !this.hasGps || ArraySize(this.racePins) == 0 {
      return;
    };
    if !this.TrackMarker(this.racePins[0]) {
      this.hasGps = false;
      if !this.gpsNoticeShown {
        this.gpsNoticeShown = true;
        this.Notify("GPS directions are unavailable: the plugin refused to track the checkpoint marker.");
      };
    };
  }

  private func RestoreTracking() -> Void {
    let mappins: ref<MappinSystem> = GameInstance.GetMappinSystem(this.GetGameInstance());
    if this.trackedBeforeRace.value != 0ul && IsDefined(mappins.GetMappin(this.trackedBeforeRace)) && this.TrackMarker(this.trackedBeforeRace) {
      return;
    };
    mappins.UntrackMappin();
  }

  // ---- markers and messages ----------------------------------------------

  // Mappins.DefaultStaticMappin has showOnMap, showOnMinimap and showInWorld set,
  // and DefaultVariant is the only variant its possibleVariants list accepts.
  private func AddPin(out pins: array<NewMappinID>, position: Vector4) -> Void {
    let data: MappinData;
    data.mappinType = t"Mappins.DefaultStaticMappin";
    data.variant = gamedataMappinVariant.DefaultVariant;
    data.active = true;
    data.visibleThroughWalls = true;
    ArrayPush(pins, GameInstance.GetMappinSystem(this.GetGameInstance()).RegisterMappin(data, position));
  }

  private func ClearPins(out pins: array<NewMappinID>) -> Void {
    let mappins: ref<MappinSystem> = GameInstance.GetMappinSystem(this.GetGameInstance());
    let i: Int32 = 0;
    while i < ArraySize(pins) {
      mappins.UnregisterMappin(pins[i]);
      i += 1;
    };
    ArrayClear(pins);
  }

  private func RefreshEditPins() -> Void {
    let i: Int32 = 0;
    this.ClearPins(this.editPins);
    if !Equals(this.phase, RacePhase.Editing) {
      return;
    };
    if this.hasStart {
      this.AddPin(this.editPins, this.startPoint);
    };
    while i < ArraySize(this.extraPoints) {
      this.AddPin(this.editPins, this.extraPoints[i]);
      i += 1;
    };
    if this.hasStart && this.hasEnd && !this.EndIsStart() {
      this.AddPin(this.editPins, this.endPoint);
    };
  }

  // Shows the START while waiting, otherwise the next three checkpoints.
  private func RefreshRacePins() -> Void {
    let i: Int32;
    let shown: Int32 = 0;
    this.ClearPins(this.racePins);
    if Equals(this.phase, RacePhase.WaitingAtStart) {
      this.AddPin(this.racePins, this.raceStart);
      this.TrackNextCheckpoint();
      return;
    };
    i = this.nextCheckpoint;
    while i < ArraySize(this.checkpoints) && shown < 2 {
      this.AddPin(this.racePins, this.checkpoints[i]);
      shown += 1;
      i += 1;
    };
    this.TrackNextCheckpoint();
  }

  private func FormatTime(seconds: Float) -> String {
    let minutes: Int32 = FloorF(seconds / 60.0);
    let rest: Float = seconds - Cast<Float>(minutes) * 60.0;
    let whole: Int32 = FloorF(rest);
    let hundredths: Int32 = FloorF((rest - Cast<Float>(whole)) * 100.0);
    let text: String = IntToString(minutes) + ":";
    if whole < 10 {
      text += "0";
    };
    text += IntToString(whole) + ".";
    if hundredths < 10 {
      text += "0";
    };
    return text + IntToString(hundredths);
  }

  private func Notify(text: String) -> Void {
    let message: SimpleScreenMessage;
    this.Log("message: " + text);
    let board: ref<IBlackboard> = GameInstance.GetBlackboardSystem(this.GetGameInstance()).Get(GetAllBlackboardDefs().UI_Notifications);
    message.isShown = true;
    message.duration = 4.0;
    message.message = text;
    message.isInstant = true;
    // WarningMessage is the channel the game's own scripts use for HUD text.
    board.SetVariant(GetAllBlackboardDefs().UI_Notifications.WarningMessage, ToVariant(message), true);
  }
}
