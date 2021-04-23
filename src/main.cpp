#include <Arduino.h>
#include <ArduinoJson.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFiMulti.h>
#include <EventDispatcher.hpp>
#include <HTTPSClient.hpp>
#include <LittleFS.h>
#include <Timer.hpp>
#include <WiFiManager.hpp>
#include <setClock.hpp>

#define SSID "ssid"
#define PASSWORD "password"

ESP8266WebServer server(80);
BearSSL::CertStore certStore;
Timer timer;
EventDispatcher dispatcher;
ESP8266WiFiMulti wifiMulti;

WiFiManager wifiManager(&wifiMulti, &dispatcher, &timer, SSID, PASSWORD);
HTTPSClient httpsClient(&certStore, &wifiManager, &timer);

StaticJsonDocument<2048> doc;
char body[2048];

void setup() {
  Serial.begin(115200);
  delay(5000);

  server.on("/", []() {
    doc["sensor"] = "dht-11";
    doc["temperature"]["value"] = 20;
    doc["temperature"]["unit"] = "C";
    doc["day"] = true;
    doc["coordinates"][0] = 23.2332323;
    doc["coordinates"][1] = 5.43434;

    serializeJson(doc, body);

    server.send(200, "application/json", body);
  });

  server.on("/user", []() {
    httpsClient.sendRequest(
        Request::build(Request::Method::GET,
                       "https://jsonplaceholder.typicode.com/users/1"),
        [](Response res) {
          doc.clear();
          if (res.error != nullptr) {
            doc["error"] = res.error;
            serializeJson(doc, body);
            server.send(500, "application/json", body);
            return;
          }

          deserializeJson(doc, res.body->readString());

          doc.remove("address");
          doc["username"] = "new user";
          doc["extra"] = 123;

          if (doc["id"].is<int>()) {
            int id = doc["id"];

            Serial.println(id);
          }

          if (doc["non-existing"].isNull()) {
            Serial.println("non-existing is null");
          }

          serializeJson(doc, body);

          server.send(200, "application/json", body);
        });
  });

  LittleFS.begin();

  int numCerts =
      certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));

  Serial.printf("Number of CA certs read: %d\n", numCerts);

  wifiManager.connect([](wl_status_t status) {
    if (status != WL_CONNECTED) {
      Serial.println("could not connect to WiFi");
      return;
    }

    setClock(&timer, [](bool) {
      server.begin();

      Serial.printf("listening on http://%s:80\n",
                    WiFi.localIP().toString().c_str());

      timer.setOnLoop([]() { server.handleClient(); });
    });
  });
}

void loop() { timer.tick(); }