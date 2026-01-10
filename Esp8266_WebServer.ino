// CEsp8266_WebServer Version 2.5.3
// Author Juan Maioli
// Cambios: Ping cada 45s y Host de Latencia Configurable.
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h> // Para el servidor web
#include <ESP8266HTTPClient.h> // Para peticiones HTTP
#include <WiFiClientSecure.h>
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
#include <time.h>
#include <EEPROM.h>           // Para guardar configuración
#include "ESP8266Ping.h"      // Librería Local de Ping
#include <ArduinoOTA.h>       // Para actualizaciones OTA

// --- Estructura de Configuración ---
struct ConfigSettings {
  char description[51]; // Max 50 chars + null terminator
  char domain[51];      // Max 50 chars + null terminator
  char pingTarget[51];  // Max 50 chars + null terminator (NUEVO)
  char otaPassword[51]; // Max 50 chars + null terminator (NUEVO OTA)
  uint8_t magic;        // Byte para verificar versión de config
};

ConfigSettings settings;

// --- Estructura WiFi ---
struct WifiNetwork {
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
};

// --- Variables Globales ---
const char* firmwareVersion = "2.5.3";
const char* hostname_prefix = "Esp8266-";
String serial_number;
String id_Wemos;
String localIP;
String publicIP = "Obteniendo..."; // Valor inicial

String wifiNetworksList = "Escaneando...";
// Variable para la lista de redes WiFi
String lastWifiScanTime = "Nunca";
// Variable para la hora de la última actualización de WiFi 
String downloadSpeed = "No medido";
// Variable para la velocidad de descarga
String lastSpeedTestTime = "Nunca";
// Variable para la hora de la última prueba de velocidad
String lanScanData = "<p>No se ha realizado el escaneo.</p>";
String lastLanScanTime = "Nunca";

// --- Variables Monitor de Latencia ---
String latencyData = "Calculando...";
String latencyStatus = "🟡 Iniciando...";
int lastLatency = 0;
int lastPacketLoss = 0;
String lastPingTimeStr = "Nunca";

// --- Variables Gráfico RSSI ---
const int HISTORY_LEN = 60; // 60 minutos
int rssiHistory[HISTORY_LEN];
String rssiGraphSvg = "";


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
unsigned long previousPingUpdate = 0; // Para el monitor de latencia
unsigned long previousRssiUpdate = 0; // Para el historial RSSI
const long timeInterval = 60000;      // 1 minuto
const long ipInterval = 1740000;      // 29 minutos
const long pingInterval = 45000;      // 45 segundos (Latencia) - CAMBIO SOLICITADO
const long rssiInterval = 60000;      // 1 minuto (Histórico RSSI) 


// --- Funciones de Persistencia ---
void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void loadSettings() {
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  // Verificar si es la primera vez o si cambió la estructura (Magic 0x44)
  if (settings.magic != 0x44) {
    strncpy(settings.description, "Casa", 50);
    strncpy(settings.domain, "ifconfig.me", 50);
    strncpy(settings.pingTarget, "8.8.8.8", 50); // Default Google DNS
    strncpy(settings.otaPassword, "ArduinoOTA", 50); // Default OTA Password
    settings.magic = 0x44; // Marcar como inicializado v2.3
    saveSettings();
  }
}

// --- Funciones Auxiliares ---
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
  // Serial.println("Iniciando escaneo de redes WiFi...");
  int n = WiFi.scanNetworks();
  // Serial.println("Escaneo completo.");

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
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

void configModeCallback(WiFiManager *myWiFiManager) {
  // Serial.println("Modo Configuración Activado");
  // Serial.print("Conéctate a la red: ");
  // Serial.println(myWiFiManager->getConfigPortalSSID());
  // Serial.print("IP del portal: ");
  // Serial.println(WiFi.softAPIP());
}

void getPublicIP() {
  WiFiClientSecure client;
  client.setInsecure();
  
  // Usar el dominio configurado en settings
  const char* host = settings.domain;

  if (!client.connect(host, 443)) {
    // Serial.println(getFormattedTime() + " - Fallo de conexión con " + String(host));
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
    // Serial.println(getFormattedTime() + " - Error al obtener la IP pública.");
  }
}

void testDownloadSpeed() {
  // Serial.println("\n--- Iniciando prueba de velocidad de descarga ---");
  HTTPClient http;
  WiFiClient client;

  const char* testUrl = "http://ipv4.download.thinkbroadband.com/5MB.zip";
  const float fileSizeMB = 5.0;

  if (http.begin(client, testUrl)) {
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
    unsigned long startTime = millis();
    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
      // Leer el cuerpo de la respuesta para asegurarse de que se descargue
      int len = http.getSize();
      uint8_t buff[128] = { 0 };
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = client.readBytes(buff, sizeof(buff));
        if (size == 0) {
          break;
        }
        if (len > 0) {
          len -= size;
        }
      }
      unsigned long endTime = millis();
      float duration = (endTime - startTime) / 1000.0;
      float speedMbps = (fileSizeMB * 8) / duration;
      downloadSpeed = String(speedMbps, 2) + " Mbps";
      lastSpeedTestTime = getFormattedTime();
      // Serial.println("Prueba de velocidad completada: " + downloadSpeed);
    } else {
      downloadSpeed = "Error en la prueba. Codigo: " + String(httpCode);
      // Serial.println(downloadSpeed);
    }
    http.end();
  } else {
    downloadSpeed = "Fallo la conexión para la prueba de velocidad.";
    // Serial.println(downloadSpeed);
  }
}

void updateLatencyData() {
    Serial.println("--- Monitor de Latencia ---");
    
    // Usar el host configurado por el usuario
    const char* remote_host = settings.pingTarget;
    int pings_to_send = 5;
    
    // Timeout normal de 1000ms para Internet
    bool result = Ping.ping(remote_host, pings_to_send, 1000); 
    
    int avg_time = Ping.averageTime();
    int lost = pings_to_send - Ping.packetsRecv();
    int loss_percent = (lost * 100) / pings_to_send;

    lastLatency = avg_time;
    lastPacketLoss = loss_percent;
    lastPingTimeStr = getFormattedTime();

    if (loss_percent == 100) {
        latencyStatus = "🔴 Sin Conexión";
    } else if (loss_percent > 0) {
        latencyStatus = "🟡 Inestable (" + String(loss_percent) + "% Loss)";
    } else if (avg_time > 150) {
        latencyStatus = "🟡 Lento (" + String(avg_time) + "ms)";
    } else {
        latencyStatus = "🟢 Estable";
    }

    latencyData = "<p><strong>Estado:</strong> " + latencyStatus + "</p>";
    latencyData += "<p><strong>Latencia Media:</strong> " + String(avg_time) + " ms</p>";
    latencyData += "<p><strong>Pérdida Paquetes:</strong> " + String(loss_percent) + "%</p>";
    
    Serial.printf("Ping a %s: %d ms avg, %d%% loss\n", remote_host, avg_time, loss_percent);
}

void updateRssiHistory() {
  int currentRssi = WiFi.RSSI();
  
  // Desplazar valores (Shift)
  for (int i = 0; i < HISTORY_LEN - 1; i++) {
    rssiHistory[i] = rssiHistory[i+1];
  }
  rssiHistory[HISTORY_LEN - 1] = currentRssi;

  // Generar SVG String
  // ViewBox 0 0 60 100.
  // X: 0 a 59 (índice)
  // Y: -30dBm (Mejor) -> 0px, -100dBm (Peor) -> 100px
  
  String points = "";
  for (int i = 0; i < HISTORY_LEN; i++) {
    int y = map(rssiHistory[i], -30, -100, 0, 100); // Invertido: señal fuerte (arriba), débil (abajo)
    // Constrain para que no se salga del gráfico
    if(y < 0) y = 0;
    if(y > 100) y = 100;
    
    points += String(i) + "," + String(y) + " ";
  }

  // Color dinámico según la señal actual
  String strokeColor = "#4CAF50"; // Verde
  if (currentRssi < -70) strokeColor = "#FFC107"; // Amarillo
  if (currentRssi < -85) strokeColor = "#F44336"; // Rojo

  rssiGraphSvg = "<svg viewBox='0 0 " + String(HISTORY_LEN - 1) + " 100' width='100%' height='200' xmlns='http://www.w3.org/2000/svg' preserveAspectRatio='none'>";
  // Fondo rejilla simple
  rssiGraphSvg += "<rect width='100%' height='100%' fill='none' stroke='var(--chart-grid)' stroke-width='1'/>";
  rssiGraphSvg += "<line x1='0' y1='25' x2='100%' y2='25' stroke='var(--chart-line-grid)' stroke-dasharray='2'/>";
  rssiGraphSvg += "<line x1='0' y1='50' x2='100%' y2='50' stroke='var(--chart-line-grid)' stroke-dasharray='2'/>";
  rssiGraphSvg += "<line x1='0' y1='75' x2='100%' y2='75' stroke='var(--chart-line-grid)' stroke-dasharray='2'/>";
  
  // El Gráfico
  rssiGraphSvg += "<polyline points='" + points + "' fill='none' stroke='" + strokeColor + "' stroke-width='2' vector-effect='non-scaling-stroke'/>";
  rssiGraphSvg += "</svg>";
}

void updateNetworkData() {
  // Limpiar el monitor serial y mover el cursor al inicio
  // Serial.print("\033[2J\033[H");
  // Serial.println("--- Chequeo de Redes (cada 29 mins) ---");

  // Chequear y mostrar IP Local
  localIP = WiFi.localIP().toString();
  // Serial.println(getFormattedTime() + " - IP Local: " + localIP);

  // Chequear y mostrar IP Pública
  getPublicIP();
  // Serial.println(getFormattedTime() + " - IP Publica: " + publicIP);

    // Escanear redes WiFi
    wifiNetworksList = scanWifiNetworks();
    lastWifiScanTime = getFormattedTime();
}

void handleSpeedTest() {
  testDownloadSpeed();
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleWifiScan() {
  wifiNetworksList = scanWifiNetworks();
  lastWifiScanTime = getFormattedTime();
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleLanScan() {
  IPAddress local = WiFi.localIP();
  String result = "";
  int found = 0;

  Serial.println("\n--- Iniciando Escaneo LAN ---");

  // Asumimos máscara /24 (255.255.255.0) - escaneamos de .1 a .254
  for (int i = 1; i < 255; i++) {
    IPAddress target(local[0], local[1], local[2], i);
    
    // Saltamos nuestra propia IP
    if (target == local) continue;
    
    Serial.print("Escaneando: ");
    Serial.print(target);

    // Ping ICMP simple con 1 intento y TIMEOUT CORTO (50ms)
    if (Ping.ping(target, 1, 50)) {
      Serial.println(" -> [ONLINE]");
      result += "<p>🟢 " + target.toString() + "</p>";
      found++;
    } else {
      Serial.println(" -> .");
    }
    yield(); // Evitar Watchdog Reset
  }

  Serial.println("--- Escaneo Finalizado ---");

  if (found == 0) {
    result = "<p>No se encontraron otros dispositivos activos (que respondan ping).</p>";
  }

  lanScanData = result;
  lastLanScanTime = getFormattedTime();

  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

void handleTimeRequest() {
  server.send(200, "text/plain", getFormattedTime());
}

void handleSaveConfig() {
  if (server.hasArg("desc")) {
    String d = server.arg("desc");
    d.trim();
    strncpy(settings.description, d.c_str(), 50);
    settings.description[50] = '\0'; // Asegurar terminación null
  }
  
  if (server.hasArg("domain")) {
    String dom = server.arg("domain");
    dom.trim();
    if (dom.length() > 0) {
      strncpy(settings.domain, dom.c_str(), 50);
      settings.domain[50] = '\0';
    }
  }

  if (server.hasArg("pingIP")) {
    String pip = server.arg("pingIP");
    pip.trim();
    if (pip.length() > 0) {
      strncpy(settings.pingTarget, pip.c_str(), 50);
      settings.pingTarget[50] = '\0';
    }
  }

  if (server.hasArg("otaPwd")) {
    String op = server.arg("otaPwd");
    op.trim();
    if (op.length() > 0) {
      strncpy(settings.otaPassword, op.c_str(), 50);
      settings.otaPassword[50] = '\0';
    }
  }

  saveSettings();
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

// --- Handler para el Servidor Web ---
void handleRoot() {

    // Calculo para Tiempo de Actividad
    unsigned long totalSeconds = millis() / 1000;
    unsigned long days = totalSeconds / 86400;
    unsigned long hours = (totalSeconds % 86400) / 3600;
    unsigned long minutes = ((totalSeconds % 86400) % 3600) / 60;
    unsigned long seconds = ((totalSeconds % 86400) % 3600) % 60;
    String uptime = "";
    if (days > 0) uptime += String(days) + "d ";
    if (hours > 0 || days > 0) uptime += String(hours) + "h ";
    if (minutes > 0 || hours > 0 || days > 0) uptime += String(minutes) + "m ";
    uptime += String(seconds) + "s";
    if (uptime == "") uptime = "0s"; // In case millis() is very small

    // Calcular CIDR
    int cidr = 0;
    IPAddress subnet = WiFi.subnetMask();
    for (int i = 0; i < 4; i++) cidr += __builtin_popcount(subnet[i]);

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    String chunk = F("<!DOCTYPE html><html lang='es'><head>");
    chunk += F("<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>");
    chunk += F("<meta http-equiv='refresh' content='1200'>");
    chunk += F("<link rel='icon' href='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2290%22>📟</text></svg>'>");
    chunk += F("<title>Estado del Dispositivo</title>");
    chunk += F("<style>:root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; --dot-color: #bbb; --dot-active-color: #717171; --input-bg: #fff; --input-border: #ccc; --chart-bg: #ffffff; --chart-grid: #dddddd; --chart-line-grid: #eeeeee; --chart-border: #cccccc; } ");
    chunk += F("@media (prefers-color-scheme: dark) { :root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; --dot-color: #555; --dot-active-color: #ccc; --input-bg: #333; --input-border: #555; --chart-bg: #2a2a2a; --chart-grid: #444444; --chart-line-grid: #333333; --chart-border: #555555; } }");
    chunk += F("body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}");
    chunk += F(".container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; width: 500px; max-width: 95%; height: 80vh; position: relative; display: flex; flex-direction: column; } ");
    chunk += F("@media (max-width: 768px) { .container { width: 95%; height: 80vh; padding: 1rem; } }");
    chunk += F("h1, h2 { color: var(--text-primary); margin-bottom: 1rem; text-align: center; } p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; } strong { color: var(--text-primary); } hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }");
    chunk += F(".carousel-container { position: relative; flex-grow: 1; overflow: hidden; } .carousel-slide { display: none; height: 100%; width: 100%; flex-basis: 100%; flex-shrink: 0; overflow-y: auto; padding-right: 15px; box-sizing: border-box; word-wrap: break-word; }");
    chunk += F(".fade { animation-name: fade; animation-duration: 0.5s; } @keyframes fade { from {opacity: .4} to {opacity: 1} }");
    chunk += F(".prev, .next { cursor: pointer; color: var(--text-primary); font-weight: bold; font-size: 24px; transition: 0.3s; user-select: none; padding: 0 15px; vertical-align: middle; } .prev:hover, .next:hover { color: var(--dot-active-color); }");
    chunk += F(".dots { text-align: center; padding-top: 20px; display: flex; justify-content: center; align-items: center; } .dot { cursor: pointer; height: 15px; width: 15px; margin: 0 5px; background-color: var(--dot-color); border-radius: 50%; display: inline-block; transition: background-color 0.3s ease; } .active, .dot:hover { background-color: var(--dot-active-color); }");
    chunk += F(".emoji-container { text-align: center; margin-top: 15px; margin-bottom: 15px; } .emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }");
    chunk += F(".button { background-color: #4CAF50; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; border: none;} .button:hover { background-color: #45a049; } .button[disabled] { background-color: #555; color: #eee; border: 1px solid #eeeeee; cursor: not-allowed; }");
    chunk += F(".center-button { text-align: center; } input[type=text] { width: 100%; padding: 12px 20px; margin: 8px 0; box-sizing: border-box; border: 1px solid var(--input-border); border-radius: 4px; background-color: var(--input-bg); color: var(--text-primary); }");
    chunk += F("</style></head><body><div class='container'>");
    chunk += F("<div class='carousel-container'>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Estado - ") + String(settings.description) + F("</h2><div class='emoji-container'><span class='emoji'>📟</span></div><br>");
    chunk += F("<h3><strong>🏷️ Versi&oacute;n:</strong> ") + String(firmwareVersion) + F("<br>");
    chunk += F("<strong>📅 Fecha:</strong> ") + getFormattedDate() + F("<br>");
    chunk += F("<strong>⌚ Hora:</strong> <span id='current-time'>") + getFormattedTime() + F("</span><br>");
    chunk += F("<strong>🖥️ Hostname:</strong> ") + id_Wemos + F("<br>");
    chunk += F("<strong>🏠 IP Privada:</strong> ") + localIP + "/" + String(cidr) + F("<br>");
    chunk += F("<strong>🚪 Puerta de Enlace:</strong> ") + WiFi.gatewayIP().toString() + F("<br>");
    chunk += F("<strong>🌐 IP P&uacute;blica:</strong> ") + publicIP + F("<br>");
    chunk += F("<strong>📶 Intensidad de Se&ntilde;al (RSSI):</strong> ") + String(WiFi.RSSI()) + F(" dBm<br>");
    chunk += F("<strong>🆔 Direcci&oacute;n MAC:</strong> ") + WiFi.macAddress() + F("<br>");
    chunk += F("<strong>💡 ID del Chip (HEX):</strong> ") + String(ESP.getChipId(), HEX) + F("<br>");
    chunk += F("<strong>💾 Memoria Flash:</strong> ") + String(ESP.getFlashChipSize() / 1024) + F(" KB<br>");
    chunk += F("<strong>🧠 Memoria Libre (Heap):</strong> ") + String(ESP.getFreeHeap() / 1024.0, 2) + F(" KB<br>");
    chunk += F("<strong>⚡ Tiempo de Actividad:</strong> ") + uptime + F("</h3></div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Redes WiFi Cercanas</h2><div class='emoji-container'><span class='emoji'>📡</span></div><br><p><strong>Escaneado:</strong> ") + lastWifiScanTime + F("</p><div class='center-button'><a href='/scanwifi' id='scanwifi-button' class='button' onclick='showWaiting(\"scanwifi-button\", \"waiting-wifi\")'>🔄 Escanear Ahora</a></div><p id='waiting-wifi' style='display:none; text-align:center;'>Escaneando redes...</p>") + wifiNetworksList + F("</div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Prueba de Velocidad</h2><div class='emoji-container'><span class='emoji'>🚀</span></div><br><p><strong>&Uacute;ltima prueba:</strong> ") + lastSpeedTestTime + F("</p><p><strong>Velocidad de Descarga:</strong> ") + downloadSpeed + F("</p><div class='center-button'><a href='/speedtest' id='speedtest-button' class='button' onclick='showWaiting(\"speedtest-button\", \"waiting-message\")'>&#x1F680; Iniciar Prueba</a></div><p id='waiting-message' style='display:none; text-align:center;'>Por favor, espere mientras se realiza la prueba...</p></div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Monitor de Latencia</h2><div class='emoji-container'><span class='emoji'>⏱️</span></div><br><p><strong>&Uacute;ltimo chequeo:</strong> ") + lastPingTimeStr + F("</p>") + latencyData + F("<p><i>Ping a ") + String(settings.pingTarget) + F("</i></p></div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Señal WiFi (1h)</h2><div class='emoji-container'><span class='emoji'>📉</span></div><br><p><strong>Actual:</strong> ") + String(WiFi.RSSI()) + F(" dBm</p><div style='background: var(--chart-bg); padding: 10px; border-radius: 5px; border: 1px solid var(--chart-border);'>") + rssiGraphSvg + F("</div><div style='display:flex; justify-content:space-between; font-size:0.8em; margin-top:5px;'><span>-30dBm</span><span>-100dBm</span></div></div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Esc&aacute;ner LAN</h2><div class='emoji-container'><span class='emoji'>🕸️</span></div><br><p>Busca dispositivos en tu red utilizando <strong>Ping (ICMP)</strong>.</p><p><strong>&Uacute;ltimo escaneo:</strong> ") + lastLanScanTime + F("</p><div class='center-button'><a href='/scanlan' id='scanlan-button' class='button' onclick='showWaiting(\"scanlan-button\", \"waiting-lan\")'>&#x1F50E; Escanear Red</a></div><p id='waiting-lan' style='display:none; text-align:center;'>Escaneando... Esto puede tardar varios segundos.</p><div style='margin-top:15px;'>") + lanScanData + F("</div></div>");
    server.sendContent(chunk);

    chunk = F("<div class='carousel-slide fade'><h2>Configuraci&oacute;n</h2><div class='emoji-container'><span class='emoji'>⚙️</span></div><br><form action='/save' method='POST'><div style='display:flex; gap:10px;'><div style='flex:1;'><p><strong>Descripci&oacute;n del Dispositivo:</strong><br><input type='text' name='desc' value='") + String(settings.description) + F("' maxlength='50' placeholder='Ej: Casa'></p></div><div style='flex:1;'><p><strong>Dominio IP P&uacute;blica:</strong><br><input type='text' name='domain' value='") + String(settings.domain) + F("' maxlength='50' placeholder='Ej: ifconfig.me'></p></div></div><div style='display:flex; gap:10px;'><div style='flex:1;'><p><strong>Host Ping Latencia:</strong><br><input type='text' name='pingIP' value='") + String(settings.pingTarget) + F("' maxlength='50' placeholder='Ej: 8.8.8.8'></p></div><div style='flex:1;'><p><strong>Contrase&ntilde;a OTA:</strong><br><input type='text' name='otaPwd' value='") + String(settings.otaPassword) + F("' maxlength='50' placeholder='Ej: ArduinoOTA'></p></div></div><div class='center-button'><button type='submit' class='button'>💾 Guardar Cambios</button></div></form></div>");
    server.sendContent(chunk);

    chunk = F("</div><div class='dots'><a class='prev' onclick='changeSlide(-1)'>&#10094;</a><span class='dot' onclick='currentSlide(1)'></span><span class='dot' onclick='currentSlide(2)'></span><span class='dot' onclick='currentSlide(3)'></span><span class='dot' onclick='currentSlide(4)'></span><span class='dot' onclick='currentSlide(5)'></span><span class='dot' onclick='currentSlide(6)'></span><span class='dot' onclick='currentSlide(7)'></span><a class='next' onclick='changeSlide(1)'>&#10095;</a></div></div>");
    chunk += F("<script>let slideIndex=1;showSlide(slideIndex);function changeSlide(n){showSlide(slideIndex+=n)}function currentSlide(n){showSlide(slideIndex=n)}function showWaiting(b,m){document.getElementById(b).setAttribute('disabled','true');document.getElementById(b).innerHTML='⏳ Trabajando...';document.getElementById(m).style.display='block'}function showSlide(n){let i;let s=document.getElementsByClassName('carousel-slide');let d=document.getElementsByClassName('dot');if(n>s.length){slideIndex=1}if(n<1){slideIndex=s.length}for(i=0;i<s.length;i++){s[i].style.display='none'}for(i=0;i<d.length;i++){d[i].className=d[i].className.replace(' active','')}s[slideIndex-1].style.display='block';d[slideIndex-1].className+=' active'}function updateTime(){fetch('/time').then(r=>r.text()).then(d=>{if(d)document.getElementById('current-time').innerText=d})}setInterval(updateTime,900000);</script></body></html>");
    server.sendContent(chunk);
}

// --- Setup y Loop ---
void setup() {
    Serial.begin(115200);

    // Cargar configuración persistente
    loadSettings();

    serial_number = WiFi.softAPmacAddress();
    id_Wemos = String(hostname_prefix) + leftRepCadena(serial_number);
    WiFi.hostname(id_Wemos);

    WiFiManager wifiManager;
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setWiFiAutoReconnect(true);

    // Serial.println("\nConectando a la red WiFi...");
    if (!wifiManager.autoConnect(id_Wemos.c_str())) {
      // Serial.println("No se pudo conectar. Reiniciando...");
      ESP.reset();
      delay(1000);
    }
    // Serial.println("\n¡Conexión exitosa!");

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    // Serial.print("Sincronizando hora...");
    time_t now = time(nullptr);
    unsigned long startNtp = millis();
    while (now < 8 * 3600 * 2 && (millis() - startNtp < 10000)) { // Timeout de 10s
      now = time(nullptr);
      delay(100); // Pequeña pausa para estabilidad
    }
    // Serial.println("\n¡Hora sincronizada (o timeout)!");

    // --- Configuración OTA ---
    ArduinoOTA.setHostname(id_Wemos.c_str());
    ArduinoOTA.setPassword(settings.otaPassword); 

    ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_FS
        type = "filesystem";
      }
      // Desmontar sistema de archivos si es necesario
      // SPIFFS.end();
      Serial.println("Iniciando actualización OTA: " + type);
    });

    ArduinoOTA.onEnd([]() {
      Serial.println("\nFin de la actualización.");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Fallo de Autenticación");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Fallo al Iniciar");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Fallo de Conexión");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Fallo de Recepción");
      else if (error == OTA_END_ERROR) Serial.println("Fallo al Finalizar");
    });

    ArduinoOTA.begin();
    Serial.println("OTA Listo");
    
    // Inicializar Array RSSI con el valor actual para no empezar en cero
    int currentRssi = WiFi.RSSI();
    for(int i=0; i<HISTORY_LEN; i++) rssiHistory[i] = currentRssi;
    updateRssiHistory();

    // Forzar la primera actualización de datos de red al iniciar
    updateNetworkData();
    updateLatencyData(); // Primera medición de latencia
    unsigned long currentMillis = millis();
    previousIpUpdate = currentMillis;
    previousTimeUpdate = currentMillis;
    previousPingUpdate = currentMillis;
    previousRssiUpdate = currentMillis;

    server.on("/", handleRoot);
    server.on("/speedtest", handleSpeedTest);
    server.on("/scanwifi", handleWifiScan); // Endpoint para escáner WiFi manual
    server.on("/scanlan", handleLanScan); // Endpoint para escáner LAN
    server.on("/time", handleTimeRequest);
    server.on("/save", HTTP_POST, handleSaveConfig); // Nuevo endpoint para guardar config
    server.begin();
    // Serial.println("Servidor Web iniciado.");
    Serial.print("Para ver el estado, visita: http://");
    Serial.print(WiFi.localIP());
    Serial.println(":3000");
}

void loop() {
    ArduinoOTA.handle(); // Manejar actualizaciones OTA

    unsigned long currentMillis = millis();
    // Tarea 1: Imprimir la hora en el Serial cada minuto
    if (currentMillis - previousTimeUpdate >= timeInterval) {
      previousTimeUpdate = currentMillis;
      // Serial.print("\033[2J\033[H");
      // Serial.print("Hora: ");
      // Serial.println(getFormattedTime());
    }

    // Tarea 2: Actualizar todos los datos de red cada 29 minutos 
    if (currentMillis - previousIpUpdate >= ipInterval) {
      previousIpUpdate = currentMillis;
      updateNetworkData();
    }
    
    // Tarea 3: Monitor de Latencia (Cada 45 segundos)
    if (currentMillis - previousPingUpdate >= pingInterval) {
      previousPingUpdate = currentMillis;
      updateLatencyData();
    }
    
    // Tarea 4: Actualizar Historial RSSI (Cada 1 minuto)
    if (currentMillis - previousRssiUpdate >= rssiInterval) {
      previousRssiUpdate = currentMillis;
      updateRssiHistory();
    }

    // Tarea 5: Atender las peticiones del cliente web
    server.handleClient();
}
