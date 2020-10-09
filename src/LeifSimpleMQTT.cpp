#include "LeifSimpleMQTT.h"
#include "LeifEspBase.h"

void MqttLibDebugPrint(const char * szText);

//#define csprintf(...) { char szTemp[256]; sprintf(szTemp,__VA_ARGS__ ); MqttLibDebugPrint(szTemp); }


#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include "WiFi.h"
#endif


static std::vector<MqttDebugPrintCallback> vecDebugPrint;

const int ipub_qos=1;
const int sub_qos=2;

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
}


void LeifSimpleMQTT::Init()
{

	strcpy(szWillTopic,String("tele/"+strID+"/LWT").c_str());

	mqtt.setWill(szWillTopic,2,true,"Offline");

	mqtt.onConnect(std::bind(&LeifSimpleMQTT::onConnect, this, std::placeholders::_1));
	mqtt.onDisconnect(std::bind(&LeifSimpleMQTT::onDisconnect, this, std::placeholders::_1));
	mqtt.onMessage(std::bind(&LeifSimpleMQTT::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));

	bSendError=false;

	bInitialized=true;
}

void LeifSimpleMQTT::Quit()
{
	PublishDirect(szWillTopic, 2, true, "Offline");
	mqtt.disconnect(false);
	bInitialized=false;
}

bool LeifSimpleMQTT::IsConnected()
{
	return mqtt.connected();
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
		if(mqtt.connected()) ulSecondCounter_MQTT++;

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

	if(WiFi.status() != WL_CONNECTED)
	{
		ulSecondCounter_WiFi=0;
		ulSecondCounter_MQTT=0;
		return;
	}

	if(!bInitialized)
	{
		ulSecondCounter_MQTT=0;
		return;
	}

	if(mqtt.connected())
	{

		if(!bEnableMQTT)
		{
			if(bWasConnected)
			{
				bWasConnected=false;
				PublishDirect(szWillTopic, 2, true, "Offline");
				mqtt.disconnect(false);
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

					IPAddress ip;
					ip.fromString(strMqttServerIP);
					//ip.fromString("172.22.22.99");
					mqtt.setServer(ip,1883);//1883
					mqtt.setCredentials(strMqttUserName.c_str(), strMqttPassword.c_str());

					csprintf("Connecting to MQTT server %s...\n",strMqttServerIP.c_str());
					bConnecting=true;
					bSendError=false;
					bInitialPublishingDone=false;

					ulConnectTimestamp=millis();
					mqtt.connect();
				}
			}
			else
			{
				//if we're still not connected after a minute, try again
				if(!ulConnectTimestamp || (millis()-ulConnectTimestamp)>60000)
				{
					csprintf("Reconnect needed, dangling flag\n");
					mqtt.disconnect(true);
					bConnecting=false;
				}

			}
		}
	}

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

void LeifSimpleMQTT::onDisconnect(int8_t reason)
{
	if(reason==TCP_DISCONNECTED)
	{
	}
	//csprintf("onDisconnect...");
	if(bConnecting)
	{
		ulLastReconnect=millis();
		ulMqttReconnectCount++;
		bConnecting=false;
		//csprintf("onDisconnect...   reason %i.. lr=%lu\n",reason,ulLastReconnect);
		csprintf("MQTT server connection failed. Retrying in %lums\n",GetReconnectInterval());
	}
	else
	{
		csprintf("MQTT server connection lost\n");
	}
}

void LeifSimpleMQTT::onMqttMessage(const char* topic, uint8_t * payload, PANGO_PROPS properties, size_t len, size_t index, size_t total)
{
	String strTopic=topic;
	_map_incoming::const_iterator citer=mapIncoming.find(strTopic);

	if(citer!=mapIncoming.end())
	{
		MqttSubscription * pProp=citer->second;
		if(pProp)
		{

			pProp->OnMqttMessage(topic, payload, properties, len, index, total);

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
			strData+="\"IPAddress\": \"";
			strData+=WiFi.localIP().toString();
			strData+="\",";
			strData+="\"Built\": \"";
			strData+=LeifGetCompileDate();
			strData+="\"";
			strData+="}";

			PublishDirect(strTopic, 2, true, strData);

		}

	}



	int interval=60;
	if(ulSecondCounter_Uptime >= 1800) interval=900;
	else if(ulSecondCounter_Uptime >= 900) interval=300;
	else if(ulSecondCounter_Uptime >= 300) interval=300;

	if((ulSecondCounter_Uptime % interval)==30)
	{
		if(IsConnected())
		{

			String strUptime;
			LeifUptimeString(strUptime);

			String strUptimeWiFi;
			LeifSecondsToUptimeString(strUptimeWiFi,GetUptimeSeconds_WiFi());

			String strUptimeMQTT;
			LeifSecondsToUptimeString(strUptimeMQTT,GetUptimeSeconds_MQTT());

			String strTopic;
			strTopic="tele/";
			strTopic+=strID;
			strTopic+="/status";

			String strData="{";
			strData+="\"Uptime\": \"";
			strData+=strUptime;
			strData+="\",";
			strData+="\"WiFi Uptime\": \"";
			strData+=strUptimeWiFi;
			strData+="\",";
			strData+="\"MQTT Uptime\": \"";
			strData+=strUptimeMQTT;
			strData+="\"";
			strData+="}";

			PublishDirect(strTopic, 2, false, strData);
		}
	}



}

void LeifSimpleMQTT::DoInitialPublishing()
{
	if(!bDoInitialPublishing)
	{
		iInitialPublishing=0;
		ulInitialPublishing=0;
		return;
	}


	if(!ulInitialPublishing)
	{
		csprintf("MQTT Connected!\n");
		iPubCount_Props=0;

		PublishDirect(szWillTopic, 2, true, "Online");
		csprintf("Published %s = Online\n",szWillTopic);

		for(size_t i=0;i<vecSub.size();i++)
		{
			bool bError=false;
			bool bSuccess=false;
			bError |= (bSuccess=mqtt.subscribe(vecSub[i]->strTopic.c_str(), sub_qos));
			csprintf("Subscribed to %s: %s\n",vecSub[i]->strTopic.c_str(),bSuccess?"SUCCESS":"FAILED");
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
	return mqtt.publish(topic.c_str(), qos, retain, (uint8_t *) payload.c_str(), payload.length(), false);
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
		mqtt.publish(topic,qos,retain,(uint8_t *) payload,length,0);
		ret=true;
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
				csprintf("Full minute with no publish successes, disconnect and try again\n");
				mqtt.disconnect(true);
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

unsigned long LeifSimpleMQTT::GetReconnectInterval()
{
	unsigned long interval=5000;
	if(ulMqttReconnectCount>=20) interval=60000;
	else if(ulMqttReconnectCount>=15) interval=30000;
	else if(ulMqttReconnectCount>=10) interval=20000;
	else if(ulMqttReconnectCount>=5) interval=10000;
	return interval;
}


void LeifSimpleMQTT::Subscribe(MqttSubscription & sub)
{
	vecSub.push_back(&sub);
	mapIncoming[sub.strTopic]=&sub;

}



void MqttSubscription::OnMqttMessage(const char* topic, uint8_t * payload, PANGO_PROPS properties, size_t len, size_t index, size_t total)
{
	if(properties.retain || total || topic)	//squelch unused parameter warnings
	{
	}

	if(index==0)
	{

		std::string temp;
		temp.assign((const char *) payload,len);

		strValue=temp.c_str();

		DoCallback();
	}

}

