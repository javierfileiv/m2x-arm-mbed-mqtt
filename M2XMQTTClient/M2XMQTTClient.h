#ifndef M2XMQTTCLIENT_H_
#define M2XMQTTCLIENT_H_

#if (!defined(MBED_PLATFORM)) && (!defined(LINUX_PLATFORM))
#error "Platform definition is missing!"
#endif

#define M2X_VERSION "0.1.0"

#ifdef MBED_PLATFORM
#include "m2x-mbed.h"
#endif /* MBED_PLATFORM */

#ifdef LINUX_PLATFORM
#include "m2x-linux.h"
#endif /* LINUX_PLATFORM */

/* If we don't have DBG defined, provide dump implementation */
#ifndef DBG
#define DBG(fmt_, data_)
#define DBGLN(fmt_, data_)
#define DBGLNEND
#endif  /* DBG */

#define MIN(a, b) (((a) > (b))?(b):(a))
#define TO_HEX(t_) ((char) (((t_) > 9) ? ((t_) - 10 + 'A') : ((t_) + '0')))
#define MAX_DOUBLE_DIGITS 7

/* For tolower */
#include <ctype.h>

#include "minimal-mqtt-static.h"
#include "minimal-json.h"

static const int E_OK = 0;
static const int E_NOCONNECTION = -1;
static const int E_DISCONNECTED = -2;
static const int E_NOTREACHABLE = -3;
static const int E_INVALID = -4;
static const int E_JSON_INVALID = -5;
static const int E_BUFFER_TOO_SMALL = -6;
static const int E_TIMESTAMP_ERROR = -8;

static const char* DEFAULT_M2X_HOST = "api-m2x.att.com";
static const int DEFAULT_M2X_PORT = 1883;

static inline bool m2x_status_is_success(int status) {
  return (status == E_OK) || (status >= 200 && status <= 299);
}

static inline bool m2x_status_is_client_error(int status) {
  return status >= 400 && status <= 499;
}

static inline bool m2x_status_is_server_error(int status) {
  return status >= 500 && status <= 599;
}

static inline bool m2x_status_is_error(int status) {
  return m2x_status_is_client_error(status) ||
      m2x_status_is_server_error(status);
}

// Null Print class used to calculate length to print
class NullPrint : public Print {
public:
  size_t counter;

  virtual size_t write(uint8_t b) {
    counter++;
    return 1;
  }

  virtual size_t write(const uint8_t* buf, size_t size) {
    counter += size;
    return size;
  }
};

// Handy helper class for printing MQTT payload using a Print
class MMQTTPrint : public Print {
public:
  mmqtt_connection *connection;
  mmqtt_s_puller puller;

  virtual size_t write(uint8_t b) {
    return write(&b, 1);
  }

  virtual size_t write(const uint8_t* buf, size_t size) {
    return mmqtt_s_encode_buffer(connection, puller, buf, size) == MMQTT_STATUS_OK ? size : -1;
  }
};

class M2XMQTTClient {
public:
  M2XMQTTClient(Client* client,
                const char* key,
                void (* idlefunc)(void) = NULL,
                bool keepalive = true,
                const char* host = DEFAULT_M2X_HOST,
                int port = DEFAULT_M2X_PORT,
                const char* path_prefix = NULL);

  // Push data stream value using PUT request, returns the HTTP status code
  // NOTE: if you want to update by a serial, use "serial/<serial ID>" as
  // the device ID here.
  template <class T>
  int updateStreamValue(const char* deviceId, const char* streamName, T value);

  // Following fields are public so mmqtt callback functions can access directly
  Client* _client;
private:
  struct mmqtt_connection _connection;
  const char* _key;
  uint16_t _key_length;
  bool _connected;
  bool _keepalive;
  const char* _host;
  int _port;
  void (* _idlefunc)(void);
  const char* _path_prefix;
  NullPrint _null_print;
  MMQTTPrint _mmqtt_print;
  int16_t _current_id;

  int connectToServer();

  template <class T>
  int writeUpdateStreamValuePayload(Print* print, const char* deviceId,
                                    const char* streamName, T value);

  int readStatusCode();
  void close();
};

// Implementations
M2XMQTTClient::M2XMQTTClient(Client* client,
                             const char* key,
                             void (* idlefunc)(void),
                             bool keepalive,
                             const char* host,
                             int port,
                             const char* path_prefix) : _client(client),
                                                        _key(key),
                                                        _idlefunc(idlefunc),
                                                        _keepalive(keepalive),
                                                        _connected(false),
                                                        _host(host),
                                                        _port(port),
                                                        _path_prefix(path_prefix),
                                                        _null_print(),
                                                        _mmqtt_print(),
                                                        _current_id(0) {
  _key_length = strlen(_key);
}

mmqtt_status_t m2x_mmqtt_puller(struct mmqtt_connection *connection) {
  const uint8_t *data = NULL;
  mmqtt_ssize_t length = 0;
  mmqtt_status_t status;
  M2XMQTTClient *client = (M2XMQTTClient *) connection->connection;
  Client *c = client->_client;
  struct mmqtt_stream *stream = mmqtt_connection_pullable_stream(connection);
  if (stream == NULL) { return MMQTT_STATUS_NOT_PULLABLE; }

  status = mmqtt_stream_external_pullable(stream, &data, &length);
  if (status != MMQTT_STATUS_OK) { return status; }

  length = c->write(data, length);
  if (length < 0) {
    c->stop();
    return MMQTT_STATUS_BROKEN_CONNECTION;
  }
  mmqtt_stream_external_pull(stream, length);
  if (mmqtt_stream_running(stream) == MMQTT_STATUS_DONE) {
    mmqtt_connection_release_write_stream(connection, stream);
  }
  return MMQTT_STATUS_OK;
}

mmqtt_status_t m2x_mmqtt_pusher(struct mmqtt_connection *connection) {
  uint8_t *data = NULL;
  mmqtt_ssize_t length = 0, i = 0;
  mmqtt_status_t status;
  M2XMQTTClient *client = (M2XMQTTClient *) connection->connection;
  Client *c = client->_client;
  struct mmqtt_stream *stream = mmqtt_connection_pushable_stream(connection);
  if (stream == NULL) { return MMQTT_STATUS_NOT_PUSHABLE; }

  status = mmqtt_stream_external_pushable(stream, &data, &length);
  if (status != MMQTT_STATUS_OK) { return status; }

  /* Maybe we need another field in signature documenting how much data we want,
   * so we can handle end condition gracefully? Not 100% if `left` field in
   * mmqtt_stream is enough
   */
  while (i < length && c->available()) {
    data[i++] = c->read();
  }
  mmqtt_stream_external_push(stream, i);
  return MMQTT_STATUS_OK;
}

size_t m2x_mjson_reader(struct mjson_ctx *ctx, char *data, size_t limit)
{
  mmqtt_connection *connection = (mmqtt_connection *) ctx->userdata;
  return mmqtt_s_decode_buffer(connection, m2x_mmqtt_pusher, (uint8_t *) data, limit,
                               limit) == MMQTT_STATUS_OK ? limit : 0;
}

int M2XMQTTClient::connectToServer() {
  mmqtt_status_t status;
  struct mmqtt_p_connect_header connect_header;
  struct mmqtt_p_connack_header connack_header;
  uint32_t packet_length;
  uint8_t flag;
  uint16_t length;
  uint8_t name[4];

  if (_client->connect(_host, _port)) {
    DBGLN("%s", F("Connected to M2X MQTT server!"));
    mmqtt_connection_init(&_connection, this);
    /* Send CONNECT packet first */
    connect_header.name = name;
    strncpy((char *)name, F("MQTT"), 4);
    connect_header.name_length = connect_header.name_max_length = 4;
    connect_header.protocol_version = 3;
    /* Clean session with username set */
    connect_header.flags = 0x82;
    connect_header.keepalive = 60;
    packet_length = mmqtt_s_connect_header_encoded_length(&connect_header) +
                    mmqtt_s_string_encoded_length(_key_length);
    status = mmqtt_s_encode_fixed_header(&_connection, m2x_mmqtt_puller,
                                         MMQTT_PACK_MESSAGE_TYPE(MMQTT_MESSAGE_TYPE_CONNECT),
                                         packet_length);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error sending connect packet fixed header: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    status = mmqtt_s_encode_connect_header(&_connection, m2x_mmqtt_puller, &connect_header);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error sending connect packet variable header: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    status = mmqtt_s_encode_string(&_connection, m2x_mmqtt_puller,
                                   (const uint8_t *) _key, _key_length);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error sending connect packet payload: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    /* Check CONNACK packet */
    do {
      status = mmqtt_s_decode_fixed_header(&_connection, m2x_mmqtt_pusher,
                                           &flag, &packet_length);
      if (status != MMQTT_STATUS_OK) {
        DBG("%s", F("Error decoding connack fixed header: "));
        DBGLN("%d", status);
        _client->stop();
        return E_DISCONNECTED;
      }
      if (MMQTT_UNPACK_MESSAGE_TYPE(flag) != MMQTT_MESSAGE_TYPE_CONNACK) {
        status = mmqtt_s_skip_buffer(&_connection, m2x_mmqtt_pusher, packet_length);
        if (status != MMQTT_STATUS_OK) {
          DBG("%s", F("Error skipping non-connack packet: "));
          DBGLN("%d", status);
          _client->stop();
          return E_DISCONNECTED;
        }
      }
    } while (MMQTT_UNPACK_MESSAGE_TYPE(flag) != MMQTT_MESSAGE_TYPE_CONNACK);
    status = mmqtt_s_decode_connack_header(&_connection, m2x_mmqtt_pusher,
                                           &connack_header);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error decoding connack variable header: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    if (connack_header.return_code != 0x0) {
      DBG("%s", F("CONNACK return code is not accepted: "));
      DBGLN("%d", connack_header.return_code);
      _client->stop();
      return E_DISCONNECTED;
    }
    /* Send SUBSCRIBE packet*/
    length = _key_length + 15 + 2;
    status = mmqtt_s_encode_fixed_header(&_connection, m2x_mmqtt_puller,
                                         MMQTT_PACK_MESSAGE_TYPE(MMQTT_MESSAGE_TYPE_SUBSCRIBE),
                                         length);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error sending subscribe packet fixed header: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    mmqtt_s_encode_uint16(&_connection, m2x_mmqtt_puller, length);
    mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, (const uint8_t *) F("m2x/"), 4);
    mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, (const uint8_t *) _key, _key_length);
    // The extra one is QoS, added here to save a function call
    mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, (const uint8_t *) F("/responses\0"), 11);
    /* Check SUBACK packet */
    do {
      status = mmqtt_s_decode_fixed_header(&_connection, m2x_mmqtt_pusher,
                                           &flag, &packet_length);
      if (status != MMQTT_STATUS_OK) {
        DBG("%s", F("Error decoding suback fixed header: "));
        DBGLN("%d", status);
        _client->stop();
        return E_DISCONNECTED;
      }
      if (MMQTT_UNPACK_MESSAGE_TYPE(flag) != MMQTT_MESSAGE_TYPE_SUBACK) {
        status = mmqtt_s_skip_buffer(&_connection, m2x_mmqtt_pusher, packet_length);
        if (status != MMQTT_STATUS_OK) {
          DBG("%s", F("Error skipping packet: "));
          DBGLN("%d", status);
          _client->stop();
          return E_DISCONNECTED;
        }
      }
    } while (MMQTT_UNPACK_MESSAGE_TYPE(flag) != MMQTT_MESSAGE_TYPE_SUBACK);
    status = mmqtt_s_skip_buffer(&_connection, m2x_mmqtt_pusher, packet_length);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error skipping suback packet: "));
      DBGLN("%d", status);
      _client->stop();
      return E_DISCONNECTED;
    }
    _mmqtt_print.connection = &_connection;
    _mmqtt_print.puller = m2x_mmqtt_puller;
    _connected = true;
    return E_OK;
  } else {
    DBGLN("%s", F("ERROR: Cannot connect to M2X MQTT server!"));
    return E_NOCONNECTION;
  }
}

template <class T>
int M2XMQTTClient::updateStreamValue(const char* deviceId, const char* streamName, T value) {
  int length;
  if (!_connected) {
    if (connectToServer() != E_OK) {
      DBGLN("%s", "ERROR: Cannot connect to M2X server!");
      return E_NOCONNECTION;
    }
  }
  length = writeUpdateStreamValuePayload(&_null_print, deviceId, streamName, value);
  mmqtt_s_encode_fixed_header(&_connection, m2x_mmqtt_puller,
                              MMQTT_PACK_MESSAGE_TYPE(MMQTT_MESSAGE_TYPE_PUBLISH),
                              length + _key_length + 15);
  mmqtt_s_encode_uint16(&_connection, m2x_mmqtt_puller, _key_length + 13);
  mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, F("m2x/"), 4);
  mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, _key, _key_length);
  mmqtt_s_encode_buffer(&_connection, m2x_mmqtt_puller, F("/requests"), 9);
  writeUpdateStreamValuePayload(&_mmqtt_print, deviceId, streamName, value);
  return readStatusCode();
}

template <class T>
int M2XMQTTClient::writeUpdateStreamValuePayload(Print* print, const char* deviceId,
                                                 const char* streamName, T value) {
  int bytes = 0;
  bytes += print->print(F("{\"id\":\""));
  bytes += print->print(_current_id);
  bytes += print->print(F("\",\"method\":\"POST\",\"resource\":\""));
  if (_path_prefix) { bytes += print->print(_path_prefix); }
  bytes += print->print(F("/v2/devices/"));
  bytes += print->print(deviceId);
  bytes += print->print(F("/streams/"));
  bytes += print->print(streamName);
  bytes += print->print(F("/values"));
  bytes += print->print(F("\",\"agent\":\""));
  bytes += print->print(USER_AGENT);
  bytes += print->print(F("\",\"body\":"));
  bytes += print->print(F("{\"value\":\""));
  bytes += print->print(value);
  bytes += print->print("\"}");
  bytes += print->print(F("}"));
}

int M2XMQTTClient::readStatusCode() {
  mmqtt_status_t status;
  uint32_t packet_length;
  uint8_t flag;
  int16_t parsed_id, response_status;
  struct mjson_ctx ctx;
  char buf[6];
  size_t buf_length;

  while (true) {
    status = mmqtt_s_decode_fixed_header(&_connection, m2x_mmqtt_pusher,
                                         &flag, &packet_length);
    if (status != MMQTT_STATUS_OK) {
      DBG("%s", F("Error decoding publish fixed header: "));
      DBGLN("%d", status);
      close();
      return E_DISCONNECTED;
    }
    if (MMQTT_UNPACK_MESSAGE_TYPE(flag) != MMQTT_MESSAGE_TYPE_PUBLISH) {
      status = mmqtt_s_skip_buffer(&_connection, m2x_mmqtt_pusher, packet_length);
      if (status != MMQTT_STATUS_OK) {
        DBG("%s", F("Error skipping non-publish packet: "));
        DBGLN("%d", status);
        close();
        return E_DISCONNECTED;
      }
    } else {
      /*
       * Since we only subscribe to one channel, there's no need to check channel name.
       */
      status = mmqtt_s_decode_string(&_connection, m2x_mmqtt_pusher, NULL, 0,
                                     NULL, NULL);
      if (status != MMQTT_STATUS_OK) {
        DBG("%s", F("Error skipping channel name string: "));
        DBGLN("%d", status);
        close();
        return E_DISCONNECTED;
      }
      /*
       * Parse JSON body for ID and status
       */
      mjson_init(&ctx, &_connection, m2x_mjson_reader);
      parsed_id = -1;
      if (mjson_readcheck_object_start(&ctx) != MJSON_OK) {
        /* Oops we have an error */
        DBG("%s", F("Publish Packet is not a JSON object!"));
        close();
        return E_DISCONNECTED;
      }
      while (mjson_read_object_separator_or_end(&ctx) != MJSON_SUBTYPE_OBJECT_END) {
        if (mjson_readcheck_string_start(&ctx) != MJSON_OK) {
          DBG("%s", F("Object key is not string!"));
          close();
          return E_DISCONNECTED;
        }
        mjson_read_full_string(&ctx, buf, 6, &buf_length);
        mjson_read_object_key_separator(&ctx);
        if (strncmp(buf, F("status"), 6) == 0) {
          mjson_read_int16(&ctx, &response_status);
        } else if (strncmp(buf, F("id"), 2) == 0) {
          /* Hack since we know the ID passed is actually an integer*/
          mjson_readcheck_string_start(&ctx);
          mjson_read_int16(&ctx, &parsed_id);
          mjson_read_string_end(&ctx);
        } else {
          mjson_skip_value(&ctx);
        }
      }
      if (parsed_id == _current_id) { return response_status; }
    }
  }
  return E_NOTREACHABLE;
}

void M2XMQTTClient::close() {
  _client->stop();
  _connected = false;
}

#endif  /* M2XMQTTCLIENT_H_ */
