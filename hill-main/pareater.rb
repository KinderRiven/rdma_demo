require "pareater"

code = Pareater.new("./Hill::CmdParser", "cpp", "hpp")

code.build do
  description "rdma app command line parser"
  boolean full: "is_server", short: "s", default: "true"
  optional full: "ib_port", short: "p", default: "1"
  optional full: "socket_port", short: "P", default: "6666"
  optional full: "device", short: "d", default: "mlx5_1"
end

code.generate!
