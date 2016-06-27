#include "mbed.h"
#include "EthernetInterface.h"

#include "minimal-mqtt.h"
#include "minimal-json.h"

#define MBED_PLATFORM
#include "M2XMQTTClient.h"

char deviceId[] = "<device id>"; // Device you want to update
char m2xKey[] = "<m2x api key>"; // Your M2X API Key or Master API Key

char name[] = "<location name>"; // Name of current location of datasource
double latitude = -37.97884;
double longitude = -57.54787; // You can also read those values from a GPS
double elevation = 15;

Client client;
M2XStreamClient m2xClient(&client, m2xKey);

EthernetInterface eth;

int main() {
  eth.init();
  eth.connect();
  printf("IP Address: %s\n", eth.getIPAddress());

  while (true) {
    int response = m2xClient.updateLocation(deviceId, name, latitude, longitude, elevation);
    printf("Update response code: %d\n", response);
    elevation++;

    if (response == -1) while (true) ;

    delay(5000);
  }
}
