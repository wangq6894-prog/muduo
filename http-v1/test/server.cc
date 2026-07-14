#include "../source/server.hpp"
void OnConnected(const PtrConnection &conn) {
  DBG_LOG("NEW CONNECTION:%p", conn.get());
}
void OnClosed(const PtrConnection &conn) {
  DBG_LOG("CONNECTION:%p CLOSED", conn.get());
}
void OnMessage(const PtrConnection &conn, Buffer *buf) {
  DBG_LOG("%s", buf->ReadPosition());
  buf->MoveReadOffset(buf->ReadAbleSize());
  const char *str = "hello world";
  conn->Send(str, strlen(str));
};
int main() {
  TcpServer server(8500);
  server.EnableInactiveRelease(10);
  server.SetClosedCallback(OnClosed);
  server.SetConnectedCallback(OnConnected);
  server.SetMessageCallback(OnMessage);
  server.Start();
  return 0;
}
