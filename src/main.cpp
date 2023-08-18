#include <Adafruit_NeoPixel.h>
#include "WiFiProv.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "ArduinoJSON.h"
// #include <esp32fota.h>
#include "Preferences.h"

#include "nvs.h"
#include "nvs_flash.h"

#define BUTTON 3
#define LED 2
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);
Preferences prefs;

String serverName = "https://light.nftunlock.com/api/light?id=3";

int disconnectCount = 0;
bool provisioning = false;

void runProvision()
{
  provisioning = true;
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_0, "0xtoohigh", "Prov_GasLight_3");
}

void setupNVS()
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void SysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id)
  {
  case WIFI_PROV_CRED_FAIL:
  {
    Serial1.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");

    WiFi.disconnect(true, true);
    ESP.restart();
    break;
  }

  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    Serial1.print("\nConnected IP address : ");
    Serial1.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    Serial1.println("\nDisconnected. Connecting to the AP again... ");
    disconnectCount += 1;
    if (disconnectCount > 4)
    {
      Serial1.println("provisioning");
      WiFi.disconnect(true, true);
      runProvision();
    }
    break;
  case ARDUINO_EVENT_PROV_START:
    Serial1.println("\nProvisioning started\nGive Credentials of your access point using \" Android app \"");
    provisioning = true;
    break;
  case ARDUINO_EVENT_PROV_CRED_RECV:
  {
    Serial1.println("\nReceived Wi-Fi credentials");
    Serial1.print("\tSSID : ");
    Serial1.println((const char *)sys_event->event_info.prov_cred_recv.ssid);
    Serial1.print("\tPassword : ");
    Serial1.println((char const *)sys_event->event_info.prov_cred_recv.password);
    Serial1.println("saving login prefs");
    prefs.begin("wifi");
    prefs.putString("ssid", (const char *)sys_event->event_info.prov_cred_recv.ssid);
    prefs.putString("pass", (const char *)sys_event->event_info.prov_cred_recv.password);
    prefs.end();
    // WiFi.begin((char const *)sys_event->event_info.prov_cred_recv.ssid, (char const *)sys_event->event_info.prov_cred_recv.password);
    break;
  }
  case ARDUINO_EVENT_PROV_CRED_FAIL:
  {
    provisioning = false;
    Serial1.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
    if (sys_event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)
      Serial1.println("\nWi-Fi AP password incorrect");
    else
      Serial1.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
    break;
  }
  case ARDUINO_EVENT_PROV_CRED_SUCCESS:
    provisioning = false;
    Serial1.println("\nProvisioning Successful");
    break;
  case ARDUINO_EVENT_PROV_END:
    provisioning = false;
    Serial1.println("\nProvisioning Ends");
    break;
  default:
    break;
  }
}

#define PWM_FREQ 1000
#define PWM_RES 10

double lastTime;
// double timerDelay = 30000;
double timerDelay = 10000;

// esp32FOTA esp32FOTA("esp32-fota-http", 1, false, true);

void provisionSetup()
{
  // runProvision();
  // return;

  prefs.begin("wifi");
  String ssid = prefs.getString("ssid");
  String pass = prefs.getString("pass");

  if (!ssid.length())
  {
    runProvision();
  }
  else
  {
    prefs.end();
    Serial1.println("has settings");
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
}

void setup()
{
  Serial1.begin(115200);
  Serial1.println("HLO");
  pixels.begin();
  pinMode(BUTTON, INPUT_PULLUP);

  // esp32FOTA.checkURL = "https://gas-light.iain.in/updates/firmware.json";

  ledcAttachPin(5, 1);
  ledcAttachPin(6, 2);
  ledcAttachPin(7, 3);
  ledcAttachPin(8, 4);
  ledcSetup(1, PWM_FREQ, PWM_RES);
  ledcSetup(2, PWM_FREQ, PWM_RES);
  ledcSetup(3, PWM_FREQ, PWM_RES);
  ledcSetup(4, PWM_FREQ, PWM_RES);

  WiFi.onEvent(SysProvEvent);
  pixels.setPixelColor(0, pixels.Color(0, 0, 100));
  pixels.show();
  provisionSetup();
  pixels.setPixelColor(0, pixels.Color(100, 100, 100));
}

int lastState = HIGH;
int currentState;

int buttonCount = 1;

void turnOffLamps()
{
  for (int i = 0; i <= 4; i++)
  {
    ledcWrite(i, 0);
  }
}

void handleNetwork()
{
  if ((millis() - lastTime) > timerDelay)
  {
    if (disconnectCount > 2 && WiFi.status() != WL_CONNECTED)
    {
      runProvision();
    }
    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {
      disconnectCount = 0;
      pixels.setPixelColor(0, pixels.Color(100, 100, 0));
      pixels.show();
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(serverName.c_str());

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode == 200)
      {
        String responseText = http.getString();

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, responseText);

        Serial1.print("HTTP Response code: ");
        Serial1.println(httpResponseCode);

        // String payload = http.getString();
        // String gasLevel = http.header("x-gas-level");

        double displayR = doc["display"]["r"];
        double displayY = doc["display"]["y"];
        double displayG = doc["display"]["g"];
        double aux = doc["display"]["aux"];

        ledcWrite(1, displayR);
        ledcWrite(2, displayY);
        ledcWrite(3, displayG);
        ledcWrite(4, aux);

        if (aux > 0)
        {
          delay(200);
          ledcWrite(4, 0);
        }

        pixels.setPixelColor(0, pixels.Color(100, 100, 0));
        pixels.show();
        delay(2000);
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
        pixels.show();
      }
      else
      {
        Serial1.print("Error code: ");
        Serial1.println(httpResponseCode);
        turnOffLamps();
        for (int ii = 0; ii < 10; ii++)
        {
          ledcWrite(1, 1000);
          delay(1000);
          ledcWrite(1, 400);
        }
      }
      // Free resources
      http.end();
    }
    else if (!provisioning)
    {
      turnOffLamps();
      Serial1.println("WiFi Disconnected");
      // disconnectCount += 1;
      pixels.setPixelColor(0, pixels.Color(100, 0, 0));
      pixels.show();
      // if (disconnectCount > 6) {
      // Attempting reconnect
      WiFi.reconnect();
      Serial1.println("Attempting reconnection");
      WiFi.disconnect(true);
      // delay(100);
      ESP.restart();
      // }
    }
    lastTime = millis();
  }
}

void setLed()
{
  pixels.clear();
  turnOffLamps();
  switch (buttonCount)
  {
  case 1:
    break;
  case 2:
    pixels.setPixelColor(0, pixels.Color(100, 0, 0));
    ledcWrite(1, 1000);
    break;
  case 3:
    pixels.setPixelColor(0, pixels.Color(100, 100, 0));
    ledcWrite(2, 1000);
    break;
  case 4:
    pixels.setPixelColor(0, pixels.Color(0, 100, 0));
    ledcWrite(3, 1000);
    break;
  case 5:
    pixels.setPixelColor(0, pixels.Color(0, 100, 0));
    ledcWrite(4, 1000);
    break;
  default:
    break;
  }
  pixels.show();
}

bool checkedFirmwareUpdate = false;

void loop()
{
  if (!checkedFirmwareUpdate && WiFi.status() == WL_CONNECTED)
  {
    pixels.setPixelColor(0, pixels.Color(0, 100, 255));
    pixels.show();
    // bool updateNeeded = esp32FOTA.execHTTPcheck();
    // if (updateNeeded)
    // {
    //   esp32FOTA.execOTA();
    // }
    pixels.setPixelColor(0, pixels.Color(0, 255, 0));
    pixels.show();
    checkedFirmwareUpdate = true;
  }
  currentState = digitalRead(BUTTON);
  if (lastState == LOW && currentState == HIGH)
  {
    Serial1.println("Button Pressed!");
    buttonCount++;
    if (buttonCount > 4)
    {
      buttonCount = 1;
    }
    setLed();
  }
  lastState = currentState;
  handleNetwork();
}
