## gateway の JSON を UDP で受け取り、node ごとのシグナルにする。
##
## tools/serial_udp_bridge.py が gateway のシリアル出力（1 行 1 JSON）を UDP に中継する。
## ゲームで使うときはこのノードを追加し、packet_received を接続する。
class_name GatewayReceiver
extends Node

## 1 パケット受信ごと。data は gateway の JSON そのまま（"accel"・"battery" など）
signal packet_received(node_id: int, data: Dictionary)
## その node から初めて受信したとき
signal node_connected(node_id: int)

@export var port: int = 9000
@export var bind_address: String = "127.0.0.1"

var packet_count := 0   ## 受信した JSON の数
var error_count := 0    ## 読めなかったパケットの数
var last_text := ""     ## 最後に受信した JSON（受信した文字列のまま）

var _udp := PacketPeerUDP.new()
var _known := {}


func _ready() -> void:
	var err := _udp.bind(port, bind_address)
	if err != OK:
		push_error("GatewayReceiver: UDP %s:%d を開けません (error %d)" % [bind_address, port, err])
		return
	print("GatewayReceiver: listening on %s:%d" % [bind_address, port])


func is_listening() -> bool:
	return _udp.is_bound()


func _process(_delta: float) -> void:
	while _udp.get_available_packet_count() > 0:
		var text := _udp.get_packet().get_string_from_utf8()
		var data = JSON.parse_string(text)
		if typeof(data) != TYPE_DICTIONARY or not data.has("node"):
			error_count += 1
			continue

		packet_count += 1
		last_text = text
		var node_id := int(data["node"])
		if not _known.has(node_id):
			_known[node_id] = true
			print("GatewayReceiver: node %d connected" % node_id)
			node_connected.emit(node_id)
		packet_received.emit(node_id, data)


func _exit_tree() -> void:
	_udp.close()
