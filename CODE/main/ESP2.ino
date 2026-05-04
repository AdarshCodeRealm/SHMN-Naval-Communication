#include "painlessMesh.h"

#define MESH_PREFIX     "MeshNet"
#define MESH_PASSWORD   "12345678"
#define MESH_PORT       5555

#define NODE_NAME "ESP2"

Scheduler userScheduler;
painlessMesh mesh;

// ================= RECEIVE =================
void receivedCallback(uint32_t from, String &msg) {

  int sep = msg.indexOf('|');
  if (sep == -1) return;

  String sender = msg.substring(0, sep);
  String text   = msg.substring(sep + 1);

  Serial.println(sender + " -> ESP2 : " + text);

  // reply to ESP1
  if (sender == "ESP1") {
    mesh.sendBroadcast("ESP2|Yes I am ESP2");
  }
}

// ================= HEARTBEAT =================
Task heartbeatTask(4000, TASK_FOREVER, []() {
  mesh.sendBroadcast("ESP2|Hi from ESP2");
});

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(100);

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  userScheduler.addTask(heartbeatTask);
  heartbeatTask.enable();
}

// ================= LOOP =================
void loop() {
  mesh.update();
}