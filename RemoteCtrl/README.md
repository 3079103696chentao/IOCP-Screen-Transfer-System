# RemoteCtrlRefactor

这份目录是对原始 `RemoteCtrl` 服务端的并行重构版，目标是只重写 IOCP 通信层，不改原有命令工作函数和 `map` 命令分发方式。

## 重构重点

- 监听、AcceptEx、Recv、Send 统一走真正的异步 IOCP 链路。
- 明确拆分 `Server / Session / IoContext / CommandWorker` 职责。
- 会话生命周期由 `shared_ptr` 托管，避免原实现中 `new` 出来的客户端对象脱离管理。
- 接收完成后按 `CPacket` 持续拆包，再投递到单独的命令线程执行 `CCommand::ExcuteCommand`。
- 命令执行结果转换成 `CPacket` 后进入每个会话自己的发送队列，继续异步 `WSASend`。

## 当前目录说明

- `EdoyunServer.h/.cpp`：新的 IOCP 服务端实现。
- `RemoteCtrl.cpp`：新的最小启动入口。
- `Command.*`、`Packet.h`、`LockDialog.*`、`EdoyunTool.*`：沿用原业务层。

## 和旧版的主要差异

- 旧版 `Recv()` 最终退回同步 `recv`，新版改为完整异步 `WSARecv`。
- 旧版发送路径没有把真正待发数据挂到 `WSASend`，新版按会话发送队列维护异步发送状态。
- 旧版 Accept 完成后对象生命周期不清晰，且没有把命令执行真正接进 IOCP，新版已经打通命令执行链路。
- 命令线程目前默认单线程，目的是在不改 `CCommand` 内部状态设计的前提下先保证行为稳定。
