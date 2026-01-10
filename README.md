# 📟 Monitor de Infraestructura y Red con ESP8266 (v2.5.3)

Este proyecto transforma un microcontrolador ESP8266 en una navaja suiza para el diagnóstico de redes. A través de un servidor web autónomo y optimizado, permite monitorear la salud de tu conexión a internet, la estabilidad del WiFi y descubrir dispositivos en tu red local.

## ✨ Características Principales

### 1. 📊 Dashboard en Tiempo Real
Interfaz web responsiva con carrusel manual, navegación inferior integrada y soporte nativo para modo oscuro.
*   **Estado General:** Uptime, IPs (Privada/Pública), Memoria Libre, Info del Chip, Versión de Firmware.
*   **Optimización de Memoria:** Utiliza *Chunked Transfer Encoding* para servir la interfaz sin saturar la RAM del ESP8266.
*   **Favicon Dinámico:** SVG incrustado (📟).

### 2. ⏱️ Monitor de Latencia (WAN)
Detecta micro-cortes y lentitud en tu conexión a Internet.
*   Realiza Pings periódicos (cada 45s) a un host configurable (ej. `8.8.8.8`).
*   Muestra estado visual: 🟢 Estable, 🟡 Lento/Inestable, 🔴 Sin Conexión.
*   Métricas: Latencia media (ms) y % de Pérdida de Paquetes.

### 3. 📉 Gráfico Histórico de Señal WiFi
Visualiza la calidad de tu conexión WiFi en la última hora.
*   Gráfico **SVG generado en el dispositivo**.
*   **Diseño Adaptativo:** Se escala correctamente al ancho del dispositivo y respeta el tema oscuro/claro del sistema.
*   Código de colores semaforizado según la intensidad (dBm).

### 4. 🕸️ Escáneres de Red
*   **Escáner LAN (ICMP):** Descubre dispositivos en tu red `/24` (IPs .1 a .254) usando Ping optimizado.
*   **Escáner WiFi:** Visualiza redes inalámbricas cercanas, su potencia y tipo de encriptación. Incluye botón de escaneo manual.

### 5. 🚀 Prueba de Velocidad
*   Mide el ancho de banda de descarga real descargando un archivo de prueba.

### 6. ⚙️ Configuración Avanzada
Guarda tus preferencias en la memoria EEPROM:
*   **General:** Nombre del dispositivo, Dominio para obtener IP Pública.
*   **Red:** Host objetivo para medir latencia.
*   **OTA:** Contraseña configurable para actualizaciones inalámbricas.

### 7. 📲 Actualizaciones OTA
*   Soporte para cargar nuevo firmware de forma inalámbrica mediante ArduinoOTA.

## 📋 Requisitos

### Hardware
*   Cualquier placa basada en ESP8266 (NodeMCU, Wemos D1 Mini).

### Software
*   [Arduino IDE](https://www.arduino.cc/en/software).
*   Librerías necesarias (Instalar desde el Gestor):
    *   `ESP8266WiFi`
    *   `ESP8266WebServer`
    *   `WiFiManager` (por tzapu)
    *   `ArduinoOTA`
*   *Nota:* La librería de Ping ya está incluida en la carpeta del proyecto (`ESP8266Ping.h/cpp`).

### 🔧 Compilación (Importante para OTA)
Para asegurar suficiente espacio para las actualizaciones OTA, selecciona el siguiente esquema de partición en Arduino IDE:
*   **Placa:** NodeMCU 1.0 (ESP-12E Module)
*   **Flash Size:** 4MB (FS:1MB OTA:~1019KB)

## 🚀 Instalación

1.  Clona este repositorio o descarga los archivos.
2.  Abre `Esp8266_WebServer.ino` en Arduino IDE.
3.  Sube el código a tu placa (la primera vez por cable USB para ajustar la partición).
4.  **Primera vez:** Conéctate a la red WiFi `Esp8266-XXXX` y configura tu WiFi local desde el Portal Cautivo.
5.  Accede al navegador usando la IP asignada (puerto 3000). Ej: `http://192.168.1.50:3000`.

## 🛠️ Historial de Versiones

*   **v2.5.3:** Optimización de diseño en la slide de Configuración (campos alineados en filas).
*   **v2.5.2:** Reubicación de flechas de navegación a la parte inferior (junto a los puntos).
*   **v2.5.1:** Visualización de máscara de red en formato CIDR (ej. /24).
*   **v2.5.0:** Agregado botón para escaneo manual de redes WiFi.
*   **v2.4.x:** Soporte para actualizaciones OTA, configuración de contraseña OTA vía web, visualización de versión de firmware en dashboard y mejoras de diseño en formulario.
*   **v2.3:** Optimización crítica de memoria (Chunked response), corrección visual del gráfico WiFi (tamaño y tema oscuro), eliminación de retardos bloqueantes en arranque y cambio de carrusel a manual.
*   **v2.2:** Host de latencia configurable, Ping cada 45s.
*   **v2.1:** Eliminado módulo de Clima. Limpieza de código.
*   **v2.0:** Añadido Gráfico SVG histórico de RSSI.
*   **v1.9:** Implementación de Monitor de Latencia WAN.
*   **v1.8:** Implementación de Escáner LAN.
*   **v1.7:** Configuración persistente (EEPROM).

---
*Desarrollado por Juan Maioli.*