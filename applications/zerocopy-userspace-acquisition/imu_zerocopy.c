#include <asm/barrier.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/spi/spi.h>

#define RING_SLOTS 62
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_CONFIG2 0x1D
#define REG_INT_ENABLE 0x38
#define REG_INT_STATUS 0x3A
#define REG_ACCEL_XOUT_H 0x3B
#define REG_SIGNAL_PATH_RESET 0x68
#define REG_USER_CTRL 0x6A
#define REG_PWR_MGMT_1 0x6B
#define REG_PWR_MGMT_2 0x6C
#define REG_WHOAMI 0x75

struct __attribute__((aligned(64))) imu_frame {
  u64 hw_timestamp_ns;
  u32 seq_num;
  s16 accel[3];
  s16 gyro[3];
  u8 reserved[40];
};

struct __attribute__((aligned(64))) imu_ring_page {
  /* Cache Line 0: Producer (Kernel) */
  u64 head;
  u64 dropped_frames;
  u8 pad0[48];

  /* Cache Line 1: Consumer (Userspace) */
  u64 tail;
  u8 pad1[56];

  /* Cache Lines 2..63: 62 Data Slots */
  struct imu_frame slots[RING_SLOTS];
};

struct imu_zc_dev {
  struct spi_device *spi;
  struct miscdevice miscdev;
  struct page *ring_page;
  struct imu_ring_page *ring;
  wait_queue_head_t wq;
  u64 irq_ts_ns;
  u32 seq_counter;
  u8 tx_buf[15] ____cacheline_aligned;
  u8 rx_buf[15] ____cacheline_aligned;
};

static struct imu_zc_dev *g_dev;

/* 1. Hard IRQ Top-Half (< 2 us): Capture deterministic timestamp */
static irqreturn_t imu_hard_irq(int irq, void *dev_id) {
  struct imu_zc_dev *dev = dev_id;
  dev->irq_ts_ns = ktime_get_ns();
  return IRQ_WAKE_THREAD;
}

/* 2. Threaded Bottom-Half (SCHED_FIFO): SPI burst + SPSC release */
static irqreturn_t imu_thread_fn(int irq, void *dev_id) {
  struct imu_zc_dev *dev = dev_id;
  struct imu_ring_page *r = dev->ring;
  u64 head = r->head;
  u64 tail = smp_load_acquire(&r->tail); /* ARM64 LDAR */
  struct spi_transfer xfer = {
      .tx_buf = dev->tx_buf,
      .rx_buf = dev->rx_buf,
      .len = 15,
      .speed_hz = 4000000,
  };

  dev->tx_buf[0] = REG_ACCEL_XOUT_H | 0x80;
  if (spi_sync_transfer(dev->spi, &xfer, 1) < 0)
    return IRQ_HANDLED;

  dev->seq_counter++;

  /* Drop incoming frame if ring is full (preserves single-writer tail invariant) */
  if ((head - tail) >= RING_SLOTS) {
    r->dropped_frames++;
    return IRQ_HANDLED;
  }

  struct imu_frame *slot = &r->slots[head % RING_SLOTS];
  slot->hw_timestamp_ns = dev->irq_ts_ns;
  slot->seq_num = dev->seq_counter;
  slot->accel[0] = (s16)((dev->rx_buf[1] << 8) | dev->rx_buf[2]);
  slot->accel[1] = (s16)((dev->rx_buf[3] << 8) | dev->rx_buf[4]);
  slot->accel[2] = (s16)((dev->rx_buf[5] << 8) | dev->rx_buf[6]);
  slot->gyro[0] = (s16)((dev->rx_buf[9] << 8) | dev->rx_buf[10]);
  slot->gyro[1] = (s16)((dev->rx_buf[11] << 8) | dev->rx_buf[12]);
  slot->gyro[2] = (s16)((dev->rx_buf[13] << 8) | dev->rx_buf[14]);

  /* ARM64 STLR: Ensures payload is visible before head advances */
  smp_store_release(&r->head, head + 1);
  wake_up_interruptible(&dev->wq);

  return IRQ_HANDLED;
}

/* 3. Zero-Copy Page Mapping */
static int imu_mmap(struct file *filp, struct vm_area_struct *vma) {
  unsigned long size = vma->vm_end - vma->vm_start;
  if (size != PAGE_SIZE)
    return -EINVAL;

  vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
  return remap_pfn_range(vma, vma->vm_start, page_to_pfn(g_dev->ring_page),
                         PAGE_SIZE, vma->vm_page_prot);
}

static __poll_t imu_poll(struct file *filp, poll_table *wait) {
  struct imu_ring_page *r = g_dev->ring;
  poll_wait(filp, &g_dev->wq, wait);
  if (READ_ONCE(r->head) != READ_ONCE(r->tail))
    return EPOLLIN | EPOLLRDNORM;
  return 0;
}

static const struct file_operations imu_fops = {
    .owner = THIS_MODULE,
    .mmap = imu_mmap,
    .poll = imu_poll,
};

static int imu_write_reg(struct spi_device *spi, u8 reg, u8 val) {
  u8 buf[2] = { (u8)(reg & 0x7F), val };
  return spi_write(spi, buf, 2);
}

static int imu_read_reg(struct spi_device *spi, u8 reg, u8 *val) {
  u8 tx[2] = { (u8)(reg | 0x80), 0x00 };
  u8 rx[2] = { 0 };
  struct spi_transfer xfer = {
      .tx_buf = tx,
      .rx_buf = rx,
      .len = 2,
  };
  int ret = spi_sync_transfer(spi, &xfer, 1);
  if (ret >= 0)
    *val = rx[1];
  return ret;
}

static int imu_probe(struct spi_device *spi) {
  int ret;
  u8 whoami = 0;
  u8 dummy = 0;

  BUILD_BUG_ON(sizeof(struct imu_ring_page) != PAGE_SIZE);

  g_dev = devm_kzalloc(&spi->dev, sizeof(*g_dev), GFP_KERNEL);
  if (!g_dev)
    return -ENOMEM;

  g_dev->spi = spi;
  init_waitqueue_head(&g_dev->wq);

  g_dev->ring_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, 0);
  if (!g_dev->ring_page)
    return -ENOMEM;
  g_dev->ring = page_address(g_dev->ring_page);

  /* 1. Reset device */
  imu_write_reg(spi, REG_PWR_MGMT_1, 0x80);
  msleep(100);

  /* 2. Reset signal paths for clean SPI communication */
  imu_write_reg(spi, REG_SIGNAL_PATH_RESET, 0x07);
  msleep(20);

  /* 3. Wake device and select PLL with X-axis gyro reference */
  imu_write_reg(spi, REG_PWR_MGMT_1, 0x01);
  usleep_range(10000, 15000); /* Wait 10 ms for PLL to stabilize */

  /* 4. Verify WHOAMI */
  ret = imu_read_reg(spi, REG_WHOAMI, &whoami);
  if (ret < 0) {
    dev_err(&spi->dev, "Failed to read WHOAMI: %d\n", ret);
    goto err_free_page;
  }
  dev_info(&spi->dev, "Discovered MPU sensor with WHOAMI=0x%02x\n", whoami);

  /* 5. Enable all accelerometer and gyroscope axes */
  imu_write_reg(spi, REG_PWR_MGMT_2, 0x00);

  /* 6. Configure Digital Low Pass Filter (DLPF: 184 Hz gyro bandwidth) */
  imu_write_reg(spi, REG_CONFIG, 0x01);

  /* 7. Configure Accelerometer DLPF (184 Hz bandwidth) */
  imu_write_reg(spi, REG_ACCEL_CONFIG2, 0x01);

  /* 8. Configure Full Scale Ranges: Accel ±2g, Gyro ±250 dps */
  imu_write_reg(spi, REG_ACCEL_CONFIG, 0x00);
  imu_write_reg(spi, REG_GYRO_CONFIG, 0x00);

  /* 9. Sample Rate Divider: 1000 Hz / (1 + 0) = 1000 Hz */
  imu_write_reg(spi, REG_SMPLRT_DIV, 0x00);

  /* 10. Clear any latched power-on interrupts */
  imu_read_reg(spi, REG_INT_STATUS, &dummy);

  /* 11. Register character device */
  g_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
  g_dev->miscdev.name = "imu_zerocopy";
  g_dev->miscdev.fops = &imu_fops;
  ret = misc_register(&g_dev->miscdev);
  if (ret)
    goto err_free_page;

  /* 12. Request threaded IRQ (GPIO 17 DRDY) */
  ret = devm_request_threaded_irq(
      &spi->dev, spi->irq, imu_hard_irq, imu_thread_fn,
      IRQF_TRIGGER_RISING | IRQF_ONESHOT, "imu_zerocopy", g_dev);
  if (ret)
    goto err_misc;

  /* 13. Enable Data-Ready interrupt on sensor */
  imu_write_reg(spi, REG_INT_ENABLE, 0x01);

  dev_info(&spi->dev, "IMU zero-copy driver initialized successfully\n");
  return 0;

err_misc:
  misc_deregister(&g_dev->miscdev);
err_free_page:
  __free_pages(g_dev->ring_page, 0);
  return ret;
}

static void imu_remove(struct spi_device *spi) {
  /* Disable sensor interrupt and put sensor to sleep */
  imu_write_reg(spi, REG_INT_ENABLE, 0x00);
  imu_write_reg(spi, REG_PWR_MGMT_1, 0x40);

  misc_deregister(&g_dev->miscdev);
  __free_pages(g_dev->ring_page, 0);
}

static const struct of_device_id imu_of_match[] = {
    {.compatible = "custom,mpu6500-zerocopy"}, {}};
MODULE_DEVICE_TABLE(of, imu_of_match);

static struct spi_driver imu_zc_driver = {
    .driver =
        {
            .name = "imu_zerocopy",
            .of_match_table = imu_of_match,
        },
    .probe = imu_probe,
    .remove = imu_remove,
};
module_spi_driver(imu_zc_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Lab");
MODULE_DESCRIPTION("High-Performance Zero-Copy MPU6500 SPI Driver");
