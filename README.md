ARM mbed M2X MQTT Client
========================

The ARM mbed MQTT client library is used to send/receive data to/from [AT&amp;T's M2X service](https://m2x.att.com/) from [mbed LPC1768](http://mbed.org/platforms/mbed-LPC1768/) or [FRDM-K64F](https://developer.mbed.org/platforms/FRDM-K64F/) microcontrollers using [MQTT](http://mqtt.org/) protocol instead of HTTP protocol.

Getting Started
==========================
1. Signup for an [M2X Account](https://m2x.att.com/signup).
2. Obtain your _Master Key_ from the Master Keys tab of your [Account Settings](https://m2x.att.com/account) screen.
2. Create your first [Device](https://m2x.att.com/devices) and copy its _Device ID_.
3. Review the [M2X API Documentation](https://m2x.att.com/developer/documentation/overview).
4. You have 2 choices for hardware: you can obtain an [mbed LPC1768](http://mbed.org/platforms/mbed-LPC1768/) with an [mbed application board](http://mbed.org/cookbook/mbed-application-board), or you can obtain a [FRDM-K64F](http://www.nxp.com/products/software-and-tools/hardware-development-tools/freedom-development-boards/freedom-development-platform-for-kinetis-k64-k63-and-k24-mcus:FRDM-K64F) board.

**NOTE**: Though [similiar boards](http://mbed.org/cookbook/Homepage) exist and may also do the job, this doc is specific to using a combination of mbed LPC1768 microcontroller and mbed application board, or a single FRDM-K64F

**ublox** Modem/GPS shield users can find a m2x working demo with freescale boards on the mbed website [here](http://developer.mbed.org/teams/ublox/code/Cellular_m2x-demo-all/).

Please consult the [M2X glossary](https://m2x.att.com/developer/documentation/glossary) if you have questions about any M2X specific terms.

How to run the examples
=======================

To run the examples, please follow the steps below:

1. Launch [mbed online compiler](https://mbed.org/compiler/) in your browser
2. Click `New` to create a new program, select `Empty Program` as the template and give it a name, then click `OK`.
3. Right click on the project you just created, select `Import Library…->From Import Wizard…`, select `Libraries` tab, use the scrollbar or the search box at the right to find `mbed` library, double click it to import it.
4. Use the same step as #3 to import `mbed-rtos` and `EthernetInterface` (mbed official) libraries.

    **NOTE**: To Run the `PostExample` example, you also need to import `LM75B` library in the same way to use the temperature sensor.

5. Right click on the project, select `Import Library…->From URL…`, paste the following URL into `Source URL` box:

    ```
    http://mbed.org/users/defmacro/code/minimal-json/
    ```

    Click `Import` to import minimal-json library.
6. Use the same step as #5 to import minimal-mqtt library:

    ```
    https://mbed.org/users/defmacro/code/minimal-mqtt/
    ```

7. Use the same step as #5 to import the following M2X client library:

    ```
    https://developer.mbed.org/users/citrusbyte/code/M2XMQTTClient/
    ```

8. Create a file named `main.cpp`, and paste in any of the examples. Modify the M2X API Key, device ID or stream name as needed by the examples. You can find existing examples at [here](https://github.com/attm2x/m2x-arm-mbed-mqtt/tree/master/examples).
9. At the top right corner, make sure you are selecting the same platform as your board.
10. When you are done, click `Compile`. if no error is found, a bin file will be downloaded to your computer.
11. Copy that bin file to your mbed microcontroller, press the `reset` button, then you should be able to run the program!

Variables used in Examples
==========================

In order to run the given examples, different variables need to be configured. We will walk through those variables in this section.

M2X API Key
-----------

Once you [register](https://m2x.att.com/signup) for an AT&amp;T M2X account, an API key is automatically generated for you. This key is called a _Primary Master Key_ and can be found in the _Master Keys_ tab of your [Account Settings](https://m2x.att.com/account). This key cannot be edited nor deleted, but it can be regenerated. It will give you full access to all APIs.

However, you can also create a _Device API Key_ associated with a given Device, you can use the Device API key to access the streams belonging to that Device.

You can customize this variable in the following line in the examples:

```
char m2xKey[] = "<M2X access key>";
```

Device ID
-------

A device is associated with a source of data. It is a set of data streams, such as streams of locations, temperatures, etc. The following line is needed to configure the device used:

```
char deviceId[] = "<device id>";
```

Stream Name
------------

A stream in a device is a set of timed series data of a specific type (i,e. humidity, temperature), you can use the M2XMQTTClient library to send stream values to M2X server, or receive stream values from M2X server. Use the following line to configure the stream if needed:

```
char streamName[] = "<stream name>";
```

Using the M2XMQTTClient library
=========================

In the M2XMQTTClient, the following API functions are provided:

* `updateStreamValue`: Send stream value to M2X server
* `postDeviceUpdates`: Post values from multiple streams to M2X server
* `updateLocation`: Send location value of a device to M2X server

Returned values
---------------

For all those functions, the HTTP status code will be returned if we can fulfill a HTTP request. For example, `200` will be returned upon success, `401` will be returned if we didn't provide a valid M2X API Key.

Otherwise, the following error codes will be used:

```
static const int E_NOCONNECTION = -1;
static const int E_DISCONNECTED = -2;
static const int E_NOTREACHABLE = -3;
static const int E_INVALID = -4;
static const int E_JSON_INVALID = -5;
```

Update stream value
-------------------

The following functions can be used to post one single value to a stream, which belongs to a device:

```
template <class T>
int updateStreamValue(const char* deviceId, const char* streamName, T value);
```

Here we use C++ templates to generate functions for different types of values, feel free to use values of `float`, `int`, `long` or even `const char*` types here.

NOTE: Our example here is configured to use a temperature sensor for generating values, this only applies to LPC1768 board with the application extention board. If you are using FRDM-K64F board, you might need to change this code or attach a separate temperature sensor.

Post device updates
-------------------

M2X also supports posting multiple values to multiple streams in one call, use the following function for this:

```
template <class T>
int postDeviceUpdates(const char* deviceId, int streamNum,
                      const char* names[], const int counts[],
                      const char* ats[], T values[]);
```

Please refer to the comments in the source code on how to use this function, basically, you need to provide the list of streams you want to post to, and values for each stream.

Update Device Location
--------------------------

You can use the following function to update the location for a device:

```
template <class T>
int updateLocation(const char* deviceId, const char* name,
                   T latitude, T longitude, T elevation);
```

Different from stream values, locations are attached to devices rather than streams. We use templates here, since the values may be in different format, for example, you can express latitudes in both `double` and `const char*`.

How to read Serial output
=========================

Though you can use `printf` on a mbed microcontroller, the output is sent to Serial output. To check what your microcontroller outputs to serial ports, follow the instructions at [here](http://mbed.org/handbook/SerialPC).

Examples
========

We provide a series of examples that will help you get an idea of how to use the `M2XMQTTClient` library to perform all kinds of tasks.

Note that the examples contain fictionary variables, and that they need to be configured as per the instructions above before running on your mbed microcontroller. Each of the examples here also needs an Internet connection setup, either via an Ethernet port or a Wifi module.

In the `UpdateStreamValue` example, the temperature sensor on the mbed application board is needed. For other examples, no extra modules are needed besides the network connection.

UpdateStreamValue
-----------------

This example shows how to post temperatures to M2X server. Before running this, you need to have a valid M2X Key, a device ID and a stream name. The mbed microcontroller need to be hooked on the mbed application board in order to use the temperature sensor.

PostDeviceUpdates
-----------------

This example shows how to post multiple values to multiple streams in one API call.

UpdateLocation
--------------

This one sends location data to M2X server. Idealy a GPS device should be used here to read the cordinates, but for simplicity, we just use pre-set values here to show how to use the API.

License
=======

This library is released under the MIT license. See [`M2XMQTTClient/LICENSE`](M2XMQTTClient/LICENSE) for the terms.
