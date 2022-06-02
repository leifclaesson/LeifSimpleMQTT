#pragma once

#include "Config.h"

#if defined(USE_PANGOLIN)
#include "PangolinMQTT.h"
#elif defined(USE_ASYNCMQTTCLIENT)
#include "AsyncMqttClient.h"
#elif defined(USE_ARDUINOMQTT)
#include "MQTT.h"
#elif defined(USE_PUBSUBCLIENT)
#include "PubSubClient.h"
#endif

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include "WiFi.h"
#endif

#include <map>


class LeifSimpleMQTT;
class MqttSubscription;

typedef std::function<void(const char * szText)> MqttDebugPrintCallback;

void MqttLibRegisterDebugPrintCallback(MqttDebugPrintCallback cb);

String MqttDeviceName(const char * in);

typedef std::map<String, MqttSubscription *> _map_incoming;
typedef std::function<void(MqttSubscription * pSource)> MqttSubscriptionCallback;



class MqttSubscription
{
public:
	String strTopic;

	void AddCallback(MqttSubscriptionCallback cb)
	{
		vecCallback.push_back(cb);
	};
		const String & GetValue()
	{
		return strValue;
	}

	const String & GetTopic()
	{
		return strTopic;
	}

	void ClearValue()
	{
		strValue.clear();
	}


private:
	std::vector<MqttSubscriptionCallback> vecCallback;

	void DoCallback()
	{
		for(size_t i=0;i<vecCallback.size();i++)
		{
			vecCallback[i](this);
		}
	};

	String strValue;

	friend class LeifSimpleMQTT;

	bool bSubscriptionFlag=false;

#if defined(USE_PANGOLIN)
	void onMqttMessage(const char* topic, uint8_t * payload, PANGO_PROPS properties, size_t len, size_t index, size_t total);
#elif defined(USE_ASYNCMQTTCLIENT)
	void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
#elif defined(USE_ARDUINOMQTT)
	void onMqttMessage(char* topic, char* payload, void * properties, size_t len, size_t index, size_t total);
#elif defined(USE_PUBSUBCLIENT)
	void onMqttMessage(char* topic, byte* payload, void * properties, unsigned int len, int index, int total);
#endif

};



class LeifSimpleMQTT
{
public:

	LeifSimpleMQTT();
	virtual ~LeifSimpleMQTT();
	
	String strMqttServerIP;
	String strMqttUserName;
	String strMqttPassword;

	String strFriendlyName;
	String strID;

	String strJsonInfoExtra; //Extra JSON to pad into the info message, must include initial comma. Example: ,"Model": "73"


	void Init();
	void Quit();

	void Loop();

	bool bTelemetrySent=false;	//set every time telemetry was sent, so we can send something too


	bool IsConnected();
	bool IsConnecting() { return bConnecting; }

	uint16_t PublishDirect(const String & topic, uint8_t qos, bool retain, const String & payload);

	void Subscribe(MqttSubscription & sub);

	MqttSubscription * NewSubscription(const String & topic);

#if defined(USE_PANGOLIN)
	PangolinMQTT mqtt;
#elif defined(USE_ASYNCMQTTCLIENT)
	AsyncMqttClient mqtt;
#elif defined(USE_ARDUINOMQTT)
	MQTTClient * pMQTT=NULL;
	WiFiClient net;
#elif defined(USE_PUBSUBCLIENT)
	PubSubClient * pMQTT=NULL;
	WiFiClient net;
#endif

	uint32_t GetUptimeSeconds_WiFi();
	uint32_t GetUptimeSeconds_MQTT();

	const uint32_t * GetUptimeSecondsPtr_WiFi() { return &ulSecondCounter_WiFi; }
	const uint32_t * GetUptimeSecondsPtr_MQTT() { return &ulSecondCounter_MQTT; }

	void SetEnableMQTT(bool bEnable) { this->bEnableMQTT=bEnable; }

#if defined(USE_PANGOLIN)
	const char * GetMqttLibraryID() { return "LeifSimpleMQTT/PangolinMQTT"; }
#elif defined(USE_ASYNCMQTTCLIENT)
	const char * GetMqttLibraryID() { return "LeifSimpleMQTT/AsyncMqttClient"; }
#elif defined(USE_ARDUINOMQTT)
	const char * GetMqttLibraryID() { return "LeifSimpleMQTT/ArduinoMQTT"; }
#elif defined(USE_PUBSUBCLIENT)
	const char * GetMqttLibraryID() { return "LeifSimpleMQTT/PubSubClient"; }
#endif



private:

	bool bEnableMQTT=true;
	bool bWasConnected=false;

	std::vector<MqttSubscription *> vecSub;


	uint16_t Publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0, bool reserved = false, uint16_t message_id = 0);

	void DoInitialPublishing();
	void DoStatusPublishing();

	unsigned long ulMqttReconnectCount=0;
	unsigned long ulLastReconnect=0;

#if defined(USE_PANGOLIN)
	void onConnect(bool sessionPresent);
	void onDisconnect(int8_t reason);
	void onMqttMessage(const char* topic,uint8_t* payload, PANGO_PROPS properties,size_t len,size_t index,size_t total);
#elif defined(USE_ASYNCMQTTCLIENT)
	void onConnect(bool sessionPresent);
	void onDisconnect(AsyncMqttClientDisconnectReason reason);
	void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
#elif defined(USE_ARDUINOMQTT)
	void onConnect(bool sessionPresent);
	void onDisconnect(int8_t reason);
	void onClientCallbackAdvanced(MQTTClient *client, char topic[], char payload[], int len);
#elif defined(USE_PUBSUBCLIENT)
	void onConnect(bool sessionPresent);
	void onDisconnect(int8_t reason);
	void onMqttMessage(char* topic, byte* payload, unsigned int len);
#endif

	bool bConnecting=false;

	bool bDoInitialPublishing=false;

	bool bInitialPublishingDone=false;

	bool bDoInitialStatusPublishing=false;

	int iInitialPublishing=0;
	int iInitialPublishing_Node=0;
	int iInitialPublishing_Prop=0;

	int iPubCount_Props=0;

	unsigned long ulInitialPublishing=0;

	unsigned long ulConnectTimestamp=0;

	bool bDebug=false;

	bool bInitialized=false;

	_map_incoming mapIncoming;

	char szWillTopic[128];

	uint32_t ulSecondCounter_Uptime=0;
	uint32_t ulSecondCounter_WiFi=0;
	uint32_t ulSecondCounter_MQTT=0;

	unsigned long ulLastLoopSecondCounterTimestamp=0;
	unsigned long ulLastLoopDeciSecondCounterTimestamp=0;

	bool bDoPublishDefaults=false;	//publish default retained values that did not yet exist in the controller
	unsigned long ulPublishDefaultsTimestamp=0;

	void HandleInitialPublishingError();

	bool bSendError=false;
	unsigned long ulSendErrorTimestamp;

	int GetErrorRetryFrequency();

	unsigned long GetReconnectInterval();

};
