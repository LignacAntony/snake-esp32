#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"
#include <Fonts/TomThumb.h>

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
WebSocketsClient webSocket;
bool connected = false;

struct Snake {
  uint8_t bodyX[MAX_LEN], bodyY[MAX_LEN];
  int length;
  int dirX, dirY, nextDirX, nextDirY; //current dir / next dir
  bool dead;
};

Snake player1;
Snake player2;
bool twoPlayers = false;
bool inMenu = true;
int menuSelection = 0;
uint8_t foodX, foodY;
unsigned long lastTick = 0;
const unsigned long TICK_MS = 180;
bool noWalls = false;

bool cellOnSnake(Snake& snake, uint8_t cellX, uint8_t cellY) {
  for (int i = 0; i < snake.length; i++)
    if (snake.bodyX[i] == cellX && snake.bodyY[i] == cellY) return true;
  return false;
}

//place la nourriture du snake aléatoirement sur une case.
void placeFood() {
  while (true) {
    uint8_t candidateX = random(GRID_W);
    uint8_t candidateY = random(GRID_H);
    if (cellOnSnake(player1, candidateX, candidateY)) continue;
    if (twoPlayers && cellOnSnake(player2, candidateX, candidateY)) continue;
    foodX = candidateX; foodY = candidateY; return;
  }
}

void resetSnake(Snake& snake, int startX, int startY, int startDirX) {
  snake.length = 3;
  for (int i = 0; i < snake.length; i++) { snake.bodyX[i] = startX - i * startDirX; snake.bodyY[i] = startY; }
  snake.dirX = startDirX; snake.dirY = 0;
  snake.nextDirX = startDirX; snake.nextDirY = 0;
  snake.dead = false;
}

void resetGame() {
  resetSnake(player1, GRID_W / 4, GRID_H / 2, 1);
  if (twoPlayers) resetSnake(player2, 3 * GRID_W / 4, GRID_H / 2, -1);
  placeFood();
}

void setDirection(Snake& snake, int dx, int dy) {
  if (dx == -snake.dirX && dy == -snake.dirY) return;
  snake.nextDirX = dx; snake.nextDirY = dy;
}

//calcule le déplacement de la tête
bool headOf(Snake& snake, int& newX, int& newY) {
  newX = snake.bodyX[0] + snake.dirX;
  newY = snake.bodyY[0] + snake.dirY;
  if (noWalls) {
    newX = (newX + GRID_W) % GRID_W;
    newY = (newY + GRID_H) % GRID_H;
    return true;
  }
  return !(newX < 0 || newX >= GRID_W || newY < 0 || newY >= GRID_H);
}

bool hitsSnake(Snake& snake, int cellX, int cellY, bool skipTail) {
  for (int i = 0; i < snake.length; i++) {
    if (skipTail && i == snake.length - 1) continue;
    if (snake.bodyX[i] == cellX && snake.bodyY[i] == cellY) return true;
  }
  return false;
}

void moveSnake(Snake& snake, int newX, int newY) {
  bool ateFood = newX == foodX && newY == foodY;
  if (ateFood && snake.length < MAX_LEN) snake.length++;
  for (int i = snake.length - 1; i > 0; i--) { snake.bodyX[i] = snake.bodyX[i - 1]; snake.bodyY[i] = snake.bodyY[i - 1]; }
  snake.bodyX[0] = newX; snake.bodyY[0] = newY;
  if (ateFood) placeFood();
}

void gameTick() {
  if (gameEnded()) return;
  //applique la direction
  player1.dirX = player1.nextDirX; player1.dirY = player1.nextDirY;
  if (twoPlayers) { player2.dirX = player2.nextDirX; player2.dirY = player2.nextDirY; }
  //calcule où vont les têtes
  int head1X, head1Y, head2X = 0, head2Y = 0;
  bool head1Valid = headOf(player1, head1X, head1Y);
  bool head2Valid = twoPlayers ? headOf(player2, head2X, head2Y) : false;
  //mort à cause du mur
  bool player1Dies = !head1Valid;
  bool player2Dies = twoPlayers && !head2Valid;

  if (!player1Dies) {
    bool player1Ate = head1X == foodX && head1Y == foodY;
    if (hitsSnake(player1, head1X, head1Y, !player1Ate)) player1Dies = true;
    if (twoPlayers && hitsSnake(player2, head1X, head1Y, false)) player1Dies = true;
  }
  if (twoPlayers && !player2Dies) {
    bool player2Ate = head2X == foodX && head2Y == foodY;
    if (hitsSnake(player2, head2X, head2Y, !player2Ate)) player2Dies = true;
    if (hitsSnake(player1, head2X, head2Y, false)) player2Dies = true;
  }
  if (twoPlayers && head1Valid && head2Valid && head1X == head2X && head1Y == head2Y) { player1Dies = true; player2Dies = true; }
  //enregistre qui meur
  if (player1Dies) player1.dead = true;
  if (twoPlayers && player2Dies) player2.dead = true;
  // dernier mouvement pour le snake survivant
  if (!player1.dead) moveSnake(player1, head1X, head1Y);
  if (twoPlayers && !player2.dead) moveSnake(player2, head2X, head2Y);
}

bool gameEnded() {
  return player1.dead || (twoPlayers && player2.dead);
}

void drawSnake(Snake& snake, bool filled) {
  for (int i = 0; i < snake.length; i++) {
    if (filled) display.fillRect(snake.bodyX[i] * CELL, snake.bodyY[i] * CELL, CELL, CELL, SSD1306_WHITE);
    else        display.drawRect(snake.bodyX[i] * CELL, snake.bodyY[i] * CELL, CELL, CELL, SSD1306_WHITE);
  }
}

// compose la scène entièrement
void draw() {
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  display.fillRect(foodX * CELL + 1, foodY * CELL + 1, CELL - 2, CELL - 2, SSD1306_WHITE);
  drawSnake(player1, true);
  if (twoPlayers) drawSnake(player2, false);

  if (!connected) {
    display.setCursor(2, 2);
    display.print("NO SERVER");
  }

  if (gameEnded()) {
    display.fillRect(6, 24, 118, 16, SSD1306_BLACK);
    display.drawRect(6, 24, 118, 16, SSD1306_WHITE);
    display.setFont(&TomThumb);
    display.setCursor(30, 34);
    if (!twoPlayers) {
      display.print("GAME OVER | SCORE : ");
      display.print(player1.length - 3);
    } else if (player1.dead && player2.dead) {
      display.print("EGALITE !");
    } else if (player1.dead) {
      display.print("JOUEUR 2 GAGNE !");
    } else {
      display.print("JOUEUR 1 GAGNE !");
    }
    display.setFont();
  }
  display.display();
}

void drawMenu() {
  display.clearDisplay();
  display.setFont();
  display.setCursor(46, 2);
  display.print("SNAKE");
  display.setCursor(16, 22);
  display.print(menuSelection == 0 ? "> 1 joueur" : "  1 joueur");
  display.setCursor(16, 40);
  display.print(menuSelection == 1 ? "> 2 joueurs" : "  2 joueurs");
  display.setFont(&TomThumb);
  display.setCursor(6, 60);
  if (!connected) display.print("NO SERVER - lance node server.js");
  else display.print("Fleches = choisir, Espace = OK");
  display.setFont();
  display.display();
}

void onDirection(String message) {
  message.trim();

  if (inMenu) {
    if (message.endsWith(":up")) menuSelection = 0;
    else if (message.endsWith(":down")) menuSelection = 1;
    else if (message == "start") { twoPlayers = (menuSelection == 1); resetGame(); inMenu = false; }
    return;
  }

  if (gameEnded()) { inMenu = true; return; }

  int colonIndex = message.indexOf(':');
  if (colonIndex < 0) return;
  int player = message.substring(0, colonIndex).toInt();
  String direction = message.substring(colonIndex + 1);

  Snake& snake = (player == 2 && twoPlayers) ? player2 : player1;
  if (direction == "up") setDirection(snake, 0, -1);
  else if (direction == "down") setDirection(snake, 0, 1);
  else if (direction == "left") setDirection(snake, -1, 0);
  else if (direction == "right") setDirection(snake, 1, 0);
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

  webSocket.begin(SERVER_HOST, PORT, "/");
  webSocket.onEvent(onWsEvent);
  webSocket.setReconnectInterval(2000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  webSocket.loop();
  if (millis() - lastTick >= TICK_MS) {
    lastTick = millis();
    if (inMenu) {
      drawMenu();
    } else {
      gameTick();
      draw();
    }
  }
}
