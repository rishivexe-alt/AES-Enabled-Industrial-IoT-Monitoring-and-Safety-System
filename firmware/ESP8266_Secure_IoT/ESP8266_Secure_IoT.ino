#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Crypto.h>
#include <AES.h>
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "YOUR_LAPTOP_IP";
const int mqtt_port = 1883;
const char* mqtt_topic = "iot/sensors";
#define DHT_PIN D2
#define DHT_TYPE DHT11
#define TRIG_PIN D1
#define ECHO_PIN D5
#define GAS_PIN A0
#define TOUCH_PIN D6
#define GREEN_LED D4
#define RED_LED D8
#define YELLOW_LED D7
#define BUZZER_PIN D0
DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient client(espClient);
const float TEMP_THRESHOLD = 35.0;
const float HUM_THRESHOLD  = 80.0;
const int   GAS_THRESHOLD  = 1000;
const float DIST_THRESHOLD = 6.0;
const int TOUCH_ACTIVE_STATE = HIGH;
byte aesKey[16] = {
  'Y','O','U','R',
  '_','1','2','8',
  '_','B','I','T',
  '_','K','E','Y'
};
AES128 aes;
bool dangerDetected = false;
bool buzzerSilenced = false;
void normalState()
{
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerSilenced = false;
}

void dangerState()
{
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  if (!buzzerSilenced)
  {
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else
  {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

float readDistance()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(
    ECHO_PIN,
    HIGH,
    30000
  );

  // Invalid / no echo
  if (duration == 0)
  {
    return -1.0;
  }

  float distance =
    duration * 0.0343 / 2.0;

  return distance;
}


String encryptPacket(String plaintext)
{
  int len = plaintext.length();

  int paddedLength =
    ((len / 16) + 1) * 16;

  if (paddedLength > 192)
  {
    Serial.println(
      "ERROR: Packet too large!"
    );

    return "";
  }


  byte iv[16];

  for (int i = 0; i < 16; i++)
  {
    iv[i] = random(0, 256);
  }


  byte input[192];

  for (int i = 0; i < len; i++)
  {
    input[i] = plaintext[i];
  }

  byte pad =
    paddedLength - len;

  for (int i = len;
       i < paddedLength;
       i++)
  {
    input[i] = pad;
  }



  byte output[192];

  aes.setKey(
    aesKey,
    16
  );

  byte currentIV[16];

  memcpy(
    currentIV,
    iv,
    16
  );


  for (
    int block = 0;
    block < paddedLength;
    block += 16
  )
  {
    byte blockData[16];



    for (int i = 0; i < 16; i++)
    {
      blockData[i] =
        input[block + i]
        ^
        currentIV[i];
    }

    aes.encryptBlock(
      output + block,
      blockData
    );


    memcpy(
      currentIV,
      output + block,
      16
    );
  }



  byte combined[208];

  memcpy(
    combined,
    iv,
    16
  );

  memcpy(
    combined + 16,
    output,
    paddedLength
  );



  const char base64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

  String encoded = "";

  int totalLength =
    16 + paddedLength;


  for (
    int i = 0;
    i < totalLength;
    i += 3
  )
  {
    unsigned long value = 0;

    int remaining =
      totalLength - i;

    value |=
      ((unsigned long)combined[i])
      << 16;

    if (remaining > 1)
    {
      value |=
        ((unsigned long)combined[i + 1])
        << 8;
    }

    if (remaining > 2)
    {
      value |= combined[i + 2];
    }


    encoded +=
      base64Table[
        (value >> 18) & 0x3F
      ];

    encoded +=
      base64Table[
        (value >> 12) & 0x3F
      ];


    if (remaining > 1)
    {
      encoded +=
        base64Table[
          (value >> 6) & 0x3F
        ];
    }
    else
    {
      encoded += '=';
    }


    if (remaining > 2)
    {
      encoded +=
        base64Table[
          value & 0x3F
        ];
    }
    else
    {
      encoded += '=';
    }
  }


  return encoded;
}



void setupWiFi()
{
  Serial.println();
  Serial.println(
    "Connecting to WiFi..."
  );

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    ssid,
    password
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 30
  )
  {
    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();


  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(
      "WiFi connected"
    );

    Serial.print(
      "ESP8266 IP: "
    );

    Serial.println(
      WiFi.localIP()
    );

    Serial.print(
      "Laptop MQTT IP: "
    );

    Serial.println(
      mqtt_server
    );
  }
  else
  {
    Serial.println(
      "WiFi connection FAILED"
    );
  }
}



void reconnectMQTT()
{
  while (!client.connected())
  {
    Serial.println();
    Serial.println(
      "Connecting to MQTT broker..."
    );


    String clientID =
      "ESP8266_Sensor_";

    clientID +=
      String(
        ESP.getChipId(),
        HEX
      );


    if (
      client.connect(
        clientID.c_str()
      )
    )
    {
      Serial.println(
        "MQTT CONNECTION SUCCESS"
      );

      Serial.print(
        "MQTT Topic: "
      );

      Serial.println(
        mqtt_topic
      );


      // Test MQTT message
      String testMessage =
        "ESP8266_MQTT_TEST";


      if (
        client.publish(
          mqtt_topic,
          testMessage.c_str()
        )
      )
      {
        Serial.println(
          "MQTT TEST PUBLISH SUCCESS"
        );
      }
      else
      {
        Serial.println(
          "MQTT TEST PUBLISH FAILED"
        );
      }
    }
    else
    {
      Serial.print(
        "MQTT connection failed, rc="
      );

      Serial.println(
        client.state()
      );

      Serial.println(
        "Retrying in 3 seconds..."
      );

      delay(3000);
    }
  }
}


void setup()
{
  Serial.begin(115200);

  delay(1000);


  pinMode(
    TRIG_PIN,
    OUTPUT
  );

  pinMode(
    ECHO_PIN,
    INPUT
  );

  pinMode(
    TOUCH_PIN,
    INPUT
  );

  pinMode(
    GREEN_LED,
    OUTPUT
  );

  pinMode(
    RED_LED,
    OUTPUT
  );

  pinMode(
    YELLOW_LED,
    OUTPUT
  );

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  digitalWrite(
    TRIG_PIN,
    LOW
  );

  digitalWrite(
    GREEN_LED,
    LOW
  );

  digitalWrite(
    RED_LED,
    LOW
  );

  digitalWrite(
    YELLOW_LED,
    LOW
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );



  dht.begin();


  randomSeed(
    micros() ^
    analogRead(A0)
  );


  Serial.println();

  Serial.println(
    "===================================="
  );

  Serial.println(
    "       SECURE IoT SENSOR NODE"
  );

  Serial.println(
    "       ESP8266 + AES-128 + MQTT"
  );

  Serial.println(
    "===================================="
  );


  Serial.println();
  Serial.println(
    "THRESHOLDS:"
  );

  Serial.print(
    "Temperature > "
  );

  Serial.print(
    TEMP_THRESHOLD
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Humidity > "
  );

  Serial.print(
    HUM_THRESHOLD
  );

  Serial.println(
    " %"
  );


  Serial.print(
    "Gas > "
  );

  Serial.println(
    GAS_THRESHOLD
  );


  Serial.print(
    "Distance < "
  );

  Serial.print(
    DIST_THRESHOLD
  );

  Serial.println(
    " cm"
  );



  normalState();

  setupWiFi();


  client.setServer(
    mqtt_server,
    mqtt_port
  );


  Serial.println();

  Serial.println(
    "System initialization complete."
  );

  Serial.println(
    "===================================="
  );
}


void loop()
{


  if (!client.connected())
  {
    reconnectMQTT();
  }

  client.loop();



  float temperature =
    dht.readTemperature();

  float humidity =
    dht.readHumidity();



  int gasLevel =
    analogRead(GAS_PIN);


  float distance =
    readDistance();



  bool touched =
    (
      digitalRead(TOUCH_PIN)
      ==
      TOUCH_ACTIVE_STATE
    );



  if (
    isnan(temperature) ||
    isnan(humidity)
  )
  {
    Serial.println(
      "DHT11 read failed!"
    );

    delay(3000);

    return;
  }



  bool temperatureDanger =
    temperature > TEMP_THRESHOLD;

  bool humidityDanger =
    humidity > HUM_THRESHOLD;

  bool gasDanger =
    gasLevel > GAS_THRESHOLD;

  bool distanceDanger =
    (
      distance > 0 &&
      distance < DIST_THRESHOLD
    );



  dangerDetected =
    temperatureDanger ||
    humidityDanger ||
    gasDanger ||
    distanceDanger;


  if (
    dangerDetected &&
    touched
  )
  {
    buzzerSilenced = true;

    Serial.println();

    Serial.println(
      ">>> TOUCH DETECTED <<<"
    );

    Serial.println(
      ">>> BUZZER SILENCED <<<"
    );
  }


  if (dangerDetected)
  {
    dangerState();
  }
  else
  {
    normalState();
  }



  String status;

  if (dangerDetected)
  {
    status = "INTRUSION";
  }
  else
  {
    status = "NORMAL";
  }


  String json = "{";

  json +=
    "\"device_id\":\"ESP8266_01\",";

  json +=
    "\"temperature\":" +
    String(temperature, 1) +
    ",";

  json +=
    "\"humidity\":" +
    String(humidity, 1) +
    ",";

  json +=
    "\"gas_level\":" +
    String(gasLevel) +
    ",";

  json +=
    "\"distance\":" +
    String(distance, 1) +
    ",";

  json +=
    "\"status\":\"" +
    status +
    "\"";

  json += "}";



  String encrypted =
    encryptPacket(json);


  if (
    encrypted.length() == 0
  )
  {
    Serial.println(
      "AES encryption failed!"
    );

    delay(4000);

    return;
  }



  Serial.println();

  Serial.println(
    "===================================="
  );

  Serial.println(
    "          SENSOR DATA"
  );

  Serial.println(
    "------------------------------------"
  );


  Serial.print(
    "Temperature : "
  );

  Serial.print(
    temperature,
    1
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Humidity    : "
  );

  Serial.print(
    humidity,
    1
  );

  Serial.println(
    " %"
  );


  Serial.print(
    "Gas Level   : "
  );

  Serial.println(
    gasLevel
  );


  Serial.print(
    "Distance    : "
  );

  Serial.print(
    distance,
    1
  );

  Serial.println(
    " cm"
  );


  Serial.print(
    "Status      : "
  );

  Serial.println(
    status
  );


  Serial.println(
    "------------------------------------"
  );


  Serial.println(
    "ORIGINAL JSON:"
  );

  Serial.println(
    json
  );


  Serial.println(
    "------------------------------------"
  );


  Serial.println(
    "AES-128 ENCRYPTED + BASE64:"
  );

  Serial.println(
    encrypted
  );


  Serial.println(
    "------------------------------------"
  );


  // ----------------------------------------------------------
  // MQTT PUBLISH
  // ----------------------------------------------------------

  bool published =
    client.publish(
      mqtt_topic,
      encrypted.c_str()
    );


  if (published)
  {
    Serial.println(
      "MQTT PUBLISH SUCCESS"
    );
  }
  else
  {
    Serial.println(
      "MQTT PUBLISH FAILED"
    );
  }


  Serial.println(
    "===================================="
  );


  delay(4000);
}
