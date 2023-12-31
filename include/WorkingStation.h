#pragma once
#include <WString.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiSTA.h>
#include <FS.h>
#include <Wire.h>

#include <PubSubClient.h>
#include "effectsManager.h"
#include "IErrorReported.h"

#include "vector"

enum WorkMode
{
	NORMAL = 0,
	POWER_SAVE = 1
};

class CWorkingStation : public IErrorReporter
{
public:
	CWorkingStation()
		: m_client(m_espClient)
	{
	};

	virtual ~CWorkingStation()
	{
		for (int i = 0; i < NUM_CHANNELS; i++)
		{
			EffectsManager* pEffect = m_vecEffects.at(i);
			delete pEffect;
		}

		m_vecEffects.clear();

		if (m_ssid)
			delete[] m_ssid;

		if (m_psk)
			delete[] m_psk;
	};

	bool Init();
	void Work();

	virtual void ReportError(String err);

private:
	bool ReconnectMQTT();
	void ConnectToWifi();
	void PublishCurrPlayEffect();

	void MQTT_Callback(char *topic, uint8_t *payload, unsigned int length);

	void NullTerminateArray(void *src, uint8_t len, void **dest);

private:
	char *m_ssid;
	char *m_psk;

	PubSubClient m_client;
	WiFiClient m_espClient;

	std::vector<EffectsManager *> m_vecEffects;
};