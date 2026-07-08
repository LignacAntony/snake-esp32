#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define CELL 4
#define GRID_W (SCREEN_WIDTH / CELL)
#define GRID_H (SCREEN_HEIGHT / CELL)
#define MAX_LEN (GRID_W * GRID_H)

#define PORT 7777

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebSocketsClient ws;
bool connected = false;

uint8_t snakeX[MAX_LEN], snakeY[MAX_LEN];
int snakeLen, dirX, dirY, nextDirX, nextDirY;
uint8_t foodX, foodY;
bool gameOver;
unsigned long lastTick = 0;
const unsigned long TICK_MS = 180;

void placeFood() {
  while (true) {
    uint8_t fx = random(GRID_W);
    uint8_t fy = random(GRID_H);
    bool onSnake = false;
    for (int i = 0; i < snakeLen; i++)
      if (snakeX[i] == fx && snakeY[i] == fy) { onSnake = true; break; }
    if (!onSnake) { foodX = fx; foodY = fy; return; }
  }
}

void resetGame() {
  snakeLen = 3;
  int cx = GRID_W / 2, cy = GRID_H / 2;
  for (int i = 0; i < snakeLen; i++) { snakeX[i] = cx - i; snakeY[i] = cy; }
  dirX = 1; dirY = 0;
  nextDirX = 1; nextDirY = 0;
  gameOver = false;
  placeFood();
}

void setDirection(int dx, int dy) {
  if (dx == -dirX && dy == -dirY) return;
  nextDirX = dx; nextDirY = dy;
}

void update() {
  if (gameOver) return;
  dirX = nextDirX; dirY = nextDirY;
  int nx = snakeX[0] + dirX;
  int ny = snakeY[0] + dirY;
  if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) { gameOver = true; return; }

  bool ate = nx == foodX && ny == foodY;
  for (int i = 0; i < snakeLen; i++) {
    if (!ate && i == snakeLen - 1) continue;
    if (snakeX[i] == nx && snakeY[i] == ny) { gameOver = true; return; }
  }

  if (ate && snakeLen < MAX_LEN) snakeLen++;
  for (int i = snakeLen - 1; i > 0; i--) { snakeX[i] = snakeX[i - 1]; snakeY[i] = snakeY[i - 1]; }
  snakeX[0] = nx; snakeY[0] = ny;
  if (ate) placeFood();
}

void draw() {
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.drawRect(foodX * CELL, foodY * CELL, CELL, CELL, SSD1306_WHITE);
  for (int i = 0; i < snakeLen; i++)
    display.fillRect(snakeX[i] * CELL, snakeY[i] * CELL, CELL, CELL, SSD1306_WHITE);

  if (!connected) {
    display.setCursor(2, 2);
    display.print("NO SERVER");
  }

  if (gameOver) {
    display.fillRect(18, 24, 92, 16, SSD1306_BLACK);
    display.drawRect(18, 24, 92, 16, SSD1306_WHITE);
    display.setCursor(24, 28);
    display.print("GAME OVER  ");
    display.print(snakeLen - 3);
  }
  display.display();
}

void onDirection(String msg) {
  msg.trim();
  if (gameOver) { resetGame(); return; }
  if (msg == "up") setDirection(0, -1);
  else if (msg == "down") setDirection(0, 1);
  else if (msg == "left") setDirection(-1, 0);
  else if (msg == "right") setDirection(1, 0);
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    connected = true;
    Serial.println("Connecte au serveur !");
  } else if (type == WStype_DISCONNECTED) {
    connected = false;
    Serial.println("Deconnecte du serveur");
  } else if (type == WStype_TEXT) {
    onDirection(String((char*)payload, length));
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin(8, 9);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connexion WiFi...");
  display.display();
  Serial.print("Connexion WiFi");

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi OK");
  display.println("");
  display.println("Serveur :");
  display.print(SERVER_HOST);
  display.print(":");
  display.println(PORT);
  display.display();
  Serial.print("Serveur : ");
  Serial.print(SERVER_HOST);
  Serial.print(":");
  Serial.println(PORT);
  delay(2000);

  randomSeed(esp_random());
  resetGame();

  ws.begin(SERVER_HOST, PORT, "/");
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(2000);
  ws.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  ws.loop();
  if (millis() - lastTick >= TICK_MS) {
    lastTick = millis();
    update();
    draw();
  }
}
