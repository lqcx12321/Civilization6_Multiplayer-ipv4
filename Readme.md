# 文明6联机 - IPv4

> 改自：[xaxys/injciv6](https://github.com/xaxys/injciv6)  
> 本仓库仅保留 **IPv4 客户端联机** 功能，GUI 已精简。

原作者仓库利用ipv6进行联机很方便，但是目前校园网环境通常不支持ipv6，于是进行部分功能添加。
利用 Hook 拦截游戏 UDP 广播，将其改为单播到指定服务器 IPv4 地址，实现基于 IP 的房间发现。

## 使用方法

**注入工具可能被杀毒软件拦截，使用前请关闭 Windows Defender 或添加白名单。**

### 0 前置配置

大体分为两步
第一步所有进行联机玩家先要进入虚拟局域网

> 参照:[docker-zerotier-planet](https://github.com/xubiaolin/docker-zerotier-planet)

第二步加入房间玩家启动gui注入ip

### 1 创建虚拟局域网

[![通过雨云一键部署](https://rainyun-apps.cn-nb1.rains3.com/materials/deploy-on-rainyun-cn.svg)](https://app.rainyun.com/apps/rca/store/6215?ref=220429)

#### 1.1 配置服务器

有免费试用，可以试试
从上面链接启动后选择最低配置，3-4个人玩完全没问题，有需求可以自行调整

![ui](assets/yuyun1.png)

#### 1.2 下载 planet 文件

请妥善保存这些文件，后续配置客户端时会用到

![ui](assets/yuyun2.png)

#### 1.3 新建网络

![ui](assets/yuyun3.png)

访问 `http://上图1:上图2` 进入 controller 页面（不要写成https）

![ui](assets/net1.png)

**默认登录信息：**
- 用户名：`admin`
- 密码：`password`

1. 登录后点击 "Networks" 菜单
2. 点击 "Add Network" 按钮创建新网络
3. 输入一个便于识别的网络名称，其他选项可保持默认
4. 点击 "Create Network" 按钮完成创建

![ui](assets/net2.png)

创建成功后系统会自动生成一个网络 ID，这个 ID 在后续客户端配置时会用到，请记录下来。

![ui](assets/net3.png)

#### 1.4 分配网络 IP

1. 选中 "Easy Setup"
![assign_id](./assets/net4.png)

2. 生成 IP 范围
![ip_addr](./assets/net5.png)

#### 1.5 客户端配置

首先去[ZeroTier 官网](https://www.zerotier.com/download/)下载一个 ZeroTier 客户端

将 `planet` 文件覆盖粘贴到 `C:\ProgramData\ZeroTier\One` 中（这个目录是个隐藏目录，需要允许查看隐藏目录才行）

1. 按 `Win + S` 搜索 "服务"
![ui](assets/service.png)

2. 找到 ZeroTier One，并且重启服务
![ui](assets/restart_service.png)

使用管理员身份打开 PowerShell，执行如下命令：
```powershell
PS C:\Windows\system32> zerotier-cli.bat join 网络id
200 join OK
PS C:\Windows\system32>
```

> **注意**：网络 ID 就是在网页里面创建的那个网络 ID

#### 1.6 授权设备
登录管理后台可以看到有个新的客户端，勾选 `Authorized` 即可

![ui](assets/net6.png)

IP assignment 里面会出现 ZeroTier 的内网 IP

![ui](assets/net7.png)

执行如下命令验证连接状态：

```powershell
PS C:\Windows\system32> zerotier-cli.bat peers
200 peers
<ztaddr>   <ver>  <role> <lat> <link> <lastTX> <lastRX> <path>
fcbaeb9b6c 1.8.7  PLANET    52 DIRECT 16       8994     1.1.1.1/9993
fe92971aad 1.8.7  LEAF      14 DIRECT -1       4150     2.2.2.2/9993
PS C:\Windows\system32>
```

可以看到有一个 `PLANET` 和 `LEAF` 角色，连接方式均为 `DIRECT`（直连）

到这里就加入网络成功了！
> [!CAUTION]
> **恭喜你成功完成了配置的 80%！**

### GUI 方式连接游戏（推荐）

1. 启动文明6
2. 以**管理员身份**运行 `kskbl-gui.exe`
3. 在「联机」页填写服务端 **IPv4 地址**
4. 点击「开始注入」，状态显示「已注入」后即可联机

服务端就是游戏里局域网开房的那个玩家，ipv4地址就是1.6授权的内网ip

### 命令行方式

1. 启动游戏后双击 `kskbl.exe` 注入
2. 在游戏目录编辑 `kskbl-config.txt`，填入服务器 IPv4 地址
3. 再次运行 `kskbl.exe` 使配置生效
4. 解除注入：运行 `civ6remove.exe`

### IPv4 联机说明

- 需先搭建虚拟局域网或确保客户端能访问服务端 IPv4
- **服务端无需注入**，仅**客户端**注入并配置服务端地址
- 分享工具时请将以下文件放在同一目录：`kskbl-gui.exe`、`kskbl.exe`、`hookdll64.dll`、`civ6remove.exe`

游戏目录一般为：

`Sid Meier's Civilization VI\Base\Binaries\Win64Steam` 或 `Win64EOS`

## 编译

需要 MinGW-w64（i686 + x86_64）和 Go 1.21+。

```bat
env.bat
mingw32-make SHELL=cmd.exe
```

## 原理简述

1. Hook `sendto`：将局域网 UDP 广播改为单播到配置的服务器 IPv4
2. Hook `recvfrom`：将收到的地址映射为 `127.0.127.x` 假地址，规避游戏对私有 IP 的过滤
3. Hook `select` / `closesocket`：配合上述 socket 操作

IPv4 联机完成房间发现后，沿用游戏原生联机机制，无需额外 Hook。

## 致谢

感谢原作者 PeaZomboss 的注入框架，以及 xaxys 的 injciv6 项目。
