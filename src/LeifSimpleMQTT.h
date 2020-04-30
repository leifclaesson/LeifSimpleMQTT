#pragma once

#include "AsyncMqttClient.h"
#include <map>

#define MQTTLIB_VERBOSE

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

	void OnMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties & properties, size_t len, size_t index, size_t total);

};



class LeifSimpleMQTT
{
public:

	LeifSimpleMQTT();
	
	String strMqttServerIP;
	String strMqttUserName;
	String strMqttPassword;

	String strFriendlyName;
	String strID;


	void Init();
	void Quit();

	void Loop();


	bool IsConnected();

	uint16_t PublishDirect(const String & topic, uint8_t qos, bool retain, const String & payload);

	AsyncMqttClient mqtt;

	unsigned long GetUptimeSeconds_WiFi();
	unsigned long GetUptimeSeconds_MQTT();

	void Subscribe(MqttSubscription & sub);

private:

	std::vector<MqttSubscription *> vecSub;


	uint16_t Publish(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr, size_t length = 0, bool dup = false, uint16_t message_id = 0);

	void DoInitialPublishing();
	void DoStatusPublishing();

	unsigned long ulMqttReconnectCount=0;
	unsigned long ulLastReconnect=0;

	void onConnect(bool sessionPresent);
	void onDisconnect(AsyncMqttClientDisconnectReason reason);
	void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

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

	unsigned long ulSecondCounter_Uptime=0;
	unsigned long ulSecondCounter_WiFi=0;
	unsigned long ulSecondCounter_MQTT=0;

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
