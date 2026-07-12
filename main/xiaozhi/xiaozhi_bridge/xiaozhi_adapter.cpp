#include "xiaozhi_adapter.h"
#include "myi2s.h"

#include <string.h>
#include <string>
#include <vector>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_tls.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_mac.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <cJSON.h>

#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>

#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_hosted.h>
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>

#include <opus_encoder.h>
#include <opus_decoder.h>
#include <opus_resampler.h>

#define TAG "XZ"

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"
#define OPUS_FRAME_MS       60
#define SAMPLE_RATE         16000
#define FRAME_SAMPLES       (SAMPLE_RATE * OPUS_FRAME_MS / 1000)  // 960
#define ENCODE_QUEUE_SZ     4
#define DECODE_QUEUE_SZ     40
#define SEND_QUEUE_SZ       40
#define WS_RX_BUF_SZ        8192
#define WS_TX_BUF_SZ        8192

/* ---- callback storage ---- */
static xiaozhi_msg_cb_t    g_msg_cb = nullptr;
static xiaozhi_status_cb_t g_status_cb = nullptr;
static xiaozhi_wifi_cb_t   g_wifi_cb = nullptr;

static void notify_status(const char *s) {
    ESP_LOGI(TAG, "Status: %s", s);
    if (g_status_cb) g_status_cb(s);
}

static void notify_msg(const char *role, const char *text) {
    ESP_LOGI(TAG, "[%s] %s", role, text);
    if (g_msg_cb) g_msg_cb(role, text);
}

static void notify_wifi(const char *ssid) {
    if (g_wifi_cb) g_wifi_cb(ssid);
}

/* ---- audio packets ---- */
struct AudioPacket {
    uint32_t timestamp;
    std::vector<uint8_t> payload;
};

/* ---- helpers ---- */
static std::string get_mac_str() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(buf);
}

static std::string get_uuid() {
    nvs_handle_t nh;
    char uuid[37] = {};
    size_t len = sizeof(uuid);
    if (nvs_open("xiaozhi", NVS_READONLY, &nh) == ESP_OK) {
        nvs_get_str(nh, "uuid", uuid, &len);
        nvs_close(nh);
    }
    if (uuid[0]) return std::string(uuid);

    // Generate a new UUID v4
    uint8_t rnd[16];
    for (int i = 0; i < 16; i++) rnd[i] = esp_random() & 0xFF;
    rnd[6] = (rnd[6] & 0x0F) | 0x40; // version 4
    rnd[8] = (rnd[8] & 0x3F) | 0x80; // variant 1
    snprintf(uuid, sizeof(uuid),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             rnd[0], rnd[1], rnd[2], rnd[3], rnd[4], rnd[5], rnd[6], rnd[7],
             rnd[8], rnd[9], rnd[10], rnd[11], rnd[12], rnd[13], rnd[14], rnd[15]);

    if (nvs_open("xiaozhi", NVS_READWRITE, &nh) == ESP_OK) {
        nvs_set_str(nh, "uuid", uuid);
        nvs_commit(nh);
        nvs_close(nh);
    }
    return std::string(uuid);
}

/* ================================================================
 * OTA server discovery (HTTP)
 * ================================================================ */
static bool ota_discover(std::string &ws_url, std::string &ws_token) {
    ESP_LOGI(TAG, "OTA: contacting %s", OTA_URL);

    esp_http_client_config_t cfg = {};
    cfg.url = OTA_URL;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size = 2048;
    cfg.buffer_size_tx = 1024;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    std::string mac = get_mac_str();
    std::string uuid = get_uuid();
    char ua[64];
    snprintf(ua, sizeof(ua), "esp32-p4/1.0.0");

    esp_http_client_set_header(client, "Device-Id", mac.c_str());
    esp_http_client_set_header(client, "Client-Id", uuid.c_str());
    esp_http_client_set_header(client, "User-Agent", ua);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, "{}", 2);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "OTA: HTTP error %d / %s", status, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    // Read response body
    char resp[4096] = {};
    int total = 0;
    int r;
    do {
        r = esp_http_client_read(client, resp + total, sizeof(resp) - 1 - total);
        if (r > 0) total += r;
    } while (r > 0 && total < (int)(sizeof(resp) - 1));
    resp[total] = '\0';
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "OTA: response %d bytes", total);

    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        ESP_LOGE(TAG, "OTA: JSON parse failed");
        return false;
    }

    // Extract websocket config
    cJSON *ws = cJSON_GetObjectItem(root, "websocket");
    if (cJSON_IsObject(ws)) {
        cJSON *u = cJSON_GetObjectItem(ws, "url");
        cJSON *t = cJSON_GetObjectItem(ws, "token");
        if (cJSON_IsString(u)) ws_url = u->valuestring;
        if (cJSON_IsString(t)) ws_token = t->valuestring;
    }

    // Check for activation required
    cJSON *act = cJSON_GetObjectItem(root, "activation");
    if (cJSON_IsObject(act)) {
        cJSON *msg = cJSON_GetObjectItem(act, "message");
        if (cJSON_IsString(msg)) {
            notify_msg("system", msg->valuestring);
        }
    }

    cJSON_Delete(root);

    if (ws_url.empty()) {
        ESP_LOGE(TAG, "OTA: no websocket config in response");
        return false;
    }

    // Save to NVS
    nvs_handle_t nh;
    if (nvs_open("websocket", NVS_READWRITE, &nh) == ESP_OK) {
        nvs_set_str(nh, "url", ws_url.c_str());
        nvs_set_str(nh, "token", ws_token.c_str());
        nvs_commit(nh);
        nvs_close(nh);
        ESP_LOGI(TAG, "OTA: saved WS config to NVS");
    }

    return true;
}

/* ---- BSP I2S wrapper ---- */
class BspAudioIo {
public:
    bool input_enabled = false;
    bool output_enabled = false;

    int ReadMono(int16_t *dst, int samples) {
        if (!input_enabled) return 0;
        std::vector<int16_t> stereo(samples * 2);
        size_t n = 0;
        esp_err_t r = i2s_channel_read(rx_handle, stereo.data(),
                                        samples * 2 * sizeof(int16_t), &n, pdMS_TO_TICKS(120));
        if (r != ESP_OK || n < sizeof(int16_t) * 2) return 0;
        int cnt = n / sizeof(int16_t);
        for (int i = 0, j = 0; i < cnt && j < samples; i += 2, j++) dst[j] = stereo[i];
        return cnt / 2;
    }

    void WriteMono(const int16_t *src, int samples) {
        if (!output_enabled) return;
        std::vector<int16_t> stereo(samples * 2);
        for (int i = 0; i < samples; i++) {
            stereo[i * 2] = src[i];
            stereo[i * 2 + 1] = src[i];
        }
        size_t n;
        i2s_channel_write(tx_handle, stereo.data(),
                           samples * 2 * sizeof(int16_t), &n, pdMS_TO_TICKS(120));
    }
};

/* ================================================================
 * Simple WebSocket client using esp_tls
 * ================================================================ */
class SimpleWebSocket {
public:
    bool Connect(const char *uri) {
        const char *p = uri;
        bool use_tls = false;
        if (strncmp(p, "wss://", 6) == 0) { use_tls = true; p += 6; }
        else if (strncmp(p, "ws://", 5) == 0) { p += 5; }
        else return false;

        std::string host;
        int port = use_tls ? 443 : 80;
        std::string path = "/";

        const char *slash = strchr(p, '/');
        const char *at = strchr(p, '@');
        const char *host_start = p;
        if (at && at < (slash ? slash : p + strlen(p))) host_start = at + 1;

        const char *port_start = strchr(host_start, ':');
        const char *path_start = strchr(host_start, '/');

        if (port_start && (!path_start || port_start < path_start)) {
            host.assign(host_start, port_start - host_start);
            port = atoi(port_start + 1);
        } else if (path_start) {
            host.assign(host_start, path_start - host_start);
        } else {
            host.assign(host_start);
        }
        if (path_start) path = path_start;

        ESP_LOGI(TAG, "Connecting to %s:%d%s (TLS=%d)", host.c_str(), port, path.c_str(), use_tls);

        if (use_tls) {
            tls_ = esp_tls_init();
            if (!tls_) return false;

            esp_tls_cfg_t tls_cfg = {};
            tls_cfg.crt_bundle_attach = esp_crt_bundle_attach;
            tls_cfg.non_block = false;
            tls_cfg.timeout_ms = 10000;

            int ret = esp_tls_conn_new_sync(host.c_str(), host.size(), port, &tls_cfg, tls_);
            if (ret != 1) {
                ESP_LOGE(TAG, "TLS connect fail: %d", ret);
                esp_tls_conn_destroy(tls_);
                tls_ = nullptr;
                return false;
            }
        } else {
            struct addrinfo hints = {}, *res;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            char port_str[8];
            snprintf(port_str, sizeof(port_str), "%d", port);
            int ret = getaddrinfo(host.c_str(), port_str, &hints, &res);
            if (ret != 0 || !res) {
                ESP_LOGE(TAG, "DNS fail for %s:%d", host.c_str(), port);
                return false;
            }
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0 || connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                freeaddrinfo(res); return false;
            }
            freeaddrinfo(res);
            tcp_sock_ = sock;
            is_plain_ = true;
        }

        return DoHandshake(host.c_str(), path.c_str());
    }

    bool IsConnected() const { return tls_ != nullptr || tcp_sock_ >= 0; }

    void Close() {
        if (!IsConnected()) return;
        uint8_t close_frame[6] = {0x88, 0x80};
        uint32_t mask = esp_random();
        memcpy(close_frame + 2, &mask, 4);
        if (is_plain_) send(GetSockfd(), close_frame, 6, 0);
        else esp_tls_conn_write(tls_, close_frame, 6);
        DestroyConn();
    }

    void DestroyConn() {
        if (is_plain_) {
            if (tcp_sock_ >= 0) close(tcp_sock_);
            tcp_sock_ = -1;
        } else if (tls_) {
            esp_tls_conn_destroy(tls_);
            tls_ = nullptr;
        }
    }

    bool SendBinary(const uint8_t *data, int len) {
        return SendFrame(0x82, data, len);
    }

    bool SendText(const char *data, int len) {
        return SendFrame(0x81, (const uint8_t*)data, len);
    }

    int Read(uint8_t *buf, int max_len, int timeout_ms) {
        if (!IsConnected()) return -1;
        int fd = GetSockfd();
        struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) return 0;

        if (is_plain_) return recv(fd, buf, max_len, 0);
        return esp_tls_conn_read(tls_, buf, max_len);
    }

private:
    esp_tls_t *tls_ = nullptr;
    bool is_plain_ = false;
    int tcp_sock_ = -1;

    int GetSockfd() {
        if (!is_plain_) {
            int fd = -1;
            esp_tls_get_conn_sockfd(tls_, &fd);
            return fd;
        }
        return tcp_sock_;
    }

    bool DoHandshake(const char *host, const char *path) {
        uint8_t rnd[16];
        for (int i = 0; i < 16; i++) rnd[i] = esp_random() & 0xFF;
        size_t key_b64_len;
        unsigned char key_b64[32];
        mbedtls_base64_encode(key_b64, sizeof(key_b64), &key_b64_len, rnd, 16);
        key_b64[key_b64_len] = '\0';

        char req[1024];
        int req_len = snprintf(req, sizeof(req),
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: %s\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n",
            path, host, key_b64);

        int ret;
        if (is_plain_) ret = send(GetSockfd(), req, req_len, 0);
        else ret = esp_tls_conn_write(tls_, req, req_len);
        if (ret <= 0) { DestroyConn(); return false; }

        char rsp[2048];
        int total = 0;
        for (int i = 0; i < 100; i++) {
            int n;
            if (is_plain_) n = recv(GetSockfd(), rsp + total, sizeof(rsp) - 1 - total, 0);
            else n = esp_tls_conn_read(tls_, rsp + total, sizeof(rsp) - 1 - total);
            if (n <= 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }
            total += n;
            rsp[total] = '\0';
            if (strstr(rsp, "\r\n\r\n")) break;
        }

        if (!strstr(rsp, "101") || !strstr(rsp, "Upgrade: websocket")) {
            ESP_LOGE(TAG, "WS handshake failed: %s", rsp);
            DestroyConn();
            return false;
        }

        ESP_LOGI(TAG, "WS connected");
        return true;
    }

    bool SendFrame(uint8_t opcode, const uint8_t *data, int len) {
        if (!IsConnected()) return false;
        std::vector<uint8_t> frame;
        frame.push_back(opcode);

        uint8_t mask_key[4];
        for (int i = 0; i < 4; i++) mask_key[i] = esp_random() & 0xFF;

        if (len < 126) {
            frame.push_back(len | 0x80);
        } else if (len < 65536) {
            frame.push_back(126 | 0x80);
            frame.push_back((len >> 8) & 0xFF);
            frame.push_back(len & 0xFF);
        } else {
            frame.push_back(127 | 0x80);
            for (int i = 7; i >= 0; i--) frame.push_back((len >> (i * 8)) & 0xFF);
        }

        frame.insert(frame.end(), mask_key, mask_key + 4);
        for (int i = 0; i < len; i++) frame.push_back(data[i] ^ mask_key[i % 4]);

        int ret;
        if (is_plain_) ret = send(GetSockfd(), frame.data(), frame.size(), 0);
        else ret = esp_tls_conn_write(tls_, frame.data(), frame.size());
        return ret > 0;
    }
};

/* ================================================================
 * XiaozhiCore - all-in-one audio + websocket assistant
 * ================================================================ */
class XiaozhiCore {
public:
    static XiaozhiCore& Inst() { static XiaozhiCore c; return c; }

    void InitSys() {
        if (sys_inited_) return;
        sys_inited_ = true;
        esp_err_t r = nvs_flash_init();
        if (r != ESP_OK && r != ESP_ERR_NVS_NO_FREE_PAGES) {
            nvs_flash_erase();
            nvs_flash_init();
        }
        esp_netif_init();
        if (esp_event_loop_create_default() != ESP_OK) {
            ESP_LOGW(TAG, "Event loop already exists");
        }
    }

    esp_err_t Start(const char *url, const char *token) {
        ws_url_ = url ? url : "";
        ws_token_ = token ? token : "";

        InitSys();

        evt_group_ = xEventGroupCreate();
        enc_queue_ = xQueueCreate(ENCODE_QUEUE_SZ, sizeof(AudioPacket));
        dec_queue_ = xQueueCreate(DECODE_QUEUE_SZ, sizeof(AudioPacket));
        snd_queue_ = xQueueCreate(SEND_QUEUE_SZ, sizeof(AudioPacket));
        pcm_queue_ = xQueueCreate(4, sizeof(AudioPacket));

        io_.input_enabled = true;
        io_.output_enabled = true;

        encoder_ = std::make_unique<OpusEncoderWrapper>(SAMPLE_RATE, 1, OPUS_FRAME_MS);
        encoder_->SetComplexity(0);
        decoder_ = std::make_unique<OpusDecoderWrapper>(24000, 1, 60);

        xTaskCreate(TaskMic,   "xz_mic",   4096, this, 8, &task_mic_);
        xTaskCreate(TaskSpk,   "xz_spk",   4096, this, 4, &task_spk_);
        xTaskCreate(TaskCodec, "xz_codec", 20480, this, 2, &task_codec_);
        xTaskCreate(TaskNet,   "xz_net",   12288, this, 5, &task_net_);

        ESP_LOGI(TAG, "Core started, waiting for Toggle");
        return ESP_OK;
    }

    void Toggle() {
        // Lazy init: if no event group, connect WiFi + discover server + start
        if (evt_group_ == nullptr) {
            InitSys();

            // Step 1: Wait for ESP-Hosted SDIO transport
            if (!WaitForHostedTransport()) {
                notify_status("WiFi HW error");
                notify_msg("system", "WiFi co-processor not responding. Check hardware.");
                return;
            }

            // Step 2: Connect WiFi (uses NVS credentials; needs provisioning if empty)
            if (!ConnectWiFi()) {
                // No saved credentials or connection failed → AP provisioning
                StartWiFiProvisioning();
                return;  // Will reboot after provisioning
            }

            // Step 3: Try NVS first for saved server URL
            nvs_handle_t nh;
            char saved_url[256] = {};
            char saved_token[256] = {};
            size_t len = sizeof(saved_url);
            if (nvs_open("websocket", NVS_READONLY, &nh) == ESP_OK) {
                nvs_get_str(nh, "url", saved_url, &len);
                len = sizeof(saved_token);
                nvs_get_str(nh, "token", saved_token, &len);
                nvs_close(nh);
            }

            if (saved_url[0]) {
                ESP_LOGI(TAG, "Using saved WS URL from NVS");
                ws_url_ = saved_url;
                ws_token_ = saved_token;
            } else {
                // Step 4: OTA discovery (WiFi is now connected)
                notify_status("Discovering server...");
                std::string discovered_url, discovered_token;
                if (!ota_discover(discovered_url, discovered_token)) {
                    notify_status("Server discovery failed");
                    notify_msg("system", "Cannot reach xiaozhi server.");
                    return;
                }
                ws_url_ = discovered_url;
                ws_token_ = discovered_token;
            }

            // Step 5: Start the core
            if (Start(ws_url_.c_str(), ws_token_.c_str()) != ESP_OK) {
                notify_status("Start failed");
                return;
            }
        }

        if (state_ == kIdle) {
            state_ = kConnecting;
            xEventGroupSetBits(evt_group_, EV_RUN);
            notify_status("Connecting...");
        } else {
            state_ = kIdle;
            xEventGroupSetBits(evt_group_, EV_STOP);
            Disconnect();
            DrainQueues();
            notify_status("Idle");
        }
    }

    bool IsStarted() const { return evt_group_ != nullptr; }

private:
    enum State { kIdle, kConnecting, kListening, kSpeaking };
    static constexpr EventBits_t EV_RUN  = BIT0;
    static constexpr EventBits_t EV_STOP = BIT1;

    BspAudioIo io_;
    State state_ = kIdle;
    std::string ws_url_, ws_token_, session_id_;
    int svr_rate_ = 24000, svr_frame_ms_ = 60, ws_ver_ = 2;
    uint32_t ts_ = 0;

    EventGroupHandle_t evt_group_ = nullptr;
    QueueHandle_t enc_queue_ = nullptr;
    QueueHandle_t dec_queue_ = nullptr;
    QueueHandle_t snd_queue_ = nullptr;
    QueueHandle_t pcm_queue_ = nullptr;
    TaskHandle_t task_mic_ = nullptr, task_spk_ = nullptr, task_codec_ = nullptr, task_net_ = nullptr;

    std::unique_ptr<OpusEncoderWrapper> encoder_;
    std::unique_ptr<OpusDecoderWrapper> decoder_;
    OpusResampler resampler_;

    SimpleWebSocket ws_;
    bool sys_inited_ = false;
    TickType_t last_rx_tick_ = 0;

    void DrainQueues() {
        if (!evt_group_) return;
        AudioPacket pkt;
        while (xQueueReceive(enc_queue_, &pkt, 0) == pdTRUE) {}
        while (xQueueReceive(dec_queue_, &pkt, 0) == pdTRUE) {}
        while (xQueueReceive(snd_queue_, &pkt, 0) == pdTRUE) {}
        while (xQueueReceive(pcm_queue_, &pkt, 0) == pdTRUE) {}
        encoder_->ResetState();
        decoder_->ResetState();
        ts_ = 0;
    }

    static void TaskMic(void *arg) {
        auto *s = (XiaozhiCore*)arg;
        std::vector<int16_t> buf(FRAME_SAMPLES);

        while (true) {
            if (s->evt_group_ == nullptr || !(xEventGroupGetBits(s->evt_group_) & EV_RUN)) {
                vTaskDelay(pdMS_TO_TICKS(20)); continue;
            }

            int n = s->io_.ReadMono(buf.data(), FRAME_SAMPLES);
            if (n >= FRAME_SAMPLES / 2) {
                AudioPacket pkt;
                pkt.timestamp = s->ts_;
                pkt.payload.assign((uint8_t*)buf.data(), (uint8_t*)(buf.data() + n));
                xQueueSend(s->enc_queue_, &pkt, 0);
            }

            if (xEventGroupGetBits(s->evt_group_) & EV_STOP) {
                xEventGroupClearBits(s->evt_group_, EV_STOP | EV_RUN);
                s->state_ = kIdle;
                notify_status("Idle");
            }
        }
    }

    static void TaskSpk(void *arg) {
        auto *s = (XiaozhiCore*)arg;
        while (true) {
            AudioPacket pkt;
            if (xQueueReceive(s->pcm_queue_, &pkt, pdMS_TO_TICKS(100)) == pdTRUE) {
                int16_t *samples = (int16_t*)pkt.payload.data();
                int count = pkt.payload.size() / sizeof(int16_t);
                s->io_.WriteMono(samples, count);
            }
        }
    }

    static void TaskCodec(void *arg) {
        auto *s = (XiaozhiCore*)arg;

        while (true) {
            AudioPacket enc;
            while (xQueueReceive(s->enc_queue_, &enc, 0) == pdTRUE) {
                s->ts_ += OPUS_FRAME_MS;
                auto pcm_vec = std::vector<int16_t>((int16_t*)enc.payload.data(),
                                                     (int16_t*)(enc.payload.data() + enc.payload.size()));
                s->encoder_->Encode(std::move(pcm_vec), [s](std::vector<uint8_t>&& opus) {
                    AudioPacket out;
                    out.timestamp = s->ts_;
                    out.payload = std::move(opus);
                    xQueueSend(s->snd_queue_, &out, 0);
                });
            }

            AudioPacket dec;
            while (xQueueReceive(s->dec_queue_, &dec, 0) == pdTRUE) {
                std::vector<int16_t> pcm;
                if (s->decoder_->Decode(std::move(dec.payload), pcm)) {
                    int out_rate = s->decoder_->sample_rate();
                    if (out_rate != SAMPLE_RATE) {
                        s->resampler_.Configure(out_rate, SAMPLE_RATE);
                        int out_n = s->resampler_.GetOutputSamples(pcm.size());
                        std::vector<int16_t> rs(out_n);
                        s->resampler_.Process(pcm.data(), pcm.size(), rs.data());
                        pcm = std::move(rs);
                    }
                    AudioPacket out;
                    out.payload.assign((uint8_t*)pcm.data(), (uint8_t*)(pcm.data() + pcm.size()));
                    xQueueSend(s->pcm_queue_, &out, 0);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    static void TaskNet(void *arg) {
        auto *s = (XiaozhiCore*)arg;

        while (true) {
            if (s->evt_group_ == nullptr || !(xEventGroupGetBits(s->evt_group_) & EV_RUN)) {
                vTaskDelay(pdMS_TO_TICKS(200)); continue;
            }

            if (s->state_ == kConnecting) {
                if (s->ConnectWs()) {
                    s->state_ = kListening;
                    notify_status("Listening");
                    notify_msg("system", "Connected. Start talking!");
                } else {
                    notify_status("Connecting...retry in 3s");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            }

            if (s->ws_.IsConnected()) {
                AudioPacket pkt;
                while (xQueueReceive(s->snd_queue_, &pkt, 0) == pdTRUE) {
                    s->SendBinary(pkt);
                }
                s->ReadWsData();
                TickType_t now = xTaskGetTickCount();
                if (now - s->last_rx_tick_ > pdMS_TO_TICKS(15000)) {
                    ESP_LOGW(TAG, "Server timeout");
                    s->Disconnect();
                    s->state_ = kIdle;
                    xEventGroupClearBits(s->evt_group_, EV_RUN);
                    notify_status("Disconnected");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    /* ---- WiFi: wait for ESP-Hosted SDIO transport ---- */
    bool WaitForHostedTransport() {
        esp_hosted_init();

        wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
        for (int i = 0; i < 60; i++) {
            esp_err_t r = esp_wifi_init(&wifi_cfg);
            if (r == ESP_OK) {
                esp_wifi_deinit();
                ESP_LOGI(TAG, "ESP-Hosted transport ready");
                return true;
            }
            if (i == 0) {
                ESP_LOGW(TAG, "Waiting for ESP-Hosted transport...");
                notify_status("Starting WiFi...");
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        ESP_LOGE(TAG, "ESP-Hosted transport timeout");
        return false;
    }

    /* ---- WiFi: connect using WifiStation (matches original wifi_board.cc) ---- */
    bool ConnectWiFi() {
        // Already connected?
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi already connected, SSID: %s", (char*)ap.ssid);
            notify_wifi((char*)ap.ssid);
            return true;
        }

        auto& ssid_manager = SsidManager::GetInstance();
        auto ssid_list = ssid_manager.GetSsidList();

        // Pre-configure default Wi-Fi network (matches original wifi_board.cc line 83-85)
        if (ssid_list.empty()) {
            ssid_manager.AddSsid("ZBCK", "ZBCK-E123");
        }

        auto& station = WifiStation::GetInstance();
        station.OnScanBegin([]() {
            notify_status("Scanning WiFi...");
        });
        station.OnConnect([](const std::string& ssid) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Connecting to %s...", ssid.c_str());
            notify_status(buf);
        });
        station.OnConnected([](const std::string& ssid) {
            notify_wifi(ssid.c_str());
            char buf[64];
            snprintf(buf, sizeof(buf), "Connected to %s", ssid.c_str());
            notify_status(buf);
        });

        station.Start();

        notify_status("WiFi connecting...");
        // Match original: 60 second timeout (wifi_board.cc line 111)
        if (!station.WaitForConnected(60000)) {
            ESP_LOGW(TAG, "WiFi connection failed");
            station.Stop();
            return false;
        }

        ESP_LOGI(TAG, "WiFi connected: %s", station.GetSsid().c_str());
        return true;
    }

    /* ---- WiFi: AP provisioning hotspot (matches original wifi_board.cc EnterWifiConfigMode) ---- */
    void StartWiFiProvisioning() {
        notify_status("WiFi Config Mode");

        auto& ap = WifiConfigurationAp::GetInstance();
        ap.SetSsidPrefix("Xiaozhi");
        ap.Start();

        std::string ssid = ap.GetSsid();
        std::string url = ap.GetWebServerUrl();

        // Match original wifi_board.cc: show SSID and URL clearly
        char msg[256];
        snprintf(msg, sizeof(msg),
            "请用手机连接WiFi热点:\n  %s\n然后浏览器访问:\n  %s\n配置WiFi后重启设备。",
            ssid.c_str(), url.c_str());
        notify_msg("system", msg);
        ESP_LOGI(TAG, "AP provisioning: SSID=%s URL=%s", ssid.c_str(), url.c_str());

        // Block forever — user must reboot after provisioning
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    bool ConnectWs() {
        if (!ConnectWiFi()) return false;

        std::string uri = ws_url_;
        if (!ws_token_.empty()) {
            size_t pos = uri.find("://");
            if (pos != std::string::npos) {
                pos += 3;
                uri.insert(pos, ws_token_ + "@");
            }
        }

        if (!ws_.Connect(uri.c_str())) {
            ESP_LOGE(TAG, "WS connect failed");
            return false;
        }

        last_rx_tick_ = xTaskGetTickCount();
        SendHello();

        for (int i = 0; i < 100 && session_id_.empty(); i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
            ReadWsData();
        }
        if (session_id_.empty()) {
            ESP_LOGW(TAG, "No server hello");
            Disconnect();
            return false;
        }

        ESP_LOGI(TAG, "Audio channel open, session=%s", session_id_.c_str());
        return true;
    }

    void SendHello() {
        cJSON *r = cJSON_CreateObject();
        cJSON_AddStringToObject(r, "type", "hello");
        cJSON_AddNumberToObject(r, "version", ws_ver_);
        cJSON_AddStringToObject(r, "transport", "websocket");
        cJSON *f = cJSON_CreateObject();
        cJSON_AddBoolToObject(f, "mcp", true);
        cJSON_AddItemToObject(r, "features", f);
        cJSON *a = cJSON_CreateObject();
        cJSON_AddStringToObject(a, "format", "opus");
        cJSON_AddNumberToObject(a, "sample_rate", SAMPLE_RATE);
        cJSON_AddNumberToObject(a, "channels", 1);
        cJSON_AddNumberToObject(a, "frame_duration", OPUS_FRAME_MS);
        cJSON_AddItemToObject(r, "audio_params", a);
        char *js = cJSON_PrintUnformatted(r);
        ws_.SendText(js, strlen(js));
        cJSON_free(js);
        cJSON_Delete(r);
    }

    void SendBinary(const AudioPacket &pkt) {
        if (!ws_.IsConnected()) return;

        if (ws_ver_ == 2) {
            uint8_t hdr[16] = {};
            *(uint16_t*)(hdr + 0) = htons((uint16_t)ws_ver_);
            *(uint32_t*)(hdr + 8) = htonl(pkt.timestamp);
            *(uint32_t*)(hdr + 12) = htonl((uint32_t)pkt.payload.size());
            std::vector<uint8_t> out(16 + pkt.payload.size());
            memcpy(out.data(), hdr, 16);
            memcpy(out.data() + 16, pkt.payload.data(), pkt.payload.size());
            ws_.SendBinary(out.data(), out.size());
        } else {
            ws_.SendBinary(pkt.payload.data(), pkt.payload.size());
        }
    }

    void Disconnect() {
        ws_.Close();
        ws_.DestroyConn();
    }

    void ReadWsData() {
        if (!ws_.IsConnected()) return;

        uint8_t buf[WS_RX_BUF_SZ];
        int len = ws_.Read(buf, sizeof(buf), 1);
        if (len <= 0) return;

        last_rx_tick_ = xTaskGetTickCount();

        int pos = 0;
        while (pos + 2 <= len) {
            uint8_t opcode = buf[pos] & 0x0F;
            bool masked = (buf[pos + 1] & 0x80) != 0;
            int plen = buf[pos + 1] & 0x7F;
            int hdr_len = 2;

            if (plen == 126) { if (pos + 4 > len) break; plen = (buf[pos+2] << 8) | buf[pos+3]; hdr_len += 2; }
            else if (plen == 127) { if (pos + 10 > len) break; plen = 0; for (int i = 0; i < 8; i++) plen = (plen << 8) | buf[pos+2+i]; hdr_len += 8; }

            uint8_t mask[4] = {};
            if (masked) { if (pos + hdr_len + 4 > len) break; memcpy(mask, buf + pos + hdr_len, 4); hdr_len += 4; }

            if (pos + hdr_len + plen > len) break;

            uint8_t *payload = buf + pos + hdr_len;
            if (masked) for (int i = 0; i < plen; i++) payload[i] ^= mask[i % 4];

            if (opcode == 0x02) {
                HandleWsBinary(payload, plen);
            } else if (opcode == 0x01) {
                HandleWsText((const char*)payload, plen);
            } else if (opcode == 0x08) {
                ESP_LOGI(TAG, "WS close received");
                Disconnect();
                state_ = kIdle;
                xEventGroupClearBits(evt_group_, EV_RUN);
                notify_status("Disconnected");
                return;
            } else if (opcode == 0x09) {
                std::vector<uint8_t> pong_frame = {0x8A, 0x80, 0, 0, 0, 0};
                ws_.SendBinary(pong_frame.data(), pong_frame.size());
            }

            pos += hdr_len + plen;
        }
    }

    void HandleWsBinary(const uint8_t *data, int len) {
        if (ws_ver_ == 2 && len >= 16) {
            uint32_t psz = ntohl(*(uint32_t*)(data + 12));
            if (psz <= (uint32_t)(len - 16)) {
                AudioPacket pkt;
                pkt.payload.assign(data + 16, data + 16 + psz);
                xQueueSend(dec_queue_, &pkt, 0);
            }
        } else if (len > 0) {
            AudioPacket pkt;
            pkt.payload.assign(data, data + len);
            xQueueSend(dec_queue_, &pkt, 0);
        }
    }

    void HandleWsText(const char *data, int len) {
        cJSON *root = cJSON_ParseWithLength(data, len);
        if (!root) return;

        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (cJSON_IsString(type)) {
            const char *tp = type->valuestring;

            if (strcmp(tp, "hello") == 0) {
                cJSON *sid = cJSON_GetObjectItem(root, "session_id");
                if (cJSON_IsString(sid)) session_id_ = sid->valuestring;
                cJSON *ap = cJSON_GetObjectItem(root, "audio_params");
                if (cJSON_IsObject(ap)) {
                    cJSON *sr = cJSON_GetObjectItem(ap, "sample_rate");
                    if (cJSON_IsNumber(sr)) {
                        svr_rate_ = sr->valueint;
                        decoder_ = std::make_unique<OpusDecoderWrapper>(svr_rate_, 1, svr_frame_ms_);
                    }
                }
                ESP_LOGI(TAG, "Server hello: session=%s rate=%d", session_id_.c_str(), svr_rate_);
            }
            else if (strcmp(tp, "stt") == 0) {
                cJSON *txt = cJSON_GetObjectItem(root, "text");
                if (cJSON_IsString(txt) && txt->valuestring[0]) {
                    notify_msg("user", txt->valuestring);
                }
            }
            else if (strcmp(tp, "tts") == 0) {
                cJSON *st = cJSON_GetObjectItem(root, "state");
                if (cJSON_IsString(st)) {
                    if (strcmp(st->valuestring, "start") == 0) {
                        state_ = kSpeaking;
                        notify_status("Speaking");
                    } else if (strcmp(st->valuestring, "stop") == 0) {
                        state_ = kListening;
                        notify_status("Listening");
                    }
                }
            }
            else if (strcmp(tp, "llm") == 0) {
                cJSON *emo = cJSON_GetObjectItem(root, "emotion");
                if (cJSON_IsString(emo)) {
                    notify_msg("assistant", emo->valuestring);
                }
            }
        }
        cJSON_Delete(root);
    }

    XiaozhiCore() {}
};

/* ================================================================
 * Public C API
 * ================================================================ */
extern "C" {

esp_err_t xiaozhi_adapter_init(const char *ws_url, const char *ws_token) {
    XiaozhiCore::Inst().InitSys();
    // Don't Start() yet - wait for Toggle() to trigger lazy init with OTA
    if (ws_url && ws_url[0]) {
        return XiaozhiCore::Inst().Start(ws_url, ws_token);
    }
    // URL is empty: the core will discover server when user clicks TALK
    ESP_LOGI(TAG, "No URL provided, will discover server on first Toggle");
    return ESP_OK;
}

void xiaozhi_adapter_toggle_chat(void) {
    XiaozhiCore::Inst().Toggle();
}

void xiaozhi_adapter_set_message_callback(xiaozhi_msg_cb_t cb) {
    g_msg_cb = cb;
}

void xiaozhi_adapter_set_status_callback(xiaozhi_status_cb_t cb) {
    g_status_cb = cb;
}

void xiaozhi_adapter_set_wifi_callback(xiaozhi_wifi_cb_t cb) {
    g_wifi_cb = cb;
}

} // extern "C"
