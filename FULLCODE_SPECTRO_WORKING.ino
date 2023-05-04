#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <LittleFS.h>

// WiFi credentials
const char* ssid = "Adrian2";
const char* password = "12345678";

// Pins for STP08DP05MTR
const int OE = 5;
const int LE = 6;
const int CLK = 7;
const int SDI = 8;

// Initial values for the LEDs
const int LED_OFF = 0;
const int LED_ON = 1;

// Current state of the LEDs
int ledState[8] = {LED_OFF, LED_OFF, LED_OFF, LED_OFF, LED_OFF, LED_OFF, LED_OFF, LED_OFF};

const int PD=13 ; 

int LEDvals[8];
float differences[8];

// Web server on port 80
WebServer server(80);


// Handle requests to the root URL
void handleRoot() {
  String html = "<html><head><title style='font-size: 32px;'>Group 52 Spectrometer</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "</head><body>";
  html += "<h1>Group 52 Spectrometer</h1>";
  html += "<form method=\"post\" action=\"/led\">";
  html += "<button type=\"submit\" name=\"calibrate\" value=\"235\">Click Here to Start Calibration</button><br>";
  //html += "<p>Click a button to turn on an LED:</p>";
  html += "<br>"; // new line

  html += "<form method=\"post\" action=\"/led\">";
  html += "<button name=\"samplestart\" value=\"233\">Click Here to Start Sample Collection</button><br>";
  // for (int i = 0; i < 8; i++) {
  //   html += "<button name=\"led\" value=\"";
  //   html += i;
  //   html += "\">LED ";
  //   html += i + 1;
  //   html += "</button><br>";
  // }
  html += "</form>";
  html += "<h2>Absorbtion Spectrum</h2>"; 
  html += "<canvas id='myChart'></canvas>";
  html += "<script>";
  html += "var ctx = document.getElementById('myChart').getContext('2d');";
  html += "var xValues = [440,470,505,525,590,660];";
  html += "var yValues = [";
  for (int i = 0; i < 6; i++) {
    html += String(differences[i]);
    if (i < 7) {
      html += ",";
    }
  }
  html += "];";
  html += "var myChart = new Chart(ctx, {type: 'line', data: {labels: xValues, datasets: [{tension: 0.2, label: 'Absorbance Spectrum', data: yValues, backgroundColor: 'rgba(34, 170, 255, 0.4)', borderColor: 'rgba(34, 170, 255, 1)', borderWidth: 2}]}, options: {scales: {x: {type: 'linear',title: {color: 'black',display: true,text: 'Wavelength(nm)'}},y: {type: 'linear',title: {color: 'black',display: true,text: 'Absorbance'},max: 1,min: 0,ticks: {stepSize: 0.05}}}}});";  // Function to update the chart with new data

  // Function to update the chart with new data
  html += "function updateChart(yValues) {";
  html += "  myChart.data.datasets[0].data = yValues;";
  html += "  myChart.update();";
  html += "}";

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}





void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("Formatting file system...");
    LittleFS.format();
    LittleFS.begin();
  }
  //Create Static IP
  //IPAddress staticIP(192, 168, 1, 100);  
  //IPAddress gateway(192, 168, 1, 1);     
  //IPAddress subnet(255, 255, 255, 0);   

 // WiFi.config(staticIP, gateway, subnet);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

// Print the IP address for the webpage
  Serial.print("Webpage available at: ");
  Serial.println(WiFi.localIP());
  
  // Set pins for STP08DP05MTR
  pinMode(LE, OUTPUT);
  pinMode(SDI, OUTPUT);
  pinMode(OE, OUTPUT);
  pinMode(CLK, OUTPUT);

  // Initialize STP08DP05MTR
  digitalWrite(OE, HIGH);
  digitalWrite(LE, LOW);
  digitalWrite(SDI, LOW);
  digitalWrite(CLK, LOW);
  //digitalWrite(OE, LOW);

  // Turn off all LEDs
  updateLEDs();
  // Web server routes
  server.on("/", handleRoot);
  server.on("/led", handleLED);
  server.begin();
}

void loop() {
  server.handleClient();
}



// Handle requests to turn on an LED
void handleLED() {
  String sweep_wd = server.arg("samplestart");
  int sw = sweep_wd.toInt();
  String led = server.arg("led");
  int index = led.toInt();
  String cali_wd = server.arg("calibrate");
  int ca = cali_wd.toInt();
  
 
  //Serial.print("Sweep state is ");
  //Serial.println(sweep_wd);
  //Serial.print("LED state is ");
  //Serial.println(led);
  //Serial.print("Calibrate state is ");
  //Serial.println(cali_wd);

  
  if (ca == 235){
    digitalWrite(OE, LOW);
    Serial.print("Entering Calibration Process...");
    Serial.println();
    calibrate();
   // Clean States
    for (int j = 0; j < 8; j++) {
      ledState[j] = LED_OFF;
    }
    for (int i = 7; i >= 0; i--) {
      digitalWrite(SDI, ledState[i]);
      digitalWrite(CLK, HIGH);
      digitalWrite(CLK, LOW);
    }
    //delay(10);
    // Latch the new LED state
    digitalWrite(LE, HIGH);
    digitalWrite(LE, LOW);
    calidone();
  }
  if (sw == 233) {
    digitalWrite(OE, LOW);
    Serial.println();
    Serial.print("Analyzing Sample...");
    Serial.println();
    sweep();
    // Clean States
    for (int j = 0; j < 8; j++) {
      ledState[j] = LED_OFF;
    }
    for (int i = 7; i >= 0; i--) {
      digitalWrite(SDI, ledState[i]);
      digitalWrite(CLK, HIGH);
      digitalWrite(CLK, LOW);
    }
    //delay(10);
    // Latch the new LED state
    digitalWrite(LE, HIGH);
    digitalWrite(LE, LOW);
  }
  
  // Turn off all LEDs
  for (int j = 0; j < 8; j++) {
    ledState[j] = LED_OFF;
  }
  // Turn on selected LED
  ledState[index] = LED_ON;
  // Update the LEDs
  updateLEDs();
  digitalWrite(OE, HIGH);
  server.sendHeader("Location", "/");
  server.send(302);
}


// Update the state of the LEDs on the STP08DP05MTR
void updateLEDs() {
  // Send the new LED state to the STP08DP05MTR
  for (int i = 7; i >= 0; i--) {
    digitalWrite(SDI, ledState[i]);
    digitalWrite(CLK, HIGH);
    digitalWrite(CLK, LOW);
  }
  // Latch the new LED state
  digitalWrite(LE, HIGH);
  digitalWrite(LE, LOW);
}
void calibrate(){
  int sum =0;
  int avg=0;
  
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  File file = LittleFS.open("/LEDdata.txt", "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  for (int i = 0; i <= 7; i++) {
    // Turn off all LEDs
    for (int j = 0; j < 8; j++) {
      ledState[j] = LED_OFF;
    }
    ledState[i] = LED_ON;
    for (int i = 7; i >= 0; i--) {
      digitalWrite(SDI, ledState[i]);
      digitalWrite(CLK, HIGH);
      digitalWrite(CLK, LOW);
    }
    // Latch the new LED state
    delay(500);
    digitalWrite(LE, HIGH);
    digitalWrite(LE, LOW);
    sum=0;
    avg=0;
    for (int i = 0; i < 10000; i++) {
      int PDValue = analogRead(PD);
      sum+= PDValue;
      
    }
    avg= sum/10000;
    
    
    
    
    //int PDValue = analogRead(PD);
    Serial.print("Sensor reading for LED ");
    Serial.print(i+1);
    Serial.print(": ");
    Serial.println(avg);

    
    if (i > 0) {
      file.print(",");
      }
      file.print(avg);
    }
    file.println(); // Add a new line after LED data is written
    file.close();
  

    delay(500);
   
}
  
void sweep() {
  // Turn on LEDs sequentially
  int sum =0;
  int avg=0;
  for (int i = 0; i <= 7; i++) {
    // Turn off all LEDs
    for (int j = 0; j < 8; j++) {
      ledState[j] = LED_OFF;
    }
    ledState[i] = LED_ON;
    for (int i = 7; i >= 0; i--) {
      digitalWrite(SDI, ledState[i]);
      digitalWrite(CLK, HIGH);
      digitalWrite(CLK, LOW);
    }
    // Latch the new LED state
    delay(500);
    digitalWrite(LE, HIGH);
    digitalWrite(LE, LOW);
    sum=0;
    avg=0;
    for (int i = 0; i < 10000; i++) {
      int PDValue = analogRead(PD);
      sum+= PDValue;
      
    }
    avg= sum/10000;
    
    //int PDValue = analogRead(PD);
    Serial.print("Sensor reading for LED ");
    Serial.print(i+1);
    Serial.print(": ");
    //Serial.println(PDValue);
    Serial.println(avg);

    LEDvals[i]= avg;
    

    delay(500);
   
  }
  
  Serial.println();
  Serial.print("Finished with data collection, update Graph ");
  updateGraph();
}



void updateGraph() {
  // Send new data to the webpage
  //Serial.print("Checkpoint 1 ");
  float difference[8];
  memset(difference, 0, sizeof(difference));
  File file = LittleFS.open("/LEDdata.txt", "r");
  if (!file) {
     Serial.println("Failed to open file for calculation, did you calibrate?");
     return;
  }
  //Serial.print("Checkpoint 2 ");
  int values[8];
  int index = 0;
  int lastComma = -1;
  while (file.available()) {
    index = 0;
    lastComma = -1;
    String line = file.readStringUntil('\n');
    for (int i = 0; i < line.length(); i++) {
      if (line.charAt(i) == ',') {
        values[index] = line.substring(lastComma + 1, i).toInt();
        index++;
        lastComma = i;
        if (index >= 8) { // add the check here
          index = 0;
        }
      }
    }
    values[index] = line.substring(lastComma + 1).toInt();
  }
  file.close();
  Serial.println();
  Serial.println("Calibrated Values array:");
  for (int i = 0; i < 8; i++) { 
     Serial.print(values[i]);
     Serial.print(" ");
  }
  Serial.println();
  Serial.println("Sample LEDvals array:");
  for (int i = 0; i < 8; i++) {
    Serial.print(LEDvals[i]);
    Serial.print(" ");
  }
  
  //Serial.print("Checkpoint 3 ");
  //String message = "updateGraph([";
  Serial.println("Difference array:");
  for (int i = 0; i < 8; i++) {
    difference[i] = float(values[i] - LEDvals[i])/float(values[i]); 
    if (difference[i] < 0.0) {
      differences[i]=0.0;
    } else{
      differences[i]=difference[i];
    }
    Serial.print(differences[i]);
    Serial.print(" ");
    //message += "[" + String(i+1) + "," + String(difference[i]) + "],";
  }
  sendChart();
  
  //message.remove(message.length() - 1); // remove the last comma
  //message += "]);";
  
Serial.println();
  //server.send(200, "text/plain", message);
  
}

void sendChart(){
  String html = "<html><head><title style='font-size: 32px;'>Group 52 Spectrometer</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "</head><body>";
  html += "<h1>Group 52 Spectrometer</h1>";
  html += "<form method=\"post\" action=\"/led\">";
  html += "<button type=\"submit\" name=\"calibrate\" value=\"235\">Click Here to Start Calibration</button><br>";
  //html += "<p>Click a button to turn on an LED:</p>";
  html += "<br>"; // new line

  html += "<form method=\"post\" action=\"/led\">";
  html += "<button name=\"samplestart\" value=\"233\">Click Here to Start Sample Collection</button><br>";
  html += "<br>"; // new line
  html += "<p id='sampleMessage' style='color: green;'>Sample Analysis Done</p>";
  // for (int i = 0; i < 8; i++) {
  //   html += "<button name=\"led\" value=\"";
  //   html += i;
  //   html += "\">LED ";
  //   html += i + 1;
  //   html += "</button><br>";
  // }
  html += "</form>";
  html += "<h2>Absorbtion Spectrum</h2>"; 
  html += "<canvas id='myChart'></canvas>";
  html += "<script>";
  html += "var ctx = document.getElementById('myChart').getContext('2d');";
  html += "var xValues = [440,470,505,525,590,660];";
  html += "var yValues = [";
  for (int i = 0; i < 6; i++) {
    html += String(differences[i]);
    if (i < 7) {
      html += ",";
    }
  }
  html += "];";
  html += "var myChart = new Chart(ctx, {type: 'line', data: {labels: xValues, datasets: [{label: 'Absorbance Spectrum', data: yValues, backgroundColor: 'rgba(34, 170, 255, 0.4)', borderColor: 'rgba(34, 170, 255, 1)', borderWidth: 2, lineTension: 0.2}]}, options: {scales: {xAxes: [{scaleLabel: {display: true, labelString: 'Wavelength (nm)'}, ticks: {min: 400, max: 700}}], yAxes: [{scaleLabel: {display: true, labelString: 'Absorbance'}, ticks: {min:0, max: 1}}]}, elements: { line: { tension: 0.2 } }}});";

  // Function to update the chart with new data
  html += "function updateChart(yValues) {";
  html += "  myChart.data.datasets[0].data = yValues;";
  html += "  myChart.update();";
  html += "}";

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}


void calidone(){
  String html = "<html><head><title style='font-size: 32px;'>Group 52 Spectrometer</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  html += "</head><body>";
  html += "<h1>Group 52 Spectrometer</h1>";
  html += "<form method=\"post\" action=\"/led\">";
  html += "<button type=\"submit\" name=\"calibrate\" value=\"235\">Click Here to Start Calibration</button><br>";
  //html += "<p>Click a button to turn on an LED:</p>";
  html += "<br>"; // new line
  html += "<p id='sampleMessage' style='color: green;'>Calibration Done</p>";

  html += "<form method=\"post\" action=\"/led\">";
  html += "<button name=\"samplestart\" value=\"233\">Click Here to Start Sample Collection</button><br>";

  
  // for (int i = 0; i < 8; i++) {
  //   html += "<button name=\"led\" value=\"";
  //   html += i;
  //   html += "\">LED ";
  //   html += i + 1;
  //   html += "</button><br>";
  // }
  html += "</form>";
  html += "<h2>Absorbtion Spectrum</h2>"; 
  html += "<canvas id='myChart'></canvas>";
  html += "<script>";
  html += "var ctx = document.getElementById('myChart').getContext('2d');";
  html += "var xValues = [440,470,505,525,590,660];";
  html += "var yValues = [";
  for (int i = 0; i < 6; i++) {
    html += String(differences[i]);
    if (i < 7) {
      html += ",";
    }
  }
  html += "];";
  html += "var myChart = new Chart(ctx, {type: 'line', data: {tension: 0.2, labels: xValues, datasets: [{label: 'Absorbance Spectrum', data: yValues, backgroundColor: 'rgba(34, 170, 255, 0.4)', borderColor: 'rgba(34, 170, 255, 1)', borderWidth: 2, lineTension: 0.2}]}, options: {scales: {x: {type: 'linear',title: {color: 'black',display: true,text: 'Wavelength(nm)'}},y: {type: 'linear',title: {color: 'black',display: true,text: 'Absorbance'},max: 1,min: 0,ticks: {stepSize: 0.05}}}}});";

  // Function to update the chart with new data
  html += "function updateChart(yValues) {";
  html += "  myChart.data.datasets[0].data = yValues;";
  html += "  myChart.update();";
  html += "}";

  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}



