#define _DISABLE_TLS_
#define GET_REQUEST "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.88 Safari/537.36\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n\r\n"

#define POST_REQUEST "POST %s HTTP/1.1\r\nHost: %s\r\nKeep-Alive: timeout=5, max=10\r\nTransfer-Encoding: chunked\r\nContent-Type: application/json\r\n\r\n"
#define END_POST_REQUEST "\r\n\r\n"

#define REQUEST_CHECK_TIME 5000
#define REQUESTS_HOST "http://test-esp-proxy-scrapper.herokuapp.com/"
#define REQUEST_PORT 3000

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

//------- Replace the following! ------
char ssid[] = "INSG - Aberto";
char password[] = "";

void sendRepply(char* data, int index);
void makeHTTPRequest(const char* host, bool https, const char* id);
void setupWebSocket();
void loadHttpRequest();

void setup() {
    Serial.begin(115200);

    // Connect to the WiFI
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.print(" ");

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

void makeHTTPRequest(const char* host, bool https, const char* id) {
    WiFiClientSecure client;

    Serial.println("[INFO] Initializing scrape connection");
    if (!client.connect(host, 443)) {
        printf("Connection failed\n");
        return;
    }
    char* request = new char[500];
    snprintf(request, 500, GET_REQUEST, "/", host);
    Serial.println(request);
    client.print(request);

    delete[] request;

    int contentLength = -1;
    int httpCode = 0;
    while (client.connected()) {
        String header = client.readStringUntil('\n');
        if (header.startsWith(F("HTTP/1."))) {
            httpCode = header.substring(9, 12).toInt();
            if (httpCode != 200) {
                Serial.println(String(F("HTTP GET code=")) + String(httpCode));
                client.stop();
                return;
            }
        }
        if (header.startsWith(F("Content-Length: "))) {
            contentLength = header.substring(15).toInt();
        }
        if (header == F("\r")) {
            break;
        }
    }

    if (!(contentLength > 0)) {
        Serial.println(F("HTTP content length=0"));
        client.stop();
        return;
    }

    // Download file
    int remaining = contentLength;
    int received;
    int index = 0;
    // read all data from server
    char content[2048] = {0};
    // int charIndex = 0;

    HTTPClient serverClient;
    if (!serverClient.begin(REQUESTS_HOST)) {  //, REQUEST_PORT)) {
        printf("[INFO] Cannot comunicate with the server\n");
        return;
    }

    serverClient.addHeader("Content-Type", "text/html; charset=UTF-8");
    serverClient.addHeader("Transfer-Encoding", "chunked");
    serverClient.sendHeader("POST");

    while (client.connected() && remaining > 0) {
        while (client.available() && remaining > 0) {
            // read up to buffer size
            received = client.readBytes(content, ((remaining > sizeof(content)) ? sizeof(content) : remaining));
            // write it to file
            serverClient.getStream().print(content);
            // sendRepply(content, index);
            index++;
            for (int i = 0; i < sizeof(content); i++) {
                content[i] = 0;
            }
            if (remaining > 0) {
                remaining -= received;
            }
            yield();
        }
    }
    serverClient.end();
    // if (charIndex > 0)
    //     sendRepply(content, index);

    if (client.connected())
        client.stop();
    Serial.println("Client disconected");
}

void loadHttpRequest() {
    HTTPClient client;
    Serial.println("[INFO] Checking server for requests");
    if (!client.begin(REQUESTS_HOST)) {  //, REQUEST_PORT)) {
        printf("[INFO] Cannot comunicate with the server\n");
        return;
    }
    int httpResponseCode = client.GET();
    Serial.print("[INFO] Received respose code: ");
    Serial.print(httpResponseCode);
    Serial.println(" from the server.");

    if (httpResponseCode <= 0) {
        Serial.println("[INFO] Failed to get data from the server.");
        return;
    }

    String payload = client.getString();
    Serial.println(payload);
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print(F("[INFO] deserializeJson() failed: "));
        Serial.println(error.f_str());
        doc.clear();
        return;
    }

    if (!doc.containsKey("request_url")) {
        Serial.println("[INFO] Server has no data to scrape.");
        return;
    }

    const char* url = doc["request_url"];
    bool https = doc["https"];
    const char* request_id = doc["id"];

    makeHTTPRequest(url, https, request_id);
    // Free resources
    doc.clear();
    client.end();
}

long last_request = 0;

void loop() {
    if (millis() - last_request > REQUEST_CHECK_TIME) {
        last_request = millis();
        loadHttpRequest();
    }
}

void sendReppluChunked(HTTPClient client, char* data) {
}

void sendRepply(char* data, int index) {
    HTTPClient client;
    if (!client.begin(REQUESTS_HOST)) {  //, REQUEST_PORT)) {
        printf("[INFO] Cannot comunicate with the server\n");
        return;
    }
    DynamicJsonDocument doc(2500);
    doc["data"] = data;
    doc["index"] = index;
    // JSON to String (serializion)
    String output;
    serializeJson(doc, output);
    Serial.print("Free Heap1: ");
    Serial.println(ESP.getFreeHeap());

    client.addHeader("Content-Type", "Application/json; charset=UTF-8");
    // client.addHeader("Transfer-Encoding", "chunked");
    client.addHeader("Content-Length", String(output.length()));

    Serial.print("Sending length: ");
    Serial.println(output.length());

    client.POST(output);

    Serial.print("Free Heap2: ");
    Serial.println(ESP.getFreeHeap());

    doc.clear();

    client.end();
}