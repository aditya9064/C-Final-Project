extends Node
class_name RespawnAuthority
# LOCAL/host respawn authority for Player #6.
#
# It is the ONLY thing that calls PlayerBody.respawn(): once a body reports
# is_ready_to_respawn() (dead AND its countdown has elapsed -- both decided by
# the core Player), this node triggers the authoritative respawn. Player input
# never calls respawn(), so a client cannot revive itself (requirement 8).
#
# In multiplayer this node is simply NOT added on clients: Aditya's server owns
# respawn timing + authorization and calls PlayerBody.respawn() itself, then
# replicates. Death determination and damage stay in Khushal's combat.

var _bodies: Array = []

func watch(body: Node) -> void:
	if body and not _bodies.has(body):
		_bodies.append(body)

func _process(_delta: float) -> void:
	for b in _bodies:
		if is_instance_valid(b) and b.is_ready_to_respawn():
			b.respawn()
