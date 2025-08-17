# OpenWrt 网络流量监控系统

本项目在 OpenWrt（一种用于路由器的嵌入式 Linux 发行版）上实现了一个网络流量监控系统。该系统作为电子科技大学实验项目的一部分开发，能够捕获和分析网络流量，提供实时统计数据，如源 IP 和目的 IP 地址、发送和接收的总字节数、流量峰值以及指定时间间隔（2 秒、10 秒、40 秒）的平均流量。数据通过 Web 界面进行可视化展示，便于监控。
> 作者只负责了抓包代码的开发和交叉编译的环境配置，**本文档的Web内容仅具有弱的参考效力（正在学习中）**

## 功能

- **流量监控**：使用 libpcap 捕获和分析网络数据包。
- **数据收集**：记录源/目的 IP、总字节数、总数据包数和峰值字节数。
- **基于时间的统计**：计算 2 秒、10 秒和 40 秒时间间隔内的平均流量。
- **Web 界面**：提供实时仪表盘以可视化流量数据。
- **跨平台兼容性**：专为 OpenWrt 设计，支持 x86_64 架构。

## 架构

系统采用多层、前后端分离的架构：

- **数据采集层**：使用 libpcap 的 C 程序捕获和处理数据包。
- **后端 API 层**：Python Flask 服务器，通过 HTTP POST 请求接收来自 C 程序的数据，并提供给前端。
- **前端层**：基于 HTML/JavaScript 的 Web 界面，使用 Chart.js 绘制图表，每 2 秒动态更新。

## 项目结构

- `project1/src`：流量监控程序的 C 源代码。
- `project1/build`：需要预先运行的 shell 脚本，指定SDK路径并设置环境变量。
- `web/`：前后端代码（部分）。

## 先决条件

- 开发机器上的类 Unix 操作系统（Linux/macOS）。
- VMware Workstation 或类似软件用于运行 OpenWrt。
- OpenWrt SDK（推荐版本 24.10.0）用于交叉编译。
- CMake 用于构建 C 程序。
- Python 3 和 pip 用于 Flask 后端。
- Web 浏览器以访问前端。

## 安装

### 设置 OpenWrt 虚拟机

1. 从 [OpenWrt 下载页面](https://downloads.openwrt.org/releases/24.10.0/targets/x86/64/) 下载 OpenWrt 镜像。
2. 使用 StarWind V2V Converter 等工具将 `.img` 文件转换为 `.vmdk`。
3. 在 VMware Workstation 中使用转换后的镜像创建虚拟机。
4. 配置虚拟机网络（例如 NAT 模式）并分配静态 IP。

### 安装依赖

**在开发机器上：**

```bash
sudo apt-get install build-essential cmake libpcap-dev libcurl4-openssl-dev
```

**在 OpenWrt 虚拟机上：**

```bash
opkg update
opkg install libpcap
```

## 配置

### 网络配置

- 在 OpenWrt 虚拟机上设置静态 IP。
- 记录 IP 地址以供后端和前端访问。

### 后端配置（参考）

- 编辑 C 源代码以设置 Flask 后端 URL。
- 配置 Flask 监听可访问的 IP/端口（例如 `0.0.0.0:5000`）。

### 前端配置（参考）

- 更新 JavaScript 中的后端 URL。

## 使用

### 运行流量监控程序

1. **在开发机器上：** 编译 C 程序（参见交叉编译）。
2. **在开发机器上：** 将二进制文件传输到 OpenWrt 虚拟机。
3. **在 OpenWrt 虚拟机上：** 以 root 权限运行：

```bash
./traffic_monitor eth0
```

> 注意：数据包捕获需要 root 权限。

### 访问 Web 界面（参考）

1. **在开发机器或服务器上：** 启动 Flask 后端：

```bash
cd backend
python app.py
```

1. **从任何机器：** 打开浏览器并访问 `http://<openwrt_ip>/flowmon/index.html`。

Web 界面提供实时仪表盘，包含：
- **网络接口和本机 IP**：显示监控的接口和本机 IP（例如 eth0, 192.168.1.1）。
- **实时峰值流量**：显示每个数据包的峰值字节数，每 2 秒更新。
- **流量统计**：总接收和发送字节数，以及最近 2 秒、10 秒、40 秒的平均流量速率。
- **IP 流量占比**：使用 Chart.js 绘制的饼状图，展示 IP 间的流量分布。
- **流量趋势**：柱状图展示过去 10 秒的流量历史。
- **实时流量明细**：可滚动的表格，列出 Top 10 流量流，包含时间戳、方向（RX/TX）、源/目的 IP、字节数、数据包数和峰值字节数。

### 在开发机器上测试

在不使用 OpenWrt 的情况下进行测试：

1. 在本地编译，无需交叉编译。
2. 以 root 权限运行：

```bash
sudo ./traffic_monitor eth0
```

3. 确保后端正在运行。

## 交叉编译

### 设置环境

1. 下载并解压 OpenWrt SDK（例如 `openwrt-sdk-24.10.0`）。
2. 加载环境脚本以配置环境变量（SDK路径需要手动设置）：

```bash
cd build
source env.sh 
```

### 构建 C 程序

1. 导航到构建目录。
2. 使用工具链运行 CMake：

```bash
cmake ..
```

3. 构建：

```bash
make
```

## 在 OpenWrt 上部署

### 传输文件

- 在 OpenWrt 上安装 Samba：

```bash
opkg install luci-app-samba4
```

- 通过 LuCI 界面配置共享目录（例如 `/mnt/share`）。
- 将二进制文件和前端文件复制到 OpenWrt 虚拟机。

### 设置 Web 服务器（参考）

1. 安装 `uhttpd`：

```bash
opkg update
opkg install uhttpd
```

2. 将前端文件放置在 `/www/flowmon/`。
3. 重启 Web 服务器：

```bash
/etc/init.d/uhttpd restart
```

## 故障排除

- **网络问题**：验证虚拟机网络设置和 IP 可达性。
- **编译错误**：确保 SDK 和工具链与 x86_64 架构匹配。
- **数据未显示**：确认 C 程序的后端 URL 和 Flask 服务器状态。

## 贡献

欢迎贡献！请 Fork 仓库，进行更改并提交 Pull Request。请遵循项目的编码标准，并在适用的情况下包含测试。

## 许可证

本项目尚未采用许可证。