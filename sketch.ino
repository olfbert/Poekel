#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Zugangsdaten für das WLAN
const char* ssid = "poekel";
const char* password = "";

// Erstellen des AsyncWebServer-Objekts auf Port 80
AsyncWebServer server(80);

// OLED Display Einstellungen
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Fleisch Pökel-Rechner</title>
<style>
  body { font-family: Arial, sans-serif; }
  .container { max-width: 600px; margin: auto; padding: 20px; }
  .input-group { margin-bottom: 10px; }
  .input-group label { display: block; }
  .input-group input, .input-group select { width: 100%; padding: 10px; }
  .result { margin-top: 20px; }
  /* Dark mode styles */
  body.dark-mode { background-color: #333; color: #fff; }
  .dark-mode-btn { position: absolute; top: 20px; right: 20px; }
  .calculate-btn { position: absolute; bottom: 20px; right: 20px; }
</style>
</head>
<body>
<div class="container">
  <button class="dark-mode-btn" onclick="toggleDarkMode()">Dark Mode / Light Mode</button>
  <h1>Fleisch Pökel-Rechner</h1>
  <div class="input-group">
    <label for="meatType">Fleischsorte:</label>
    <input type="text" id="meatType" placeholder="z.B. Rind, Schwein, Huhn...">
  </div>
  <div class="input-group">
    <label for="thickness">Dicke des Fleisches (in cm):</label>
    <input type="number" id="thickness" placeholder="Dicke eingeben...">
  </div>
  <div class="input-group">
    <label for="weight">Gewicht des Fleisches (in kg):</label>
    <input type="number" id="weight" placeholder="Gewicht eingeben...">
  </div>
  <div class="input-group">
    <label for="startDate">Startdatum:</label>
    <input type="date" id="startDate">
  </div>
  <div class="input-group">
    <label for="saltAmount">Pökelsalzgehalt (g/kg): <span id="saltAmountDisplay">35</span></label>
    <input type="range" id="saltAmount" min="20" max="50" value="35" onchange="updateSaltAmountDisplay()">
  </div>
  <button class="calculate-btn" onclick="calculateCuring()">Berechnen</button>
  <div class="result" id="result"></div>
  <div class="result" id="saltResult"></div>
  <div class="result" id="endDateResult"></div>
  <button id="addToCalendar" style="display:none;" onclick="addToCalendar()">Zum Kalender hinzufügen</button>
</div>

<script>
function calculateCuring() {
  var meatType = document.getElementById('meatType').value;
  var thickness = parseFloat(document.getElementById('thickness').value);
  var weight = parseFloat(document.getElementById('weight').value);
  var startDate = new Date(document.getElementById('startDate').value);
  var curingTime = thickness + 2; // Ein Tag pro cm Fleischdicke plus zwei Tage Sicherheit
  var saltAmountPerKg = parseFloat(document.getElementById('saltAmount').value);
  var saltAmount = weight * saltAmountPerKg;
  
  // Korrektur der Berechnung des Enddatums
  var endDate = new Date(startDate.getTime());
  endDate.setDate(startDate.getDate() + curingTime);

  document.getElementById('result').innerText = 'Das ' + meatType + ' muss mindestens ' + curingTime + ' Tage gepökelt werden.';
  document.getElementById('saltResult').innerText = 'Menge des Pökelsalzes: ' + saltAmount.toFixed(2) + ' g';
  document.getElementById('endDateResult').innerText = 'Enddatum für das Pökeln: ' + endDate.toLocaleDateString('de-DE');
  
  document.getElementById('addToCalendar').style.display = 'block';
}

function addToCalendar() {
  var meatType = document.getElementById('meatType').value;
  var startDate = new Date(document.getElementById('startDate').value);
  var endDate = new Date(startDate.getTime());
  var curingTime = parseFloat(document.getElementById('thickness').value) + 2;
  endDate.setDate(startDate.getDate() + curingTime);
  var eventTitle = 'Pökelung von ' + meatType;
  var options = {
    title: eventTitle,
    start: startDate,
    end: endDate,
    allDay: false
  };
  
  var calendarURL = 'data:text/calendar;charset=utf-8,' + encodeURIComponent(createCalendarEvent(options));
  var downloadLink = document.createElement('a');
  downloadLink.href = calendarURL;
  downloadLink.download = 'event.ics';
  downloadLink.click();
}

function createCalendarEvent(options) {
  var event = [
    'BEGIN:VCALENDAR',
    'VERSION:2.0',
    'BEGIN:VEVENT',
    'UID:' + options.uid || Date.now(),
    'DTSTAMP:' + formatDate(new Date()),
    'DTSTART:' + formatDate(options.start),
    'DTEND:' + formatDate(options.end),
    'SUMMARY:' + options.title,
    'END:VEVENT',
    'END:VCALENDAR'
  ].join('\n');
  
  return event;
}

function formatDate(date) {
  var year = date.getFullYear();
  var month = (date.getMonth() + 1).toString().padStart(2, '0');
  var day = date.getDate().toString().padStart(2, '0');
  var hours = date.getHours().toString().padStart(2, '0');
  var minutes = date.getMinutes().toString().padStart(2, '0');
  var seconds = date.getSeconds().toString().padStart(2, '0');
  
  return year + month + day + 'T' + hours + minutes + seconds;
}

function toggleDarkMode() {
  document.body.classList.toggle('dark-mode');
}

function updateSaltAmountDisplay() {
  var saltAmountPerKg = document.getElementById('saltAmount').value;
  document.getElementById('saltAmountDisplay').innerText = saltAmountPerKg;
}
</script>
</body>
</html>
)rawliteral";

unsigned long startMillis;  // Startzeit des Geräts

void setup() {
  Serial.begin(115200);
  
  // Initialisieren des OLED Displays
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  
  // Begrüßungsnachricht anzeigen für 10 Sekunden
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print(F("Poekel-Rechner"));
  display.display();
  delay(10000); // 10 Sekunden warten
  
  // AP (Access Point) Modus einstellen
  Serial.print("Setting AP (Access Point)...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Startzeit speichern
  startMillis = millis();
  
  // Route für die Wurzel ("/") festlegen
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Server starten
  server.begin();
}

void loop() {
  // Berechne die vergangene Zeit seit dem Start
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - startMillis;
  
  // Berechne Stunden, Minuten und Sekunden
  unsigned long elapsedSeconds = elapsedMillis / 1000;
  unsigned int hours = elapsedSeconds / 3600;
  unsigned int minutes = (elapsedSeconds % 3600) / 60;
  unsigned int seconds = elapsedSeconds % 60;
  
  // Anzeige aktualisieren
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.printf("Online seit: %02u:%02u:%02u", hours, minutes, seconds);
  display.display();
}
