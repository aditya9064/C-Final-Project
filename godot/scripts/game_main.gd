extends Node3D
# Playable game scene: arena + local player + match world + HUD. Combat (#8/#9)
# and networking (#10/#11) arrive in teammates' lanes; this drives Player #6 and
# feeds the HUD #7 live C++ state (health/ammo/team + phase/timer/score).

const TEAL_SPAWN := Vector3(-18, 1.2, 0)
const ORANGE_SPAWN := Vector3(18, 1.2, 0)

var _world: Node        # GameWorldNode (C++)
var _local: Node        # the local PlayerBody

func _ready() -> void:
	FireteamInput.ensure_actions()
	var arena: Node = load("res://scenes/arena.tscn").instantiate()
	arena.qa_mode = false  # playable: no debug camera / test humans / cover tests
	add_child(arena)

	# Match flow lives in C++ (GameWorldNode owns a fireteam::GameState).
	_world = GameWorldNode.new()
	add_child(_world)
	_world.start_match()

	# Local player: teal (male). A non-local orange (female) stands opposite as a
	# reference until networking spawns real remote players.
	_local = _spawn(0, TEAL_SPAWN, true)
	_spawn(1, ORANGE_SPAWN, false)

	var hud: Node = load("res://scenes/hud.tscn").instantiate()
	add_child(hud)
	hud.bind(_local, _world)

func _spawn(team: int, pos: Vector3, local: bool) -> Node:
	var p: Node = load("res://scenes/player.tscn").instantiate()
	p.team_id = team
	p.is_local = local
	p.position = pos
	add_child(p)
	return p
