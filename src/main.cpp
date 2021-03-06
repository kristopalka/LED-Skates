#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WS2812FX.h>
#include <WiFiUdp.h>

#include "web/arg.cpp"

#define IS_SERVER true  // mode san be SERVER or CLIENT

#define WIFI_SSID "LedSkates"
#define WIFI_PASSWORD "1234567890"
#define HTTP_PORT 80
#define UDP_PORT 8080
#define UDP_MAX_LENGTH 255

#define LED_PIN D3
#define LED_COUNT 14

IPAddress host_ip(192, 168, 100, 1);
IPAddress client_ip(192, 168, 100, 2);
IPAddress gateway(192, 168, 100, 1);
IPAddress subnet(255, 255, 255, 0);

unsigned long auto_last_change = 0;
unsigned long last_wifi_check_time = 0;
bool auto_cycle = false;

WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiUDP UDP;
char packetBuffer[UDP_MAX_LENGTH];

void change_mode(Arg arg);

#if IS_SERVER == true
////////////////////////////////////////////// Webserver Functions //////////////////////////////////////////////
#include "web/index.html.cpp"
#include "web/main.js.cpp"

String modes_html = "";
ESP8266WebServer server(HTTP_PORT);

void html_modes_setup() {
    modes_html.reserve(5000);

    modes_html = "";
    uint8_t num_modes = ws2812fx.getModeCount();
    for (uint8_t i = 0; i < num_modes; i++) {
        modes_html += "<li><a href='#'>";
        modes_html += ws2812fx.getModeName(i);
        modes_html += "</a></li>";
    }
}

void srv_handle_not_found() {
    server.send(404, "text/plain", "File Not Found");
}

void srv_handle_index_html() {
    server.send_P(200, "text/html", index_html);
}

void srv_handle_main_js() {
    server.send_P(200, "application/javascript", main_js);
}

void srv_handle_modes() {
    server.send(200, "text/plain", modes_html);
}

void srv_handle_set() {
    for (uint8_t i = 0; i < server.args(); i++) {
        Arg arg(server.argName(i), server.arg(i));

        UDP.beginPacket(client_ip, UDP_PORT);
        UDP.write(arg.toString().c_str());
        UDP.endPacket();

        // delay(330);

        change_mode(arg);
    }
    server.send(200, "text/plain", "OK");
}

void web_server_setup() {
    Serial.print("HTTP server setup > ");

    server.on("/", srv_handle_index_html);
    server.on("/main.js", srv_handle_main_js);
    server.on("/modes", srv_handle_modes);
    server.on("/set", srv_handle_set);
    server.onNotFound(srv_handle_not_found);
    server.begin();

    Serial.println("success!");
}

void wifi_server_setup() {
    Serial.print("Wifi setup > ");
    Serial.print("starting access point > ");
    WiFi.softAPConfig(host_ip, gateway, subnet);

    bool success = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    if (success) {
        Serial.print(WiFi.softAPIP());
        Serial.println(" > success!");
    } else {
        Serial.println("failed. Resetting...");
        delay(1000);
        ESP.reset();
    }
}
#else

void wifi_client_setup() {
    Serial.print("Wifi setup > ");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.mode(WIFI_STA);

    if (!WiFi.config(client_ip, gateway, subnet)) {
        Serial.println("STA Failed to configure");
        delay(1000);
        ESP.reset();
    }

    Serial.print("connecting to AP: ");
    Serial.print(WIFI_SSID);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print(" > connected! IP address: ");
    Serial.println(WiFi.localIP());
}

void receive_packet() {
    if (UDP.parsePacket()) {
        int len = UDP.read(packetBuffer, UDP_MAX_LENGTH);
        packetBuffer[len] = 0;

        Arg arg(packetBuffer);
        change_mode(arg);
    }
}
#endif

////////////////////////////////////////////// SETUP //////////////////////////////////////////////
void leds_setup() {
    Serial.print("WS2812FX setup > ");

    ws2812fx.init();
    ws2812fx.setMode(FX_MODE_STATIC);
    ws2812fx.setColor(0x00ff00);
    ws2812fx.setSpeed(1000);
    ws2812fx.setBrightness(128);
    ws2812fx.start();

    Serial.println("success!");
}

void setup() {
    Serial.begin(74880);
    delay(300);

    leds_setup();

#if IS_SERVER == true
    html_modes_setup();
    web_server_setup();
    wifi_server_setup();
#else
    wifi_client_setup();
#endif

    UDP.begin(UDP_PORT);
}

////////////////////////////////////////////// LOOP //////////////////////////////////////////////

void auto_switch_mode() {
    unsigned long now = millis();

    if (auto_cycle && (now - auto_last_change > 4000)) {  // cycle effect mode every 10 seconds
        uint8_t next_mode = (ws2812fx.getMode() + 1) % ws2812fx.getModeCount();

        ws2812fx.setMode(next_mode);
        Serial.print("mode is ");
        Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
        auto_last_change = now;
    }
}

void change_mode(Arg arg) {
    Serial.print("Arg: ");
    Serial.println(arg.toString());

    if (arg.key == "c") {
        uint32_t tmp = (uint32_t)strtol(arg.value.c_str(), NULL, 10);
        if (tmp <= 0xFFFFFF) {
            ws2812fx.setColor(tmp);
        }
    }

    if (arg.key == "m") {
        uint8_t tmp = (uint8_t)strtol(arg.value.c_str(), NULL, 10);
        uint8_t new_mode = tmp % ws2812fx.getModeCount();
        ws2812fx.setMode(new_mode);
        auto_cycle = false;
        Serial.print("mode is ");
        Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    }

    if (arg.key == "b") {
        if (arg.value.c_str()[0] == '-') {
            ws2812fx.setBrightness(ws2812fx.getBrightness() * 0.8);
        } else if (arg.value.c_str()[0] == ' ') {
            ws2812fx.setBrightness(min((int)(max(ws2812fx.getBrightness(), (uint8_t)5) * 1.2), 255));
        } else {  // set brightness directly
            uint8_t tmp = (uint8_t)strtol(arg.value.c_str(), NULL, 10);
            ws2812fx.setBrightness(tmp);
        }
        Serial.print("brightness is ");
        Serial.println(ws2812fx.getBrightness());
    }

    if (arg.key == "s") {
        if (arg.value.c_str()[0] == '-') {
            ws2812fx.setSpeed(max(ws2812fx.getSpeed(), (uint16_t)5) * 1.2);
        } else if (arg.value.c_str()[0] == ' ') {
            ws2812fx.setSpeed(ws2812fx.getSpeed() * 0.8);
        } else {
            uint16_t tmp = (uint16_t)strtol(arg.value.c_str(), NULL, 10);
            ws2812fx.setSpeed(tmp);
        }
        Serial.print("speed is ");
        Serial.println(ws2812fx.getSpeed());
    }

    if (arg.key == "a") {
        if (arg.value.c_str()[0] == '-') {
            auto_cycle = false;
        } else {
            auto_cycle = true;
            auto_last_change = 0;
        }
    }
}

void loop() {
#if IS_SERVER == true
    server.handleClient();
#else
    receive_packet();
#endif
    ws2812fx.service();
    auto_switch_mode();
}
