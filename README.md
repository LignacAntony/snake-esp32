# Snake ESP32-S3

[![CI](https://github.com/LignacAntony/snake-esp32/actions/workflows/ci.yml/badge.svg)](https://github.com/LignacAntony/snake-esp32/actions/workflows/ci.yml)

Snake game running on an ESP32-S3 with a 128x64 OLED display, controlled from a
computer keyboard over WebSocket.

The ESP32 runs the game and draws it on the OLED. A small Node.js server on the
computer captures the keyboard and streams the directions to the board.

```
keyboard ──► Node server (server.js) ──WebSocket──► ESP32-S3 ──► OLED
```

- **ESP32** — WebSocket client, game logic, OLED rendering (`snake_client/`)
- **Computer** — WebSocket server, keyboard capture (`server/`)

## Hardware

- ESP32-S3 (tested on an ESP32-S3-N16R8)
- SSD1306-compatible OLED, 128x64, I2C (address `0x3C`)

| OLED | ESP32-S3 |
|------|----------|
| VCC  | 3V3      |
| GND  | GND      |
| SDA  | GPIO8    |
| SCL  | GPIO9    |

## ESP32 sketch

1. Install the ESP32 board package in the Arduino IDE.
2. Install the libraries: **Adafruit SSD1306**, **Adafruit GFX Library**,
   **WebSockets** (by Markus Sattler).
3. Copy `snake_client/secrets.h.example` to `snake_client/secrets.h` and fill in
   your WiFi credentials and `SERVER_HOST` — the LAN IP of the computer that runs
   the server. Start the server once (see below); it prints the IP to use.
4. Flash `snake_client/snake_client.ino` (board: **ESP32S3 Dev Module**). After
   flashing, the OLED shows `Connexion WiFi...`, then the server IP, then the game.

## Server

```bash
cd server
npm install
node server.js
```

On startup the server prints the IP to set as `SERVER_HOST` in
`snake_client/secrets.h`:

```
Snake server listening on port 7777
Set SERVER_HOST in snake_client/secrets.h to: 192.168.1.42
Play with the arrow keys or ZQSD. Ctrl+C to quit.
```

Focus the terminal and play with the **arrow keys** or **ZQSD**. Any key
restarts after a game over.

## Troubleshooting

The OLED shows what the board is doing:

- Stuck on `Connexion WiFi...` → wrong WiFi name/password in `secrets.h`.
- `NO SERVER` shown over the game → the board is on WiFi but can't reach the
  server: the server isn't running, or `SERVER_HOST` is the wrong IP. The boot
  screen shows the IP the board is dialing — check it matches your computer.
- `Port 7777 is already in use` when starting the server → another server is
  already running; stop it, then start it again.

`secrets.h` is compiled into the sketch, so **re-flash the board after changing it**.

## Layout

```
snake_client/   ESP32 sketch (Arduino)
server/         Node.js WebSocket server
.github/        CI: compiles the sketch and checks the server
```
