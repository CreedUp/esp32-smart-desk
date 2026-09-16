#include "bsp_lcd.h"

#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#include "bsp_pca9557.h"
#include "bsp_spi.h"

/*
 * LCD 的命令/数据选择引脚。
 *
 * DC = 0：命令
 * DC = 1：参数或像素
 */
#define BSP_LCD_DC_GPIO               GPIO_NUM_39

/*
 * LCD 的背光开启选择引脚。
 *
 * BL = 0：开启
 * BL = 1：关闭
 */
#define BSP_LCD_BACKLIGHT_GPIO          GPIO_NUM_42

#define BSP_LCD_BACKLIGHT_ON_LEVEL      0
#define BSP_LCD_BACKLIGHT_OFF_LEVEL     1

/*
 * 立创官方例程使用 SPI mode 2。
 *
 * CPOL = 1：时钟空闲时为高电平。
 * CPHA = 0：在第一个有效边沿采样。
 */
#define BSP_LCD_SPI_MODE              2

/*
 * LCD SPI 时钟为 80 MHz。
 */
#define BSP_LCD_PIXEL_CLOCK_HZ        (80U * 1000U * 1000U)

/*
 * 允许同时排队的 LCD SPI 事务数量。
 */
#define BSP_LCD_TRANSACTION_QUEUE     10U

/*
 * ST7789 的命令和参数基本单位都是 8 bit。
 */
#define BSP_LCD_COMMAND_BITS          8
#define BSP_LCD_PARAMETER_BITS        8

static const char *TAG = "bsp_lcd";

//一行像素的DMA缓冲区
static DMA_ATTR uint16_t lcd_line_buffer[BSP_LCD_H_RES];

/*
 * Panel IO 句柄。
 *
 * 它代表：
 * “ST7789 与 SPI3 总线之间的通信通道”。
 *
 */
esp_lcd_panel_io_handle_t lcd_io_handle = NULL;

/*
 * ST7789 Panel 句柄。
 *
 * lcd_io_handle 负责“怎么传输”；
 * lcd_panel_handle 负责“屏幕是什么型号、如何初始化和绘图”。
 */
esp_lcd_panel_handle_t lcd_panel_handle = NULL;

//LCD背光控制函数
esp_err_t bsp_lcd_backlight_set(bool on)
{
    int level = on ? BSP_LCD_BACKLIGHT_ON_LEVEL
               : BSP_LCD_BACKLIGHT_OFF_LEVEL;

    esp_err_t ret = gpio_set_level(BSP_LCD_BACKLIGHT_GPIO, level);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set LCD backlight: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

//LCD填充屏幕颜色函数
esp_err_t bsp_lcd_fill_color(uint16_t color)
{
    if (lcd_panel_handle == NULL)
    {
        ESP_LOGE(TAG, "LCD is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * 先生成一整行相同颜色的像素。
     */
    for (int x = 0; x < BSP_LCD_H_RES; x++)
    {
        lcd_line_buffer[x] = color;
    }

    /*
     * 把同一行数据依次写入屏幕的240行。
     */
    for (int y = 0; y < BSP_LCD_V_RES; y++)
    {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(
            lcd_panel_handle,
            0,
            y,
            BSP_LCD_H_RES,
            y + 1,
            lcd_line_buffer
        );

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG,
                     "Failed to draw LCD line %d: %s",
                     y,
                     esp_err_to_name(ret));
            return ret;
        }
    }

    /*
     * 打开显示输出。
     *
     * 这个调用还会等待前面已经进入SPI队列的颜色数据发送完成，
     * 因此返回后可以安全地继续使用或修改行缓冲区。
     */
    esp_err_t ret = esp_lcd_panel_disp_on_off(
        lcd_panel_handle,
        true
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable LCD display: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t bsp_lcd_init(void)
{
    /*
     * 防止重复创建 Panel IO。
     */
    if ((lcd_io_handle != NULL) ||
        (lcd_panel_handle != NULL))
    {
        ESP_LOGW(TAG, "LCD already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret;

    /*
     * 配置LCD背光引脚。
     */
    const gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << BSP_LCD_BACKLIGHT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ret = gpio_config(&backlight_config);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure LCD backlight GPIO: %s",
                 esp_err_to_name(ret));
        return ret;
    }

    /*
     * 初始化过程中先关闭背光。
     *
     * 这样可以避免ST7789初始化时，用户看到白屏、闪屏
     * 或不完整的初始化画面。
     */
    ret = bsp_lcd_backlight_set(false);

    if (ret != ESP_OK)
    {
        return ret;
    }

    /*
     * 配置 LCD 在 SPI 总线上的通信参数。
     */
    const esp_lcd_panel_io_spi_config_t io_config = {
        /*
         * LCD_CS 不连接 ESP32 GPIO，而是连接 PCA9557 IO0。
         *
         * -1 表示 SPI 驱动不控制 CS。
         */
        .cs_gpio_num = -1,

        /*
         * GPIO39 用于区分命令和数据。
         */
        .dc_gpio_num = BSP_LCD_DC_GPIO,

        /*
         * ST7789 使用 SPI mode 2。
         */
        .spi_mode = BSP_LCD_SPI_MODE,

        /*
         * SPI 时钟频率。
         */
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,

        /*
         * SPI 事务队列深度。
         */
        .trans_queue_depth = BSP_LCD_TRANSACTION_QUEUE,

        /*
         * 当前还没有配置 DMA 完成回调。
         * 后面接入 LVGL 时再设置。
         */
        .on_color_trans_done = NULL,
        .user_ctx = NULL,

        /*
         * ST7789 命令和参数宽度均为 8 bit。
         */
        .lcd_cmd_bits = BSP_LCD_COMMAND_BITS,
        .lcd_param_bits = BSP_LCD_PARAMETER_BITS,
    };

    /*
     * 在已经初始化的 SPI3 总线上创建 LCD Panel IO。
     *
     * 参数1：
     * SPI 总线。esp_lcd 将 SPI Host 编号作为总线句柄使用。
     *
     * 参数2：
     * LCD SPI 通信配置。
     *
     * 参数3：
     * 输出创建成功的 Panel IO 句柄。
     */
    ret = esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)BSP_SPI_HOST,
        &io_config,
        &lcd_io_handle
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to create LCD Panel IO: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(
        TAG,
        "LCD Panel IO initialized: DC=%d, mode=%d, clock=%u Hz, queue=%u",
        (int)BSP_LCD_DC_GPIO,
        BSP_LCD_SPI_MODE,
        (unsigned int)BSP_LCD_PIXEL_CLOCK_HZ,
        (unsigned int)BSP_LCD_TRANSACTION_QUEUE
    );

    /*
    * 配置具体的 LCD 控制器。
    */
    const esp_lcd_panel_dev_config_t panel_config = {
        /*
        * LCD RST 接在开发板共享 RESET 网络上，
        * 没有独立连接到 ESP32 GPIO。
        *
        * -1 表示没有独立硬件复位 GPIO。
        * 调用 esp_lcd_panel_reset() 时，
        * ST7789 驱动会发送软件复位命令。
        */
        .reset_gpio_num = -1,

        /*
        * 像素颜色元素顺序。
        */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,

        /*
        * RGB565 是多字节像素格式。
        * BIG 表示高字节先发送。
        */
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,

        /*
        * RGB565 每个像素占 16 bit。
        */
        .bits_per_pixel = 16,

        /*
        * 当前不使用额外厂商扩展配置。
        */
        .vendor_config = NULL,
    };

    //创建lcd_panel_handle句柄
    ret = esp_lcd_new_panel_st7789(
        lcd_io_handle,
        &panel_config,
        &lcd_panel_handle
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to create ST7789 Panel: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    //复位
    ret = esp_lcd_panel_reset(lcd_panel_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to reset ST7789: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
    * LCD_CS 低电平有效。
    *
    * false 表示 PCA9557 IO0 输出低电平，
    * 从现在开始 ST7789 会接收 SPI 命令。
    */
    ret = bsp_pca9557_lcd_cs_set(false);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to select LCD: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    //初始化
    ret = esp_lcd_panel_init(lcd_panel_handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to initialize ST7789: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    //设置颜色反转
    ret = esp_lcd_panel_invert_color(
        lcd_panel_handle,
        true
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to set LCD color inversion: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    //设置xy反转
    ret = esp_lcd_panel_swap_xy(
        lcd_panel_handle,
        true
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to swap LCD axes: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    //设置x镜像
    ret = esp_lcd_panel_mirror(
        lcd_panel_handle,
        true,
        false
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to mirror LCD: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(
        TAG,
        "ST7789 initialized: 320x240, RGB565, SPI mode=%d, clock=%u Hz",
        BSP_LCD_SPI_MODE,
        (unsigned int)BSP_LCD_PIXEL_CLOCK_HZ
    );

    /*
    * 先填充白色，再打开背光。
    *
    * BSP_LCD_COLOR_WHITE = 0xFFFF，
    * RGB565中的红、绿、蓝分量全部为最大值。
    */
    ret = bsp_lcd_fill_color(BSP_LCD_COLOR_BLACK);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fill LCD: %s",
                esp_err_to_name(ret));
        return ret;
    }

    /*
    * 最后才打开背光。
    */
    ret = bsp_lcd_backlight_set(true);

    if (ret != ESP_OK)
    {
        return ret;
    }

    ESP_LOGI(TAG, "LCD initialized successfully");

    return ESP_OK;
}

esp_err_t bsp_lcd_draw_color_bars(void)
{
    if (lcd_panel_handle == NULL)
    {
        ESP_LOGE(TAG, "LCD is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * 320像素分成5个色条：
     *
     * 320 / 5 = 64像素
     */
    static const uint16_t colors[5] = {
        BSP_LCD_COLOR_RED,
        BSP_LCD_COLOR_GREEN,
        BSP_LCD_COLOR_BLUE,
        BSP_LCD_COLOR_WHITE,
        BSP_LCD_COLOR_BLACK
    };

    /*
     * 生成一行五色条像素。
     *
     * x =   0～63  ：红色
     * x =  64～127 ：绿色
     * x = 128～191 ：蓝色
     * x = 192～255 ：白色
     * x = 256～319 ：黑色
     */
    for (int x = 0; x < BSP_LCD_H_RES; x++)
    {
        int color_index = x / 64;
        lcd_line_buffer[x] = colors[color_index];
    }

    /*
     * 五色条每一行都相同，
     * 因此把同一个行缓冲区写入全部240行。
     */
    for (int y = 0; y < BSP_LCD_V_RES; y++)
    {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(
            lcd_panel_handle,
            0,
            y,
            BSP_LCD_H_RES,
            y + 1,
            lcd_line_buffer
        );

        if (ret != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Failed to draw color bar line %d: %s",
                y,
                esp_err_to_name(ret)
            );

            return ret;
        }
    }

    /*
     * 确保最后一行发送完成，并打开显示输出。
     */
    esp_err_t ret = esp_lcd_panel_disp_on_off(
        lcd_panel_handle,
        true
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Failed to enable LCD display: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(TAG, "LCD color bars displayed");

    return ESP_OK;
}

//获取esp_lcd_panel_io_handle_t的句柄
esp_lcd_panel_io_handle_t bsp_lcd_get_io_handle(void)
{
    return lcd_io_handle;
}

//获取esp_lcd_panel_handle_t的句柄
esp_lcd_panel_handle_t bsp_lcd_get_panel_handle(void)
{
    return lcd_panel_handle;
}