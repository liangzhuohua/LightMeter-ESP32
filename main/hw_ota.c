#include "hw_ota.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include <string.h>

static const char* TAG = "hw_ota";

/* AP 热点配置：手机连接这个 WiFi 后访问 192.168.4.1 上传固件 */
#define OTA_AP_SSID     "ESP32S3_OTA"
#define OTA_AP_PASS     "12345678"
#define OTA_AP_CHANNEL  1
#define OTA_AP_MAX_CONN 1

/* 全局状态变量 */
static hw_ota_state_t g_ota_state = HW_OTA_IDLE;       /* 当前 OTA 状态 */
static hw_ota_progress_cb_t g_progress_cb = NULL;       /* UI 进度回调函数 */
static httpd_handle_t g_server = NULL;                  /* HTTP 服务器句柄 */
static esp_ota_handle_t g_ota_handle = 0;               /* OTA 写入句柄 */
static const esp_partition_t* g_update_partition = NULL; /* 目标 OTA 分区（ota_0 或 ota_1） */
static int g_ota_total_written = 0;                     /* 已写入字节数 */
static int g_ota_total_size = 0;                        /* 固件总字节数 */
static bool g_ap_started = false;                       /* AP 是否已启动 */
static esp_netif_t* g_ap_netif = NULL;                  /* AP 网络接口句柄，stop 时需要销毁 */

/* 上传网页的 HTML 源码，包含拖拽上传、进度条、状态显示 */
static const char HTML_UPLOAD_PAGE[] =
"<!DOCTYPE html>"
"<html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32-S3 OTA</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:Arial,sans-serif;background:#1a1a2e;color:#fff;display:flex;"
"justify-content:center;align-items:center;min-height:100vh}"
".card{background:#2a2a3e;border-radius:15px;padding:30px;width:90%;max-width:400px;text-align:center}"
"h1{color:#87ceeb;margin-bottom:20px;font-size:22px}"
".info{background:#1a1a2e;border-radius:10px;padding:15px;margin-bottom:20px;text-align:left}"
".info p{margin:8px 0;font-size:14px;color:#aaa}"
".info span{color:#fff}"
".upload-area{border:2px dashed #87ceeb;border-radius:10px;padding:30px;margin-bottom:20px;"
"cursor:pointer;transition:all .3s}"
".upload-area:hover{border-color:#00ff00;background:rgba(0,255,0,.05)}"
".upload-area.dragover{border-color:#00ff00;background:rgba(0,255,0,.1)}"
"input[type=file]{display:none}"
".btn{background:#0066cc;color:#fff;border:none;border-radius:8px;padding:12px 30px;"
"font-size:16px;cursor:pointer;width:100%;transition:all .3s}"
".btn:hover{background:#0055aa}"
".btn:disabled{background:#555;cursor:not-allowed}"
".progress{margin-top:20px;display:none}"
".progress-bar{background:#1a1a2e;border-radius:10px;height:20px;overflow:hidden}"
".progress-fill{background:linear-gradient(90deg,#0066cc,#00ff00);height:100%;width:0%;"
"transition:width .3s;border-radius:10px}"
".progress-text{margin-top:8px;font-size:14px;color:#aaa}"
".status{margin-top:15px;padding:10px;border-radius:8px;display:none;font-size:14px}"
".status.success{background:rgba(0,255,0,.15);color:#00ff00;display:block}"
".status.fail{background:rgba(255,107,107,.15);color:#ff6b6b;display:block}"
"</style></head><body>"
"<div class='card'>"
"<h1>ESP32-S3 Firmware Upgrade</h1>"
"<div class='info'>"
"<p>SSID: <span>ESP32S3_OTA</span></p>"
"<p>IP: <span>192.168.4.1</span></p>"
"</div>"
"<div class='upload-area' id='dropZone' onclick='document.getElementById(\"fileInput\").click()'>"
"<p style='color:#87ceeb;font-size:16px'>Click or drag .bin file here</p>"
"<input type='file' id='fileInput' accept='.bin' onchange='onFileSelect(this)'>"
"</div>"
"<button class='btn' id='uploadBtn' onclick='startUpload()' disabled>Start Upgrade</button>"
"<div class='progress' id='progress'>"
"<div class='progress-bar'><div class='progress-fill' id='progressFill'></div></div>"
"<div class='progress-text' id='progressText'>0%</div>"
"</div>"
"<div class='status' id='status'></div>"
"</div>"
"<script>"
"var selectedFile=null;"
"var dropZone=document.getElementById('dropZone');"
"dropZone.addEventListener('dragover',function(e){e.preventDefault();dropZone.classList.add('dragover')});"
"dropZone.addEventListener('dragleave',function(){dropZone.classList.remove('dragover')});"
"dropZone.addEventListener('drop',function(e){e.preventDefault();dropZone.classList.remove('dragover');"
"if(e.dataTransfer.files.length>0){document.getElementById('fileInput').files=e.dataTransfer.files;onFileSelect(document.getElementById('fileInput'))}});"
"function onFileSelect(input){if(input.files.length>0){selectedFile=input.files[0];"
"document.getElementById('uploadBtn').disabled=false;"
"dropZone.innerHTML='<p style=\"color:#00ff00\">'+selectedFile.name+' ('+(selectedFile.size/1024).toFixed(1)+' KB)</p>'}}"
"function startUpload(){if(!selectedFile)return;"
"document.getElementById('uploadBtn').disabled=true;"
"document.getElementById('progress').style.display='block';"
"var xhr=new XMLHttpRequest();"
"xhr.upload.onprogress=function(e){if(e.lengthComputable){"
"var pct=Math.round(e.loaded/e.total*100);"
"document.getElementById('progressFill').style.width=pct+'%';"
"document.getElementById('progressText').textContent=pct+'%'}};"
"xhr.onload=function(){"
"if(xhr.status===200){"
"document.getElementById('status').className='status success';"
"document.getElementById('status').textContent='Upgrade successful! Device rebooting...'}"
"else{document.getElementById('status').className='status fail';"
"document.getElementById('status').textContent='Upgrade failed: '+xhr.responseText}};"
"xhr.onerror=function(){document.getElementById('status').className='status fail';"
"document.getElementById('status').textContent='Network error'};"
"xhr.open('POST','/upload');xhr.send(selectedFile)}"
"</script></body></html>";

/* 通知状态变更，更新 UI 进度 */
static void notify_state(hw_ota_state_t state, int progress) {
    g_ota_state = state;
    if (g_progress_cb) {
        g_progress_cb(state, progress);
    }
}

/*
 * HTTP POST /upload 处理函数
 * 接收浏览器上传的固件 .bin 文件，写入 OTA 分区
 *
 * 流程：
 * 1. 获取下一个可用的 OTA 分区（ota_0 或 ota_1）
 * 2. esp_ota_begin() 初始化 OTA 写入
 * 3. 循环接收 HTTP 数据，esp_ota_write() 写入分区
 * 4. esp_ota_end() 完成写入并校验
 * 5. esp_ota_set_boot_partition() 设置下次从新分区启动
 * 6. esp_restart() 重启到新固件
 */
static esp_err_t upload_post_handler(httpd_req_t* req) {
    int content_len = req->content_len;
    if (content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid content length");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA upload start, size: %d bytes", content_len);

    /* 找到非当前运行的 OTA 分区作为写入目标 */
    g_update_partition = esp_ota_get_next_update_partition(NULL);
    if (g_update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        notify_state(HW_OTA_FAIL, 0);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing to partition: %s at offset 0x%lx",
             g_update_partition->label, (unsigned long)g_update_partition->address);

    /* 开始 OTA 写入，分配内部缓冲区 */
    esp_err_t err = esp_ota_begin(g_update_partition, content_len, &g_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        notify_state(HW_OTA_FAIL, 0);
        return ESP_FAIL;
    }

    g_ota_total_written = 0;
    g_ota_total_size = content_len;
    notify_state(HW_OTA_UPLOADING, 0);

    /* 分块接收并写入固件数据 */
    char buf[4096];
    int received;
    int last_progress = 0;

    while (g_ota_total_written < content_len) {
        int to_read = sizeof(buf);
        if (content_len - g_ota_total_written < to_read) {
            to_read = content_len - g_ota_total_written;
        }

        received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Upload connection error");
            esp_ota_abort(g_ota_handle);
            notify_state(HW_OTA_FAIL, 0);
            return ESP_FAIL;
        }

        /* 将接收到的数据写入 OTA 分区 */
        err = esp_ota_write(g_ota_handle, buf, received);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(g_ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA write failed");
            notify_state(HW_OTA_FAIL, 0);
            return ESP_FAIL;
        }

        g_ota_total_written += received;

        /* 进度变化时通知 UI 更新 */
        int progress = (int)((uint64_t)g_ota_total_written * 100 / content_len);
        if (progress != last_progress) {
            last_progress = progress;
            notify_state(HW_OTA_UPLOADING, progress);
        }
    }

    ESP_LOGI(TAG, "OTA write complete: %d bytes", g_ota_total_written);

    /* 写入完成，验证固件完整性 */
    notify_state(HW_OTA_VERIFYING, 100);
    err = esp_ota_end(g_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
        notify_state(HW_OTA_FAIL, 0);
        return ESP_FAIL;
    }

    /* 验证通过，设置下次启动从新分区引导 */
    err = esp_ota_set_boot_partition(g_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Set boot partition failed");
        notify_state(HW_OTA_FAIL, 0);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA success, will reboot to new partition");
    notify_state(HW_OTA_SUCCESS, 100);

    /* 先回复浏览器，再延时重启 */
    httpd_resp_sendstr(req, "OK");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

/* HTTP GET / 处理函数：返回上传网页 */
/* HTTP GET / 处理：返回固件上传网页 */
static esp_err_t index_get_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_UPLOAD_PAGE, strlen(HTML_UPLOAD_PAGE));
    return ESP_OK;
}

/* 启动 HTTP 服务器，注册首页和上传两个 URI */
/* 启动HTTP服务器，注册首页和上传URI */
static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 2;
    config.stack_size = 8192;

    if (httpd_start(&g_server, &config) == ESP_OK) {
        httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler };
        httpd_uri_t upload_uri = { .uri = "/upload", .method = HTTP_POST, .handler = upload_post_handler };
        httpd_register_uri_handler(g_server, &index_uri);
        httpd_register_uri_handler(g_server, &upload_uri);
        ESP_LOGI(TAG, "HTTP server started");
        return g_server;
    }
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

/* 停止HTTP服务器 */
static void stop_webserver(void) {
    if (g_server) {
        httpd_stop(g_server);
        g_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

/* 注册 UI 进度回调，OTA 状态变化时通过此函数通知 UI 层 */
void hw_ota_register_progress_cb(hw_ota_progress_cb_t cb) {
    g_progress_cb = cb;
}

/*
 * 启动 OTA 升级模式
 *
 * 流程：
 * 1. 创建 AP 网络接口（ESP32 变成 WiFi 热点）
 * 2. 切换 WiFi 模式为 AP+STA 共存（保持原有 STA 连接）
 * 3. 启动 HTTP 服务器，等待浏览器上传固件
 */
esp_err_t hw_ota_start(void) {
    if (g_ota_state != HW_OTA_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    notify_state(HW_OTA_AP_STARTING, 0);

    /* 创建 AP 网络接口，自动分配 IP 192.168.4.1 并启动 DHCP 服务器 */
    g_ap_netif = esp_netif_create_default_wifi_ap();

    /* 配置 AP 热点参数 */
    wifi_config_t ap_config = {0};
    strncpy((char*)ap_config.ap.ssid, OTA_AP_SSID, sizeof(ap_config.ap.ssid) - 1);
    strncpy((char*)ap_config.ap.password, OTA_AP_PASS, sizeof(ap_config.ap.password) - 1);
    ap_config.ap.channel = OTA_AP_CHANNEL;
    ap_config.ap.max_connection = OTA_AP_MAX_CONN;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    /* 切换为 AP+STA 共存模式，这样原有 WiFi 连接不会断开 */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    g_ap_started = true;
    ESP_LOGI(TAG, "AP started: SSID=%s, IP=192.168.4.1", OTA_AP_SSID);

    /* 启动 HTTP 服务器，监听 80 端口 */
    start_webserver();

    notify_state(HW_OTA_AP_READY, 0);

    return ESP_OK;
}

/*
 * 停止 OTA 升级模式
 *
 * 流程：
 * 1. 停止 HTTP 服务器
 * 2. WiFi 模式切回纯 STA
 * 3. 销毁 AP 网络接口（下次 OTA 时重新创建）
 * 4. 重置所有状态变量
 */
void hw_ota_stop(void) {
    stop_webserver();

    if (g_ap_started) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        g_ap_started = false;
        ESP_LOGI(TAG, "AP stopped, restored STA mode");
    }

    /* 销毁 AP 网络接口，否则下次 esp_netif_create_default_wifi_ap() 会报重复 key 错误 */
    if (g_ap_netif) {
        esp_netif_destroy(g_ap_netif);
        g_ap_netif = NULL;
    }

    g_ota_state = HW_OTA_IDLE;
    g_ota_total_written = 0;
    g_ota_total_size = 0;
}

/* 获取当前OTA状态 */
hw_ota_state_t hw_ota_get_state(void) {
    return g_ota_state;
}
