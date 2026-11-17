/**
 * @file bsp_can.cpp
 * @brief MSPM0 BspCan — 非阻塞发送 + StreamBuffer 接收 + ISR
 *
 * @note 接收数据流:
 *   轮询: recvPoll() → MCAN FIFO → StreamBuffer → recvRead() 系统阻塞取
 *   中断: ISR → MCAN FIFO → StreamBuffer → recvRead() 系统阻塞取
 *   系统阻塞 = FreeRTOS 任务挂起等 StreamBuffer，不占 CPU。
 */

#include "bsp/bsp_can.hpp"
#include <string.h>

/** @brief 当前实例（中断入口通过 BspCan::instance() 取） */
BspCan *BspCan::_self = nullptr;

/* ================================================================
 *  辅助
 * ================================================================ */

/**
 * @brief 进入 CAN 配置模式 (CCCR.CCE=1, CCCR.INIT=1)
 *
 * 在修改 CAN 配置寄存器前必须调用，等待 INIT 置位后返回。
 */
void BspCan::write_unlock()
{
  _cfg.inst->MCANSS.MCAN.MCAN_CCCR |= (MCAN_CCCR_CCE_MASK | MCAN_CCCR_INIT_MASK);
  while (0U == (_cfg.inst->MCANSS.MCAN.MCAN_CCCR & MCAN_CCCR_INIT_MASK))
  {
  }
}

/**
 * @brief 退出 CAN 配置模式, 进入正常工作模式
 *
 * 清除 CCCR.CCE 和 CCCR.INIT, 等待 INIT 清零后返回。
 */
void BspCan::write_lock()
{
  _cfg.inst->MCANSS.MCAN.MCAN_CCCR &= ~MCAN_CCCR_CCE_MASK;
  _cfg.inst->MCANSS.MCAN.MCAN_CCCR &= ~MCAN_CCCR_INIT_MASK;
  while (0U != (_cfg.inst->MCANSS.MCAN.MCAN_CCCR & MCAN_CCCR_INIT_MASK))
  {
  }
}

/** @brief 打包 13 字节消息 [id:4][dlc:1][data:8] 写入 StreamBuffer */
void BspCan::_push_rx(uint32_t id, uint8_t dlc, const uint8_t *data)
{
  uint8_t msg[CAN_MSG_SIZE];

  msg[0] = (uint8_t)(id & 0xFFU);
  msg[1] = (uint8_t)((id >> 8U) & 0xFFU);
  msg[2] = (uint8_t)((id >> 16U) & 0xFFU);
  msg[3] = (uint8_t)((id >> 24U) & 0xFFU);
  msg[4] = dlc;

  if (dlc > 0U && data)
  {
    memcpy(&msg[5], data, dlc);
  }
  if (dlc < 8U)
  {
    memset(&msg[5 + dlc], 0, (size_t)(8U - dlc));
  }

  /* 非阻塞写入 StreamBuffer，满则丢弃 */
  xStreamBufferSend(_rx_stream, msg, CAN_MSG_SIZE, 0U);
}

/* ================================================================
 *  API
 * ================================================================ */

void BspCan::init(const Config &cfg, Mode mode)
{
  _cfg = cfg;
  _self = this;

  /* 创建 RX StreamBuffer */
  _rx_stream = xStreamBufferCreate((size_t)CAN_RX_BUF_MSGS * CAN_MSG_SIZE, 1U);

  /* 全局滤波器：非匹配帧进 FIFO0（全收） */
  DL_MCAN_ConfigParams mcfg;
  memset(&mcfg, 0, sizeof(mcfg));
  mcfg.filterConfig.anfs = 0U;
  mcfg.filterConfig.anfe = 0U;
  DL_MCAN_config(_cfg.inst, &mcfg);

  set_loopback(mode == LOOPBACK);
}

bool BspCan::send(uint32_t id, const uint8_t *data, uint8_t len)
{
  if (DL_MCAN_getTxBufReqPend(_cfg.inst) & 0x01U)
  {
    return false;
  }

  DL_MCAN_TxBufElement tx;
  memset(&tx, 0, sizeof(tx));
  tx.id  = (id & 0x7FFU) << 18U;
  tx.xtd = 0U;
  tx.rtr = 0U;
  tx.fdf = 0U;
  tx.efc = 0U;
  tx.dlc = (len > 8U) ? 8U : len;
  if (len > 0U && data)
  {
    memcpy(tx.data, data, tx.dlc);
  }

  DL_MCAN_writeMsgRam(_cfg.inst, DL_MCAN_MEM_TYPE_BUF, 0U, &tx);
  DL_MCAN_TXBufAddReq(_cfg.inst, 0U);
  return true;
}

void BspCan::recv_poll()
{
  DL_MCAN_RxFIFOStatus st;
  st.num = DL_MCAN_RX_FIFO_NUM_0;
  DL_MCAN_getRxFIFOStatus(_cfg.inst, &st);

  while (st.fillLvl > 0U)
  {
    DL_MCAN_RxBufElement rx;
    DL_MCAN_readMsgRam(_cfg.inst, DL_MCAN_MEM_TYPE_FIFO, 0U, DL_MCAN_RX_FIFO_NUM_0, &rx);
    DL_MCAN_writeRxFIFOAck(_cfg.inst, DL_MCAN_RX_FIFO_NUM_0, st.getIdx);

    uint32_t id  = (rx.id >> 18U) & 0x7FFU;
    uint8_t  dlc = (rx.dlc > 8U) ? 8U : (uint8_t)rx.dlc;
    _push_rx(id, dlc, rx.data);

    st.num = DL_MCAN_RX_FIFO_NUM_0;
    DL_MCAN_getRxFIFOStatus(_cfg.inst, &st);
  }
}

bool BspCan::recv_read(uint32_t *id, uint8_t *data, uint8_t *len, uint32_t timeout)
{
  uint8_t msg[CAN_MSG_SIZE];
  size_t  n = xStreamBufferReceive(_rx_stream, msg, CAN_MSG_SIZE, timeout);
  if (n != CAN_MSG_SIZE)
  {
    return false;
  }

  *id  = (uint32_t)msg[0]
         | ((uint32_t)msg[1] << 8U)
         | ((uint32_t)msg[2] << 16U)
         | ((uint32_t)msg[3] << 24U);
  *len = msg[4];

  if (*len > 0U && data)
  {
    memcpy(data, &msg[5], *len);
  }
  return true;
}

void BspCan::get_status(Status *st)
{
  DL_MCAN_ErrCntStatus ec;
  DL_MCAN_getErrCounters(_cfg.inst, &ec);

  DL_MCAN_ProtocolStatus ps;
  DL_MCAN_getProtocolStatus(_cfg.inst, &ps);

  st->tx_err_cnt    = ec.transErrLogCnt;
  st->rx_err_cnt    = ec.recErrCnt;
  st->activity      = ps.act;
  st->last_err_code = ps.lastErrCode;
  st->bus_off       = (ps.busOffStatus != 0U);
}

void BspCan::set_loopback(bool enable)
{
  write_unlock();
  DL_MCAN_lpbkModeEnable(_cfg.inst, DL_MCAN_LPBK_MODE_INTERNAL, enable);
  _loopback = enable;
  write_lock();
}

void BspCan::enable_irq()
{
  DL_MCAN_enableIntr(_cfg.inst, DL_MCAN_INTR_MASK_ALL, 0U);
  DL_MCAN_enableIntr(_cfg.inst,
                     DL_MCAN_INTERRUPT_RF0N | DL_MCAN_INTERRUPT_RF0L,
                     1U);
  DL_MCAN_clearInterruptStatus(_cfg.inst, DL_MCAN_MSP_INTERRUPT_LINE0);
  __DSB();
  NVIC_EnableIRQ(_cfg.irq);
}

void BspCan::disable_irq()
{
  NVIC_DisableIRQ(_cfg.irq);
}

size_t BspCan::rx_available() const
{
  return xStreamBufferBytesAvailable(_rx_stream);
}

/* ================================================================
 *  ISR: MCAN FIFO → StreamBuffer
 * ================================================================ */

void BspCan::isr_handler()
{
  uint32_t status = DL_MCAN_getIntrStatus(_cfg.inst);
  DL_MCAN_clearIntrStatus(_cfg.inst, status, DL_MCAN_INTR_SRC_MCAN_LINE_0);

  if (status & MCAN_IR_RF0N_MASK)
  {
    DL_MCAN_RxFIFOStatus st;
    st.num = DL_MCAN_RX_FIFO_NUM_0;
    DL_MCAN_getRxFIFOStatus(_cfg.inst, &st);

    while (st.fillLvl > 0U)
    {
      DL_MCAN_RxBufElement rx;
      DL_MCAN_readMsgRam(_cfg.inst, DL_MCAN_MEM_TYPE_FIFO, 0U, DL_MCAN_RX_FIFO_NUM_0, &rx);
      DL_MCAN_writeRxFIFOAck(_cfg.inst, DL_MCAN_RX_FIFO_NUM_0, st.getIdx);

      uint32_t id  = (rx.id >> 18U) & 0x7FFU;
      uint8_t  dlc = (rx.dlc > 8U) ? 8U : (uint8_t)rx.dlc;

      /* 打包后写入 StreamBuffer */
      uint8_t msg[CAN_MSG_SIZE];
      msg[0] = (uint8_t)(id & 0xFFU);
      msg[1] = (uint8_t)((id >> 8U) & 0xFFU);
      msg[2] = (uint8_t)((id >> 16U) & 0xFFU);
      msg[3] = (uint8_t)((id >> 24U) & 0xFFU);
      msg[4] = dlc;
      memcpy(&msg[5], rx.data, dlc);
      if (dlc < 8U)
      {
        memset(&msg[5 + dlc], 0, (size_t)(8U - dlc));
      }

      BaseType_t woken = pdFALSE;
      xStreamBufferSendFromISR(_rx_stream, msg, CAN_MSG_SIZE, &woken);
      portYIELD_FROM_ISR(woken);

      st.num = DL_MCAN_RX_FIFO_NUM_0;
      DL_MCAN_getRxFIFOStatus(_cfg.inst, &st);
    }
  }
}

/* ================================================================
 *  中断入口（按宿主的 board_instances.hpp 清单展开）
 * ================================================================ */

#include "bsp/bsp_instances.hpp"

M0BSP_CAN_INSTANCES(M0BSP_CAN_ISR)
