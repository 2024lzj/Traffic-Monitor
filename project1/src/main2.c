#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in_systm.h>
#include <arpa/inet.h>
#include <pcap.h>
#include <ifaddrs.h>
#include <net/if.h>

// 流量统计结构体
typedef struct {
    char src_ip[INET6_ADDRSTRLEN];  // 源IP
    char dst_ip[INET6_ADDRSTRLEN];  // 目的IP
    int direction;                  // 0:入站(RX), 1:出站(TX)
    long total_bytes;               // 总字节数
    long total_packets;             // 总包数
    long peak_bytes;                // 峰值字节数(瞬时)
} FlowRecord;

// 时间段统计结构体
typedef struct {
    long period_bytes[3];   // [0]:2s, [1]:10s, [2]:40s
    time_t last_update;     // 上次更新时间
    int count;              // 统计周期计数
} TimeStats;

// 全局变量
static FlowRecord *flow_table = NULL;
static int flow_count = 0;
static TimeStats time_stats = {0};
static long total_rx_bytes = 0;    // 总接收流量
static long total_tx_bytes = 0;    // 总发送流量
static long global_peak = 0;       // 全局峰值

// 线程控制
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int running = 1;

// 配置参数
typedef struct {
    char *interface;        // 监控接口
    char local_ip[INET6_ADDRSTRLEN]; // 本机IP
    int sample_rate;        // 采样率(秒)
} Config;

static Config config = {
    .interface = "eth0",
    .local_ip = "",
    .sample_rate = 2
};

// 获取本机IP地址
void get_local_ip() {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr)) {
        perror("getifaddrs");
        return;
    }

    // 清空IP缓冲区
    memset(config.local_ip, 0, INET_ADDRSTRLEN);
    
    // 优先尝试用户指定的接口
    int found = 0;
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || 
            strcmp(ifa->ifa_name, config.interface) != 0) 
            continue;

        // 检查接口状态
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING)) {
            printf("Interface %s is down or not running\n", ifa->ifa_name);
            continue;
        }

        // IPv4地址
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, config.local_ip, INET_ADDRSTRLEN);
            found = 1;
            printf("Found IP on specified interface %s: %s\n", config.interface, config.local_ip);
            break;
        }
    }

    // 如果指定接口未找到IP，尝试常见接口
    if (!found) {
        // OpenWrt常见接口优先级
        const char *common_interfaces[] = {"br-lan", "eth0", "wan", "wlan0", NULL};
        
        for (int i = 0; common_interfaces[i] != NULL; i++) {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr == NULL || 
                    strcmp(ifa->ifa_name, common_interfaces[i]) != 0) 
                    continue;

                // 跳过回环接口
                if (strcmp(ifa->ifa_name, "lo") == 0) continue;
                
                // 检查接口状态
                if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING)) {
                    printf("Interface %s is down or not running\n", ifa->ifa_name);
                    continue;
                }

                // IPv4地址
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                    inet_ntop(AF_INET, &addr->sin_addr, config.local_ip, INET_ADDRSTRLEN);
                    found = 1;
                    printf("Found IP on common interface %s: %s\n", common_interfaces[i], config.local_ip);
                    break;
                }
            }
            if (found) break;
        }
    }

    // 如果仍未找到，回退到遍历所有非回环接口
    if (!found) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL || 
                ifa->ifa_addr->sa_family != AF_INET ||
                strcmp(ifa->ifa_name, "lo") == 0)
                continue;
                
            // 检查接口状态
            if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_RUNNING)) {
                printf("Interface %s is down or not running\n", ifa->ifa_name);
                continue;
            }
            
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, config.local_ip, INET_ADDRSTRLEN);
            found = 1;
            printf("Fallback IP on %s: %s\n", ifa->ifa_name, config.local_ip);
            break;
        }
    }

    freeifaddrs(ifaddr);
    
    // 最终未找到IP
    if (!found) {
        fprintf(stderr, "Error: Failed to find valid IPv4 address\n");
        strcpy(config.local_ip, "127.0.0.1"); // 使用回环地址作为后备
    }
}

// 判断数据包方向 (0:入站, 1:出站)
int determine_direction(const char *dst_ip) {
    // 目的IP是本机IP -> 入站
    if (strcmp(dst_ip, config.local_ip) == 0) {
        return 0;
    }
    // 否则是出站
    return 1;
}

// 更新流量记录
void update_flow_record(const char *src_ip, const char *dst_ip, 
                        int length, int direction) {
    pthread_mutex_lock(&stats_mutex);
    
    // 更新全局统计
    if (direction == 0) {
        total_rx_bytes += length;  // 入站
    } else {
        total_tx_bytes += length;  // 出站
    }
    
    // 更新峰值
    if (length > global_peak) {
        global_peak = length;
    }
    
    // 查找或创建流量记录
    int found = 0;
    for (int i = 0; i < flow_count; i++) {
        if (strcmp(flow_table[i].src_ip, src_ip) == 0 && 
            strcmp(flow_table[i].dst_ip, dst_ip) == 0 &&
            flow_table[i].direction == direction) {
            // 更新现有记录
            flow_table[i].total_bytes += length;
            flow_table[i].total_packets++;
            if (length > flow_table[i].peak_bytes) {
                flow_table[i].peak_bytes = length;
            }
            found = 1;
            break;
        }
    }
    
    if (!found) {
        // 创建新记录
        flow_table = realloc(flow_table, (flow_count + 1) * sizeof(FlowRecord));
        strncpy(flow_table[flow_count].src_ip, src_ip, INET6_ADDRSTRLEN);
        strncpy(flow_table[flow_count].dst_ip, dst_ip, INET6_ADDRSTRLEN);
        flow_table[flow_count].direction = direction;
        flow_table[flow_count].total_bytes = length;
        flow_table[flow_count].total_packets = 1;
        flow_table[flow_count].peak_bytes = length;
        flow_count++;
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

// 更新时间段统计
void update_time_stats(long bytes) {
    pthread_mutex_lock(&stats_mutex);
    
    time_t now = time(NULL);
    
    // 初始化
    if (time_stats.last_update == 0) {
        time_stats.last_update = now;
        time_stats.count = 0;
        for (int i = 0; i < 3; i++) {
            time_stats.period_bytes[i] = 0;
        }
    }
    
    // 更新所有时间段
    time_stats.period_bytes[0] += bytes;  // 2秒周期
    time_stats.period_bytes[1] += bytes;  // 10秒周期
    time_stats.period_bytes[2] += bytes;  // 40秒周期
    
    // 每2秒重置短期统计
    if (now - time_stats.last_update >= 2) {
        time_stats.period_bytes[0] = 0;
        time_stats.last_update = now;
        time_stats.count++;
        if (time_stats.count >= 5) {
            // 每10秒重置10秒统计
            time_stats.period_bytes[1] = 0;
            if (time_stats.count >= 20) {
                // 每40秒重置40秒统计
                time_stats.period_bytes[2] = 0;
                time_stats.count = 0; // 重置计数器
            }
        }
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

// 数据包处理回调
void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    (void)user;
    
    // 基本长度检查
    if (header->caplen < 14) return;
    
    char src_ip[INET6_ADDRSTRLEN] = {0};
    char dst_ip[INET6_ADDRSTRLEN] = {0};
    
    // IPv4处理
    if (packet[12] == 0x08 && packet[13] == 0x00) {
        const struct ip *ip_header = (struct ip*)(packet + 14);
        inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);
    }
    // IPv6处理
    else if (packet[12] == 0x86 && packet[13] == 0xdd) {
        const struct ip6_hdr *ip6_header = (struct ip6_hdr*)(packet + 14);
        inet_ntop(AF_INET6, &(ip6_header->ip6_src), src_ip, INET6_ADDRSTRLEN);
        inet_ntop(AF_INET6, &(ip6_header->ip6_dst), dst_ip, INET6_ADDRSTRLEN);
    } else {
        return; // 非IP数据包
    }
    
    // 判断数据包方向
    int direction = determine_direction(dst_ip);
    
    // 更新统计
    update_flow_record(src_ip, dst_ip, header->len, direction);
    update_time_stats(header->len);
}

// 数据包捕获线程
void *capture_thread(void *arg) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(config.interface, BUFSIZ, 1, 1000, errbuf);
    
    if (!handle) {
        fprintf(stderr, "无法打开设备 %s: %s\n", config.interface, errbuf);
        return NULL;
    }
    
    // 设置过滤器
    struct bpf_program fp;
    char filter_exp[] = "ip or ip6";
    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "过滤器错误: %s\n", pcap_geterr(handle));
        pcap_close(handle);
        return NULL;
    }
    
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "设置过滤器失败: %s\n", pcap_geterr(handle));
        pcap_close(handle);
        return NULL;
    }
    
    printf("开始捕获 %s 的流量...\n", config.interface);
    pcap_loop(handle, 0, packet_handler, NULL);
    
    pcap_close(handle);
    return NULL;
}

// 显示统计信息
void display_stats() {
    pthread_mutex_lock(&stats_mutex);
    
    // 清屏
    printf("\033[H\033[J");
    
    printf("\n=== 实时流量统计 === [接口: %s]\n", config.interface);
    printf("本机IP: %s\n", config.local_ip);
    printf("总接收流量: %-10ld bytes | 总发送流量: %-10ld bytes\n", 
           total_rx_bytes, total_tx_bytes);
    printf("瞬时峰值: %ld bytes/包\n", global_peak);
    
    printf("\n=== 时间段平均流量 ===\n");
    printf("  最近2秒: %8.2f KB/s\n", time_stats.period_bytes[0] / 2048.0);
    printf("  最近10秒: %8.2f KB/s\n", time_stats.period_bytes[1] / 10240.0);
    printf("  最近40秒: %8.2f KB/s\n", time_stats.period_bytes[2] / 40960.0);
    
    if (flow_count > 0) {
        printf("\n=== 流量明细 === (显示前10条)\n");
        printf("方向 | %-15s -> %-15s | 总流量(bytes) | 包数 | 峰值\n", "源IP", "目的IP");
        printf("------------------------------------------------------------\n");
        
        int display_count = flow_count > 10 ? 10 : flow_count;
        for (int i = 0; i < display_count; i++) {
            printf(" %s  | %-15s -> %-15s | %-13ld | %-5ld | %ld\n",
                   flow_table[i].direction == 0 ? "RX" : "TX",
                   flow_table[i].src_ip,
                   flow_table[i].dst_ip,
                   flow_table[i].total_bytes,
                   flow_table[i].total_packets,
                   flow_table[i].peak_bytes);
        }
    }
    
    pthread_mutex_unlock(&stats_mutex);
}

// 清理资源
void cleanup() {
    if (flow_table) {
        free(flow_table);
        flow_table = NULL;
    }
    flow_count = 0;
}

// 主函数
int main(int argc, char *argv[]) {
    // 解析命令行参数
    if (argc > 1) {
        config.interface = argv[1];
    }
    
    // 获取本机IP
    get_local_ip();
    if (strlen(config.local_ip) == 0) {
        fprintf(stderr, "警告: 无法获取 %s 的IP地址\n", config.interface);
        strcpy(config.local_ip, "未知");
    }
    
    // 创建捕获线程
    pthread_t tid;
    if (pthread_create(&tid, NULL, capture_thread, NULL)) {
        perror("创建捕获线程失败");
        return 1;
    }
    
    // 主循环
    while (running) {
        sleep(config.sample_rate);
        display_stats();
        
        // 简单退出条件（实际应使用信号处理）
        if (total_rx_bytes + total_tx_bytes > 10000000) {
            running = 0;
        }
    }
    
    // 清理
    pthread_join(tid, NULL);
    cleanup();
    printf("\n流量监控已停止\n");
    return 0;
}