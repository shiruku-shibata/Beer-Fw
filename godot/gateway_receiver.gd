## gateway の JSON を UDP で受け取り、node ごとのシグナルにする。
##
## tools/serial_udp_bridge.py が gateway のシリアル出力（1 行 1 JSON）を UDP に中継する。
## ゲームで使うときはこのノードを追加し、packet_received を接続する。
class_name GatewayReceiver
extends Node

## node の 1 パケット受信ごと（node 1〜255）。data は gateway の JSON そのまま（"accel"・"battery" など）
signal packet_received(node_id: int, data: Dictionary)
## その node から初めて受信したとき（node 1〜255。gateway 自身では出ない）
signal node_connected(node_id: int)
## gateway 自身の加速度の行（"node":0）を受信するごと。data には "accel" などが入る（"rx"/"lost" は無い）
signal gateway_received(data: Dictionary)

## gateway 自身の行の "node"（FW の GATEWAY_SELF_NODE_ID）
const GATEWAY_NODE_ID := 0

@export var port: int = 9000
@export var bind_address: String = "127.0.0.1"

var packet_count := 0   ## 受信した JSON の数
var error_count := 0    ## 読めなかったパケットの数
var last_text := ""     ## 最後に受信した JSON（受信した文字列のまま）

var _udp := PacketPeerUDP.new()
var _known := {}


## UDP ポートを開いて受信を始める。開けなければエラーを出す。
func _ready() -> void:
	var err := _udp.bind(port, bind_address)
	if err != OK:
		push_error("GatewayReceiver: UDP %s:%d を開けません (error %d)" % [bind_address, port, err])
		return
	print("GatewayReceiver: listening on %s:%d" % [bind_address, port])


## UDP ポートを開けて受信待ちになっているか。
func is_listening() -> bool:
	return _udp.is_bound()


## 届いた UDP パケットをすべて読み、JSON として解釈できたものを通知する。
## gateway 自身の行は gateway_received、node の行は packet_received（初めての node なら先に node_connected）。
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
		if node_id == GATEWAY_NODE_ID:
			gateway_received.emit(data)
			continue
		if not _known.has(node_id):
			_known[node_id] = true
			print("GatewayReceiver: node %d connected" % node_id)
			node_connected.emit(node_id)
		packet_received.emit(node_id, data)


## ノード削除時に UDP ポートを閉じる。
func _exit_tree() -> void:
	_udp.close()
