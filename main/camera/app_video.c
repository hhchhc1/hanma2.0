#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "esp_cam_sensor_types.h"
#include "app_video.h"
#include "gpio_sccb.h"
#include "esp_sccb_io_interface.h"
#include "driver/ledc.h"

// Forward declaration - esp_video_create_csi_video_device is in esp_video's private API
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev);
#endif
#include "ov5647.h"
#ifdef CONFIG_CAMERA_GC0308
#include "gc0308.h"
#endif

// Camera SCCB pin config — shares I2C_NUM_1 / GPIO28-29 with audio codec
#define CAM_SCCB_I2C_PORT       I2C_NUM_1
#define CAM_SCCB_I2C_SCL_PIN    GPIO_NUM_29
#define CAM_SCCB_I2C_SDA_PIN    GPIO_NUM_28
#define CAM_SCCB_I2C_FREQ       100000
#define CAM_RESET_PIN           GPIO_NUM_26
#define CAM_PWDN_PIN            GPIO_NUM_27
#define CAM_XCLK_PIN            GPIO_NUM_32
#define CAM_XCLK_FREQ_HZ        20000000

static const char *TAG = "app_video";

#define MAX_BUFFER_COUNT (3)
static uint8_t *buffer[MAX_BUFFER_COUNT];
static uint32_t buffer_size[MAX_BUFFER_COUNT];

esp_err_t app_video_init(int fd, video_fmt_t init_fmt)
{
    esp_err_t ret = ESP_OK;
    int fmt_index = 0;
    struct v4l2_format init_format;
    struct v4l2_capability capability;
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        ret = ESP_FAIL;
        goto exit_0;
    }

    ESP_LOGI(TAG, "driver: %s card: %s", capability.driver, capability.card);

    memset(&init_format, 0, sizeof(struct v4l2_format));
    init_format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &init_format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        ret = ESP_FAIL;
        goto exit_0;
    }
    ESP_LOGI(TAG, "width=%" PRIu32 " height=%" PRIu32,
        init_format.fmt.pix.width, init_format.fmt.pix.height);

    while (1) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = fmt_index++,
            .type = type,
        };

        if (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
            ESP_LOGW(TAG, "enum all fmt");
            ret = ESP_ERR_INVALID_ARG;
            goto exit_0;
        }

        if (fmtdesc.pixelformat != init_fmt) continue;

        struct v4l2_format format = {
            .type = type,
            .fmt.pix.width = init_format.fmt.pix.width,
            .fmt.pix.height = init_format.fmt.pix.height,
            .fmt.pix.pixelformat = fmtdesc.pixelformat,
        };

        if (ioctl(fd, VIDIOC_S_FMT, &format) != 0) {
            if (errno == ESRCH) continue;
            ESP_LOGE(TAG, "failed to set format");
            ret = ESP_FAIL;
            goto exit_0;
        }

        ESP_LOGI(TAG, "Capture %s format", (char *)fmtdesc.description);
        break;
    }

    return ret;
exit_0:
    close(fd);
    return ret;
}

esp_err_t app_video_main(i2c_master_bus_handle_t i2c_bus_handle)
{
    // XCLK on GPIO32 via LEDC — required by OV5647 regardless of SCCB method
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CAM_XCLK_FREQ_HZ,
        .clk_cfg = LEDC_USE_XTAL_CLK,
        .duty_resolution = LEDC_TIMER_1_BIT,
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    ESP_LOGI(TAG, "LEDC timer config %d Hz: %s", CAM_XCLK_FREQ_HZ, (ret == ESP_OK) ? "OK" : esp_err_to_name(ret));
    if (ret == ESP_OK) {
        ledc_channel_config_t ledc_chan = {
            .gpio_num = CAM_XCLK_PIN,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_0,
            .timer_sel = LEDC_TIMER_0,
            .duty = 1,
            .hpoint = 0,
        };
        ret = ledc_channel_config(&ledc_chan);
        ESP_LOGI(TAG, "LEDC XCLK on GPIO%d: %s", CAM_XCLK_PIN, (ret == ESP_OK) ? "OK" : esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (i2c_bus_handle != NULL) {
        esp_video_init_csi_config_t csi_config[] = {{
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = i2c_bus_handle,
                .freq = CAM_SCCB_I2C_FREQ,
            },
            .reset_pin = CAM_RESET_PIN,
            .pwdn_pin  = CAM_PWDN_PIN,
        }};

        esp_video_init_config_t cam_config = {
            .csi = csi_config,
        };

        return esp_video_init(&cam_config);
    }

    // ---- GPIO bit-banged SCCB fallback ----
    // Scan SCCB bus for any responding device
    ESP_LOGI(TAG, "Scanning SCCB bus on SDA=%d SCL=%d...", CAM_SCCB_I2C_SDA_PIN, CAM_SCCB_I2C_SCL_PIN);
    ret = gpio_sccb_scan(CAM_SCCB_I2C_SDA_PIN, CAM_SCCB_I2C_SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCCB bus scan found NO devices at all — check camera power, FPC, sensor type");
    }

    // Try OV5647
    esp_sccb_io_handle_t sccb = NULL;
    ret = gpio_sccb_new_io(CAM_SCCB_I2C_SDA_PIN, CAM_SCCB_I2C_SCL_PIN,
                            OV5647_SCCB_ADDR, &sccb);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO SCCB create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_cam_sensor_config_t cfg = {
        .sccb_handle = sccb,
        .reset_pin = CAM_RESET_PIN,
        .pwdn_pin  = CAM_PWDN_PIN,
        .xclk_pin = -1,
        .xclk_freq_hz = 0,
        .sensor_port = ESP_CAM_SENSOR_MIPI_CSI,
    };

    esp_cam_sensor_device_t *cam_dev = ov5647_detect(&cfg);
    if (!cam_dev) {
#ifdef CONFIG_CAMERA_GC0308
        ESP_LOGI(TAG, "OV5647 not detected, trying GC0308...");
        sccb->del(sccb);

        ret = gpio_sccb_new_io(CAM_SCCB_I2C_SDA_PIN, CAM_SCCB_I2C_SCL_PIN,
                               GC0308_SCCB_ADDR, &sccb);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "GPIO SCCB create failed for GC0308");
            return ret;
        }
        cfg.sccb_handle = sccb;
        cam_dev = gc0308_detect(&cfg);
#else
        ESP_LOGE(TAG, "no camera detected");
        sccb->del(sccb);
        return ESP_FAIL;
#endif
    }

    if (!cam_dev) {
        ESP_LOGE(TAG, "no camera detected via GPIO SCCB");
        sccb->del(sccb);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "camera detected");
    ret = esp_video_create_csi_video_device(cam_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CSI device create failed: %s", esp_err_to_name(ret));
        cam_dev->ops->del(cam_dev);
        sccb->del(sccb);
        return ret;
    }

    return ESP_OK;
}

static int video_open(int port)
{
    char name[16];
    int fd = -1;

    if (snprintf(name, sizeof(name), "/dev/video%d", port) <= 0) return -1;

    fd = open(name, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Open video %s fail", name);
        return -1;
    }
    return fd;
}

int app_video_open(int port)
{
    int fd = video_open(port);
    if (fd < 0) ESP_LOGE(TAG, "Open video fail");
    return fd;
}

esp_err_t camera_stream_start(int video_fd)
{
    ESP_LOGI(TAG, "Camera Start");
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        ESP_LOGE(TAG, "failed to start stream");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t camera_stream_stop(int video_fd)
{
    ESP_LOGI(TAG, "Camera Stop");
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMOFF, &type)) {
        ESP_LOGE(TAG, "failed to stop stream");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t camera_set_bufs(int video_fd, int fb_num, void **fb)
{
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num too large");
        return ESP_FAIL;
    }

    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    memset(&req, 0, sizeof(req));
    req.count = fb_num;
    req.type = type;
    req.memory = fb ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;

    if (ioctl(video_fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "req bufs failed");
        goto errout;
    }

    for (int i = 0; i < fb_num; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = type;
        buf.memory = req.memory;
        buf.index = i;

        if (ioctl(video_fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "query buf failed");
            goto errout;
        }

        if (req.memory == V4L2_MEMORY_MMAP) {
            buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd, buf.m.offset);
            if (buffer[i] == NULL) {
                ESP_LOGE(TAG, "mmap failed");
                goto errout;
            }
            buffer_size[i] = buf.length;
        } else {
            if (!fb[i]) {
                ESP_LOGE(TAG, "frame buffer is NULL");
                goto errout;
            }
            buf.m.userptr = (unsigned long)fb[i];
            buffer[i] = (uint8_t *)fb[i];
            buffer_size[i] = buf.length;
        }

        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "queue frame buffer failed");
            goto errout;
        }
    }

    return ESP_OK;

errout:
    close(video_fd);
    return ESP_FAIL;
}

esp_err_t camera_get_bufs(int fb_num, void **fb)
{
    if (fb_num > MAX_BUFFER_COUNT) {
        ESP_LOGE(TAG, "buffer num too large");
        return ESP_FAIL;
    }
    for (int i = 0; i < fb_num; i++) {
        if (buffer[i] != NULL) {
            fb[i] = buffer[i];
        } else {
            ESP_LOGE(TAG, "frame buffer is NULL");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

uint32_t camera_get_buf_size(int fb_num)
{
    if (fb_num > MAX_BUFFER_COUNT) return 0;
    return buffer_size[fb_num];
}
