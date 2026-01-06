# 📟 Monitor de Infraestructura y Red con ESP8266 (v2.2)

Este proyecto transforma un microcontrolador ESP8266 en una navaja suiza para el diagnóstico de redes. A través de un servidor web autónomo, permite monitorear la salud de tu conexión a internet, la estabilidad del WiFi y descubrir dispositivos en tu red local.

## ✨ Características Principales

### 1. 📊 Dashboard en Tiempo Real
Interfaz web responsiva con carrusel automático (Slide Show) y modo oscuro automático.
*   **Estado General:** Uptime, IPs (Privada/Pública), Memoria Libre, Info del Chip.
*   **Favicon Dinámico:** SVG incrustado (📟).

### 2. ⏱️ Monitor de Latencia (WAN)
Detecta micro-cortes y lentitud en tu conexión a Internet.
*   Realiza Pings periódicos (cada 45s) a un host configurable (ej. `8.8.8.8`).
*   Muestra estado visual: 🟢 Estable, 🟡 Lento/Inestable, 🔴 Sin Conexión.
*   Métricas: Latencia media (ms) y % de Pérdida de Paquetes.

### 3. 📉 Gráfico Histórico de Señal WiFi
Visualiza la calidad de tu conexión WiFi en la última hora.
*   Gráfico **SVG generado en el dispositivo** (sin librerías JS externas).
*   Código de colores semaforizado según la intensidad (dBm).

### 4. 🕸️ Escáner LAN (ICMP)
Descubre qué dispositivos están conectados a tu red.
*   Escanea todo el segmento de red `/24` (IPs .1 a .254).
*   Utiliza Ping (ICMP) optimizado (50ms timeout) para una detección rápida.
*   **Nota:** Incluye librería `ESP8266Ping` localmente modificada para mayor velocidad.

### 5. 🚀 Prueba de Velocidad
*   Mide el ancho de banda de descarga real descargando un archivo de prueba.

### 6. ⚙️ Configuración Persistente
Guarda tus preferencias en la memoria EEPROM (no se borran al reiniciar):
*   **Nombre/Descripción:** (Ej. "Oficina", "Casa").
*   **Dominio IP Pública:** Servicio para obtener la IP WAN (Ej. `ifconfig.me`).
*   **Host de Latencia:** IP o Dominio al cual hacer Ping (Ej. `1.1.1.1` o `google.com`).

## 📋 Requisitos

### Hardware
*   Cualquier placa basada en ESP8266 (NodeMCU, Wemos D1 Mini).

### Software
*   [Arduino IDE](https://www.arduino.cc/en/software).
*   Librerías necesarias (Instalar desde el Gestor):
    *   `ESP8266WiFi`
    *   `ESP8266WebServer`
    *   `WiFiManager` (por tzapu)
*   *Nota:* La librería de Ping ya está incluida en la carpeta del proyecto (`ESP8266Ping.h/cpp`), no es necesario instalarla aparte.

## 🚀 Instalación

1.  Clona este repositorio o descarga los archivos.
2.  Abre `Esp8266_WebServer.ino` en Arduino IDE.
3.  Sube el código a tu placa.
4.  **Primera vez:** Conéctate a la red WiFi `Esp8266-XXXX` y configura tu WiFi local desde el Portal Cautivo.
5.  Accede al navegador usando la IP asignada (puerto 3000). Ej: `http://192.168.1.50:3000`.

## 🛠️ Historial de Versiones

*   **v2.2:** Host de latencia configurable, Ping cada 45s.
*   **v2.1:** Eliminado módulo de Clima. Limpieza de código.
*   **v2.0:** Añadido Gráfico SVG histórico de RSSI.
*   **v1.9:** Implementación de Monitor de Latencia WAN.
*   **v1.8:** Implementación de Escáner LAN.
*   **v1.7:** Configuración persistente (EEPROM).

---
*Desarrollado por Juan Maioli.*