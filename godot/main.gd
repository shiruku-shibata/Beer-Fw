## 接続確認用の画面。node ごとに最新の受信値を表示する。
extends Control

const STALE_SEC := 1.0   # この時間受信がなければ「途絶」と表示する [s]

var _receiver: GatewayReceiver
var _status: Label
var _cards: HFlowContainer
var _raw: Label
var _nodes := {}          # node_id -> NodeCard

var _rate := 0.0          # 受信レート [packet/s]
var _rate_count := 0
var _rate_since := 0
var _last_rx_msec := -1


func _ready() -> void:
	_build_ui()
	_receiver = GatewayReceiver.new()
	_receiver.packet_received.connect(_on_packet)
	add_child(_receiver)


func _process(_delta: float) -> void:
	var now := Time.get_ticks_msec()
	if now - _rate_since >= 1000:
		_rate = _rate_count * 1000.0 / (now - _rate_since)
		_rate_count = 0
		_rate_since = now

	if not _receiver.is_listening():
		_status.text = "UDP %d could not be opened (another app may be using it)" % _receiver.port
		_status.modulate = Color(1, 0.4, 0.4)
	elif _last_rx_msec < 0:
		_status.text = "Listening on UDP %d ... start tools/serial_udp_bridge.py" % _receiver.port
		_status.modulate = Color(1, 0.85, 0.3)
	else:
		_status.text = "UDP %d  |  packets %d  |  %.1f pkt/s  |  last %.1f s ago  |  errors %d" % [
			_receiver.port, _receiver.packet_count, _rate, (now - _last_rx_msec) / 1000.0, _receiver.error_count]
		_status.modulate = Color(0.5, 1, 0.5) if now - _last_rx_msec < STALE_SEC * 1000 else Color(1, 0.4, 0.4)

	for card in _nodes.values():
		card.refresh_stale(now, STALE_SEC)


func _on_packet(node_id: int, data: Dictionary) -> void:
	_rate_count += 1
	_last_rx_msec = Time.get_ticks_msec()

	if not _nodes.has(node_id):
		var card := NodeCard.new(node_id)
		_nodes[node_id] = card
		_cards.add_child(card)
		# node 番号順に並べる
		var ids := _nodes.keys()
		ids.sort()
		_cards.move_child(card, ids.find(node_id))

	_nodes[node_id].apply(data)
	_raw.text = _receiver.last_text


func _build_ui() -> void:
	var bg := ColorRect.new()
	bg.color = Color(0.11, 0.12, 0.14)
	bg.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(bg)

	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for side in ["left", "right", "top", "bottom"]:
		margin.add_theme_constant_override("margin_" + side, 16)
	add_child(margin)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 12)
	margin.add_child(vbox)

	var title := Label.new()
	title.text = "Gateway connection test"
	title.add_theme_font_size_override("font_size", 24)
	vbox.add_child(title)

	_status = Label.new()
	vbox.add_child(_status)

	_cards = HFlowContainer.new()
	_cards.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_cards.add_theme_constant_override("h_separation", 12)
	_cards.add_theme_constant_override("v_separation", 12)
	vbox.add_child(_cards)

	var raw_title := Label.new()
	raw_title.text = "Last JSON"
	raw_title.modulate = Color(0.7, 0.7, 0.7)
	vbox.add_child(raw_title)

	_raw = Label.new()
	_raw.autowrap_mode = TextServer.AUTOWRAP_ARBITRARY
	_raw.add_theme_font_size_override("font_size", 12)
	_raw.text = "-"
	vbox.add_child(_raw)


# =============================================
# node 1 台分の表示
# =============================================
class NodeCard extends PanelContainer:
	const ACCEL_RANGE := 2.0   # 加速度バーの表示範囲 [G]

	var _id: int
	var _last_msec := 0
	var _title: Label
	var _info: Label
	var _bars := {}        # "x" -> ProgressBar
	var _values := {}      # "x" -> Label
	var _tilt: TiltView
	var _extra: Label

	func _init(node_id: int) -> void:
		_id = node_id
		_last_msec = Time.get_ticks_msec()
		custom_minimum_size = Vector2(280, 0)

		var margin := MarginContainer.new()
		for side in ["left", "right", "top", "bottom"]:
			margin.add_theme_constant_override("margin_" + side, 10)
		add_child(margin)

		var v := VBoxContainer.new()
		margin.add_child(v)

		_title = Label.new()
		_title.add_theme_font_size_override("font_size", 20)
		v.add_child(_title)

		_info = Label.new()
		v.add_child(_info)

		var row := HBoxContainer.new()
		v.add_child(row)

		var axes := VBoxContainer.new()
		axes.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(axes)
		for axis in ["x", "y", "z"]:
			var line := HBoxContainer.new()
			var axis_label := Label.new()
			axis_label.text = axis
			axis_label.custom_minimum_size.x = 14
			var bar := ProgressBar.new()
			bar.min_value = -ACCEL_RANGE
			bar.max_value = ACCEL_RANGE
			bar.step = 0.0
			bar.show_percentage = false
			bar.custom_minimum_size = Vector2(100, 14)
			bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
			bar.add_theme_stylebox_override("background", _flat(Color(0.2, 0.22, 0.26)))
			bar.add_theme_stylebox_override("fill", _flat(Color(0.35, 0.7, 1.0)))
			var value := Label.new()
			value.custom_minimum_size.x = 56
			value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
			line.add_child(axis_label)
			line.add_child(bar)
			line.add_child(value)
			axes.add_child(line)
			_bars[axis] = bar
			_values[axis] = value

		_tilt = TiltView.new()
		row.add_child(_tilt)

		_extra = Label.new()
		v.add_child(_extra)

	static func _flat(color: Color) -> StyleBoxFlat:
		var sb := StyleBoxFlat.new()
		sb.bg_color = color
		sb.set_corner_radius_all(3)
		return sb

	func apply(data: Dictionary) -> void:
		_last_msec = Time.get_ticks_msec()
		_info.text = "seq %d   rx %d   lost %d" % [int(data.get("seq", 0)), int(data.get("rx", 0)), int(data.get("lost", 0))]

		var a: Dictionary = data.get("accel", {})
		for axis in _bars:
			var g := float(a.get(axis, 0.0))
			_bars[axis].value = g
			_values[axis].text = "%+.2f" % g
		_tilt.set_accel(float(a.get("x", 0.0)), float(a.get("y", 0.0)))

		var lines := PackedStringArray()
		if data.has("battery"):
			var b: Dictionary = data["battery"]
			var flag := ""
			if int(b.get("alarm", 0)):
				flag = "  EMPTY"
			elif int(b.get("low", 0)):
				flag = "  LOW"
			lines.append("battery %d%%%s" % [int(b.get("level", -1)), flag])
		if data.has("vibration"):
			lines.append("vibration %d%%" % int(data["vibration"]))
		if data.has("drink"):
			var d: Dictionary = data["drink"]
			var state := "stopped"
			if int(d.get("active", 0)):
				state = "DRINKING"
			elif int(d.get("armed", 0)):
				state = "armed"
			lines.append("drink %s  tilt %d deg  gulps %d" % [state, int(d.get("tilt", 0)), int(d.get("gulps", 0))])
		_extra.text = "\n".join(lines)

	func refresh_stale(now_msec: int, stale_sec: float) -> void:
		var stale := now_msec - _last_msec > stale_sec * 1000
		_title.text = "Node %d%s" % [_id, "   (no data)" if stale else ""]
		_title.modulate = Color(1, 0.4, 0.4) if stale else Color(0.5, 1, 0.5)


# =============================================
# 傾きの表示（重力の x / y 成分を円の中の点で示す）
# =============================================
class TiltView extends Control:
	const RADIUS := 34.0

	var _pos := Vector2.ZERO

	func _init() -> void:
		custom_minimum_size = Vector2(RADIUS * 2 + 8, RADIUS * 2 + 8)

	func set_accel(x: float, y: float) -> void:
		_pos = Vector2(x, y).limit_length(1.0)
		queue_redraw()

	func _draw() -> void:
		var c := size / 2
		draw_circle(c, RADIUS, Color(0.2, 0.22, 0.26))
		draw_arc(c, RADIUS, 0, TAU, 48, Color(0.5, 0.5, 0.55), 1.0)
		draw_line(c - Vector2(RADIUS, 0), c + Vector2(RADIUS, 0), Color(0.35, 0.35, 0.4))
		draw_line(c - Vector2(0, RADIUS), c + Vector2(0, RADIUS), Color(0.35, 0.35, 0.4))
		draw_circle(c + _pos * RADIUS, 6, Color(1, 0.75, 0.2))
