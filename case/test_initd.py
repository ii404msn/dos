import engine_pb2
import dos_pb2
import initd_pb2
import client

def test_error_cmd():
  cmd = "cd /notexistdir"
  user = dos_pb2.User()
  user.name = "root"
  user.uid = 0
  user.gid = 0
  process = dos_pb2.Process(user = user, args=[cmd])
  process.name = "test_error_cmd"
  request = initd_pb2.ForkRequest(process = process)
  channel = client.Channel("127.0.0.1:9000")
  controller = client.Controller()
  initd = initd_pb2.Initd_Stub(channel)
  response = initd.Fork(controller, request)

if __name__ == "__main__":
  test_error_cmd()

