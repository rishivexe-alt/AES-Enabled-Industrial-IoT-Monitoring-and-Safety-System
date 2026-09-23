#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#include <Crypto.h>
#include <AES.h>

// ============================================================
// SECURE IoT SENSOR NODE
// ESP8266 + DHT11 + MQ-135 + HC-SR04 + TOUCH
// AES-128-CBC + MQTT
// ============================================================


// ============================================================
// WIFI + MQTT CONFIGURATION
// ============================================================

// IMPORTANT:
// Do NOT commit real credentials to GitHub.
// Replace these locally before uploading to ESP8266.

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Change this to the laptop's current IPv4 address.
const char* mqtt_server = "YOUR_LAPTOP_IP";

const int mqtt_port = 1883;

const char* mqtt_topic = "iot/sensors";


// ============================================================
// PIN CONFIGURATION
// ============================================================

// DHT11
#define DHT_PIN D2
#define DHT_TYPE DHT11

// Ultrasonic
#define TRIG_PIN D1
#define ECHO_PIN D5

// MQ-135
#define GAS_PIN A0

// Touch sensor
#define TOUCH_PIN D6

// LEDs
#define GREEN_LED D4
#define RED_LED D8
#define YELLOW_LED D7

// Buzzer
#define BUZZER_PIN D0


// ============================================================
// SENSOR OBJECTS
// ============================================================

DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient espClient;
PubSubClient client(espClient);


// ============================================================
// SAFETY THRESHOLDS
// ============================================================

const float TEMP_THRESHOLD = 35.0;
const float HUM_THRESHOLD  = 80.0;
const int   GAS_THRESHOLD  = 1000;

// IMPORTANT: 6 cm
const float DIST_THRESHOLD = 6.0;


// ============================================================
// TOUCH CONFIGURATION
// ============================================================

const int TOUCH_ACTIVE_STATE = HIGH;


// ============================================================
// AES-128 KEY
// ============================================================

// IMPORTANT:
// Do NOT publish the real shared key.
// Configure the same 16-byte key locally in the
// ESP8266 firmware and Python receiver.

byte aesKey[16] = {
  'Y','O','U','R',
  '_','1','2','8',
  '_','B','I','T',
  '_','K','E','Y'
};


// ============================================================
// AES OBJECT
// ============================================================

AES128 aes;


// ============================================================
// SYSTEM STATE
// ============================================================

bool dangerDetected = false;
bool buzzerSilenced = false;


// ============================================================
// NORMAL STATE
// ============================================================

void normalState()
{
  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);

  // Yellow LED represents normal/load state
  digitalWrite(YELLOW_LED, HIGH);

  digitalWrite(BUZZER_PIN, LOW);

  buzzerSilenced = false;
}


// ============================================================
// DANGER STATE
// ============================================================

void dangerState()
{
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  // Safety/load output OFF
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


// ============================================================
// ULTRASONIC DISTANCE
// ============================================================

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


// ============================================================
// AES CBC + PKCS#7 + BASE64
//
// FORMAT:
//
// [16 BYTE IV] + [CIPHERTEXT]
//
// Then Base64 encoded.
//
// This matches the corresponding Python
// AES-CBC receiver.
// ============================================================

String encryptPacket(String plaintext)
{
  int len = plaintext.length();

  // ----------------------------------------------------------
  // PKCS#7 PADDING
  // ----------------------------------------------------------

  int paddedLength =
    ((len / 16) + 1) * 16;

  if (paddedLength > 192)
  {
    Serial.println(
      "ERROR: Packet too large!"
    );

    return "";
  }


  // ----------------------------------------------------------
  // RANDOM IV
  // ----------------------------------------------------------

  byte iv[16];

  for (int i = 0; i < 16; i++)
  {
    iv[i] = random(0, 256);
  }


  // ----------------------------------------------------------
  // CREATE PADDED PLAINTEXT
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // AES-128 CBC ENCRYPTION
  // ----------------------------------------------------------

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

    // CBC:
    // blockData = plaintextBlock XOR previousCiphertext
    // First block uses IV.

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

    // Current ciphertext becomes
    // IV for the next block.

    memcpy(
      currentIV,
      output + block,
      16
    );
  }


  // ----------------------------------------------------------
  // IV + CIPHERTEXT
  // ----------------------------------------------------------

  byte combined[208];

  // First 16 bytes = IV
  memcpy(
    combined,
    iv,
    16
  );

  // Remaining bytes = ciphertext
  memcpy(
    combined + 16,
    output,
    paddedLength
  );


  // ----------------------------------------------------------
  // BASE64 ENCODING
  // ----------------------------------------------------------

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


// ============================================================
// WIFI SETUP
// ============================================================

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


// ============================================================
// MQTT RECONNECT
// ============================================================

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


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);


  // ----------------------------------------------------------
  // PIN INITIALIZATION
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // INITIAL OUTPUT STATE
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // DHT11
  // ----------------------------------------------------------

  dht.begin();


  // ----------------------------------------------------------
  // RANDOM SEED
  // ----------------------------------------------------------

  randomSeed(
    micros() ^
    analogRead(A0)
  );


  // ----------------------------------------------------------
  // HEADER
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // THRESHOLDS
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // NORMAL INITIAL STATE
  // ----------------------------------------------------------

  normalState();


  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  setupWiFi();


  // ----------------------------------------------------------
  // MQTT
  // ----------------------------------------------------------

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


// ============================================================
// MAIN LOOP
// ============================================================

void loop()
{
  // ----------------------------------------------------------
  // MQTT CONNECTION
  // ----------------------------------------------------------

  if (!client.connected())
  {
    reconnectMQTT();
  }

  client.loop();


  // ----------------------------------------------------------
  // READ DHT11
  // ----------------------------------------------------------

  float temperature =
    dht.readTemperature();

  float humidity =
    dht.readHumidity();


  // ----------------------------------------------------------
  // READ MQ-135
  // ----------------------------------------------------------

  int gasLevel =
    analogRead(GAS_PIN);


  // ----------------------------------------------------------
  // READ HC-SR04
  // ----------------------------------------------------------

  float distance =
    readDistance();


  // ----------------------------------------------------------
  // READ TOUCH
  // ----------------------------------------------------------

  bool touched =
    (
      digitalRead(TOUCH_PIN)
      ==
      TOUCH_ACTIVE_STATE
    );


  // ----------------------------------------------------------
  // DHT ERROR
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // SAFETY CONDITIONS
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // OVERALL DANGER
  // ----------------------------------------------------------

  dangerDetected =
    temperatureDanger ||
    humidityDanger ||
    gasDanger ||
    distanceDanger;


  // ----------------------------------------------------------
  // TOUCH / BUZZER SILENCING
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // OUTPUT CONTROL
  // ----------------------------------------------------------

  if (dangerDetected)
  {
    dangerState();
  }
  else
  {
    normalState();
  }


  // ----------------------------------------------------------
  // STATUS
  // ----------------------------------------------------------

  String status;

  if (dangerDetected)
  {
    status = "INTRUSION";
  }
  else
  {
    status = "NORMAL";
  }


  // ----------------------------------------------------------
  // JSON TELEMETRY
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // AES-128-CBC ENCRYPTION
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // SERIAL MONITOR
  // ----------------------------------------------------------

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
