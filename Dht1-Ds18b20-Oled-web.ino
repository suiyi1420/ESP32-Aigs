#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "images.h"
#include <DHT.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SPIFFS.h"
#include <WebSocketsClient.h>
#include <nvs.h>
#include <ArduinoJson.h>
#include "Update.h"

#define SCREEN_WIDTH 128 // OLED 宽像素
#define SCREEN_HEIGHT 64 // OLED 高像素
SSD1306Wire  display(0x3c, 21, 22);
OLEDDisplayUi ui     ( &display );
int frameCount = 4;
int overlaysCount = 1;
BH1750 lightMeter;
#define DHTPIN 27     // DHT11数据引脚，ESP32-CAM开发板引脚则为13
#define DHTTYPE    DHT11     // 温湿度传感器类型
DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
int contentLength = 0;
int port = 80; // Non https. For HTTPS 443. As of today, HTTPS doesn't work.
String Version = "v1.1.0.20200513";
#define QWPIN 25 //定义陶瓷灯继电器引脚
#define GZPIN 26//定义电灯继电器引脚
#define SDPIN 32//定义加湿器继电器引脚
#define SWPIN 33//定义加热棒继电器引脚
#define WSPIN 23//定义喂食器继电器引脚

#define ONE_WIRE_BUS 4  // DS18B20数据引脚，ESP32-CAM开发板引脚则为2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
size_t content_len;
nvs_handle handle;
File fsUploadFile;
static const char *NVS_CUSTOMER = "aigs";
const char* AP_SSID  = "aigs"; //热点名称
const char* AP_PASS  = "12345678";  //密码
const char* S_SSID  = "S_SSID"; //保存用wifi ssid地址
const char* S_PASS  = "S_PASS";  //保存用wifi password地址
const char* S_qiwenM  = "S_qiwenM";  //保存用wifi password地址
const char* S_shiduM  = "S_shiduM";  //保存用wifi password地址
const char* S_guangzhaoM  = "S_guangzhaoM";  //保存用wifi password地址
const char* S_shuiwenM  = "S_shuiwenM";  //保存用wifi password地址
WebSocketsClient webSocket;
AsyncWebServer server(80);
bool setWifi = false;
String mac; String ip;
struct MinMax {
  float value;
  int auto_value;//自动控制,0关闭，1打开
  float min_value;//启动值
  float max_value;//停止值
  int state;//控制器状态：0关闭，1打开
};
struct MinMax qiwenM = {0, 0, 26.0, 30.0, 0};
struct MinMax shiduM = {0, 0, 40.0, 75.0, 0};
struct MinMax guangzhaoM = {0, 0, 200.0, 500.0, 0};
struct MinMax shuiwenM = {0, 0, 24.0, 26.0, 0};
struct MinMax weishiM = {0, 0, 0, 0, 0};
String getData() {

  String getDataString = "{type:'getData',qiwen:'" + String(qiwenM.value) + "',guangzhao:'" + String(guangzhaoM.value) + "',shidu:'"
                         + String(shiduM.value) + "',shiuwen:'" + String(shuiwenM.value) + "',ip:'" + String(ip) + "',mac:'" + mac
                         + "',qiwenState:'" + String(qiwenM.state) + "',qiwenAuto:'" + String(qiwenM.auto_value)
                         + "',guangzhaoState:'" + String(guangzhaoM.state) + "',guangzhaoAuto:'" + String(guangzhaoM.auto_value)
                         + "',shiduState:'" + String(shiduM.state) + "',shiduAuto:'" + String(shiduM.auto_value)
                         + "',shuiwenState:'" + String(shuiwenM.state) + "',shuiwenAuto:'" + String(shuiwenM.auto_value)
                         + "',weishiState:'" + String(weishiM.state) + "',version:'" + Version+"'}";
  return getDataString;
}
String getMinMax(String typeString) {
  String minMax = "";
  if ( typeString == "qiwen")
    minMax = "{type:'getMinMax',datatype:'qiwen',min:'" + String(qiwenM.min_value) + "',max:'" + String(qiwenM.max_value) + "'}";
  else if ( typeString == "shidu")
    minMax = "{type:'getMinMax',datatype:'shidu',min:'" + String(shiduM.min_value) + "',max:'" + String(shiduM.max_value) + "'}";
  else if ( typeString == "guangzhao")
    minMax = "{type:'getMinMax',datatype:'guangzhao',min:'" + String(guangzhaoM.min_value) + "',max:'" + String(guangzhaoM.max_value) + "'}";
  else if ( typeString == "shuiwen")
    minMax = "{type:'getMinMax',datatype:'shuiwen',min:'" + String(shuiwenM.min_value) + "',max:'" + String(shuiwenM.max_value) + "'}";

  return minMax;
}
void setMinMax(String typeString, String minS, String maxS) {
  ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
  if (typeString == "qiwen") {
    qiwenM.min_value = atof(minS.c_str());
    qiwenM.max_value = atof(maxS.c_str());
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_qiwenM, &qiwenM, sizeof(qiwenM)) );
  } else if (typeString == "shidu") {
    shiduM.min_value = atof(minS.c_str());
    shiduM.max_value = atof(maxS.c_str());
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shiduM, &shiduM, sizeof(shiduM)) );
  } else if (typeString == "guangzhao") {
    guangzhaoM.min_value = atof(minS.c_str());
    guangzhaoM.max_value = atof(maxS.c_str());
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_guangzhaoM, &guangzhaoM, sizeof(guangzhaoM)) );
  } else if (typeString == "shuiwen") {
    shuiwenM.min_value = atof(minS.c_str());
    shuiwenM.max_value = atof(maxS.c_str());
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shuiwenM, &shuiwenM, sizeof(shuiwenM)) );
  }
  ESP_ERROR_CHECK( nvs_commit(handle) );
  nvs_close(handle);
}
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n", payload);

      // send message to server when Connected
      webSocket.sendTXT("Connected");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] Connected to url: %s\n", payload);
      DynamicJsonDocument jsonData(1024);
      deserializeJson(jsonData, payload);
      String s_type = jsonData["s_type"];
      if (s_type == "getMinMax") {
        String getMinMaxString = getMinMax(jsonData["type"]);
        webSocket.sendTXT(getMinMaxString);
      } else if (s_type == "changeState") {
        String stateString = jsonData["state"];
        changeState(jsonData["type"], stateString.toInt());
      } else if (s_type == "setAuto") {
        String stateString = jsonData["auto_value"];
        String typeString = jsonData["type"];
        Serial.printf("type: %s\n", typeString);
        Serial.printf("stateString: %s\n", stateString);
        setAuto(typeString, stateString.toInt());
      } else if (s_type == "setMinMax") {
        setMinMax(jsonData["type"], jsonData["min"], jsonData["max"]);
      } else if (s_type == "setWifi") {
        setWifi = true;
        String ssid = jsonData["ssid"];
        String password = jsonData["password"];
        char ssid2[20]; char password2[20];
        strcpy(ssid2, ssid.c_str());
        strcpy(password2, password.c_str());
        setWIFI(ssid2, password2);
      }else if (s_type == "restart") {
        ESP.restart();
      }
      break;

  }

}

void autoControl() {
  if (qiwenM.auto_value == 1) {
    if (qiwenM.value < qiwenM.min_value) {
      digitalWrite(QWPIN, HIGH);
      qiwenM.state = 1;
    }
    if (qiwenM.value > qiwenM.max_value) {
      digitalWrite(QWPIN, LOW);
      qiwenM.state = 0;
    }
  } else {
    if (qiwenM.state == 0) {
      digitalWrite(QWPIN, LOW);
    } else {
      digitalWrite(QWPIN, HIGH);
    }
  }
  if (guangzhaoM.auto_value == 1) {
    if (guangzhaoM.value < guangzhaoM.min_value) {
      digitalWrite(GZPIN, HIGH);
      guangzhaoM.state = 1;
    }
    if (guangzhaoM.value > guangzhaoM.max_value) {
      digitalWrite(GZPIN, LOW);
      guangzhaoM.state = 0;
    }
  } else {
    if (guangzhaoM.state == 0) {
      digitalWrite(GZPIN, LOW);
    } else {
      digitalWrite(GZPIN, HIGH);
    }
  }
  if (shiduM.auto_value == 1) {
    if (shiduM.value < shiduM.min_value) {
      digitalWrite(SDPIN, HIGH);
      shiduM.state = 1;
    }
    if (shiduM.value > shiduM.max_value) {
      digitalWrite(SDPIN, LOW);
      shiduM.state = 0;
    }
  } else {
    if (shiduM.state == 0) {
      digitalWrite(SDPIN, LOW);
    } else {
      digitalWrite(SDPIN, HIGH);
    }
  }
  if (shuiwenM.auto_value == 1) {
    if (shuiwenM.value < shuiwenM.min_value) {
      digitalWrite(SWPIN, HIGH);
      shuiwenM.state = 1;
    }
    if (shuiwenM.value > shuiwenM.max_value) {
      digitalWrite(SWPIN, LOW);
      shuiwenM.state = 0;
    }
  } else {
    if (shuiwenM.state == 0) {
      digitalWrite(SWPIN, LOW);
    } else {
      digitalWrite(SWPIN, HIGH);
    }
  }
}
void setAuto(String typeString, int state) {
  ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
  if (typeString == "qiwen") {
    qiwenM.auto_value = state;
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_qiwenM, &qiwenM, sizeof(qiwenM)) );
  } else if (typeString == "shidu") {
    shiduM.auto_value = state;
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shiduM, &shiduM, sizeof(shiduM)) );
  } else if (typeString == "guangzhao") {
    guangzhaoM.auto_value = state;
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_guangzhaoM, &guangzhaoM, sizeof(guangzhaoM)) );
  } else if (typeString == "shuiwen") {
    shuiwenM.auto_value = state;
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shuiwenM, &shuiwenM, sizeof(shuiwenM)) );
  }
  ESP_ERROR_CHECK( nvs_commit(handle) );
  nvs_close(handle);
}
void changeState(String typeString, int state) {
  ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
  if (typeString == "qiwen" && qiwenM.auto_value == 0) {
    if (state == 1) {
      digitalWrite(QWPIN, HIGH);
      qiwenM.state = 1;
    } else {
      digitalWrite(QWPIN, LOW);
      qiwenM.state = 0;
    }
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_qiwenM, &qiwenM, sizeof(qiwenM)) );
  } else if (typeString == "shidu" && shiduM.auto_value == 0) {
    if (state == 1) {
      digitalWrite(SDPIN, HIGH);
      shiduM.state = 1;
    } else {
      digitalWrite(SDPIN, LOW);
      shiduM.state = 0;
    }
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shiduM, &shiduM, sizeof(shiduM)) );
  } else if (typeString == "guangzhao" && guangzhaoM.auto_value == 0) {
    if (state == 1) {
      digitalWrite(GZPIN, HIGH);
      guangzhaoM.state = 1;
    } else {
      digitalWrite(GZPIN, LOW);
      guangzhaoM.state = 0;
    }
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_guangzhaoM, &guangzhaoM, sizeof(guangzhaoM)) );
  } else if (typeString == "shuiwen" && shuiwenM.auto_value == 0) {
    if (state == 1) {
      digitalWrite(SWPIN, HIGH);
      shuiwenM.state = 1;
    } else {
      digitalWrite(SWPIN, LOW);
      shuiwenM.state = 0;
    }
    ESP_ERROR_CHECK( nvs_set_blob( handle, S_shuiwenM, &shuiwenM, sizeof(shuiwenM)) );
  } else if (typeString == "weishi") {
    if (state == 1) {
      digitalWrite(WSPIN, HIGH);
      weishiM.state = 1;
    } else {
      digitalWrite(WSPIN, LOW);
      weishiM.state = 0;
    }
  }
  ESP_ERROR_CHECK( nvs_commit(handle) );
  nvs_close(handle);
}
bool setWIFI(char* ssid, char* password) {
  WiFi.begin(ssid, password);
  int num = 0;
  bool isLink = true;
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    num++;
    if (num >= 20) {
      isLink = false;
      break;
    }
  }
  Serial.println(ssid);
  Serial.println(password);
  // Print ESP Local IP Address
  if (isLink) {
    Serial.println(WiFi.localIP());
    ip = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
    ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
    ESP_ERROR_CHECK( nvs_set_str( handle, S_SSID, ssid) );
    ESP_ERROR_CHECK( nvs_set_str( handle, S_PASS, password) );
    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);
  }
  setWifi = false;
  return isLink;
}
void firstDataSet() {
  struct MinMax s_qiwenM, s_shiduM, s_guangzhaoM, s_shuiwenM;
  uint32_t len = sizeof(qiwenM);
  ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
  if (nvs_get_blob (handle, S_qiwenM, &s_qiwenM, &len) == ESP_OK) {
    qiwenM.auto_value = s_qiwenM.auto_value;
    qiwenM.min_value = s_qiwenM.min_value;
    qiwenM.max_value = s_qiwenM.max_value;
    qiwenM.state = s_qiwenM.state;
  }
  if (nvs_get_blob (handle, S_shiduM, &s_shiduM, &len) == ESP_OK) {
    shiduM.auto_value = s_shiduM.auto_value;
    shiduM.min_value = s_shiduM.min_value;
    shiduM.max_value = s_shiduM.max_value;
    shiduM.state = s_shiduM.state;
  }
  if (nvs_get_blob (handle, S_guangzhaoM, &s_guangzhaoM, &len) == ESP_OK) {
    guangzhaoM.auto_value = s_guangzhaoM.auto_value;
    guangzhaoM.min_value = s_guangzhaoM.min_value;
    guangzhaoM.max_value = s_guangzhaoM.max_value;
    guangzhaoM.state = s_guangzhaoM.state;
  }
  if (nvs_get_blob (handle, S_shuiwenM, &s_shuiwenM, &len) == ESP_OK) {
    shuiwenM.auto_value = s_shuiwenM.auto_value;
    shuiwenM.min_value = s_shuiwenM.min_value;
    shuiwenM.max_value = s_shuiwenM.max_value;
    shuiwenM.state = s_shuiwenM.state;
  }
  nvs_close(handle);
}
void InitWifi() {
  char sid[30]; char pasd[30];
  uint32_t str_length = 32;
  ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
  if (nvs_get_str (handle, S_SSID, sid, &str_length) == ESP_OK && nvs_get_str (handle, S_PASS, pasd, &str_length) == ESP_OK) {
    Serial.print("sid:");
    Serial.println(sid);
    Serial.print("pasd:");
    Serial.println(pasd);
    WiFi.begin(sid, pasd);
    int num = 0;
    bool isLink = true;
    Serial.println("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      num++;
      if (num >= 20) {
        isLink = false;
        break;
      }
    }
    // Print ESP Local IP Address
    if (isLink) {
      Serial.println(WiFi.localIP());
      ip = String() + WiFi.localIP()[0] + "." + WiFi.localIP()[1] + "." + WiFi.localIP()[2] + "." + WiFi.localIP()[3];
    }
    setWifi = false;
  }
  nvs_close(handle);
}

void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(0, 0, mac);
  display->drawString(0, 54, ip);
}

void drawLogo(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // draw an xbm image.
  // Please note that everything that should be transitioned
  // needs to be drawn relative to x and y

  display->drawXbm(x, y + 14, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
}

void drawTem(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Demonstrates the 3 included default sizes. The fonts come from SSD1306Fonts.h file
  // Besides the default fonts there will be a program to convert TrueType fonts into this format
 display->drawXbm(x, y + 14, Tem_width, Tem_height, Tem_bits);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  String tem=String(shuiwenM.value);
  display->drawString(5 + Tem_width, 20 + y, tem.substring(0,tem.length()-1)+"°C");
}

void drawDht(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Text alignment demo
  display->drawXbm(x, y + 14, shidu_width, shidu_height, shidu_bits);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  display->drawString(5 + Tem_width, 20 + y, String(shiduM.value)+"%");
}

void drawSun(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  // Demo for drawStringMaxWidth:
  // with the third parameter you can define the width after which words will be wrapped.
  // Currently only spaces and "-" are allowed for wrapping
  display->drawXbm(x, y + 14, light_width, light_height, light_bits);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_24);
  String guan=String(guangzhaoM.value);
  display->drawString(Tem_width, 20 + y, guan.substring(0,guan.length()-1)+"Lx");
}


FrameCallback frames[] = { drawLogo, drawTem, drawDht, drawSun };
OverlayCallback overlays[] = { msOverlay };
void setup() {
  Serial.begin(115200);
  pinMode (QWPIN, OUTPUT);
  pinMode (GZPIN, OUTPUT);
  pinMode (SDPIN, OUTPUT);
  pinMode (SWPIN, OUTPUT);
  pinMode (WSPIN, OUTPUT);
  digitalWrite(QWPIN, LOW);
  digitalWrite(GZPIN, LOW);
  digitalWrite(SDPIN, LOW);
  digitalWrite(SWPIN, LOW);
  digitalWrite(WSPIN, LOW);

  firstDataSet();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  dht.begin();
  sensors.begin();
  WiFi.mode(WIFI_AP_STA);
  boolean result = WiFi.softAP(AP_SSID, AP_PASS, 1); //开启WIFI热点
  if (result)
  {
    IPAddress myIP = WiFi.softAPIP();

    //打印相关信息
    Serial.println("");
    Serial.print("Soft-AP IP address = ");
    Serial.println(myIP);
    Serial.println(String("MAC address = ")  + WiFi.softAPmacAddress().c_str());
    Serial.println("waiting ...");
    ip=myIP.toString();
  } else {  //开启热点失败
    Serial.println("WiFiAP Failed");
    delay(3000);
    ESP.restart();  //复位esp32
  }
  
  
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.println();


  mac = WiFi.macAddress();
  mac.replace(":", "");

  server.on("/", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });
  /*server.on("/wifi", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/wifi.html", "text/html");
  });*/
  server.on("/style.css", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });
  server.on("/jquery.js", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/jquery.js", "text/javascript");
  });
  server.on("/mdi.woff2", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/mdi.woff2", "application/x-font-woff");
  });
  /*server.on("/mb1.jpg", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/mb1.jpg", "image/jpg");
  });*/
  server.on("/getData", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", getData());
  });
  server.on("/getMinMax", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String typeString = request->arg("type");
    request->send(200, "text/plain", getMinMax(typeString));
  });
  server.on("/setMinMax", HTTP_POST, [] (AsyncWebServerRequest * request) {
    String typeString = request->arg("type");
    String minString = request->arg("min");
    String maxString = request->arg("max");
    setMinMax(typeString, minString, maxString);
    request->send(200, "text/plain", "success");
  });
  server.on("/setAuto", HTTP_POST, [] (AsyncWebServerRequest * request) {
    String typeString = request->arg("type");
    String auto_value = request->arg("auto_value");
    setAuto(typeString, auto_value.toInt());
    request->send(200, "text/plain", "success");
  });
  server.on("/changeState", HTTP_POST, [] (AsyncWebServerRequest * request) {
    String typeString = request->arg("type");
    String state = request->arg("state");
    changeState(typeString, state.toInt());
    request->send(200, "text/plain", "success");
  });
  server.on("/setWIFI", HTTP_POST, [] (AsyncWebServerRequest * request) {
    setWifi = true;
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    char ssid2[20]; char password2[20];
    strcpy(ssid2, ssid.c_str());
    strcpy(password2, password.c_str());
    if (setWIFI(ssid2, password2)) {
      request->send(200, "text/plain", "{state:'1'}");
    } else {
      request->send(200, "text/plain", "{state:'0'}");
    }

  });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest * request) {
    ESP.restart();
  });
  /*server.on("/serverIndex", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/upload.html", "text/html");
  });*/
  /*handling uploading firmware file */
   server.on("/update", HTTP_POST, [](AsyncWebServerRequest * request) {},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data,
     size_t len, bool final) {
    if (filename.indexOf("bin") > -1) {
      binUpdate(request, filename, index, data, len, final);
    } else {
      fileUpload(request, filename, "", index, data, len, final);
    }
  });

  server.begin();
  Update.onProgress(printProgress);

  // server address, port and URL
  webSocket.begin("www.fallwings.top", 80, "/aigs/websocket/socketServer?deviceNum=" + mac);

  // event handler
  webSocket.onEvent(webSocketEvent);

  // try ever 5000 again if connection has failed
  webSocket.setReconnectInterval(5000);
  InitWifi();
  ui.setTargetFPS(20);
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(RIGHT_BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, frameCount);
  ui.setOverlays(overlays, overlaysCount);
  ui.init();
  display.flipScreenVertically();

}

void loop() {
  
  
  webSocket.loop();
  
  float t = dht.readTemperature();
    float h = dht.readHumidity();
    float lux = lightMeter.readLightLevel();
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    qiwenM.value = t;
    shiduM.value = h;
    guangzhaoM.value = lux;
    shuiwenM.value = temperatureC;
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
    Serial.println(t);
    Serial.print(temperatureC);
    Serial.println("ºC");
    autoControl();
    String getDataString = getData();
    webSocket.sendTXT(getDataString);
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    int remainingTimeBudget = ui.update();
  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    delay(remainingTimeBudget);
  }
  
}
void printProgress(size_t prg, size_t sz) {
  Serial.printf("Progress: %d%%\n", (prg * 100) / content_len);
}
void binUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.println("Update");
    content_len = request->contentLength();
    // if filename includes spiffs, update the spiffs partition
    int cmd = (filename.indexOf("bin") > -1) ? U_FLASH : U_SPIFFS ;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
      Update.printError(Serial);
    }
  }

  if (Update.write(data, len) != len) {
    Update.printError(Serial);

  }

  if (final) {
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "Please wait while the device reboots");
    response->addHeader("Refresh", "20");
    response->addHeader("Location", "/");
    request->send(response);
    if (!Update.end(true)) {
      Update.printError(Serial);
    } else {
      Serial.println("Update complete");
      Serial.flush();
      request->send(200, "text/plain", "{state:'1'}");
      ESP.restart();
    }
  }
}
void fileUpload(AsyncWebServerRequest *request, String filename, String redirect, size_t index, uint8_t *data, size_t len, bool final) {



  if (!index) {
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.println((String)"UploadStart: " + filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
  }
  for (size_t i = 0; i < len; i++) {
    fsUploadFile.write(data[i]);
    //Serial.write(data[i]);
  }
  if (final) {
    Serial.println((String)"UploadEnd: " + filename);
    fsUploadFile.close();
    request->send(200, "text/plain", "{state:'1'}");
  }

}
