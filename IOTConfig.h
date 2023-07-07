typedef struct {
  char HEADER[2]{'\0','\0'};
  char SSID[HTTP_REQ_PARAM_VALUE_LENGTH];
  char PASS[HTTP_REQ_PARAM_VALUE_LENGTH];
  char DEVID[HTTP_REQ_PARAM_VALUE_LENGTH];
  char MQTT_IP[HTTP_REQ_PARAM_VALUE_LENGTH];
  char MQTT_PORT[HTTP_REQ_PARAM_VALUE_LENGTH];
  char MQTT_USER[HTTP_REQ_PARAM_VALUE_LENGTH];
  char MQTT_PASS[HTTP_REQ_PARAM_VALUE_LENGTH];
  char MQTT_TOPIC[HTTP_REQ_PARAM_VALUE_LENGTH];
  char SEND_SECS[HTTP_REQ_PARAM_VALUE_LENGTH];
} ConfigData;

typedef struct {
  ConfigData data;
  const PROGMEM char *names[9]{"SSID", "PASS", "DEVID", "MQTT_IP", "MQTT_PORT", "MQTT_USER", "MQTT_PASS", "MQTT_TOPIC", "SEND_SECS"};
  bool inited() {
    return (data.HEADER[0] == 'A' && data.HEADER[1] == 'K');
  }
  char* values(int i) {
    return (char*)this+sizeof(data.HEADER)+(HTTP_REQ_PARAM_VALUE_LENGTH*i);
  }
  byte num() {
    return sizeof(names)/sizeof(char*); 
  }
  void init() {
    data.HEADER[0] = 'A';
    data.HEADER[1] = 'K';
    copyString(F("Wifi-NAME"), data.SSID, HTTP_REQ_PARAM_VALUE_LENGTH); 
    copyString(F("Wifi-PASS"), data.PASS, HTTP_REQ_PARAM_VALUE_LENGTH);
    copyString(F("0001"), data.DEVID, HTTP_REQ_PARAM_VALUE_LENGTH);
    copyString(F("influx.host.comain"), data.MQTT_IP, 0);  // 192.168.1.111
    copyString(F("8883"), data.MQTT_PORT, 0); // nontls: 1883, tls: 8883
    copyString(F("sensor"), data.MQTT_USER, 0); 
    copyString(F("mqtt-password"), data.MQTT_PASS, 0);
    sprintf(data.MQTT_TOPIC, "sensors/%s\0", data.DEVID  );
    copyString(F("1"), data.SEND_SECS, 0);
  }
  int8_t indexOf(const char* name) {
    for(int i=0;i<num();i++) {
      if(strcmp(name, names[i]) == 0) {
        return i;
      }
    }
    return -1;
  }
  void set(int i, const char* value) {
    copyString(value, (char*)this+sizeof(data.HEADER)+(HTTP_REQ_PARAM_VALUE_LENGTH*i), HTTP_REQ_PARAM_VALUE_LENGTH);
  }
} Config;
