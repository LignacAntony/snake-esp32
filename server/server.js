const WebSocket = require('ws');
const readline = require('readline');
const os = require('os');

const PORT = 7777;
const wss = new WebSocket.Server({ port: PORT });
const clients = new Set();

wss.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`Port ${PORT} is already in use (is the server already running?)`);
  } else {
    console.error(err.message);
  }
  process.exit(1);
});

const addresses = Object.values(os.networkInterfaces())
  .flat()
  .filter((a) => (a.family === 'IPv4' || a.family === 4) && !a.internal)
  .map((a) => a.address);

console.log(`Snake server listening on port ${PORT}`);
console.log(`Set SERVER_HOST in snake_client/secrets.h to: ${addresses.join(' or ') || '<your LAN IP>'}`);
console.log('Play with the arrow keys or ZQSD. Ctrl+C to quit.');

wss.on('connection', (ws) => {
  clients.add(ws);
  console.log(`client connected (${clients.size})`);
  ws.isAlive = true;
  ws.on('pong', () => { ws.isAlive = true; });
  ws.on('close', () => {
    clients.delete(ws);
    console.log(`client disconnected (${clients.size})`);
  });
  ws.on('error', (err) => console.error('ws error:', err.message));
});

setInterval(() => {
  for (const ws of clients) {
    if (!ws.isAlive) {
      ws.terminate();
      continue;
    }
    ws.isAlive = false;
    ws.ping();
  }
}, 10000);

function broadcast(dir) {
  for (const ws of clients) {
    if (ws.readyState === WebSocket.OPEN) ws.send(dir);
  }
}

const keys = {
  up: 'up', z: 'up',
  down: 'down', s: 'down',
  left: 'left', q: 'left',
  right: 'right', d: 'right',
};

readline.emitKeypressEvents(process.stdin);
if (process.stdin.isTTY) process.stdin.setRawMode(true);

process.stdin.on('keypress', (str, key) => {
  if (key.ctrl && key.name === 'c') process.exit();
  const dir = keys[key.name];
  if (dir) broadcast(dir);
});
