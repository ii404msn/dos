import engine_pb2
import dos_pb2
import master_pb2
import client

def start_propose():
  addr = "127.0.0.1:9527"
  propose = master_pb2.Propose()
  propose.endpoint = "127.0.0.1:8527"
  propose.pod_name = "3_pod.dfs"
  req = master_pb2.ScaleUpProposeRequest(proposes = [propose])
  channel = client.Channel(addr)
  master = master_pb2.Master_Stub(channel)
  controller = client.Controller()
  controller.SetTimeout(10)
  response = master.ScaleUpPropose(controller, req)


if __name__ == '__main__':
  start_propose()
