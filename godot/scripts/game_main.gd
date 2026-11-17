extends Node3D
# Playable game scene (Increment 2): the arena + a local player. Combat (#8/#9),
# networking (#10/#11) and the full HUD (#7) arrive in later increments; for now
# this is the single-player drop-in to drive and look at Player #6.

const TEAL_SPAWN := Vector3(-18, 1.2, 0)
const ORANGE_SPAWN := Vector3(18, 1.2, 0)

func _ready() -> void:
	FireteamInput.ensure_actions()
	add_child(load("res://scenes/arena.tscn").instantiate())
	# Local player: teal (male). A non-local orange (female) stands opposite as a
	# reference until networking spawns real remote players.
	_spawn(0, TEAL_SPAWN, true)
	_spawn(1, ORANGE_SPAWN, false)

func _spawn(team: int, pos: Vector3, local: bool) -> void:
	var p: Node = load("res://scenes/player.tscn").instantiate()
	p.team_id = team
	p.is_local = local
	p.position = pos
	add_child(p)
