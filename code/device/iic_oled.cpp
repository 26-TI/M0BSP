/**
 * @file iic_oled.cpp
 * @brief I2C OLED 设备驱动 — 实现
 *
 * @note 所有逻辑保持与原 OLED.c 一致，仅重构为 C++ 类形式。
 */

#include "device/iic_oled.hpp"
#include "device/fort.h"

/* ==================== 构造 ==================== */
IicOled::IicOled() {}

namespace
{
  /** @brief 字节内 bit 顺序反转（用于字形上下翻转） */
  inline uint8_t reverse_bits8(uint8_t b)
  {
    b = (uint8_t)((b & 0xF0U) >> 4 | (b & 0x0FU) << 4);
    b = (uint8_t)((b & 0xCCU) >> 2 | (b & 0x33U) << 2);
    b = (uint8_t)((b & 0xAAU) >> 1 | (b & 0x55U) << 1);
    return b;
  }
} // namespace

/* ==================== 初始化 ==================== */
bool IicOled::init(BspI2c *i2c, uint8_t dev_addr, Rotation rot)
{
  if (!i2c)
    return false;

  _i2c      = i2c;
  _dev_addr = dev_addr;
  _io_ok    = true;

  /* 先把控制器/FIFO 恢复到干净状态：烧录器的 reset 可能打断上一帧传输，
     导致命令流与从机状态错位（花屏）。 */
  _i2c->recover();

  /* 段重映射（A0=正常 / A1=左右镜像）与 COM 扫描方向（C0=正常 / C8=上下镜像） */
  uint8_t seg_remap = 0xA0;
  uint8_t com_scan  = 0xC0;
  switch (rot)
  {
  case Rotation::ROT_180:
    seg_remap = 0xA1;
    com_scan  = 0xC8;
    break;
  case Rotation::MIRROR_X:
    seg_remap = 0xA1;
    com_scan  = 0xC0;
    break;
  case Rotation::MIRROR_Y:
    seg_remap = 0xA0;
    com_scan  = 0xC8;
    break;
  case Rotation::ROT_0:
  default:
    seg_remap = 0xA0;
    com_scan  = 0xC0;
    break;
  }

  /* SSD1306 初始化序列（与原 OLED_Init 一致） */
  write_cmd(0xAE); // 关闭显示
  write_cmd(0x00); // 列低地址
  write_cmd(0x10); // 列高地址
  write_cmd(0x40); // 起始行
  write_cmd(0x81); // 对比度设置
  write_cmd(0xCF); // 对比度值
  write_cmd(seg_remap); // 段重映射（方向）
  write_cmd(com_scan);  // COM 扫描方向（方向）
  write_cmd(0xA6); // 正常显示（非反色）
  write_cmd(0xA8); // 多路复用比
  write_cmd(0x3F); // 1/64 占空比
  write_cmd(0xD3); // 显示偏移
  write_cmd(0x00); // 偏移 0
  write_cmd(0xD5); // 显示时钟分频/振荡频率
  write_cmd(0x80); // 默认值
  write_cmd(0xD9); // 预充电周期
  write_cmd(0xF1); // 值
  write_cmd(0xDA); // COM 引脚配置
  write_cmd(0x12); // 交替模式
  write_cmd(0xDB); // VCOMH 电平
  write_cmd(0x30); // 值
  write_cmd(0x20); // 寻址模式
  write_cmd(0x02); // 页寻址模式
  write_cmd(0x8D); // 电荷泵
  write_cmd(0x14); // 使能
  clear();
  write_cmd(0xAF); // 开启显示

  return _io_ok;
}

/* ==================== 命令发送 ==================== */
void IicOled::write_cmd(uint8_t cmd)
{
  uint8_t data[2] = {0x00, cmd};
  /* 记录写失败：初始化结束时据此返回 false，宿主可重试 */
  if (!_i2c->write(_dev_addr, data, 2))
  {
    _io_ok = false;
  }
}

/* ==================== 显示控制 ==================== */

void IicOled::display_on()
{
  write_cmd(0x8D); // 电荷泵使能
  write_cmd(0x14); // 开启电荷泵
  write_cmd(0xAF); // 点亮屏幕
}

void IicOled::display_off()
{
  write_cmd(0x8D); // 电荷泵使能
  write_cmd(0x10); // 关闭电荷泵
  write_cmd(0xAE); // 关闭屏幕
}

void IicOled::color_turn(bool enable)
{
  if (enable)
    write_cmd(0xA7); // 反色显示
  else
    write_cmd(0xA6); // 正常显示
}

void IicOled::display_turn(bool enable)
{
  if (enable)
  {
    write_cmd(0xC0); // COM 反转
    write_cmd(0xA0); // 段反转
  }
  else
  {
    write_cmd(0xC8); // COM 正常
    write_cmd(0xA1); // 段正常
  }
}

/* ==================== 显存操作 ==================== */

void IicOled::clear()
{
  for (uint8_t i = 0; i < 8; i++)
  {
    for (uint8_t n = 0; n < 128; n++)
    {
      _gram[n][i] = 0x00;
    }
  }
  refresh();
}

void IicOled::refresh()
{
  uint8_t buf[129];
  buf[0] = 0x40; /* 控制字节: 数据模式 */

  _io_ok = true;

  for (uint8_t page = 0; page < 8; page++)
  {
    /* 设置页地址和列起始地址 */
    write_cmd(0xB0 + page);
    write_cmd(0x00);
    write_cmd(0x10);

    /* 拷贝当前 128 列像素数据 */
    for (uint8_t col = 0; col < 128; col++)
      buf[col + 1] = _gram[col][page];

    /* 一次写入 129 字节 (1 ctrl + 128 pixel) */
    if (!_i2c->write(_dev_addr, buf, 129))
    {
      _io_ok = false;
    }
  }
}

void IicOled::show_char(uint8_t x, uint8_t y, uint8_t chr)
{
  /* 8x16 字模：每个字符占 8 列 × 2 page。
   * 字模位序与面板扫描约定不一致时，用 _font_flip 在**字形层面**修正
   * （只影响单字，不影响字符位置与排版）。 */
  const uint8_t *glyph = font_8x16[chr - ' '];
  const bool flip_h = (_font_flip == FontFlip::H || _font_flip == FontFlip::HV);
  const bool flip_v = (_font_flip == FontFlip::V || _font_flip == FontFlip::HV);

  for (uint8_t i = 0; i < 8; i++)
  {
    const uint8_t col = flip_h ? (uint8_t)(7U - i) : i;

    uint8_t lo = glyph[i];      /* 上半页：第 0~7 行 */
    uint8_t hi = glyph[i + 8];  /* 下半页：第 8~15 行 */

    if (flip_v)
    {
      /* 上下翻转 = 两个半页交换，且每个字节内的 bit 顺序也要反 */
      const uint8_t nlo = reverse_bits8(glyph[i + 8]);
      const uint8_t nhi = reverse_bits8(glyph[i]);
      lo                = nlo;
      hi                = nhi;
    }

    _gram[x * 8 + col][y * 2]     = lo;
    _gram[x * 8 + col][y * 2 + 1] = hi;
  }
}

void IicOled::show_string(uint8_t x, uint8_t y, const char *str)
{
  uint8_t j = 0;
  while (str[j] != '\0')
  {
    show_char(x, y, str[j]);
    x++;
    if (x > 15)
      return;
    j++;
  }
}
