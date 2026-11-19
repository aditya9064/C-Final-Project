extends Node

const EYE_HEIGHT := 1.63
const SHOT_MASK := 3

var session: FireteamSession


func setup(host_session: FireteamSession) -> void:
	session = host_session
	session.shot_fired.connect(_on_shot_fired)


func _on_shot_fired(player_id: int, yaw: float, pitch: float, damage: int, shot_range: float) -> void:
	var state := session.get_state()
	if not state.host or state.phase != 1:
		push_error("Shot resolution requires an active authoritative host")
		return
	var shooter: CharacterBody3D = session.get_node("HostPlayer" if player_id == 1 else "ClientPlayer")
	var target: CharacterBody3D = session.get_node("ClientPlayer" if player_id == 1 else "HostPlayer")
	var origin: Vector3 = state.players[player_id - 1].position + Vector3.UP * EYE_HEIGHT
	var direction := Vector3(sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch))
	var query := PhysicsRayQueryParameters3D.create(origin, origin + direction * shot_range, SHOT_MASK)
	query.exclude = [shooter.get_rid()]
	var hit := session.get_world_3d().direct_space_state.intersect_ray(query)
	if hit.get("collider") == target:
		if not session.damage_player(2 if player_id == 1 else 1, damage):
			push_error("Could not apply authoritative shot damage")
