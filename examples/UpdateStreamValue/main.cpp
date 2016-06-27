#include "mbed.h"
#include "LM75B.h"
#include "EthernetInterface.h"

#include "minimal-mqtt.h"
#include "minimal-json.h"

#define MBED_PLATFORM
#include "M2XMQTTClient.h"

char deviceId[] = "<device id>"; // Device you want to push to
char streamName[] = "<stream name>"; // Stream you want to push to
char m2xKey[] = "<m2x api key>"; // Your M2X API Key or Master API Key

Client client;
M2XStreamClient m2xClient(&client, m2xKey);

EthernetInterface eth;
LM75B tmp(p28,p27);

int main() {
  eth.init();
  eth.connect();
  printf("IP Address: %s\n", eth.getIPAddress());

  while (true) {
    double val = tmp.read();
    printf("Current temperature is: %lf", val);

    int response = m2xClient.updateStreamValue(deviceId, streamName, val);
    printf("Response code: %d\n", response);

    if (response == -1) while (true) ;

    delay(5000);
  }
}
