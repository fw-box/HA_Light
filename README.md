# Lamp

Home Assistant MQTT discovery support.

Prepare a NodeMCU and a WS2812 light board.

Connect the input of WS2812 light board to NodeMCU's D3

## 準備硬體
- NodeMCU(ESP8266)
- WS2812 8X8
- 足夠電流的 5V 電源供應器

## 接線
- WS2812 IN 接至 NodeMCU 的D3(GPIO0)

## 實作三個 topic
- homeassistant/light/my_light/config
- homeassistant/light/my_light/state
- homeassistant/light/my_light/set

config 與 state 是 publish topic

set 是 subscribe topic
