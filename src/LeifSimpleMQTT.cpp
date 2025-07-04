#include "LeifSimpleMQTT.h"
#include "LeifEspBase.h"
#include <string>

void MqttLibDebugPrint(const char * szText);

//#define csprintf(...) { char szTemp[256]; sprintf(szTemp,__VA_ARGS__ ); MqttLibDebugPrint(szTemp); }


#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include "WiFi.h"
#endif

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
#include <ETH.h>
#endif

static std::vector<MqttDebugPrintCallback> vecDebugPrint;

int ipub_qos=1;
#if defined(USE_PUBSUBCLIENT)
int sub_qos=1;
#else
int sub_qos=2;
#endif

#if defined(USE_MPSCG)
static void DummyTopicCallback(const char*, const size_t)
{
}
#endif

void MqttLibRegisterDebugPrintCallback(MqttDebugPrintCallback cb)
{
	vecDebugPrint.push_back(cb);

}

void MqttLibDebugPrint(const char * szText)
{
	for(size_t i=0;i<vecDebugPrint.size();i++)
	{
		vecDebugPrint[i](szText);
	}

}


//#define MQTTLIB_VERBOSE

LeifSimpleMQTT::LeifSimpleMQTT()
{
#if defined(USE_ARDUINOMQTT)
	pMQTT=new MQTTClient(ARDUINOMQTT_BUFSIZE);
#elif defined(USE_PUBSUBCLIENT)
	pMQTT=new PubSubClient(net);
#endif
}

LeifSimpleMQTT::~LeifSimpleMQTT()
{
#if defined(USE_ARDUINOMQTT)
	delete pMQTT;
#elif defined(USE_PUBSUBCLIENT)
	delete pMQTT;
#endif
}

void LeifSimpleMQTT::Init()
{
	csprintf(PSTR("Initializing: %s\n"),GetMqttLibraryID());

	strcpy(szWillTopic,String("tele/"+strID+"/LWT").c_str());

#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)

	mqtt.setWill(szWillTopic,2,true,"Offline");

	mqtt.onConnect(std::bind(&LeifSimpleMQTT::onConnect, this, std::placeholders::_1));
	mqtt.onDisconnect(std::bind(&LeifSimpleMQTT::onDisconnect, this, std::placeholders::_1));
	mqtt.onMessage(std::bind(&LeifSimpleMQTT::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));

#elif defined(USE_ARDUINOMQTT)

	pMQTT->setWill(szWillTopic,"Offline",true,2);

	pMQTT->onMessageAdvanced([this](MQTTClient *client, char topic[], char bytes[], int len)
			{
				this->onClientCallbackAdvanced(client,topic,bytes,len);
			});

#elif defined(USE_PUBSUBCLIENT)
	pMQTT->setCallback([this](char* topic, byte* payload, unsigned int length)
			{
				onMqttMessage(topic,payload,length);
			});
#elif defined(USE_MPSCG)
	mqttClient.begin(client);
	mqttClient.setWill(szWillTopic,"Offline",true,2);
	mqttClient.subscribe([this](const char * topic, const char * payload, const size_t size)
	{
		//csprintf("mqtt message: %s=%s\n",topic,payload);
		onMqttMessage(topic, payload, size);
	});
#endif

	bSendError=false;

	bInitialized=true;
}

void LeifSimpleMQTT::Quit()
{
	PublishDirect(szWillTopic, 2, true, "Offline");
#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
	mqtt.disconnect(false);
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
	pMQTT->disconnect();
#elif defined(USE_MPSCG)
	mqttClient.disconnect();
#endif
	bInitialized=false;
}

bool LeifSimpleMQTT::IsConnected()
{
#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
	return mqtt.connected();
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
	return pMQTT->connected();
#elif defined(USE_MPSCG)
	return mqttClient.isConnected();
#endif
}


int iWiFiRSSI=0;

void LeifSimpleMQTT::Loop()
{

	bool bEvenSecond=false;

	if((int) (millis()-ulLastLoopSecondCounterTimestamp)>=1000)
	{
		ulLastLoopSecondCounterTimestamp+=1000;
		ulSecondCounter_Uptime++;
		if(WiFi.status() == WL_CONNECTED) ulSecondCounter_WiFi++;
		if(IsConnected()) ulSecondCounter_MQTT++;
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
		uint32_t _eth_ip=ETH.localIP();
		if(_eth_ip!=0) ulSecondCounter_Ethernet++;
#endif

		bEvenSecond=true;
	}


	bool bEvenDeciSecond=false;

	if((int) (millis()-ulLastLoopDeciSecondCounterTimestamp)>=100)
	{
		ulLastLoopDeciSecondCounterTimestamp+=100;
		bEvenDeciSecond=true;
	}

	if(!bEvenDeciSecond) return;

	if(bEvenSecond)
	{
	}

	bool bConnected=false;
	if(WiFi.status() == WL_CONNECTED) bConnected=true;

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
	uint32_t _eth_ip=ETH.localIP();
	if(_eth_ip!=0) bConnected=true;
#endif

	if(!bConnected)
	{
		ulSecondCounter_WiFi=0;
		ulSecondCounter_MQTT=0;
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
		ulSecondCounter_Ethernet=0;
#endif

		return;
	}

	if(!bInitialized)
	{
		ulSecondCounter_MQTT=0;
		return;
	}

#if defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
	pMQTT->loop();
#elif defined(USE_MPSCG)
	if(mqttClient.isConnected())
	{
		mqttClient.update();
	}
#endif

	if(IsConnected())
	{

		if(!bEnableMQTT)
		{
			if(bWasConnected)
			{
				bWasConnected=false;
				PublishDirect(szWillTopic, 2, true, "Offline");
#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
				mqtt.disconnect(false);
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
				pMQTT->disconnect();
#elif defined(USE_MPSCG)
				mqttClient.disconnect();
#endif
			}
			return;
		}

		bWasConnected=true;

		DoInitialPublishing();

//		pubsubClient.loop();
		ulMqttReconnectCount=0;

		if(bEvenSecond)
		{
			DoStatusPublishing();
		}


	}
	else
	{

		//csprintf("not connected. bConnecting=%i\n",bConnecting);

		ulSecondCounter_MQTT=0;

		if(bEnableMQTT)
		{


			if(!bConnecting)
			{

				//csprintf("millis()-ulLastReconnect=%i  interval=%i\n",millis()-ulLastReconnect,interval);

				if(!ulLastReconnect || (millis()-ulLastReconnect)>GetReconnectInterval())
				{

					if(!HasServerIP())
					{
						csprintf(PSTR("No MQTT server IP configured.\n"));
						ulLastReconnect=millis();
						return;
					}

					IPAddress ip;
					ip.fromString(strMqttServerIP);

					//ip.fromString("172.22.22.99");

					csprintf(PSTR("Connecting to MQTT server %s...\n"),strMqttServerIP.c_str());
					bConnecting=true;
					bSendError=false;
					bInitialPublishingDone=false;

					ulConnectTimestamp=millis();


					uint8_t mac[6];
					WiFi.macAddress(mac);

					char szMacString[10];
					sprintf(szMacString,PSTR("%02x%02x%02x"),mac[3],mac[4],mac[5]);

					String strClientID=strID;
					strClientID += "-";
					strClientID += szMacString;



#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
					mqtt.setServer(ip,1883);//1883
					mqtt.setCredentials(strMqttUserName.c_str(), strMqttPassword.c_str());
					mqtt.connect();
#elif defined(USE_ARDUINOMQTT)

					pMQTT->begin(ip, net);

					int ret=pMQTT->connect( strClientID.c_str(), strMqttUserName.c_str(), strMqttPassword.c_str());
					if(ret)
					{
						//csprintf("MQTT connect %s %s %s returned %i\n",strID.c_str(), strMqttUserName.c_str(), strMqttPassword.c_str(),ret);
						onConnect(false);
					}
#elif defined(USE_PUBSUBCLIENT)
					pMQTT->setServer(ip,1883);
					pMQTT->setSocketTimeout(GetReconnectTimeout());
					int ret=pMQTT->connect(strClientID.c_str(), strMqttUserName.c_str(), strMqttPassword.c_str(), szWillTopic, 1, 1, "Offline");
					if(ret)
					{
						onConnect(false);
					}
					else
					{
						onDisconnect(0);
					}
#elif defined(USE_MPSCG)
#if defined(ARDUINO_ARCH_ESP32)
					//ip.fromString("172.22.16.44");
					//csprintf("connecting to IP %s, attempt %u mod %u timeout %i\n",ip.toString().c_str(),ulMqttReconnectCount, ulMqttReconnectCount % 3, timeout);
					int ret=client.connect(ip, 1883, GetReconnectTimeout() );
#else
					int ret=client.connect(ip, 1883 );
#endif
					//csprintf("returned %i\n",ret);
					if(ret==1)
					{
						//strMqttPassword="wrong";
						//csprintf("connected! logging in: %s/%s (id %s)\n",strMqttUserName.c_str(),strMqttPassword.c_str(),strClientID.c_str());
						if(mqttClient.connect(strClientID.c_str(),strMqttUserName.c_str(),strMqttPassword.c_str()))
						{
							//csprintf("logged in\n");
							onConnect(false);
						}
						else
						{
							client.stop();
							onDisconnect(0);
						}


					}
					else
					{
						onDisconnect(0);
					}
#endif
				}
			}
			else
			{
				//if we're still not connected after a minute, try again
				if(!ulConnectTimestamp || (millis()-ulConnectTimestamp)>60000)
				{
					csprintf(PSTR("Reconnect needed, dangling flag\n"));
#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
					mqtt.disconnect(true);
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
					pMQTT->disconnect();
#elif defined(USE_MPSCG)
					mqttClient.disconnect();
#endif
					bConnecting=false;
				}

			}
		}
	}

}

bool LeifSimpleMQTT::HasServerIP()
{
	IPAddress ip;
	ip.fromString(strMqttServerIP);
	uint32_t mqtt_ip=ip;
	return mqtt_ip!=0;
}

void LeifSimpleMQTT::onConnect(bool sessionPresent)
{
	if(sessionPresent)	//squelch unused parameter warning
	{
	}
#ifdef MQTTLIB_VERBOSE
	csprintf("onConnect... %p\n",this);
#endif
	bConnecting=false;

	bDoInitialPublishing=true;
	ulInitialPublishing=0;
	iInitialPublishing=0;
	iInitialPublishing_Node=0;
	iInitialPublishing_Prop=0;
	iPubCount_Props=0;

	ulSecondCounter_MQTT=0;

}


#if defined(USE_PANGOLIN)
void LeifSimpleMQTT::onDisconnect(int8_t reason)
{
	if(reason==TCP_DISCONNECTED)
	{
	}
#elif defined(USE_ASYNCMQTTCLIENT)
void LeifSimpleMQTT::onDisconnect(AsyncMqttClientDisconnectReason reason)
{
	if(reason==AsyncMqttClientDisconnectReason::TCP_DISCONNECTED)
	{
	}
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT) | defined(USE_MPSCG)
void LeifSimpleMQTT::onDisconnect(int8_t reason)
{
	(void)(reason);
#endif
	//csprintf("onDisconnect...");
	if(bConnecting)
	{
		ulLastReconnect=millis();
		ulMqttReconnectCount++;
		bConnecting=false;
		//csprintf("onDisconnect...   reason %i.. lr=%lu\n",reason,ulLastReconnect);
		csprintf(PSTR("MQTT server connection failed. Retrying in %lums\n"),GetReconnectInterval());
	}
	else
	{
		csprintf(PSTR("MQTT server connection lost\n"));
	}
}

#if defined(USE_PANGOLIN)
void LeifSimpleMQTT::onMqttMessage(const char* topic, uint8_t * payload, PANGO_PROPS properties, size_t len, size_t index, size_t total)
{
#elif defined(USE_ASYNCMQTTCLIENT)
void LeifSimpleMQTT::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
#elif defined(USE_ARDUINOMQTT)
void LeifSimpleMQTT::onClientCallbackAdvanced(MQTTClient *client, char topic[], char payload[], int len)
{
	void * properties=NULL;
	uint8_t total=0; uint8_t index=0;
	(void)(client);
#elif defined(USE_PUBSUBCLIENT)
void LeifSimpleMQTT::onMqttMessage(char* topic, byte* payload, unsigned int len)
{
	void * properties=NULL;
	uint8_t total=0; uint8_t index=0;
#elif defined(USE_MPSCG)
void LeifSimpleMQTT::onMqttMessage(const char* topic, const char* payload, size_t len)
{
	void * properties=NULL;
	uint8_t total=0; uint8_t index=0;
#endif
	String strTopic=topic;
	_map_incoming::const_iterator citer=mapIncoming.find(strTopic);

	if(citer!=mapIncoming.end())
	{
		MqttSubscription * pProp=citer->second;
		if(pProp)
		{

			pProp->onMqttMessage(topic, payload, properties, len, index, total);

		}
	}

	//csprintf("RECEIVED %s %s\n",topic,payload);
}



void LeifSimpleMQTT::DoStatusPublishing()
{

	if(bDoInitialStatusPublishing)
	{
		if(ulSecondCounter_MQTT>=5)
		{
			bDoInitialStatusPublishing=false;

			String strTopic;
			strTopic="tele/";
			strTopic+=strID;
			strTopic+="/info";







			String strData="{";
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
			strData+=PSTR("\"Ethernet IP\": \"");
			strData+=ETH.localIP().toString();
			strData+="\"";
			strData+=",";
			strData+=PSTR("\"Ethernet MAC\": \"");
			strData+=ETH.macAddress();
			strData+="\"";
			strData+=",";
#endif

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
			strData+=PSTR("\"WiFi IP\": \"");
#else
			strData+=PSTR("\"IPAddress\": \"");
#endif
			strData+=WiFi.localIP().toString();
			strData+="\"";
			strData+=",";
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
			strData+="\"WiFi MAC\": \"";
#else
			strData+="\"MAC\": \"";
#endif
			strData+=WiFi.macAddress();
			strData+="\"";

			if(LeifGetVersionText().length())
			{
				strData+=",";
				strData+=PSTR("\"Version\": \"");
				strData+=LeifGetVersionText();
				strData+="\"";
			}
			else
			{
				strData+=PSTR(",\"Built\": \"");
				strData+=LeifGetCompileDate();
				strData+="\"";
			}
			strData+=strJsonInfoExtra;

			strData+="}";

			PublishDirect(strTopic, 2, true, strData);

		}

	}



	int interval=60;
	if(ulSecondCounter_Uptime >= 1800) interval=900;
	else if(ulSecondCounter_Uptime >= 900) interval=300;
	else if(ulSecondCounter_Uptime >= 300) interval=300;

	if((ulSecondCounter_MQTT % interval)==20)
	{
		if(IsConnected())
		{

			String strUptime;
			LeifUptimeString(strUptime);

			String strUptimeWiFi;
			LeifSecondsToUptimeString(strUptimeWiFi,GetUptimeSeconds_WiFi());

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
			String strUptimeEthernet;
			LeifSecondsToUptimeString(strUptimeEthernet,GetUptimeSeconds_Ethernet());
#endif

			String strUptimeMQTT;
			LeifSecondsToUptimeString(strUptimeMQTT,GetUptimeSeconds_MQTT());

			String strTopic;
			strTopic=PSTR("tele/");
			strTopic+=strID;
			strTopic+=PSTR("/status");

			String strData="{";
			strData+=PSTR("\"Uptime\": \"");
			strData+=strUptime;
			strData+=PSTR("\",\"WiFi Uptime\": \"");
			strData+=strUptimeWiFi;
#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
			strData+=PSTR("\",\"ETH Uptime\": \"");
			strData+=strUptimeEthernet;
#endif
			strData+=PSTR("\",\"RSSI\": \"");
			strData+=String(WiFi.RSSI());
			strData+=PSTR("\",\"MQTT Uptime\": \"");
			strData+=strUptimeMQTT;
			strData+=PSTR("\",\"Heap\": \"");
#if ARDUINO_ESP8266_MAJOR >= 3
			{
				ESP.setIramHeap();
				uint32_t heapFreeIram=ESP.getFreeHeap();
				ESP.resetHeap();
				ESP.setDramHeap();
				uint32_t heapFreeDram=ESP.getFreeHeap();
				ESP.resetHeap();
				strData+="D ";
				strData+=String(heapFreeDram);
				strData+=", I ";
				strData+=String(heapFreeIram);
			}
#else
			strData+=String(ESP.getFreeHeap());
#endif
			strData+="\"";

			strData+="}";

			PublishDirect(strTopic, 2, false, strData);

			bTelemetrySent=true;
		}
	}



}

void LeifSimpleMQTT::DoInitialPublishing()
{
#if defined(USE_ARDUINOMQTT)
	MQTTClient & mqtt=*pMQTT;
#elif defined(USE_PUBSUBCLIENT)
	PubSubClient & mqtt=*pMQTT;
#elif defined(USE_MPSCG)
	MyPubSubClient & mqtt=mqttClient;
#endif

	if(!bDoInitialPublishing)
	{
		iInitialPublishing=0;
		ulInitialPublishing=0;
		return;
	}


	if(!ulInitialPublishing)
	{
		csprintf(PSTR("MQTT Connected!\n"));
		iPubCount_Props=0;

		PublishDirect(szWillTopic, 2, true, "Online");
		csprintf(PSTR("Published %s = Online\n"),szWillTopic);


		for(size_t i=0;i<vecSub.size();i++)
		{
			vecSub[i]->bSubscriptionFlag=false;
		}

		for(size_t i=0;i<vecSub.size();i++)
		{
			if(!vecSub[i]->bSubscriptionFlag)
			{
				vecSub[i]->bSubscriptionFlag=true;
				bool bError=false;
				bool bSuccess=false;
#if defined(USE_MPSCG)
				bError |= (bSuccess=mqtt.subscribe(vecSub[i]->strTopic.c_str(), sub_qos, DummyTopicCallback));
#else
				bError |= (bSuccess=mqtt.subscribe(vecSub[i]->strTopic.c_str(), sub_qos));
#endif
#ifdef MQTTLIB_VERBOSE
				csprintf(PSTR("Subscribed to %s: %s\n"),vecSub[i]->strTopic.c_str(),bSuccess?"SUCCESS":"FAILED");
#endif
			}
		}

		bDoInitialStatusPublishing=true;

	}

	ulInitialPublishing=millis();

#ifdef MQTTLIB_VERBOSE
	if(bDebug) csprintf("IPUB: %i        Node=%i  Prop=%i\n",iInitialPublishing, iInitialPublishing_Node, iInitialPublishing_Prop);
#endif

}

uint16_t LeifSimpleMQTT::PublishDirect(const String & topic, uint8_t qos, bool retain, const String & payload)
{
#if defined(USE_PANGOLIN)
	return mqtt.publish(topic.c_str(), qos, retain, (uint8_t *) payload.c_str(), payload.length(), false);
#elif defined(USE_ASYNCMQTTCLIENT)
	return mqtt.publish(topic.c_str(), qos, retain, payload.c_str(), payload.length());
#elif defined(USE_ARDUINOMQTT)
	return pMQTT->publish(topic, payload, retain, qos)==true;
#elif defined(USE_PUBSUBCLIENT)
	(void)(qos);
	return pMQTT->publish(topic.c_str(), (const uint8_t *) payload.c_str(), payload.length(), retain);
#elif defined(USE_MPSCG)
	return mqttClient.publish(topic.c_str(), payload.c_str(), retain, qos);
#endif
}

bool bFailPublish=false;

uint16_t LeifSimpleMQTT::Publish(const char* topic, uint8_t qos, bool retain, const char* payload, size_t length, bool reserved, uint16_t message_id)
{
	(void)(reserved);
	(void)(message_id);
	if(!IsConnected()) return 0;
	uint16_t ret=0;

	if(!bFailPublish)
	{
		if(!length) length=strlen(payload);
#if defined(USE_PANGOLIN)
		mqtt.publish(topic,qos,retain,(uint8_t *) payload,length,0);
		ret=true;
#elif defined(USE_ASYNCMQTTCLIENT)
		ret=mqtt.publish(topic,qos,retain,payload,length,false,message_id);
#elif defined(USE_ARDUINOMQTT)
		ret=pMQTT->publish(topic, payload, retain, qos);
#elif defined(USE_PUBSUBCLIENT)
		(void)(qos);
		ret=pMQTT->publish(topic, (uint8_t *) payload, length, retain);
#elif defined(USE_MPSCG)
		ret=mqttClient.publish(topic, payload, retain, qos);
#endif
	}

	//csprintf("Publish %s: ret %i\n",topic,ret);

	if(!ret)
	{	//failure
		if(!bSendError)
		{
			bSendError=true;
			ulSendErrorTimestamp=millis();
		}
		else
		{
			if((int) (millis()-ulSendErrorTimestamp) > 60000)	//a full minute with no successes
			{
				csprintf(PSTR("Full minute with no publish successes, disconnect and try again\n"));
#if defined(USE_PANGOLIN) | defined(USE_ASYNCMQTTCLIENT)
				mqtt.disconnect(true);
#elif defined(USE_ARDUINOMQTT) | defined(USE_PUBSUBCLIENT)
				pMQTT->disconnect();
#elif defined(USE_MPSCG)
				mqttClient.disconnect();
				onDisconnect(0);
#endif
				bSendError=false;
				bConnecting=false;
			}
		}
	}
	else
	{	//success
		bSendError=false;

	}

	return ret;
}


String MqttDeviceName(const char * in)
{
	String ret;

	enum eLast
	{
		last_hyphen,
		last_upper,
		last_lower,
		last_number,
	};

	eLast last=last_hyphen;

	while(*in)
	{
		char input=*in++;

		if(input>='0' && input<='9')
		{
			ret+=input;
			last=last_number;
		}
		else if(input>='A' && input<='Z')
		{
			if(last==last_lower)
			{
				ret+='-';
			}

			ret+=(char) (input | 0x20);

			last=last_upper;
		}
		else if(input>='a' && input<='z')
		{
			ret+=input;
			last=last_lower;
		}
		else
		{
			if(last!=last_hyphen)
			{
				ret+="-";
			}

			last=last_hyphen;
		}

	}

	return ret;
}


int LeifSimpleMQTT::GetErrorRetryFrequency()
{
	int iErrorDuration=(int) (millis()-ulSendErrorTimestamp);
	if(iErrorDuration >= 20000)
	{
		return 10000;
	}
	if(iErrorDuration >= 5000)
	{
		return 5000;
	}

	return 1000;
}

uint32_t LeifSimpleMQTT::GetUptimeSeconds_WiFi()
{
	return ulSecondCounter_WiFi;
}

uint32_t LeifSimpleMQTT::GetUptimeSeconds_MQTT()
{
	return ulSecondCounter_MQTT;
}

#if defined(USE_ETHERNET) & defined(ARDUINO_ARCH_ESP32)
uint32_t LeifSimpleMQTT::GetUptimeSeconds_Ethernet()
{
	return ulSecondCounter_Ethernet;
}
#endif

unsigned long LeifSimpleMQTT::GetReconnectInterval()
{
	unsigned long interval=5000;
	if(!HasServerIP())
	{
		return 60000;
	}
	if(ulMqttReconnectCount>=20) interval=60000;
	else if(ulMqttReconnectCount>=15) interval=30000;
	else if(ulMqttReconnectCount>=10) interval=20000;
	else if(ulMqttReconnectCount>=5) interval=10000;
	return interval;
}

int LeifSimpleMQTT::GetReconnectTimeout()
{
	if(ulMqttReconnectCount<8)
	{
		return ((ulMqttReconnectCount % 3) == 2)?2000:1000;
	}
	return 5000;
}


void LeifSimpleMQTT::Subscribe(MqttSubscription & sub)
{
	vecSub.push_back(&sub);
	mapIncoming[sub.strTopic]=&sub;

}

MqttSubscription * LeifSimpleMQTT::NewSubscription(const String & topic)
{
	MqttSubscription * ret=NULL;
	_map_incoming::const_iterator iter=mapIncoming.find(topic);
	if(iter==mapIncoming.end())	//not found, this is a new subscription!
	{
#ifdef MQTTLIB_VERBOSE
		csprintf("NEW subscription to %s\n",topic.c_str());
#endif
		ret=new MqttSubscription;
		ret->strTopic=topic;
		Subscribe(*ret);
		return ret;
	}
	else
	{
#ifdef MQTTLIB_VERBOSE
		csprintf("Duplicate subscription to %s\n",topic.c_str());
#endif
		//we already have a subscription to this topic!
		ret=iter->second;
		return ret;
	}
}


#if defined(USE_PANGOLIN)
void MqttSubscription::onMqttMessage(const char* topic, uint8_t * payload, PANGO_PROPS properties, size_t len, size_t index, size_t total)
{
#elif defined(USE_ASYNCMQTTCLIENT)
void MqttSubscription::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
#elif defined(USE_ARDUINOMQTT)
void MqttSubscription::onMqttMessage(char* topic, char* payload, void * properties, size_t len, size_t index, size_t total)
{
	(void)(properties); (void)(total); (void)(topic);
#elif defined(USE_PUBSUBCLIENT)
void MqttSubscription::onMqttMessage(char* topic, byte* payload, void * properties, unsigned int len, int index, int total)
{
	(void)(properties); (void)(total); (void)(topic);
#elif defined(USE_MPSCG)
void MqttSubscription::onMqttMessage(const char* topic, const char* payload, void * properties, size_t len, int index, int total)
{
	(void)(topic); (void)(properties); (void)(total);
#endif

	if(index==0)
	{

		{
			std::string temp;
			temp.assign((const char *) payload,len);

			strValue=temp.c_str();
		}

		DoCallback();
	}

}


void MqttSubscription::DoCallback()
{

	for(size_t i=0;i<vecCallback.size();i++)
	{
		vecCallback[i](this);
	}
#ifdef MQTTLIB_VERBOSE
	csprintf("%s CLEAR PAYLOAD!\n", strTopic.c_str());
#endif
	if(bClearPayloadAfterCallback)
	{
		strValue="";
	}
};

void MqttSubscription::SetClearPayloadAfterCallback(bool bEnable)
{
	bClearPayloadAfterCallback=bEnable;
}
