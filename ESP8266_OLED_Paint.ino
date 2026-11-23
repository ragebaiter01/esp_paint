/*
  ESP8266 Wemos D1 mini + SSD1306 OLED (128x64)
  ----------------------------------------------
  Starts a small web server with a 128x64 pixel drawing canvas. Strokes are
  sent as line segments to the ESP8266, which draws them on the OLED.

  Libraries (Arduino Library Manager):
    - Adafruit SSD1306
    - Adafruit GFX Library
  ESP8266WiFi, ESP8266WebServer and Wire are included with the ESP8266 board
  package ("esp8266 by ESP8266 Community").

  Default I2C wiring on a Wemos D1 mini:
    OLED SDA -> D2 (GPIO4)
    OLED SCL -> D1 (GPIO5)
    OLED VCC -> 3V3, GND -> G
  Most SSD1306 modules use I2C address 0x3C. Change OLED_ADDRESS to 0x3D
  if that is the address of your display.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ----- Network configuration -----
enum NetworkMode : uint8_t {
  AP_ONLY,       // Always provide the ESP-Paint access point.
  WIFI_ONLY,     // Join the existing Wi-Fi network only.
  AP_AND_WIFI    // Provide the access point and join the existing Wi-Fi.
};

// Choose AP_ONLY, WIFI_ONLY or AP_AND_WIFI.
const NetworkMode NETWORK_MODE = AP_ONLY;

const char AP_SSID[] = "ESP-Paint";
const char AP_PASSWORD[] = "password123"; // At least 8 characters for WPA2.

// Used by WIFI_ONLY and AP_AND_WIFI. Enter your router credentials here.
const char WIFI_SSID[] = "YOUR_WIFI_NAME";
const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// ----- Display configuration -----
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1; // No separate reset pin on most I2C modules.
constexpr uint8_t OLED_ADDRESS = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
ESP8266WebServer server(80);

// This page is served directly from flash (PROGMEM), preserving ESP8266 RAM.
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>ESP-Paint</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { margin: 0; padding: 20px; background: #15171a; color: #f4f4f4; }
    main { max-width: 560px; margin: 0 auto; }
    h1 { margin: 0 0 6px; font-size: 1.35rem; }
    p { margin: 0 0 14px; color: #b9bec6; }
    .frame { width: 100%; max-width: 512px; aspect-ratio: 2 / 1; }
    canvas { display: block; width: 100%; height: 100%; box-sizing: border-box;
      border: 2px solid #7a8490; background: #000; image-rendering: pixelated;
      touch-action: none; cursor: crosshair; }
    button { margin-top: 14px; padding: 9px 16px; border: 0; border-radius: 6px;
      background: #e5e7eb; color: #111; font-size: 1rem; cursor: pointer; }
    #status { min-height: 1.3em; margin-top: 10px; font-size: .9rem; }
  </style>
</head>
<body>
  <main>
    <h1>ESP-Paint</h1>
    <p>Draw with a finger or mouse. The canvas is exactly 128 × 64 pixels.</p>
    <div class="frame"><canvas id="canvas" width="128" height="64"></canvas></div>
    <button id="clear" type="button">Clear display</button>
    <div id="status" aria-live="polite"></div>
  </main>
  <script>
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    const status = document.getElementById('status');
    let drawing = false;
    let last = null;
    const jobs = [];
    let sending = false;

    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, 128, 64);
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1;
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    function pixelPoint(event) {
      const r = canvas.getBoundingClientRect();
      return {
        x: Math.max(0, Math.min(127, Math.floor((event.clientX - r.left) * 128 / r.width))),
        y: Math.max(0, Math.min(63, Math.floor((event.clientY - r.top) * 64 / r.height)))
      };
    }

    function enqueue(job) {
      jobs.push(job);
      pump();
    }

    async function pump() {
      if (sending || jobs.length === 0) return;
      sending = true;
      const job = jobs.shift();
      try {
        const body = new URLSearchParams(job);
        const response = await fetch(job.clear ? '/clear' : '/line', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body
        });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        status.textContent = 'Connected';
      } catch (error) {
        status.textContent = 'Connection error. Reload the page to reconnect.';
      }
      sending = false;
      pump();
    }

    function point(event) {
      const p = pixelPoint(event);
      if (last) {
        ctx.beginPath();
        ctx.moveTo(last.x + 0.5, last.y + 0.5);
        ctx.lineTo(p.x + 0.5, p.y + 0.5);
        ctx.stroke();
        if (last.x !== p.x || last.y !== p.y) {
          enqueue({ x0: last.x, y0: last.y, x1: p.x, y1: p.y });
        }
      } else {
        // A tap without movement still draws and transmits one pixel.
        ctx.fillStyle = '#fff';
        ctx.fillRect(p.x, p.y, 1, 1);
        ctx.fillStyle = '#000';
        enqueue({ x0: p.x, y0: p.y, x1: p.x, y1: p.y });
      }
      last = p;
    }

    canvas.addEventListener('pointerdown', event => {
      event.preventDefault();
      drawing = true;
      last = null;
      canvas.setPointerCapture(event.pointerId);
      point(event);
    });
    canvas.addEventListener('pointermove', event => {
      if (!drawing) return;
      event.preventDefault();
      point(event);
    });
    function stopDrawing() { drawing = false; last = null; }
    canvas.addEventListener('pointerup', stopDrawing);
    canvas.addEventListener('pointercancel', stopDrawing);
    canvas.addEventListener('lostpointercapture', stopDrawing);

    document.getElementById('clear').addEventListener('click', () => {
      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, 128, 64);
      ctx.fillStyle = '#000';
      enqueue({ clear: '1' });
    });
  </script>
</body>
</html>
)rawliteral";

// Check that a request parameter is a non-negative integer within the screen.
bool readPixelArgument(const char *name, int maximum, int &value) {
  if (!server.hasArg(name)) return false;
  const String text = server.arg(name);
  if (text.length() == 0) return false;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
  }
  const long parsed = text.toInt();
  if (parsed < 0 || parsed > maximum) return false;
  value = static_cast<int>(parsed);
  return true;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleLine() {
  int x0, y0, x1, y1;
  if (!readPixelArgument("x0", SCREEN_WIDTH - 1, x0) ||
      !readPixelArgument("y0", SCREEN_HEIGHT - 1, y0) ||
      !readPixelArgument("x1", SCREEN_WIDTH - 1, x1) ||
      !readPixelArgument("y1", SCREEN_HEIGHT - 1, y1)) {
    server.send(400, "text/plain", "Invalid or missing pixel coordinates");
    return;
  }

  display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
  display.display(); // Transfer the framebuffer to the OLED now.
  server.send(204, "text/plain", "");
}

void handleClear() {
  display.clearDisplay();
  display.display();
  server.send(204, "text/plain", "");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void startNetwork() {
  if (NETWORK_MODE == AP_ONLY) {
    WiFi.mode(WIFI_AP);
  } else if (NETWORK_MODE == WIFI_ONLY) {
    WiFi.mode(WIFI_STA);
  } else {
    WiFi.mode(WIFI_AP_STA);
  }

  if (NETWORK_MODE == AP_ONLY || NETWORK_MODE == AP_AND_WIFI) {
    if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
      Serial.println(F("Could not start access point"));
    } else {
      Serial.print(F("Access point: "));
      Serial.println(AP_SSID);
      Serial.print(F("Open http://"));
      Serial.println(WiFi.softAPIP());
    }
  }

  if (NETWORK_MODE == WIFI_ONLY || NETWORK_MODE == AP_AND_WIFI) {
    Serial.print(F("Connecting to Wi-Fi: "));
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000UL) {
      delay(250);
      yield();
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("Wi-Fi connected. Open http://"));
      Serial.println(WiFi.localIP());
    } else {
      Serial.println(F("Wi-Fi connection timed out."));
      if (NETWORK_MODE == WIFI_ONLY) {
        Serial.println(F("Check credentials and reset the board."));
      } else {
        Serial.println(F("The access point is still available."));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // Wemos D1 mini defaults: SDA = D2/GPIO4, SCL = D1/GPIO5.
  Wire.begin(D2, D1);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 not found. Check wiring and I2C address."));
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  startNetwork();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/line", HTTP_POST, handleLine);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("HTTP server started"));
}

void loop() {
  server.handleClient();
  yield();
}
