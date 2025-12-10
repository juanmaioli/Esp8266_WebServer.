// CavaWiFi Version 1.6
// Author Juan Maioli
// Añadida la obtención de datos de Cava cada 29 minutos.
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> // Para el servidor web
#include <ESP8266HTTPClient.h> // Para peticiones HTTP
#include <WiFiClientSecure.h>
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <time.h>             // Para la sincronización de hora (NTP)

// Definición de la estructura movida a la parte superior
struct WifiNetwork {
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
};

// --- Variables Globales ---
const char *host = "ifconfig.me";
String serial_number;
String id_Wemos;
String localIP;
String publicIP = "Obteniendo..."; // Valor inicial
String cavaData = "Cargando...";
// Variable para los datos de Cava
String wifiNetworksList = "Escaneando...";
// Variable para la lista de redes WiFi
String lastWifiScanTime = "Nunca";
// Variable para la hora de la última actualización de WiFi 

// --- Configuración del Servidor Web ---
ESP8266WebServer server(3000);
// --- Configuración de Hora (NTP) ---
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -3 * 3600;
// Zona horaria de Argentina (GMT-3)
const int daylightOffset_sec = 0;
// Argentina no usa horario de verano

// --- Configuración de Temporizadores (millis) ---
unsigned long previousTimeUpdate = 0;
unsigned long previousIpUpdate = 0;
const long timeInterval = 60000;      // 1 minuto (en milisegundos)
const long ipInterval = 1740000;
// 29 minutos (en milisegundos)


// --- Funciones ---
String leftRepCadena(String mac) {
  mac.replace(":", "");
  mac = mac.substring(mac.length() - 4);
  return mac;
}

void sortNetworks(WifiNetwork *networks, int count) {
  for (int i = 0; i < count - 1; i++) {
    for (int j = 0; j < count - i - 1; j++) {
      if (networks[j].rssi < networks[j + 1].rssi) {
        WifiNetwork temp = networks[j];
        networks[j] = networks[j + 1];
        networks[j + 1] = temp;
      }
    }
  }
}

String scanWifiNetworks() {
  Serial.println("Iniciando escaneo de redes WiFi...");
  int n = WiFi.scanNetworks();
  Serial.println("Escaneo completo.");

  if (n == 0) {
    return "<p>No se encontraron redes.</p>";
  } else {
    WifiNetwork *networks = new WifiNetwork[n];
    for (int i = 0; i < n; ++i) {
      networks[i].ssid = WiFi.SSID(i);
      networks[i].rssi = WiFi.RSSI(i);
      networks[i].encryptionType = WiFi.encryptionType(i);
    }

    sortNetworks(networks, n);

    String list = "";
    for (int i = 0; i < n; ++i) {
      list += "<p>";
      // Emojis corregidos con secuencias de bytes hexadecimales UTF-8
      list += (networks[i].encryptionType == ENC_TYPE_NONE) ? "\xF0\x9F\x8C\x90 " : "\xF0\x9F\x94\x92 ";
      list += networks[i].ssid;
      list += " (";
      list += networks[i].rssi;
      list += " dBm)";
      list += "</p>";
      if (i < n - 1) {
        list += "";
      }
    }

    delete[] networks;
    return list;
  }
}


String getFormattedTime() {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) {
    return "No sincronizada";
  }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getFormattedDate() {
  time_t now = time(nullptr);
  if (now < 8 * 3600 * 2) {
    return "Fecha no sincronizada";
  }
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%Y/%m/%d", &timeinfo);
  return String(buffer);
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Modo Configuración Activado");
  Serial.print("Conéctate a la red: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("IP del portal: ");
  Serial.println(WiFi.softAPIP());
}

void getPublicIP() {
  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(host, 443)) {
    Serial.println(getFormattedTime() + " - Fallo de conexión con ifconfig.me");
    return;
  }

  client.print(String("GET /ip HTTP/1.1\r\n") + "Host: " + host + "\r\n" + "User-Agent: ESP8266-IP-Checker\r\n" + "Connection: close\r\n\r\n");
  if (client.find("\r\n\r\n")) {
    String newPublicIP = client.readStringUntil('\n');
    newPublicIP.trim();
    if (newPublicIP.length() > 0) {
      publicIP = newPublicIP;
    }
  } else {
    Serial.println(getFormattedTime() + " - Error al obtener la IP pública.");
  }
}

void updateNetworkData() {
  // Limpiar el monitor serial y mover el cursor al inicio
  Serial.print("\033[2J\033[H");
  Serial.println("--- Chequeo de Redes (cada 29 mins) ---");

  // Chequear y mostrar IP Local
  localIP = WiFi.localIP().toString();
  Serial.println(getFormattedTime() + " - IP Local: " + localIP);

  // Chequear y mostrar IP Pública
  getPublicIP();
  Serial.println(getFormattedTime() + " - IP Publica: " + publicIP);

    // Obtener y mostrar datos de Cava

    Serial.println("\n--- Obteniendo datos de Cava ---");
    HTTPClient http;

    WiFiClient client; // Cliente para HTTP normal

    if (http.begin(client, "http://pikapp.com.ar/cava/txt/")) {

      int httpCode = http.GET();
      if (httpCode > 0 && httpCode == HTTP_CODE_OK) {

        cavaData = http.getString();

        Serial.println(cavaData);

      } else {

        cavaData = "Error al obtener datos de Cava. Codigo: " + String(httpCode);
        Serial.println(cavaData);

      }

      http.end();

    } else {

      cavaData = "Fallo la conexión con el servidor de Cava.";
      Serial.println(cavaData);

    }

    // Escanear redes WiFi
    wifiNetworksList = scanWifiNetworks();
    lastWifiScanTime = getFormattedTime();
}

// --- Handler para el Servidor Web ---
void handleRoot() {
    // --- Formateo de datos de Cava para la web ---
    String formattedCavaData = cavaData;
// --- Construcción de la página HTML ---
    String page = "<!DOCTYPE html><html lang='es'><head>";
    page += "<meta charset='UTF-8'>";
    page += "<meta http-equiv='refresh' content='60'>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    page += "<title>Estado del Dispositivo</title>";
    page += "<style>";
    page += ":root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; }";
    page += "@media (prefers-color-scheme: dark) {";
    page += ":root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; }";
    page += "}";
    page += "body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}";
    page += ".container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; max-width: 90%; min-width: 320px; }";
    page += "h1 { color: var(--text-primary); margin-bottom: 1.5rem; }";
    page += "p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; }";
    page += "strong { color: var(--text-primary); }";
    page += "hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }";
    // page += "pre { background-color: var(--pre-bg); color: var(--text-secondary); padding: 1rem; border-radius: 4px; white-space: pre-wrap; word-wrap: break-word; font-family: 'Courier New', Courier, monospace; line-height: 1.6; text-align: left; }";
    page += ".emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }";
    page += "</style></head><body><div class='container'>";
    page += "<h1>Estado del Dispositivo</h1>";
    page += "<p><strong>Fecha:</strong> " + getFormattedDate() + "</p>";
    page += "<p><strong>Hora:</strong> " + getFormattedTime() + "</p>";
    page += "<p><strong>IP Privada:</strong> " + localIP + "</p>";
    page += "<p><strong>M&aacute;scara de Red:</strong> " + WiFi.subnetMask().toString() + "</p>";
    page += "<p><strong>Puerta de Enlace:</strong> " + WiFi.gatewayIP().toString() + "</p>";
    page += "<p><strong>IP P&uacute;blica:</strong> " + publicIP + "</p>";
    page += "<p><strong>Intensidad de Se&ntilde;al (RSSI):</strong> " + String(WiFi.RSSI()) + " dBm</p>";
    page += "<p><strong>Direcci&oacute;n MAC:</strong> " + WiFi.macAddress() + "</p>";
    page += "<p><strong>Memoria Libre (Heap):</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";
    page += "<p><strong>Tiempo de Actividad:</strong> " + String(millis() / 1000) + " seg</p>";
    page += "<hr><h2>Datos de Clima</h2><p>" + formattedCavaData + "</p>";
    page += "<hr><h2>Redes WiFi Cercanas</h2><p><strong>Escaneado:</strong> " + lastWifiScanTime + "</p><hr>" + wifiNetworksList;
    page += "</div></body></html>";

    server.send(200, "text/html", page);
}

// --- Setup y Loop ---
void setup() {
    delay(1500);
    Serial.begin(115200);
    serial_number = WiFi.softAPmacAddress();
    id_Wemos = "WiFiSensor-" + leftRepCadena(serial_number);
    WiFi.hostname(id_Wemos);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setWiFiAutoReconnect(true);

    Serial.println("\nConectando a la red WiFi...");
    if (!wifiManager.autoConnect(id_Wemos.c_str())) {
      Serial.println("No se pudo conectar. Reiniciando...");
      ESP.reset();
      delay(1000);
    }
    Serial.println("\n¡Conexión exitosa!");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.print("Sincronizando hora...");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
      delay(500);
      Serial.print(".");
      now = time(nullptr);
    }
    Serial.println("\n¡Hora sincronizada!");

    // Forzar la primera actualización de datos de red al iniciar
    updateNetworkData();
    unsigned long currentMillis = millis();
    previousIpUpdate = currentMillis;
    previousTimeUpdate = currentMillis;

    server.on("/", handleRoot);
    server.begin();
    Serial.println("Servidor Web iniciado.");
    Serial.print("Para ver el estado, visita: http://");
    Serial.print(WiFi.localIP());
    Serial.println(":3000");
}

void loop() {
    unsigned long currentMillis = millis();
    // Tarea 1: Imprimir la hora en el Serial cada minuto
    if (currentMillis - previousTimeUpdate >= timeInterval) {
      previousTimeUpdate = currentMillis;
      Serial.print("\033[2J\033[H");
      Serial.print("Hora: ");
      Serial.println(getFormattedTime());
    }

    // Tarea 2: Actualizar todos los datos de red cada 29 minutos 
    if (currentMillis - previousIpUpdate >= ipInterval) {
      previousIpUpdate = currentMillis;
      updateNetworkData();
    }

    // Tarea 3: Atender las peticiones del cliente web
    server.handleClient();
}
