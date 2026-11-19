extends CanvasLayer

# Adapted from the HUD in Koushik's PR #20; only replicated state is displayed.
const TEAL := Color("0faaa6")
const ORANGE := Color("ff8b29")
const PANEL_BG := Color(0.07, 0.08, 0.09, 0.72)
const PHASE_NAMES := ["LOBBY / WAITING", "IN GAME", "MATCH OVER"]

var health_bar: ProgressBar
var health_label: Label
var team_label: Label
var phase_label: Label
var result_label: Label
var crosshair: Control
var ammo_label: Label
var _team_id := 0


func _ready() -> void:
	var root := Control.new()
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(root)
	_build_crosshair(root)
	_build_matchbar(root)
	_build_health(root)
	_build_ammo(root)
	hide()


func _panel() -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = PANEL_BG
	style.set_corner_radius_all(6)
	style.set_content_margin_all(10)
	return style


func _label(text: String, size: int, color: Color) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	return label


func _build_crosshair(root: Control) -> void:
	crosshair = Control.new()
	crosshair.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	crosshair.mouse_filter = Control.MOUSE_FILTER_IGNORE
	root.add_child(crosshair)
	for direction in [Vector2.UP, Vector2.DOWN, Vector2.LEFT, Vector2.RIGHT]:
		var arm := ColorRect.new()
		arm.color = Color(1, 1, 1, 0.9)
		arm.mouse_filter = Control.MOUSE_FILTER_IGNORE
		if direction.x != 0:
			arm.size = Vector2(9, 2)
			arm.position = Vector2(5 if direction.x > 0 else -14, -1)
		else:
			arm.size = Vector2(2, 9)
			arm.position = Vector2(-1, 5 if direction.y > 0 else -14)
		crosshair.add_child(arm)


func _build_matchbar(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel())
	panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	panel.grow_horizontal = Control.GROW_DIRECTION_BOTH
	panel.position.y = 14
	root.add_child(panel)
	var column := VBoxContainer.new()
	column.alignment = BoxContainer.ALIGNMENT_CENTER
	panel.add_child(column)
	phase_label = _label("LOBBY / WAITING", 16, Color.WHITE)
	phase_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	column.add_child(phase_label)
	result_label = _label("", 16, Color.WHITE)
	result_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	column.add_child(result_label)


func _build_health(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel())
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_LEFT)
	panel.position = Vector2(22, -22)
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	root.add_child(panel)
	var column := VBoxContainer.new()
	panel.add_child(column)
	team_label = _label("", 14, TEAL)
	column.add_child(team_label)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", 10)
	column.add_child(row)
	health_bar = ProgressBar.new()
	health_bar.show_percentage = false
	health_bar.custom_minimum_size = Vector2(190, 18)
	row.add_child(health_bar)
	health_label = _label("", 20, Color.WHITE)
	row.add_child(health_label)

func _build_ammo(root: Control) -> void:
	var panel := PanelContainer.new()
	panel.add_theme_stylebox_override("panel", _panel())
	panel.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
	panel.position = Vector2(-22, -22)
	panel.grow_horizontal = Control.GROW_DIRECTION_BEGIN
	panel.grow_vertical = Control.GROW_DIRECTION_BEGIN
	root.add_child(panel)
	ammo_label = _label("", 20, Color.WHITE)
	panel.add_child(ammo_label)


func update_state(state: Dictionary, controls_active: bool) -> void:
	var player_id: int = state.local_player_id
	visible = state.active and controls_active and player_id >= 1 and player_id <= state.players.size()
	if not visible:
		return
	var player: Dictionary = state.players[player_id - 1]
	if player.id != player_id:
		hide()
		return
	if _team_id != player_id:
		_team_id = player_id
		var accent := TEAL if player_id == 1 else ORANGE
		team_label.text = "TEAL" if player_id == 1 else "ORANGE"
		team_label.add_theme_color_override("font_color", accent)
		var fill := StyleBoxFlat.new()
		fill.bg_color = accent
		fill.set_corner_radius_all(3)
		health_bar.add_theme_stylebox_override("fill", fill)
	health_bar.max_value = player.max_health
	health_bar.value = player.health
	health_label.text = str(player.health)
	ammo_label.text = "RELOADING" if player.reloading else "%d / %d" % [player.ammo, player.max_ammo]
	phase_label.text = PHASE_NAMES[state.phase]
	result_label.text = ""
	result_label.visible = state.phase == 2 and state.winner_id in [1, 2]
	if result_label.visible:
		result_label.text = "TEAL WINS" if state.winner_id == 1 else "ORANGE WINS"
	crosshair.visible = state.phase == 1 and player.health > 0
