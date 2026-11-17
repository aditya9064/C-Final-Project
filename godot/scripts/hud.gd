extends CanvasLayer
# HUD #7 (Koushik). Reads the C++ snapshot ONLY (PlayerBody getters +
# GameWorldNode phase/timer/score) and formats it -- no game logic lives here.
# Values with no live source yet (kill feed, hit markers) are driven by explicit
# SEAM methods combat (#8/#9) / networking (#10/#11) will call, and are labelled
# as pending rather than faked. Theme follows the local player's team colour.
#
# bind(player, world) wires it; call once after both exist.

const TEAL := Color("0faaa6")    # matches build_arena.gd C_TEAL
const ORANGE := Color("ff8b29")  # matches build_arena.gd C_ORANGE
const PANEL_BG := Color(0.07, 0.08, 0.09, 0.72)
const PHASE_NAMES := {0: "LOBBY", 1: "IN GAME", 2: "MATCH OVER"}

var _player: Node          # PlayerBody
var _world: Node           # GameWorldNode
var _accent := TEAL

var _hp_bar: ProgressBar
var _hp_label: Label
var _team_chip: Label
var _ammo_label: Label
var _reload_bar: ProgressBar
var _phase_label: Label
var _timer_label: Label
var _score_teal: Label
var _score_orange: Label
var _crosshair: Control
var _respawn_box: VBoxContainer
var _respawn_title: Label
var _respawn_sub: Label
var _hitmarker: Control
var _hit_t := 0.0
var _feed: VBoxContainer

func bind(player: Node, world: Node) -> void:
	_player = player
	_world = world
	if _player and _player.has_method("get_team"):
		_accent = ORANGE if _player.get_team() == 1 else TEAL
	_apply_accent()

# ---------------------------------------------------------------- seam (combat)
func flash_hitmarker() -> void:    # combat calls this when the local shot lands
	_hit_t = 0.18
func push_feed(text: String, team: int) -> void:  # kill/elim feed entry
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", 15)
	l.add_theme_color_override("font_color", ORANGE if team == 1 else TEAL)
	_feed.add_child(l)
	_feed.move_child(l, 0)
	while _feed.get_child_count() > 5:
		_feed.get_child(_feed.get_child_count() - 1).queue_free()

# ----------------------------------------------------------------------- build
func _ready() -> void:
	var root := Control.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)

	_build_crosshair(root)
	_build_matchbar(root)
	_build_health(root)
	_build_ammo(root)
	_build_respawn(root)
	_build_hitmarker(root)
	_build_feed(root)
	_apply_accent()

func _panel(bg: Color) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = bg
	sb.set_corner_radius_all(6)
	sb.set_content_margin_all(10)
	return sb

func _mklabel(text: String, size: int, col: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", col)
	return l

func _build_crosshair(root: Control) -> void:
	_crosshair = Control.new()
	_crosshair.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_crosshair.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_crosshair.grow_vertical = Control.GROW_DIRECTION_BOTH
	_crosshair.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(_crosshair)
	var gap := 5.0
	var arm := 9.0
	var th := 2.0
	for d in [Vector2(0, -1), Vector2(0, 1), Vector2(-1, 0), Vector2(1, 0)]:
		var r := ColorRect.new()
		r.color = Color(1, 1, 1, 0.9)
		if d.x != 0:
			r.size = Vector2(arm, th)
			r.position = Vector2(gap if d.x > 0 else -gap - arm, -th * 0.5)
		else:
			r.size = Vector2(th, arm)
			r.position = Vector2(-th * 0.5, gap if d.y > 0 else -gap - arm)
		_crosshair.add_child(r)

func _build_matchbar(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel(PANEL_BG))
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	panel.grow_horizontal = Control.GROW_DIRECTION_BOTH  # centre on the anchor, not grow-right
	panel.position.y = 14
	root.add_child(panel)
	var v := VBoxContainer.new()
	v.alignment = BoxContainer.ALIGNMENT_CENTER
	panel.add_child(v)
	var top := HBoxContainer.new()
	top.alignment = BoxContainer.ALIGNMENT_CENTER
	top.add_theme_constant_override("separation", 14)
	v.add_child(top)
	_phase_label = _mklabel("IN GAME", 15, Color(0.75, 0.78, 0.8))
	_timer_label = _mklabel("0:00", 26, Color.WHITE)
	top.add_child(_phase_label)
	top.add_child(_timer_label)
	var score := HBoxContainer.new()
	score.alignment = BoxContainer.ALIGNMENT_CENTER
	score.add_theme_constant_override("separation", 8)
	v.add_child(score)
	_score_teal = _mklabel("0", 20, TEAL)
	_score_orange = _mklabel("0", 20, ORANGE)
	score.add_child(_mklabel("TEAL", 13, TEAL))
	score.add_child(_score_teal)
	score.add_child(_mklabel(":", 18, Color(0.6, 0.6, 0.6)))
	score.add_child(_score_orange)
	score.add_child(_mklabel("ORANGE", 13, ORANGE))

func _build_health(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel(PANEL_BG))
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT)
	panel.position = Vector2(22, -22)
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	root.add_child(panel)
	var v := VBoxContainer.new()
	panel.add_child(v)
	_team_chip = _mklabel("TEAL", 14, TEAL)
	v.add_child(_team_chip)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	v.add_child(row)
	_hp_bar = ProgressBar.new()
	_hp_bar.min_value = 0
	_hp_bar.max_value = 100
	_hp_bar.value = 100
	_hp_bar.show_percentage = false
	_hp_bar.custom_minimum_size = Vector2(190, 18)
	row.add_child(_hp_bar)
	_hp_label = _mklabel("100", 20, Color.WHITE)
	row.add_child(_hp_label)

func _build_ammo(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel(PANEL_BG))
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
	panel.position = Vector2(-22, -22)
	panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	root.add_child(panel)
	var v := VBoxContainer.new()
	v.alignment = BoxContainer.ALIGNMENT_CENTER
	panel.add_child(v)
	_ammo_label = _mklabel("30 / 30", 30, Color.WHITE)
	_ammo_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	v.add_child(_ammo_label)
	_reload_bar = ProgressBar.new()
	_reload_bar.min_value = 0
	_reload_bar.max_value = 1
	_reload_bar.show_percentage = false
	_reload_bar.custom_minimum_size = Vector2(150, 8)
	_reload_bar.visible = false
	v.add_child(_reload_bar)

func _build_respawn(root: Control) -> void:
	_respawn_box = VBoxContainer.new()
	_respawn_box.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_respawn_box.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_respawn_box.grow_vertical = Control.GROW_DIRECTION_BOTH
	_respawn_box.alignment = BoxContainer.ALIGNMENT_CENTER
	_respawn_box.visible = false
	root.add_child(_respawn_box)
	_respawn_title = _mklabel("ELIMINATED", 40, Color(0.95, 0.25, 0.25))
	_respawn_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_respawn_sub = _mklabel("", 20, Color.WHITE)
	_respawn_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_respawn_box.add_child(_respawn_title)
	_respawn_box.add_child(_respawn_sub)

func _build_hitmarker(root: Control) -> void:
	_hitmarker = Control.new()
	_hitmarker.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_hitmarker.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_hitmarker.grow_vertical = Control.GROW_DIRECTION_BOTH
	_hitmarker.modulate = Color(1, 1, 1, 0)
	root.add_child(_hitmarker)
	for d in [Vector2(-1, -1), Vector2(1, -1), Vector2(-1, 1), Vector2(1, 1)]:
		var r := ColorRect.new()
		r.color = Color(1, 0.95, 0.9)
		r.size = Vector2(8, 2)
		r.rotation = deg_to_rad(45 if d.x * d.y > 0 else -45)
		r.position = d * 11
		_hitmarker.add_child(r)

func _build_feed(root: Control) -> void:
	var box := VBoxContainer.new()
	box.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	box.position = Vector2(-16, 16)
	box.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	box.alignment = BoxContainer.ALIGNMENT_BEGIN
	root.add_child(box)
	_feed = VBoxContainer.new()
	box.add_child(_feed)
	# Honest placeholder: the feed is wired via push_feed(); combat fills it.
	var note := _mklabel("kill feed — combat #8/#9", 12, Color(0.55, 0.57, 0.6))
	note.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	box.add_child(note)

func _apply_accent() -> void:
	if _hp_bar:
		var fill := StyleBoxFlat.new()
		fill.bg_color = _accent
		fill.set_corner_radius_all(3)
		_hp_bar.add_theme_stylebox_override("fill", fill)
		var bg := StyleBoxFlat.new()
		bg.bg_color = Color(0.2, 0.2, 0.22, 0.9)
		bg.set_corner_radius_all(3)
		_hp_bar.add_theme_stylebox_override("background", bg)
	if _reload_bar:
		var rf := StyleBoxFlat.new()
		rf.bg_color = _accent
		_reload_bar.add_theme_stylebox_override("fill", rf)
	if _team_chip:
		_team_chip.text = "ORANGE" if _accent == ORANGE else "TEAL"
		_team_chip.add_theme_color_override("font_color", _accent)

# ---------------------------------------------------------------------- update
func _format_time(sec: float) -> String:
	var s := int(max(0.0, sec))
	return "%d:%02d" % [s / 60, s % 60]

func _process(delta: float) -> void:
	# hit marker fade runs regardless of binding
	if _hit_t > 0.0:
		_hit_t -= delta
		_hitmarker.modulate.a = clamp(_hit_t / 0.18, 0.0, 1.0)

	if _player != null and is_instance_valid(_player):
		var hp: int = _player.get_health()
		var maxhp: int = _player.get_max_health()
		_hp_bar.max_value = maxhp
		_hp_bar.value = hp
		_hp_label.text = str(hp)
		var frac := float(hp) / float(max(1, maxhp))
		_hp_label.add_theme_color_override("font_color",
			Color(0.95, 0.3, 0.3) if frac <= 0.25 else Color.WHITE)

		var ammo: int = _player.get_ammo()
		var mag: int = _player.get_magazine_size()
		_ammo_label.text = "%d / %d" % [ammo, mag]
		_ammo_label.add_theme_color_override("font_color",
			Color(0.95, 0.3, 0.3) if ammo <= 5 else Color.WHITE)
		var reloading: bool = _player.is_reloading()
		_reload_bar.visible = reloading
		if reloading:
			_reload_bar.value = _player.get_reload_progress()

		var awaiting: bool = _player.is_awaiting_respawn()
		var dead_stuck: bool = _player.is_ready_to_respawn()
		_respawn_box.visible = awaiting or dead_stuck
		_crosshair.visible = not (awaiting or dead_stuck)
		if awaiting:
			_respawn_title.text = "ELIMINATED"
			_respawn_sub.text = "Respawning in %d" % int(ceil(_player.get_respawn_remaining()))
		elif dead_stuck:
			_respawn_title.text = "ELIMINATED"
			_respawn_sub.text = "waiting for respawn (not wired yet)"

	if _world != null and is_instance_valid(_world):
		_timer_label.text = _format_time(_world.get_match_time())
		var phase: int = _world.get_phase()
		_phase_label.text = PHASE_NAMES.get(phase, "—")
		_score_teal.text = str(_world.get_score(0))
		_score_orange.text = str(_world.get_score(1))
