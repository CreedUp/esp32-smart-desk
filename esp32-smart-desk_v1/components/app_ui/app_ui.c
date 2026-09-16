#include "app_ui.h"

#include <stddef.h>
#include <stdint.h>

#include "bsp_lcd.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

/*
 * LVGL 每个绘制缓冲区可以保存的屏幕行数。
 *
 * 屏幕宽度为 320 像素，颜色格式为 RGB565：
 *
 * 单个缓冲区：
 * 320 × 20 × 2 = 12800 字节
 *
 * 双缓冲区：
 * 12800 × 2 = 25600 字节
 */
#define APP_UI_DRAW_BUFFER_LINES    20

/*
 * 首页状态文字的更新时间。
 *
 * 每1000毫秒，也就是每1秒更新一次。
 */
#define APP_UI_STATUS_UPDATE_PERIOD_MS    1000U

/*
 * 首页状态定时器回调函数。
 *
 * 这个函数由LVGL任务调用，
 * 因此函数内部可以直接调用LVGL API，
 * 不需要再次调用lvgl_port_lock()。
 */
static void app_ui_status_timer_cb(lv_timer_t *timer)
{
    /*
     * 取出创建定时器时传入的状态标签指针。
     */
    lv_obj_t *status_label =
        (lv_obj_t *)lv_timer_get_user_data(timer);

    /*
     * lv_tick_get()返回LVGL运行的毫秒数。
     *
     * 除以1000以后得到运行秒数。
     */
    uint32_t running_seconds =
        lv_tick_get() / 1000U;

    /*
     * 修改状态标签。
     *
     * lv_label_set_text_fmt()支持类似printf的格式化参数。
     */
    lv_label_set_text_fmt(
        status_label,
        "LVGL running: %lu s",
        (unsigned long)running_seconds
    );
}

static const char *TAG = "app_ui";

/*
 * LVGL 显示设备对象。
 *
 * 它代表 LVGL 内部的一块显示屏，
 * 后面创建界面以及设置屏幕旋转时会用到。
 */
static lv_display_t *display = NULL;

/*
 * 创建首页。
 *
 * 调用本函数之前必须已经获得LVGL互斥锁。
 */
static esp_err_t app_ui_create_main_screen(void)
{
    /*
     * 获取当前活动页面。
     */
    lv_obj_t *screen = lv_screen_active();

    /*
     * 设置首页背景。
     */
    lv_obj_set_style_bg_color(
        screen,
        lv_color_hex(0x101820),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    /*
     * 创建标题。
     */
    lv_obj_t *title_label =
        lv_label_create(screen);

    lv_label_set_text(
        title_label,
        "ESP32 Smart Desk"
    );

    lv_obj_set_style_text_color(
        title_label,
        lv_color_hex(0xFFFFFF),
        LV_PART_MAIN
    );

    lv_obj_align(
        title_label,
        LV_ALIGN_CENTER,
        0,
        -15
    );

    /*
     * 创建运行状态标签。
     */
    lv_obj_t *status_label =
        lv_label_create(screen);

    lv_label_set_text(
        status_label,
        "LVGL running: 0 s"
    );

    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(0x55FF88),
        LV_PART_MAIN
    );

    lv_obj_align(
        status_label,
        LV_ALIGN_CENTER,
        0,
        15
    );

    /*
     * 创建LVGL软件定时器。
     *
     * 参数1：定时器到期时调用的回调函数
     * 参数2：调用周期，单位为毫秒
     * 参数3：传递给回调函数的用户指针
     */
    lv_timer_t *status_timer = lv_timer_create(
        app_ui_status_timer_cb,
        APP_UI_STATUS_UPDATE_PERIOD_MS,
        status_label
    );

    if (status_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create status timer");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t app_ui_init(void)
{
    /*
     * 防止重复初始化。
     */
    if (display != NULL)
    {
        ESP_LOGW(TAG, "LVGL display already initialized");
        return ESP_OK;
    }

    /*
     * 从 bsp_lcd 获取前面创建好的两个句柄。
     */
    esp_lcd_panel_io_handle_t io_handle =
        bsp_lcd_get_io_handle();

    esp_lcd_panel_handle_t panel_handle =
        bsp_lcd_get_panel_handle();

    /*
     * 如果句柄为空，说明 bsp_lcd_init() 还没有执行成功。
     */
    if ((io_handle == NULL) || (panel_handle == NULL))
    {
        ESP_LOGE(TAG, "LCD is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * 创建 LVGL Port 配置。
     *
     * 使用 esp_lvgl_port 提供的默认配置：
     *
     * task_priority      = 4
     * task_stack         = 7168 字节
     * task_affinity      = -1
     * task_max_sleep_ms  = 500
     * timer_period_ms    = 5
     */
    const lvgl_port_cfg_t lvgl_config =
        ESP_LVGL_PORT_INIT_CONFIG();

    /*
     * 初始化 LVGL Port。
     *
     * 该函数会完成：
     * 1. 调用 lv_init()
     * 2. 创建 LVGL 任务
     * 3. 创建 LVGL 时间基准定时器
     * 4. 创建 LVGL 互斥锁
     */
    esp_err_t ret = lvgl_port_init(&lvgl_config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to initialize LVGL port: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * 配置 LVGL 显示设备。
     */
    const lvgl_port_display_cfg_t display_config =
    {
        /*
         * esp_lcd 的 SPI Panel IO 句柄。
         *
         * esp_lvgl_port 通过它注册
         * SPI DMA 传输完成回调。
         */
        .io_handle = io_handle,

        /*
         * ST7789 Panel 句柄。
         *
         * LVGL 刷新屏幕时，最终会使用该句柄调用：
         * esp_lcd_panel_draw_bitmap()
         */
        .panel_handle = panel_handle,

        /*
         * 某些屏幕的数据接口和控制接口使用不同句柄。
         * 当前 ST7789 只使用一个 panel_handle。
         */
        .control_handle = NULL,

        /*
         * 单个绘制缓冲区的大小，单位是像素。
         *
         * 320 × 20 = 6400 像素
         * RGB565 每像素占用 2 字节。
         */
        .buffer_size =
            BSP_LCD_H_RES * APP_UI_DRAW_BUFFER_LINES,

        /*
         * 创建两个绘制缓冲区。
         *
         * 当 SPI DMA 正在发送第一个缓冲区时，
         * LVGL 可以在第二个缓冲区继续绘制。
         */
        .double_buffer = true,

        /*
         * PSRAM 帧缓冲区到内部 DMA 缓冲区的中转大小。
         *
         * 当前绘制缓冲区直接放在内部 DMA RAM，
         * 因此不需要中转缓冲区。
         */
        .trans_size = 0,

        /*
         * LVGL 看到的逻辑分辨率。
         */
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,

        /*
         * ST7789 是 RGB565 彩色屏，不是单色屏。
         */
        .monochrome = false,

        /*
         * 与 bsp_lcd_init() 中的硬件旋转设置保持一致：
         *
         * esp_lcd_panel_swap_xy(panel, true)
         * esp_lcd_panel_mirror(panel, true, false)
         */
        .rotation =
        {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },

        /*
         * 某些屏幕要求刷新区域按照特定边界对齐。
         * ST7789 当前不需要额外调整。
         */
        .rounder_cb = NULL,

        /*
         * 屏幕颜色格式为 RGB565。
         */
        .color_format = LV_COLOR_FORMAT_RGB565,

        .flags =
        {
            /*
             * 绘制缓冲区必须可以被 SPI DMA 访问。
             */
            .buff_dma = true,

            /*
             * 绘制缓冲区放在内部 RAM。
             *
             * ESP32-S3 的普通 SPI DMA 不能直接使用所有类型的
             * PSRAM 缓冲区，因此先使用内部 DMA RAM最稳妥。
             */
            .buff_spiram = false,

            /*
             * 已经由 ST7789 硬件完成坐标旋转，
             * 不需要 LVGL 做软件旋转。
             */
            .sw_rotate = false,

            /*
             * bsp_lcd 使用：
             *
             * LCD_RGB_DATA_ENDIAN_LITTLE
             *
             * 与 ESP32 本机的 RGB565 uint16_t 缓冲区匹配，
             * 所以 LVGL 不需要再次交换高低字节。
             */
            .swap_bytes = false,

            /*
             * 使用局部刷新。
             *
             * 只有界面发生变化的区域才会重新绘制。
             */
            .full_refresh = false,

            /*
             * direct_mode 要求使用完整屏幕缓冲区。
             * 当前使用20行局部缓冲，所以关闭。
             */
            .direct_mode = false,
        },
    };

    /*
     * 把 esp_lcd 显示设备注册给 LVGL。
     *
     * 返回值不是 esp_err_t：
     * 成功返回 lv_display_t 指针；
     * 失败返回 NULL。
     */
    display = lvgl_port_add_disp(&display_config);

    if (display == NULL)
    {
        ESP_LOGE(TAG, "Failed to add LVGL display");

        /*
         * 显示设备注册失败，释放刚才初始化的 LVGL Port。
         */
        lvgl_port_deinit();

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "LVGL display registered: %dx%d, buffer=%d lines",
        BSP_LCD_H_RES,
        BSP_LCD_V_RES,
        APP_UI_DRAW_BUFFER_LINES
    );

    /*
     * LVGL不是线程安全的。
     *
     * esp_lvgl_port已经创建了独立的LVGL任务。
     * 当前app_main任务要调用LVGL API时，
     * 必须先获取LVGL互斥锁。
     *
     * 参数0表示一直等待，直到成功获得锁。
     */
    if (!lvgl_port_lock(0))
    {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return ESP_FAIL;
    }

    /*
     * 把当前显示设备设置成默认显示设备。
     *
     * 当前只有一块LCD，实际上第一块注册的显示设备
     * 通常已经是默认显示设备。这里显式设置可以让
     * 代码的含义更加明确。
     */
    lv_display_set_default(display);

    app_ui_create_main_screen();
    /*
     * 所有LVGL API调用结束后释放互斥锁。
     */
    lvgl_port_unlock();

    return ESP_OK;
}