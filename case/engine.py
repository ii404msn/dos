import engine_pb2
import dos_pb2
import client

def start_container(addr):
  container = dos_pb2.Container()
  container.type = 2
  container.reserved = False
  container.uri = "http://127.0.0.1:8989/dfs.tar.gz"  
  req = engine_pb2.RunContainerRequest(container = container)
  req.name = "dfs"
  channel = client.Channel(addr)
  engine = engine_pb2.Engine_Stub(channel)
  controller = client.Controller()
  controller.SetTimeout(10)
  response = engine.RunContainer(controller, req)


if __name__ == '__main__':
  start_container("127.0.0.1:7676")
