# LeifSimpleMQTT
Very light weight MQTT helper Library for ESP8266 and ESP32

No documentation yet except for the example in [LeifESPBaseMQTT](https://github.com/leifclaesson/LeifESPBaseMQTT/).

This is designed for speed, for example inside a light bulb which is normally physically powered off.
The Homie convention, while wonderful, is too heavy for this purpose as the huge number of required topics adds noticeably to the handshaking time.
Imagine how poorly things would work if you had 50 lightbulbs (not unreasonable if they're downlights in a large living room) all trying to publish their Homie topics at the same time...

LeifSimpleMQTT does _not_ support discovery, that's not its purpose.
Instead, only the following topics are published (assuming bathroom-mcu is the topic name):

Immediately on connect:

Publish tele/hostname/LWT = Online
Subscribe cmnd/commandtopic
Subscribe cmnd/grouptopic

After five seconds:

Publish tele/hostname/info = {"IPAddress": "192.168.x.x","Built": "Apr 1 2020 15:30:26"}

After 30 seconds and then at progressively longer intervals, least often every 15 minutes:

Publish tele/hostname/status = { "Uptime": "00:02:30", "WiFi Uptime": "00:02:26", "MQTT Uptime": "00:02:26" }

That's all, the rest depends on handlers in lambda functions.


## dependencies

[AsyncMqttClient](https://github.com/marvinroger/async-mqtt-client)

[ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
