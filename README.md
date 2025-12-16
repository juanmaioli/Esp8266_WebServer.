# 🤖 Monitor de Red y Clima con ESP8266

Este proyecto transforma un microcontrolador ESP8266 en un completo monitor de sistema y red. A través de un servidor web integrado, muestra información detallada del dispositivo, datos climáticos externos y una lista de las redes WiFi cercanas en una interfaz moderna y responsiva.

## ✨ Características Principales

-   **Portal Cautivo (WiFiManager):** Configuración de red WiFi sencilla la primera vez, sin credenciales en el código. El ESP8266 crea un AP para que te conectes y configures la red local.
-   **Servidor Web Avanzado:** Interfaz web en el puerto `3000`, responsiva (PC/móvil) y con soporte para tema claro/oscuro automático.
-   **Monitor de Dispositivo Completo:**
    -   **Red:** IP Privada/Pública, Máscara, Gateway, RSSI y MAC.
    -   **Hardware:** ID del Chip, memoria flash, memoria libre y tiempo de actividad.
    -   **Hora:** Fecha y Hora sincronizadas por NTP (Argentina, GMT-3).
-   **Datos Externos:** Obtiene y muestra datos de clima desde el servicio de `pikapp.com.ar`.
-   **Escáner WiFi:** Detecta, ordena por potencia y muestra las redes WiFi cercanas, indicando si son abiertas o seguras.
-   **Prueba de Velocidad:** Mide la velocidad de descarga de la conexión a internet directamente desde el dispositivo.
-   **Actualizaciones Dinámicas y Automáticas:**
    -   **Hora dinámica:** La hora se actualiza cada 15 minutos sin recargar la página.
    -   **Refresco de página:** La página web completa se recarga cada 20 minutos.
    -   **Actualización de datos:** Los datos de red (IP pública, escaneo WiFi) y clima se actualizan en segundo plano cada 29 minutos.

## 📋 Requisitos

### Hardware
-   Una placa de desarrollo basada en ESP8266 (ej. NodeMCU, WEMOS D1 Mini).

### Software
-   [Arduino IDE](https://www.arduino.cc/en/software).
-   El paquete de soporte para placas ESP8266 instalado en el IDE de Arduino.
-   La biblioteca **`WiFiManager`** de `tzapu`. Puedes instalarla desde el "Gestor de Bibliotecas" en el IDE de Arduino.

## 🚀 Instalación y Puesta en Marcha

1.  Abre el archivo `Esp8266_WebServer.ino` en el IDE de Arduino.
2.  Asegúrate de tener instalada la biblioteca `WiFiManager` como se indica en los requisitos.
3.  Sube (flashea) el código a tu placa ESP8266.
4.  Abre el **Monitor Serie** con una velocidad de `115200` baudios para ver los mensajes de estado.

### Primera Configuración (vía WiFiManager)

-   La primera vez que el dispositivo se inicie (o si no puede conectarse a la red guardada), creará un **Punto de Acceso WiFi** llamado `WiFiSensor-XXXX` (donde `XXXX` son los últimos 4 dígitos de su MAC).
-   Conéctate a esa red WiFi desde tu teléfono, tablet o PC.
-   Una vez conectado, se debería abrir automáticamente un **portal de configuración** en tu navegador. Si no es así, abre el navegador y ve a la dirección `192.168.4.1`.
-   En el portal, selecciona tu red WiFi local, introduce la contraseña y haz clic en "Guardar".
-   El dispositivo se reiniciará y se conectará a tu red. El Monitor Serie te mostrará la dirección IP que le fue asignada.

## 💻 Uso

1.  Una vez que el dispositivo esté conectado a tu red, abre un navegador web.
2.  Navega a la dirección que se mostró en el Monitor Serie, seguida del puerto `3000`. Por ejemplo: `http://192.168.1.100:3000`.
3.  Verás la página de estado con el carrusel de información. Puedes usar las flechas para navegar entre las diferentes vistas o esperar a que roten automáticamente cada 30 segundos.

---
*Basado en el código original de Juan Maioli.*
