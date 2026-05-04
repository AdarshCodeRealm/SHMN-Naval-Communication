#include "painlessMesh.h"
#include <WiFi.h>
#include <WebServer.h>

#define MESH_PREFIX     "MeshNet"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       5555

#define NODE_NAME "ESP1"
#define MAX_NODES 10

Scheduler userScheduler;
painlessMesh mesh;
WebServer server(80);

// Simple node structure to avoid std::map issues
struct Node {
  char name[16];
  unsigned long lastSeen;
};

Node nodes[MAX_NODES];
int nodeCount = 0;
portMUX_TYPE nodeMux = portMUX_INITIALIZER_UNLOCKED;

// ================= RECEIVE =================
void receivedCallback(uint32_t from, String &msg) {

  int sep = msg.indexOf('|');
  if (sep == -1) return;

  String sender = msg.substring(0, sep);
  String text   = msg.substring(sep + 1);

  portENTER_CRITICAL(&nodeMux);
  
  // Find or add node
  int idx = -1;
  for (int i = 0; i < nodeCount; i++) {
    if (strcmp(nodes[i].name, sender.c_str()) == 0) {
      idx = i;
      break;
    }
  }
  
  if (idx == -1 && nodeCount < MAX_NODES) {
    idx = nodeCount++;
    strcpy(nodes[idx].name, sender.c_str());
  }
  
  if (idx != -1) {
    nodes[idx].lastSeen = millis();
  }
  
  portEXIT_CRITICAL(&nodeMux);

  Serial.println(sender + " -> ESP1 : " + text);
}

// ================= HEARTBEAT =================
Task heartbeatTask(3000, TASK_FOREVER, []() {
  String msg = String(NODE_NAME) + "|Hi from ESP1";
  mesh.sendBroadcast(msg);
});

// ================= STATUS JSON =================
String getStatus() {
  String json = "[";
  bool first = true;
  
  portENTER_CRITICAL(&nodeMux);
  
  unsigned long currentTime = millis();
  for (int i = 0; i < nodeCount; i++) {
    String status = (currentTime - nodes[i].lastSeen < 10000) ? "online" : "offline";

    if (!first) json += ",";
    json += "{\"node\":\"";
    json += nodes[i].name;
    json += "\",\"status\":\"";
    json += status;
    json += "\"}";
    first = false;
  }
  
  portEXIT_CRITICAL(&nodeMux);
  
  json += "]";
  return json;
}

// ================= WEB PAGE =================
void handleRoot() {
  String html = R"rawliteral(
  <html>
  <head><title>ESP1 Gateway</title></head>
  <body>
    <h1>ESP1 Gateway Dashboard</h1>
    <h3>Mesh Monitor</h3>
    <div id="data"></div>

    <script>
      setInterval(() => {
        fetch('/nodes')
          .then(res => res.json())
          .then(data => {
            let out = "";
            data.forEach(n => {
              out += "<p>" + n.node + " : " + n.status + "</p>";
            });
            document.getElementById("data").innerHTML = out;
          });
      }, 2000);
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// ================= API =================
void handleNodes() {
  server.send(200, "application/json", getStatus());
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(100);

  WiFi.softAP("MeshMonitor", "12345678");

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(heartbeatTask);
  heartbeatTask.enable();

  server.on("/", handleRoot);
  server.on("/nodes", handleNodes);
  server.begin();

  Serial.println("ESP1 Gateway Ready");
  Serial.println(WiFi.softAPIP());
}

// ================= LOOP =================
void loop() {
  mesh.update();
  server.handleClient();
}