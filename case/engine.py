import engine_pb2
import dos_pb2
import client

def start_container(addr):
  container = dos_pb2.Container()
  req = engine_pb2.RunContainerRequest(container = container)
  req.name = "test"
  channel = client.Channel(addr)
  engine = engine_pb2.Engine_Stub(channel)
  controller = client.Controller()
  controller.SetTimeout(10)
  response = engine.RunContainer(controller, req)


if __name__ == '__main__':
  start_container("127.0.0.1:7676")
