#include "nemu.h"
#include "monitor/monitor.h"

typedef uint32_t (*read_func) (paddr_t addr, int len);
typedef void (*write_func)(paddr_t addr, int len, uint32_t data);

static uint32_t ddr_read(paddr_t addr, int len);
static void ddr_write(paddr_t addr, int len, uint32_t data);
static uint32_t uartlite_read(paddr_t addr, int len);
static void uartlite_write(paddr_t addr, int len, uint32_t data);
static void gpio_write(paddr_t addr, int len, uint32_t data);
static uint32_t invalid_read(paddr_t addr, int len);
static void invalid_write(paddr_t addr, int len, uint32_t data);

// the memory mapping of mips32-npc
//0x00000000 - 0x00001fff: bram
//0x10000000 - 0x1fffffff: ddr
//0x40000000 - 0x40000fff: gpio-trap
//0x40001000 - 0x40001fff: uartlite
//0x40010000 - 0x4001ffff: vga
struct mmap_region {
  uint32_t start, end;
  read_func read;
  write_func write;
} mmap_table [] = {
  {0x00000000, 0x00001fff, invalid_read, invalid_write},
  {0x10000000, 0x1fffffff, ddr_read, ddr_write},
  {0x40000000, 0x40000fff, invalid_read, gpio_write},
  {0x40001000, 0x40001fff, uartlite_read, uartlite_write},
  {0x40010000, 0x4001ffff, invalid_read, invalid_write}
};

#define NR_REGION (sizeof(mmap_table) / sizeof(mmap_table[0]))

uint32_t find_region(vaddr_t addr) {
  int ret = -1;
  for(int i = 0; i < NR_REGION; i++)
    if(addr >= mmap_table[i].start && addr <= mmap_table[i].end) {
      ret = i;
      break;
    }
  Assert(ret != -1, "address(0x%08x) is out of bound", addr);
  return ret;
}

uint32_t vaddr_read(vaddr_t addr, int len) {
  int idx = find_region(addr);
  return mmap_table[idx].read(addr - mmap_table[idx].start, len);
}

void vaddr_write(vaddr_t addr, int len, uint32_t data) {
  int idx = find_region(addr);
  return mmap_table[idx].write(addr - mmap_table[idx].start, len, data);
}

#define DDR_SIZE (128 * 1024 * 1024)

uint8_t ddr[DDR_SIZE];

/* Memory accessing interfaces */

#define check_ddr(addr, len) \
  Assert(addr >= 0 && addr < DDR_SIZE && addr + len <= DDR_SIZE, \
      "address(0x%08x) is out side DDR", addr);

static uint32_t ddr_read(paddr_t addr, int len) {
  check_ddr(addr, len);
  return *((uint32_t *)((uint8_t *)ddr + addr)) & (~0u >> ((4 - len) << 3));
}

static void ddr_write(paddr_t addr, int len, uint32_t data) {
  check_ddr(addr, len);
  memcpy((uint8_t *)ddr + addr, &data, len);
}

/* serial port */
#define SERIAL_PORT ((volatile char *)0x40001000)
#define Rx 0x0
#define Tx 0x04
#define STAT 0x08
#define CTRL 0x0c

#define check_uartlite(addr, len) \
  Assert(addr >= 0 && addr <= STAT, \
      "address(0x%08x) is out side UARTLite", addr); \
  Assert(len == 1, \
      "UARTLite only allow byte read/write");

static uint32_t uartlite_read(paddr_t addr, int len) {
  /* CTRL not yet implemented, only allow byte read/write */
  check_uartlite(addr, len);
  switch (addr) {
    // only allow stat read
    // Rx not supported
    case STAT:
      // ready for Tx
      // no valid Rx
      return 0;
      break;
    default:
      Assert(false, "UARTLite: address(0x%08x) is not readable", addr);
      break;
  }
}

static void uartlite_write(paddr_t addr, int len, uint32_t data) {
  check_uartlite(addr, len);
  switch (addr) {
    case Tx:
      printf("%c", (char)data);
      break;
    default:
      Assert(false, "UARTLite: address(0x%08x) is not writable", addr);
      break;
  }
}

/* gpio trap */

#define check_gpio(addr, len) \
  Assert(addr == 0, \
      "address(0x%08x) is out side GPIO", addr); \
  Assert(len == 1, \
      "GPIO only allow byte read/write");

static void gpio_write(paddr_t addr, int len, uint32_t data) {
  check_gpio(addr, len);
  if ((unsigned char)data == 0) {
    Log("HIT GOOD TRAP");
  }
  else
    Log("HIT BAD TRAP");
  nemu_state = NEMU_END;
}

static uint32_t invalid_read(paddr_t addr, int len) {
  Assert(false, "invalid read at address(0x%08x)", addr);
}

static void invalid_write(paddr_t addr, int len, uint32_t data) {
  Assert(false, "invalid write at address(0x%08x)", addr);
}
