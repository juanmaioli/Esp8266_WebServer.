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
const char* hostname_prefix = "Esp8266-";
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
String downloadSpeed = "No medido";
// Variable para la velocidad de descarga
String lastSpeedTestTime = "Nunca";
// Variable para la hora de la última prueba de velocidad

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
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
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

void testDownloadSpeed() {
  Serial.println("\n--- Iniciando prueba de velocidad de descarga ---");
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
      Serial.println("Prueba de velocidad completada: " + downloadSpeed);
    } else {
      downloadSpeed = "Error en la prueba. Codigo: " + String(httpCode);
      Serial.println(downloadSpeed);
    }
    http.end();
  } else {
    downloadSpeed = "Fallo la conexión para la prueba de velocidad.";
    Serial.println(downloadSpeed);
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

void handleSpeedTest() {
  testDownloadSpeed();
  server.sendHeader("Location", String("/"), true);
  server.send(302, "text/plain", "");
}

// --- Handler para el Servidor Web ---
void handleRoot() {
    // --- Formateo de datos de Cava para la web ---
    String formattedCavaData = cavaData;

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

// --- Construcción de la página HTML ---
    String page = "<!DOCTYPE html><html lang='es'><head>";
    page += "<meta charset='UTF-8'>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    page += "<title>Estado del Dispositivo</title>";
    page += "<style>";
    page += ":root { --bg-color: #f0f2f5; --container-bg: #ffffff; --text-primary: #1c1e21; --text-secondary: #4b4f56; --pre-bg: #f5f5f5; --hr-color: #e0e0e0; --dot-color: #bbb; --dot-active-color: #717171; }";
    page += "@media (prefers-color-scheme: dark) {";
    page += ":root { --bg-color: #121212; --container-bg: #1e1e1e; --text-primary: #e0e0e0; --text-secondary: #b0b3b8; --pre-bg: #2a2a2a; --hr-color: #3e4042; --dot-color: #555; --dot-active-color: #ccc; }";
    page += "}";
    page += "body { background-color: var(--bg-color); color: var(--text-secondary); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 1rem 0;}";
    page += ".container { background-color: var(--container-bg); padding: 2rem; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); text-align: left; max-width: 40%; height: 80vh; position: relative; display: flex; flex-direction: column; }"; // PC view
    page += "@media (max-width: 768px) {"; // Mobile view
    page += ".container { max-width: 80%; height: 80vh; }";
    page += "}";
    page += "h1, h2 { color: var(--text-primary); margin-bottom: 1.5rem; }";
    page += "p { color: var(--text-secondary); font-size: 1.1rem; margin: 0.5rem 0; }";
    page += "strong { color: var(--text-primary); }";
    page += "hr { border: 0; height: 1px; background-color: var(--hr-color); margin: 1.5rem 0; }";
    page += ".carousel-container { position: relative; flex-grow: 1; overflow: hidden; }";
    page += ".carousel-slide { display: none; height: 100%; width: 100%; flex-basis: 100%; flex-shrink: 0; overflow-y: auto; padding-right: 15px; box-sizing: border-box; word-wrap: break-word; }";
    page += ".fade { animation-name: fade; animation-duration: 0.5s; }";
    page += "@keyframes fade { from {opacity: .4} to {opacity: 1} }";
    page += ".prev, .next { cursor: pointer; position: absolute; top: 50%; transform: translateY(-50%); width: auto; padding: 16px; color: var(--text-primary); font-weight: bold; font-size: 24px; transition: 0.3s; user-select: none; z-index: 10; }";
    page += ".prev { left: -50px; }";
    page += ".next { right: -50px; }";
    page += ".prev:hover, .next:hover { background-color: rgba(0,0,0,0.2); border-radius: 50%; }";
    page += ".dots { text-align: center; padding-top: 20px; }";
    page += ".dot { cursor: pointer; height: 15px; width: 15px; margin: 0 2px; background-color: var(--dot-color); border-radius: 50%; display: inline-block; transition: background-color 0.3s ease; }";
    page += ".active, .dot:hover { background-color: var(--dot-active-color); }";
    page += ".emoji-container { text-align: center; margin-top: 15px; margin-bottom: 15px; }";
    page += ".emoji { font-size: 4em; line-height: 1; display: inline-block; vertical-align: middle; }";
    page += ".button { background-color: #4CAF50; color: white; padding: 10px 20px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin: 10px 0; cursor: pointer; border-radius: 5px; }";
    page += ".button:hover { background-color: #45a049; }";
    page += ".button[disabled] { background-color: #cccccc; cursor: not-allowed; }";
    page += ".center-button { text-align: center; }";
    page += "@media (max-width: 768px) {"; // Mobile view
    page += ".container { max-width: 80%; height: 80vh; }";
    page += ".prev, .next { top: auto; bottom: 5px; transform: translateY(0); }";
    page += ".prev { left: 10px; }";
    page += ".next { right: 10px; }";
    page += "}";
    page += "</style></head><body><div class='container'>";
    
    // --- Navigation Buttons ---
    page += "<a class='prev' onclick='changeSlide(-1)'>&#10094;</a>";
    page += "<a class='next' onclick='changeSlide(1)'>&#10095;</a>";

    // --- Carousel ---
    page += "<div class='carousel-container'>";
    
    // --- Slide 1: Estado del Dispositivo ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Estado del Dispositivo</h2>";
    page += "<h3><strong>📅 Fecha:</strong> " + getFormattedDate() + "</h3>";
    page += "<h3><strong>⌚ Hora:</strong> " + getFormattedTime() + "</h3>";
    page += "<h3><strong>🖥️ Hostname:</strong> " + id_Wemos + "</h3>";
    page += "<h3><strong>🏠 IP Privada:</strong> " + localIP + "</h3>";
    page += "<h3><strong>↔️ M&aacute;scara de Red:</strong> " + WiFi.subnetMask().toString() + "</h3>";
    page += "<h3><strong>🚪 Puerta de Enlace:</strong> " + WiFi.gatewayIP().toString() + "</h3>";
    page += "<h3><strong>🌐 IP P&uacute;blica:</strong> " + publicIP + "</h3>";
    page += "<h3><strong>📶 Intensidad de Se&ntilde;al (RSSI):</strong> " + String(WiFi.RSSI()) + " dBm</h3>";
    page += "<h3><strong>🆔 Direcci&oacute;n MAC:</strong> " + WiFi.macAddress() + "</h3>";
    page += "<h3><strong>💡 ID del Chip (HEX):</strong> " + String(ESP.getChipId(), HEX) + "</h3>";
    page += "<h3><strong>💾 Memoria Flash:</strong> " + String(ESP.getFlashChipSize() / 1024) + " KB</h3>";
    page += "<h3><strong>🧠 Memoria Libre (Heap):</strong> " + String(ESP.getFreeHeap() / 1024.0, 2) + " KB</h3>";
    page += "<h3><strong>⚡ Tiempo de Actividad:</strong> " + uptime + "</h3>";
    page += "</div>";

    // --- Slide 2: Datos de Clima ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Datos del Clima</h2>";
    page += "<div><p>" + formattedCavaData + "</p></div>";
    page += "</div>";

    // --- Slide 3: Redes WiFi Cercanas ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Redes WiFi Cercanas</h2>";
    page += "<p><strong>Escaneado:</strong> " + lastWifiScanTime + "</p><hr>";
    page += wifiNetworksList;
    page += "</div>";

    // --- Slide 4: Prueba de Velocidad ---
    page += "<div class='carousel-slide fade'>";
    page += "<h2>Prueba de Velocidad</h2>";
    page += "<p><strong>&Uacute;ltima prueba:</strong> " + lastSpeedTestTime + "</p>";
    page += "<p><strong>Velocidad de Descarga:</strong> " + downloadSpeed + "</p>";
    page += "<div class='center-button'>";
    page += "<a href='/speedtest' id='speedtest-button' class='button' onclick='showWaiting()'>&#x1F680; Iniciar Prueba</a>";
    page += "</div>";
    page += "<p id='waiting-message' style='display:none; text-align:center;'>Por favor, espere mientras se realiza la prueba...</p>";
    page += "</div>";

    page += "</div>"; // end carousel-container

    // --- Dots ---
    page += "<div class='dots'>";
    page += "<span class='dot' onclick='currentSlide(1)'></span>";
    page += "<span class='dot' onclick='currentSlide(2)'></span>";
    page += "<span class='dot' onclick='currentSlide(3)'></span>";
    page += "<span class='dot' onclick='currentSlide(4)'></span>";
    page += "</div>";

    page += "</div>"; // end container

    // --- JavaScript ---
    page += "<script>";
    page += "let slideIndex = 1;";
    page += "showSlide(slideIndex);";
    page += "function changeSlide(n) { showSlide(slideIndex += n); }";
    page += "function currentSlide(n) { showSlide(slideIndex = n); }";
    page += "function showWaiting() {";
    page += "  document.getElementById('speedtest-button').setAttribute('disabled', 'true');";
    page += "  document.getElementById('speedtest-button').innerHTML = 'Midiendo...';";
    page += "  document.getElementById('waiting-message').style.display = 'block';";
    page += "}";
    page += "function showSlide(n) {";
    page += "let i; let slides = document.getElementsByClassName('carousel-slide');";
    page += "let dots = document.getElementsByClassName('dot');";
    page += "if (n > slides.length) { slideIndex = 1; }";
    page += "if (n < 1) { slideIndex = slides.length; }";
    page += "for (i = 0; i < slides.length; i++) { slides[i].style.display = 'none'; }";
    page += "for (i = 0; i < dots.length; i++) { dots[i].className = dots[i].className.replace(' active', ''); }";
    page += "slides[slideIndex - 1].style.display = 'block';";
    page += "dots[slideIndex - 1].className += ' active';";
    page += "}";
    page += "setInterval(function() { changeSlide(1); }, 30000);"; // Auto-rotate every 30 seconds
    page += "</script>";
    
    page += "</body></html>";

    server.send(200, "text/html", page);
}

// --- Setup y Loop ---
void setup() {
    delay(1500);
    Serial.begin(115200);
    serial_number = WiFi.softAPmacAddress();
    id_Wemos = String(hostname_prefix) + leftRepCadena(serial_number);
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
    server.on("/speedtest", handleSpeedTest);
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
