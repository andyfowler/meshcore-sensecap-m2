/* m2modem_rak2287: Raspberry Pi 4 + RAK2287 (SX1302 + 2x SX1250) as an openHop TCP modem.
 *
 * Port of m2modem 0.1 by Andy Fowler (github.com/andyfowler/meshcore-sensecap-m2,
 * feed/m2modem/src/m2modem.c, MIT license, see LICENSE-m2modem) from the SenseCAP M2 to the
 * MNTD / RAK hotspot hardware.
 *
 * Drives SX1250 radio A directly as an SX126x LoRa transceiver (the SX1302 is only an SPI
 * pass-through), keeps SX1250 radio B in I/Q mode as the SX1302's clock so the front end can be
 * switched, and serves the openHop modem wire protocol on TCP (localhost only).
 *
 *   m2modem_rak2287 [-d spidev] [-R reset_script] [-P max_dbm] [-p port] [-n] [-v]
 *
 *   -d  SPI device of the SX1302 (default /dev/spidev0.0, as RAK's gateway images use)
 *   -R  concentrator reset script; run with no argument at start (reset and release) and with
 *       --hold at stop (default /usr/local/lib/m2modem/reset_concentrator.sh)
 *   -P  transmit power cap in dBm, 12..27 (default 12). Requests map to the calibration table (CAL)
 *       entries 12, 15, 18, 21, 24, and 27 dBm, rounded down (see CAL)
 *   -p  TCP port (default 5055), bound to 127.0.0.1 only
 *   -n  receive only: every transmit request is refused, the front end is never switched to TX
 *   -v  verbose
 *
 * A test binary built with -DM2_TEST_DRIVE (build_m2modem.sh, OUT and EXTRA_CFLAGS) adds -T (the
 * config's power field is the SX1250 drive in dBm, at most 22, bypassing the calibration table and
 * the -P cap) and -A 0|1 (PA gain register, default 1). It exists only for the power calibration
 * and is never installed.
 *
 * Changes from m2modem 0.1:
 *   - SPI device is an option, default /dev/spidev0.0 (the M2 uses /dev/spidev0.1).
 *   - Reset uses the reset_concentrator.sh next to this file (GPIO 17 and 25, reset active high).
 *   - Transmit table is RAK's RAK2287 US915 table (rak_common_for_gateway,
 *     lora/rak2287/global_conf_{i2c,uart,usb}/global_conf.us_902_928.json), not Seeed's M2 table.
 *   - Power capped at 12 dBm unless -P raises it (to 27 at most in 0.4). Receive-only switch (-n).
 *   - Fails loud: exits on a wrong chip version, any failed HAL call, a failed SPI open, a
 *     front-end readback mismatch, the radio not reaching STDBY_XOSC, or an image calibration
 *     error, instead of warning and carrying on.
 *   - TCP listener bound to 127.0.0.1 (0.1 bound all interfaces).
 *   - Only radio A (the RAK2287's transmit chain) is driven; the -r option is gone.
 *   - After each transmission (done or timed out) the radio goes to standby before the front
 *     end leaves transmit, and PA_EN is read back low; a stuck PA_EN is fatal.
 *   - SIGHUP and SIGQUIT clean up like SIGTERM; signals interrupt blocked calls; client
 *     writes time out after 1 s.
 *   - Outgoing frames are size-checked (0.1 could overflow its frame buffer echoing a long
 *     SET_CAD_PARAMS payload). CONFIG_RESP reports the power actually used.
 *
 * Changes in 0.2:
 *   - Noise floor: mean of the last 20 idle RSSI samples (one per 0.5 s, none within 0.5 s of packet
 *     activity); 0.1's estimator only accepted samples below -105 dBm, so it never left -120.
 *   - radio_state in the status reply comes from the chip's own mode (0 idle/rx, 1 tx, 2 error);
 *     0.1 always sent 0. GetRssiInst is read in 0.5 dB steps (0.1 truncated to whole dB).
 *
 * Changes in 0.3:
 *   - A receive IRQ that holds only PreambleDetected / SyncWordValid / HeaderValid and is never
 *     followed by RxDone or an error is cleared once it has stood longer than a 255-byte packet's
 *     airtime plus margin (at least 1 s); 0.2 left it set, which kept "activity" alive forever,
 *     stalled noise sampling, and hid a radio that had left RX. While idle in RX, the chip mode is
 *     read every 1.5 s; if it is not RX the event is logged and counted, the radio config is
 *     re-applied, and RX is re-entered (three in a row are fatal).
 *   - SPI/HAL failures are counted; 5 consecutive failures take the fatal path (concentrator held in
 *     reset), so a service manager restarts the daemon. An all-ones read counts as a failure.
 *   - Default preamble follows MeshCore firmware (RadioLibWrappers.h preambleLengthForSF): 32
 *     symbols at SF8 and below, 16 above, whenever the client sends no preamble (a 13-byte config,
 *     or a preamble field of 0), including after a client changes SF.
 *   - Transmit power: a calibration table (measured against the HAL's RAK entries on this hardware)
 *     replaces the unchecked "drive dBm = power index - 4" formula. See CAL below.
 *
 * Changes in 0.4:
 *   - Calibration table extended with the 21, 24, and 27 dBm entries; -P accepts 12 to 27. See CAL.
 *
 * Unverified assumptions carried over from m2modem 0.1 (see docs/MESHCORE_PORT_PLAN.md):
 *   - The SX1250 executes SX126x LoRa modem commands once register 0x0587 is cleared.
 *     Semtech's data sheet does not document this; Semtech's HAL defines the commands.
 *
 * Frame: 0xAA | CMD | LEN(2 LE) | PAYLOAD | CRC16-CCITT-FALSE(2 LE) over CMD+LEN+PAYLOAD.
 *
 * Changes in 0.5 to 0.12:
 *   - Logging: RX and CRC-error lines carry signal RSSI, frequency error, and "det" (ms since the first
 *     detection flag of that reception); a latched detection that never becomes a packet is logged as
 *     "detection with no packet"; an "rx health" line every 5 minutes (idle level, gain registers).
 *   - Gain-control lockup: bit 0 of register 0x08B5 is set at every radio config (rx_knobs agcfix=0
 *     turns it off). A gain watch re-enters receive if the idle level sits 12 dB above the learned
 *     floor for 3 s, and logs registers 0x08BF and 0x08C0.
 *   - A CAD request that arrives while a packet is being received is answered "busy" without leaving
 *     receive (rx_knobs inflight=0 restores the old behavior, which aborted the reception).
 *   - Runtime knob files, read while running: /etc/m2modem/rx_knobs (agcfix, watch, inflight) and
 *     /etc/m2modem/tx_knobs (mq, cap). No file means defaults.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <linux/spi/spidev.h>
#include "loragw_hal.h"
#include "loragw_reg.h"
#include "loragw_com.h"
#include "loragw_sx1302.h"
#include "loragw_sx1250.h"

#ifdef M2_TEST_DRIVE
#define VERSION "m2modem_rak2287 0.12 TEST-DRIVE (from m2modem 0.1)"
#else
#define VERSION "m2modem_rak2287 0.12 (from m2modem 0.1)"
#endif

/* protocol */
enum { CMD_TX_REQUEST = 0x01, CMD_TX_DONE = 0x02, CMD_TX_FAIL = 0x03, CMD_RX_PACKET = 0x04,
       CMD_SET_CONFIG = 0x10, CMD_GET_CONFIG = 0x11, CMD_CONFIG_RESP = 0x12,
       CMD_STATUS_REQ = 0x20, CMD_STATUS_RESP = 0x21, CMD_NOISE_REQ = 0x22, CMD_NOISE_RESP = 0x23,
       CMD_CAD_REQUEST = 0x30, CMD_RX_START = 0x31, CMD_CAD_RESP = 0x32, CMD_RX_STARTED = 0x33,
       CMD_SET_CAD_PARAMS = 0x34, CMD_CAD_PARAMS_RESP = 0x35, CMD_AUTH = 0x50, CMD_AUTH_OK = 0x51,
       CMD_GET_VERSION = 0x70, CMD_VERSION_RESP = 0x71, CMD_ERROR = 0xFE, CMD_PING = 0xFF };
enum { ERR_CRC_MISMATCH = 0x01, ERR_INVALID_CMD = 0x02, ERR_RADIO_BUSY = 0x03, ERR_TX_TIMEOUT = 0x04,
       ERR_PAYLOAD_TOO_BIG = 0x05, ERR_INVALID_CONFIG = 0x06, ERR_CAD_FAILED = 0x07 };

static int verbose = 0;
static int rx_only = 0;
static const char *spi_path = "/dev/spidev0.0";
static const char *reset_script = "/usr/local/lib/m2modem/reset_concentrator.sh";

static void logf_(const char *fmt, ...) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); struct tm tm; localtime_r(&ts.tv_sec, &tm);
    char tb[32]; strftime(tb, sizeof tb, "%H:%M:%S", &tm);
    fprintf(stderr, "%s.%03ld ", tb, ts.tv_nsec / 1000000);
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr);
}
#define LOG(...) logf_(__VA_ARGS__)
#define DBG(...) do { if (verbose) logf_(__VA_ARGS__); } while (0)

static void msleep(int ms) { struct timespec t = { ms / 1000, (ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static uint64_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000; }

/* ---------------- shutdown ---------------- */
static int spi_fd = -1;
static int hal_connected = 0;
static int ls = -1, client = -1;

static int run_reset(const char *arg) {
    char c[512];
    snprintf(c, sizeof c, "%s %s 2>&1", reset_script, arg);
    int st = system(c);
    if (st == -1 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        LOG("reset script failed: '%s' (status %d)", c, st);
        return -1;
    }
    return 0;
}

static void standby(void);
static int in_exit = 0;            /* set once cleanup starts: SPI failures no longer escalate */
static void cleanup(void) {
    in_exit = 1;
    if (client >= 0) { close(client); client = -1; }
    if (ls >= 0) { close(ls); ls = -1; }
    if (spi_fd >= 0) { standby(); close(spi_fd); spi_fd = -1; }
    if (hal_connected) {
        lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_PA_EN, 0);
        lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_LNA_EN, 0);
        lgw_reg_w(SX1302_REG_AGC_MCU_CTRL_FORCE_HOST_FE_CTRL, 0);
        sx1302_radio_host_ctrl(false);
        lgw_disconnect();
        hal_connected = 0;
    }
    /* hold the concentrator in reset, so nothing transmits or draws PA current while the daemon is down */
    if (run_reset("--hold") != 0) LOG("WARNING: could not hold the concentrator in reset");
}

static void die(const char *fmt, ...) {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); struct tm tm; localtime_r(&ts.tv_sec, &tm);
    char tb[32]; strftime(tb, sizeof tb, "%H:%M:%S", &tm);
    fprintf(stderr, "%s.%03ld FATAL: ", tb, ts.tv_nsec / 1000000);
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr);
    cleanup();
    exit(1);
}
#define CHECK_HAL(call) do { if ((call) != LGW_REG_SUCCESS) die("%s failed", #call); } while (0)

/* SPI / HAL failure accounting. Every low-level helper below reports into this; a run of
 * SPI_FAIL_LIMIT consecutive failures takes the fatal path (cleanup holds the concentrator in reset,
 * exit status 1), so a service manager restarts the daemon from a clean reset. Any success resets
 * the run. During cleanup the counter is ignored so the shutdown itself cannot recurse. */
#define SPI_FAIL_LIMIT 5
static int spi_fail_run = 0; static uint32_t spi_fail_total = 0;
static void spi_fail_note(const char *what, int op) {
    spi_fail_run++; spi_fail_total++;
    LOG("SPI failure %d of %d (%s 0x%02x): %s", spi_fail_run, SPI_FAIL_LIMIT, what, op, errno ? strerror(errno) : "short or bad transfer");
    if (!in_exit && spi_fail_run >= SPI_FAIL_LIMIT)
        die("%d consecutive SPI failures (last: %s 0x%02x), %u in total", spi_fail_run, what, op, spi_fail_total);
}
static void spi_ok(void) { spi_fail_run = 0; }
#ifdef M2_TEST_DRIVE
static int inject_spi_fail = 0;      /* test binary only (-F 2): every SPI transfer fails */
#endif
/* HAL register write whose result the caller cannot act on: count it. */
static void hal_w(uint16_t reg, int32_t v) {
    if (lgw_reg_w(reg, v) != LGW_REG_SUCCESS) spi_fail_note("HAL register write", reg); else spi_ok();
}

/* ---------------- SX1250-as-SX126x over the SX1302 SPI mux ---------------- */
static const uint8_t mux = LGW_SPI_MUX_TARGET_RADIOA;

static int cmd_w(uint8_t op, const uint8_t *args, int n) {
    uint8_t buf[300]; buf[0] = mux; buf[1] = op; if (n) memcpy(buf + 2, args, n);
    struct spi_ioc_transfer k; memset(&k, 0, sizeof k);
    k.tx_buf = (unsigned long)buf; k.len = 2 + n;
    msleep(1); /* BUSY is the 32 MHz clock output on these boards, not readable: fixed delay */
    errno = 0;
#ifdef M2_TEST_DRIVE
    if (inject_spi_fail && !in_exit) { spi_fail_note("command write (injected)", op); return -1; }
#endif
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &k) == 2 + n) { spi_ok(); return 0; }
    spi_fail_note("command write", op);
    return -1;
}
static int cmd_r(uint8_t op, const uint8_t *args, int na, uint8_t *out, int n) {
    uint8_t tx[16]; tx[0] = mux; tx[1] = op; if (na) memcpy(tx + 2, args, na);
    struct spi_ioc_transfer k[2]; memset(k, 0, sizeof k);
    k[0].tx_buf = (unsigned long)tx; k[0].len = 2 + na;
    k[1].rx_buf = (unsigned long)out; k[1].len = n;
    msleep(1);
    errno = 0;
#ifdef M2_TEST_DRIVE
    if (inject_spi_fail && !in_exit) { spi_fail_note("command read (injected)", op); return -1; }
#endif
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(2), k) == 2 + na + n) { spi_ok(); return 0; }
    spi_fail_note("command read", op);
    return -1;
}
static void reg_w(uint16_t a, uint8_t v) { uint8_t b[3] = { a >> 8, a & 0xff, v }; cmd_w(0x0D, b, 3); }
static int reg_rd(uint16_t a) { uint8_t ar[2] = { a >> 8, a & 0xff }, o[2] = {0, 0}; if (cmd_r(0x1D, ar, 2, o, 2) != 0) return -1; return o[1]; }
/* A read that completes but returns all ones (MISO stuck high) is a failure too: the run is
 * advanced from the value it had before the read, since the transfer itself reset it. */
static int ones_fault(int run_before, const char *what, int op) {
    spi_fail_run = run_before; spi_fail_note(what, op); return -1;
}
/* GetStatus: returns the status byte, or -1 if the read failed. */
static int status_byte(void) {
    uint8_t o[1] = {0}; int before = spi_fail_run;
    if (cmd_r(0xC0, NULL, 0, o, 1) != 0) return -1;
    if (o[0] == 0xFF) return ones_fault(before, "GetStatus read all ones", 0xC0);
    return o[0];
}
/* GetIrqStatus: returns 0xFFFF if the read failed or came back all ones (callers treat that as a fault). */
static uint16_t irq_get(void) {
    uint8_t o[3] = {0}; int before = spi_fail_run;
    if (cmd_r(0x12, NULL, 0, o, 3) != 0) return 0xFFFF;
    uint16_t v = (o[1] << 8) | o[2];
    if (v == 0xFFFF) ones_fault(before, "GetIrqStatus read all ones", 0x12);
    return v;
}
static void irq_clear(void) { uint8_t a[2] = { 0xFF, 0xFF }; cmd_w(0x02, a, 2); }
/* GetRssiInst: byte 1 is the level in 0.5 dB steps below 0 dBm. Returns dBm, or 1.0 if the read failed. */
static double rssi_inst(void) { uint8_t r[2] = {0}; if (cmd_r(0x15, NULL, 0, r, 2) != 0) return 1.0; return -r[1] / 2.0; }
/* GetDeviceErrors: returns 0xFFFF if the read failed. */
static uint16_t device_errors(void) { uint8_t o[3] = {0}; if (cmd_r(0x17, NULL, 0, o, 3) != 0) return 0xFFFF; return (o[1] << 8) | o[2]; }

/* radio config (openHop RADIO_CONFIG "<IIBBbHB") */
/* MeshCore firmware: 32 preamble symbols at SF8 and below, 16 above (RadioLibWrappers.h
 * preambleLengthForSF). Used whenever the client sets no preamble. */
static uint8_t preamble_for_sf(uint8_t sf) { return sf <= 8 ? 32 : 16; }
static int preamble_explicit = 0;           /* 1 = the client's last config carried a preamble */
static struct { uint32_t freq, bw; uint8_t sf, cr; int8_t power; uint16_t sync; uint8_t preamble; } cfg =
    { 910525000, 62500, 7, 5, 12, 0x12, 32 };    /* 32 = preamble_for_sf(7) */
static struct { uint8_t sym_code, peak, min; } cad = { 0x01, 22, 10 };

/* Front-end control through the SX1302. Only works while the SX1302 core is clocked (radio B in
 * I/Q mode). PA_EN high = transmit path; LNA_EN high = receive LNA. PA_GAIN selects the PA. */
/* Front end to receive, then read PA_EN back: returns 0 only if PA_EN is confirmed low. */
static int fe_rx(void) {
    int32_t v = -1;
    if (lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_PA_EN, 0) != LGW_REG_SUCCESS) return -1;
    if (lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_LNA_EN, 1) != LGW_REG_SUCCESS) return -1;
    if (lgw_reg_r(SX1302_REG_AGC_MCU_RF_EN_A_PA_EN, &v) != LGW_REG_SUCCESS || v != 0) return -1;
    return 0;
}
static int fe_tx(void) {
    if (lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_LNA_EN, 0) != LGW_REG_SUCCESS) return -1;
    return lgw_reg_w(SX1302_REG_AGC_MCU_RF_EN_A_PA_EN, 1) == LGW_REG_SUCCESS ? 0 : -1;
}

/* CAL: transmit power calibration, measured 2026-11-24 on one unit indoors with a second, rooftop unit as the
 * receiver, test channel 919.875 MHz, SF11, 250 kHz, sync 0x12, 30-packet runs about 1 s apart.
 * "target" is the level the HAL gives on this board at that RAK2287 US915 table entry (rf_power dBm;
 * rak_common_for_gateway global_conf.us_902_928.json); "drive" is the SX1250
 * SetTxParams power (dBm, integer steps) and "pa" the SX1302 PA gain register (1 = PA on) that gave
 * the same mean received level (rssic) when the SX1250 is driven directly as a LoRa transmitter.
 * Method: modem runs interleaved with HAL runs at the matching RAK table entries (HAL, modem, HAL,
 * modem, ...), mean rssic over the decoded packets of each run, pooled per setting. Run counts for the
 * 21, 24, and 27 dBm entries: HAL 2, 2, and 12 runs; modem (drive 15, 18, and 22) 3, 2, and 8 runs.
 * The test path fades by several dB over tens of seconds, so the top three entries match
 * the HAL to about +/-2 dB, not better. The 12, 15, and 18 dBm entries (drive 5, 9, and 11) were
 * matched the same way, within about 1 dB. Drive 22 dBm is the SX1250's maximum, so the 27 dBm entry
 * cannot go higher. 0.1 and 0.2 assumed drive = power index - 4, which measured 3.6 dB low at 12 dBm.
 * A request is rounded down to the nearest entry and limited by -P; a request below 12 uses the 12
 * dBm entry, and the reply reports the power actually used. */
static const struct { int8_t target; int8_t drive; uint8_t pa; } CAL[] = {
    {12, 5, 1}, {15, 9, 1}, {18, 11, 1}, {21, 15, 1}, {24, 18, 1}, {27, 22, 1}};
#define CAL_MAX_DBM 27
#ifdef M2_TEST_DRIVE
#define TESTTAG " (TEST: SX1250 drive, not calibrated dBm)"
#else
#define TESTTAG ""
#endif
static int max_power = 12;
static int tx_pa_gain = 0, tx_drive = 0, tx_power_eff = 0;
#ifdef M2_TEST_DRIVE
/* Test binary only (-T): the config's power field is taken as the SX1250 drive in dBm, with the PA
 * gain from -A, bypassing CAL and the -P cap. Drive above 22 dBm (the SX1250 maximum) is refused. */
static int test_drive_mode = 0, test_pa = 1;
/* -A 0..3: SX1302 AGC_MCU_PA_GAIN_PA_A_GAIN (2-bit field, loragw_reg.c). -O 1: read the SX126x OCP register
 * (0x08E7) before and after SetPaConfig and log both; -O 2: also write 0x38 (140 mA) after SetPaConfig and read
 * back. Test experiments of 2026-11-24 (taskC); not in the production path. */
static int test_ocp = 0;
static int reg_r(uint16_t a) { uint8_t ar[2] = { a >> 8, a & 0xff }, o[2] = {0, 0}; if (cmd_r(0x1D, ar, 2, o, 2) != 0) return -1; return o[1]; }
/* -F 1: 15 s after start, put the chip in standby behind the loop's back (recovery test).
 * -F 2: 15 s after start, make every SPI transfer fail (fatal-exit test). */
static int test_fault = 0;
#endif
static int power_lookup(int want) {
#ifdef M2_TEST_DRIVE
    if (test_drive_mode) {
        if (want > 22 || want < -9) { LOG("TEST: drive %d dBm outside -9..22, refused", want); return -1; }
        tx_pa_gain = test_pa; tx_drive = want; tx_power_eff = want;
        return 0;
    }
#endif
    if (want > max_power) want = max_power;
    if (want < CAL[0].target) LOG("requested %d dBm is below the calibrated range; using %d dBm", want, CAL[0].target);
    int i = 0, n = sizeof CAL / sizeof CAL[0];
    while (i + 1 < n && CAL[i + 1].target <= want) i++;
    tx_pa_gain = CAL[i].pa; tx_drive = CAL[i].drive; tx_power_eff = CAL[i].target;
    return 0;
}

static int bw_code(uint32_t bw) {
    switch (bw) { case 7800: return 0x00; case 10400: return 0x08; case 15600: return 0x01; case 20800: return 0x09;
        case 31250: return 0x02; case 41700: return 0x0A; case 62500: return 0x03; case 125000: return 0x04;
        case 250000: return 0x05; case 500000: return 0x06; default: return -1; }
}
static int ldro_needed(void) { /* symbol time > 16.38 ms */ return ((1000000.0 * (1 << cfg.sf)) / cfg.bw) > 16380.0; }

enum { RS_RX, RS_TX, RS_CAD, RS_IDLE } rstate = RS_IDLE;

static void set_packet_params(uint8_t len) {
    uint8_t a[6] = { 0x00, cfg.preamble, 0x00 /* explicit */, len, 0x01 /* CRC */, 0x00 /* IQ std */ };
    cmd_w(0x8C, a, 6);
}
static void standby(void) { uint8_t a[1] = { 0x01 }; cmd_w(0x80, a, 1); msleep(1); }

/* Receive knobs, read every 5 s from RX_KNOBS (absent file: defaults).
 *   agcfix=1  (default) set bit 0 of register 0x08B5; agcfix=0 clear it.
 *             (Undocumented register. MeshCore PR 3824, Meshtastic PR 9777, and openHop's own SX1262
 *             driver set the bit against a gain-control lockup after a strong off-channel signal.
 *             Measured on an SX1250 (RAK2287) on 2026-11-24: about 52 lockups per 15 minutes
 *             without the bit, none in 15 minutes with it.)
 *   inflight=1 (default) a channel check that arrives while a packet is being received is answered
 *             "busy" without leaving receive; inflight=0: always run the chip's channel check, which
 *             aborts the reception (the behavior up to 0.9).
 *   watch=0   gain watch off; watch=1 log only; watch=2 (default) log and re-enter receive. */
#define RX_KNOBS "/etc/m2modem/rx_knobs"
static int inflight_guard = 1;
static int floor_reset = 0;                 /* set at every radio config: the gain watch relearns its floor */
static int agcfix = 1, agc_watch_mode = 2, agcfix_logged = -2, agcfix_pending = 0;
static void apply_agcfix(void) {
    if (agcfix < 0) return;
    int v = reg_rd(0x08B5);
    if (v < 0) return;
    int w = agcfix ? (v | 0x01) : (v & ~0x01);
    reg_w(0x08B5, (uint8_t)w);
    int r = reg_rd(0x08B5);
    if (agcfix_logged != agcfix || r != w) { LOG("gain fix: register 0x08B5 was 0x%02x, wrote 0x%02x, reads 0x%02x (agcfix=%d)", v, w, r & 0xFF, agcfix); agcfix_logged = agcfix; }
}
static int rx_knobs_read(void) {             /* returns 1 when agcfix changed */
    int fix = 1, watch = 2, infl = 1; char line[64];
    FILE *f = fopen(RX_KNOBS, "r");
    if (f) {
        while (fgets(line, sizeof line, f)) { int v; if (sscanf(line, "agcfix=%d", &v) == 1) fix = v ? 1 : 0; else if (sscanf(line, "watch=%d", &v) == 1 && v >= 0 && v <= 2) watch = v; else if (sscanf(line, "inflight=%d", &v) == 1) infl = v ? 1 : 0; }
        fclose(f);
    }
    if (watch != agc_watch_mode) { LOG("gain watch mode %d -> %d", agc_watch_mode, watch); agc_watch_mode = watch; }
    if (infl != inflight_guard) { LOG("in-flight guard %d -> %d", inflight_guard, infl); inflight_guard = infl; }
    if (fix != agcfix) { agcfix = fix; return 1; }
    return 0;
}

static int first_config = 1;
static int radio_apply_config(void) {
    int bwc = bw_code(cfg.bw);
    if (bwc < 0 || cfg.sf < 5 || cfg.sf > 12 || cfg.cr < 5 || cfg.cr > 8) return -1;
    if (cfg.freq < 902000000U || cfg.freq > 928000000U) return -1;     /* US band only */
    standby();
    uint8_t a[8];
    a[0] = 0x01; cmd_w(0x8A, a, 1);                                         /* LoRa */
    uint32_t fr = (uint32_t)((uint64_t)cfg.freq * (1 << 25) / 32000000U);
    a[0] = fr >> 24; a[1] = fr >> 16; a[2] = fr >> 8; a[3] = fr; cmd_w(0x86, a, 4);
    /* Image calibration error = device-errors bit 4, as Semtech's sx1250_calibrate checks. The flag
       may be sticky and the HAL has no command to clear it, so: on the first config any set flag is
       fatal (as in the HAL); afterwards only a 0 -> 1 change counts, and an already-set flag is
       logged as "cannot tell". */
    uint16_t de0 = device_errors();
    a[0] = 0xE1; a[1] = 0xE9; cmd_w(0x98, a, 2); msleep(10);                 /* image cal 902-928 */
    uint16_t de = device_errors();
    if (de0 == 0xFFFF || de == 0xFFFF) { LOG("device errors could not be read"); return -1; }
    if (de & 0x0010) {
        if (first_config || !(de0 & 0x0010)) { LOG("image calibration error (device errors 0x%04x)", de); return -1; }
        LOG("WARNING: image calibration error flag was already set (0x%04x); cannot tell whether this calibration passed", de);
    }
    first_config = 0;
    a[0] = cfg.sf; a[1] = bwc; a[2] = cfg.cr - 4; a[3] = ldro_needed(); cmd_w(0x8B, a, 4);
    set_packet_params(0xFF);
    /* sync word: 0xXY -> 0xX4 0xY4 (RadioLib convention); full 16-bit values passed through */
    uint16_t sw = cfg.sync <= 0xFF ? (uint16_t)((((cfg.sync & 0xF0) | 0x04) << 8) | (((cfg.sync & 0x0F) << 4) | 0x04)) : cfg.sync;
    reg_w(0x0740, sw >> 8); reg_w(0x0741, sw & 0xff);
#ifdef M2_TEST_DRIVE
    int ocp0 = test_ocp ? reg_r(0x08E7) : 0;
#endif
    a[0] = 0x04; a[1] = 0x07; a[2] = 0x00; a[3] = 0x01; cmd_w(0x95, a, 4);   /* HP PA */
#ifdef M2_TEST_DRIVE
    if (test_ocp) {
        int ocp1 = reg_r(0x08E7);
        LOG("TEST OCP 0x08E7: before SetPaConfig 0x%02x, after 0x%02x (sync-word check reg 0x0740 = 0x%02x)", ocp0 & 0xFF, ocp1 & 0xFF, reg_r(0x0740) & 0xFF);
        if (test_ocp >= 2) { reg_w(0x08E7, 0x38); LOG("TEST OCP 0x08E7: wrote 0x38, reads back 0x%02x", reg_r(0x08E7) & 0xFF); }
    }
#endif
    if (power_lookup(cfg.power) != 0) return -1;
    a[0] = (uint8_t)(int8_t)tx_drive; a[1] = 0x04; cmd_w(0x8E, a, 2);        /* SX1250 drive, 200 us ramp */
    hal_w(SX1302_REG_AGC_MCU_PA_GAIN_PA_A_GAIN, rx_only ? 0 : tx_pa_gain);
    a[0] = 0x00; a[1] = 0x00; cmd_w(0x8F, a, 2);                             /* buffer base */
    reg_w(0x08AC, 0x96);                                                     /* RX boosted gain */
    apply_agcfix();
    floor_reset = 1;
    LOG("radio config: %.3f MHz BW%u SF%u CR4/%u, TX %d dBm by calibration table (PA %d, SX1250 drive %d dBm)%s, sync 0x%04x, preamble %u (%s), ldro %d",
        cfg.freq / 1e6, cfg.bw, cfg.sf, cfg.cr, tx_power_eff, tx_pa_gain, tx_drive,
        rx_only ? " [receive only]" : "", sw, cfg.preamble,
        preamble_explicit ? "set by client" : "default for this SF", ldro_needed());
    return 0;
}

static void radio_rx(void) {
    uint8_t a[8];
    standby();
    set_packet_params(0xFF);
    a[0] = 0x02; a[1] = 0x76; a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 0; cmd_w(0x08, a, 8);
    irq_clear();
    a[0] = 0x00; a[1] = 0x00; cmd_w(0x8F, a, 2);
    a[0] = 0xFF; a[1] = 0xFF; a[2] = 0xFF; cmd_w(0x82, a, 3);                /* RX continuous */
    rstate = RS_RX;
}

/* LoRa time on air, microseconds (Semtech formula, explicit header, CRC on) */
static uint32_t toa_us(int len) {
    double ts = (double)(1 << cfg.sf) / cfg.bw;
    int de = ldro_needed();
    int num = 8 * len - 4 * cfg.sf + 28 + 16;
    int den = 4 * (cfg.sf - 2 * de);
    int n = num > 0 ? (num + den - 1) / den : 0;
    double nsym = 8 + n * cfg.cr;
    return (uint32_t)((cfg.preamble + 4.25 + nsym) * ts * 1e6);
}

/* Transmit test knobs, read before every transmit from TX_KNOBS (absent file: stock behavior, nothing
 * touched). Lines: "mq=1" sets bit 2 of register 0x0889 before the transmit (Semtech DS.SX1261-2 section
 * 15.1, required for bandwidths other than 500 kHz), "mq=0" clears it; "cap=N" lowers the transmit power
 * to the calibration entry at or below N dBm for as long as the line is there (never raises it).
 * knob_txt is appended to the TX log line. */
#define TX_KNOBS "/etc/m2modem/tx_knobs"
static char knob_txt[64] = "";
static int knob_cap_applied = 0;
static void tx_knobs(void) {
    int mq = -1, cap = 0, have = 0; char line[64];
    FILE *f = fopen(TX_KNOBS, "r");
    if (f) {
        have = 1;
        while (fgets(line, sizeof line, f)) { int v; if (sscanf(line, "mq=%d", &v) == 1) mq = v ? 1 : 0; else if (sscanf(line, "cap=%d", &v) == 1) cap = v; }
        fclose(f);
    }
    knob_txt[0] = 0;
    if (cap > 0 || knob_cap_applied) {
        int want = (cap > 0 && cap < cfg.power) ? cap : cfg.power;
        if (power_lookup(want) == 0) {
            uint8_t a[2] = { (uint8_t)(int8_t)tx_drive, 0x04 }; cmd_w(0x8E, a, 2);
            hal_w(SX1302_REG_AGC_MCU_PA_GAIN_PA_A_GAIN, tx_pa_gain);
        }
        knob_cap_applied = cap > 0;
    }
    int before = -1, after = -1;
    if (mq >= 0) {
        before = reg_rd(0x0889);
        if (before >= 0) { reg_w(0x0889, mq ? (before | 0x04) : (before & ~0x04)); after = reg_rd(0x0889); }
    }
    snprintf(knob_txt, sizeof knob_txt, " [knobs: mq=%d reg0889 0x%02x->0x%02x, power %d dBm]", mq, before & 0xFF, after & 0xFF, tx_power_eff);
    if (!have) knob_txt[0] = 0;
}

static int radio_tx(const uint8_t *data, int len) {
    uint8_t a[8], wb[260];
    if (rx_only) return -2;
    standby();
    set_packet_params(len);
    tx_knobs();
    a[0] = 0x00; a[1] = 0x00; cmd_w(0x8F, a, 2);
    wb[0] = 0; memcpy(wb + 1, data, len); cmd_w(0x0E, wb, len + 1);
    a[0] = 0x02; a[1] = 0x01; a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 0; cmd_w(0x08, a, 8);
    irq_clear();
    if (fe_tx() != 0) { if (fe_rx() != 0) die("front end stuck: PA_EN not confirmed low"); return -1; }
    a[0] = 0; a[1] = 0; a[2] = 0; cmd_w(0x83, a, 3);
    rstate = RS_TX;
    uint64_t t0 = now_ms(), limit = toa_us(len) / 1000 + 2000;
    int done = -1;
    while (now_ms() - t0 < limit) {
        msleep(2);
        uint16_t irq = irq_get();
        if (irq == 0xFFFF) { LOG("TX: IRQ read 0xFFFF (SPI fault?)"); break; }
        if (irq & 0x0001) { irq_clear(); done = (int)(now_ms() - t0); break; }
    }
    standby();                     /* stop the radio before the front end leaves transmit */
    if (fe_rx() != 0) die("front end stuck: PA_EN not confirmed low after TX");
    return done;
}

/* returns 1 busy, 0 clear, -1 error */
static int radio_cad(void) {
    uint8_t a[8];
    standby();
    a[0] = cad.sym_code; a[1] = cad.peak; a[2] = cad.min; a[3] = 0x00; a[4] = a[5] = a[6] = 0; cmd_w(0x88, a, 7);
    a[0] = 0x01; a[1] = 0x80; a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 0; cmd_w(0x08, a, 8); /* CadDone|CadDetected */
    irq_clear();
    cmd_w(0xC5, NULL, 0);
    rstate = RS_CAD;
    uint64_t t0 = now_ms();
    while (now_ms() - t0 < 500) {
        msleep(1);
        uint16_t irq = irq_get();
        if (irq == 0xFFFF) continue;                                  /* failed read, already counted */
        if (irq & 0x0080) { int busy = (irq & 0x0100) ? 1 : 0; irq_clear(); return busy; }
    }
    return -1;
}

static int rx_in_flight(void);
/* ---------------- TCP framing ---------------- */
static uint8_t rxbuf[4096]; static int rxlen = 0;
static uint32_t st_rx = 0, st_tx = 0, st_crc = 0; static int16_t last_rssi = 0, last_snr10 = 0;
static uint64_t t_start;
/* Noise floor: mean of the last NF_N idle RSSI samples (the same scheme openHop's own SX1262 driver
 * uses). Until the first sample is accepted it reports NF_UNSET. 0.1 only accepted samples below
 * (estimate + 15) starting from -120 dBm, so a real floor above -105 dBm was never accepted. */
#define NF_N 20
#define NF_UNSET (-120.0)
#define NF_GATE_DB 10.0
static double nf_win[NF_N]; static int nf_cnt = 0, nf_pos = 0, nf_rejects = 0;
static double noise_acc = NF_UNSET; static uint64_t last_activity_ms = 0;
static void noise_sample(double rs) {
    if (rs > 0.0 || rs < -127.5) return;                          /* failed or out-of-range read */
    if (nf_cnt >= 5 && rs > noise_acc + NF_GATE_DB) {            /* a packet or burst, not the floor */
        if (++nf_rejects < NF_N) return;
        nf_cnt = nf_pos = 0;                                      /* the level really moved: start over */
    }
    nf_rejects = 0;
    nf_win[nf_pos] = rs; nf_pos = (nf_pos + 1) % NF_N; if (nf_cnt < NF_N) nf_cnt++;
    double sum = 0; for (int i = 0; i < nf_cnt; i++) sum += nf_win[i];
    noise_acc = sum / nf_cnt;
}

static uint16_t crc16(const uint8_t *d, int n) {
    uint16_t c = 0xFFFF;
    for (int i = 0; i < n; i++) { c ^= d[i] << 8; for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1; }
    return c;
}
static void send_frame(uint8_t cmd, const void *pl, int n) {
    if (client < 0) return;
    if (n < 0 || n > 294) { LOG("send_frame: payload %d bytes too large, dropped", n); return; }
    uint8_t f[300]; f[0] = 0xAA; f[1] = cmd; f[2] = n & 0xff; f[3] = n >> 8;
    if (n) memcpy(f + 4, pl, n);
    uint16_t c = crc16(f + 1, 3 + n); f[4 + n] = c & 0xff; f[5 + n] = c >> 8;
    if (write(client, f, 6 + n) != 6 + n) DBG("short write");
}
static void send_err(uint8_t code) { send_frame(CMD_ERROR, &code, 1); }

static void cfg_payload(uint8_t *p) {
    /* report the power actually used (after the cap and RAK's table), not the request */
    memcpy(p, &cfg.freq, 4); memcpy(p + 4, &cfg.bw, 4); p[8] = cfg.sf; p[9] = cfg.cr; p[10] = (uint8_t)(int8_t)tx_power_eff;
    memcpy(p + 11, &cfg.sync, 2); p[13] = cfg.preamble;
}

static void handle(uint8_t cmd, const uint8_t *pl, int n) {
    DBG("cmd 0x%02x len %d", cmd, n);
    switch (cmd) {
    case CMD_PING: send_frame(CMD_PING, NULL, 0); break;
    case CMD_AUTH: send_frame(CMD_AUTH_OK, NULL, 0); break;
    case CMD_GET_VERSION: send_frame(CMD_VERSION_RESP, VERSION, strlen(VERSION)); break;
    case CMD_SET_CONFIG: {
        if (n < 13) { send_err(ERR_INVALID_CONFIG); break; }
        typeof(cfg) old = cfg; int old_explicit = preamble_explicit;
        memcpy(&cfg.freq, pl, 4); memcpy(&cfg.bw, pl + 4, 4); cfg.sf = pl[8]; cfg.cr = pl[9]; cfg.power = (int8_t)pl[10];
        memcpy(&cfg.sync, pl + 11, 2);
        /* no preamble field (13-byte config) or a field of 0 = not set: use MeshCore's rule for this SF */
        preamble_explicit = (n >= 14 && pl[13] != 0);
        cfg.preamble = preamble_explicit ? pl[13] : preamble_for_sf(cfg.sf);
        if (radio_apply_config() != 0) {
            LOG("rejected config %u Hz BW%u SF%u CR%u", cfg.freq, cfg.bw, cfg.sf, cfg.cr);
            cfg = old; preamble_explicit = old_explicit;
            if (radio_apply_config() != 0) die("could not restore the previous radio config");
            radio_rx(); send_err(ERR_INVALID_CONFIG); break;
        }
        radio_rx();
        uint8_t p[14]; cfg_payload(p); send_frame(CMD_CONFIG_RESP, p, 14); break; }
    case CMD_GET_CONFIG: { uint8_t p[14]; cfg_payload(p); send_frame(CMD_CONFIG_RESP, p, 14); break; }
    case CMD_RX_START: radio_rx(); send_frame(CMD_RX_STARTED, NULL, 0); break;
    case CMD_TX_REQUEST: {
        if (n < 1 || n > 255) { send_err(ERR_PAYLOAD_TOO_BIG); break; }
        if (rx_only) { LOG("TX %3dB refused: receive-only mode", n); send_frame(CMD_TX_FAIL, NULL, 0); break; }
        int ms = radio_tx(pl, n);
        radio_rx();
        if (ms < 0) { LOG("TX %dB: timeout", n); send_frame(CMD_TX_FAIL, NULL, 0); break; }
        st_tx++; uint32_t air = toa_us(n);
        LOG("TX %3dB airtime %u ms (radio %d ms) at %d dBm%s, front end back in RX (PA_EN low)%s: %02x%02x...", n, air / 1000, ms, tx_power_eff,
            TESTTAG, knob_txt, pl[0], n > 1 ? pl[1] : 0);
        send_frame(CMD_TX_DONE, &air, 4); break; }
    case CMD_CAD_REQUEST: {
        int fl = rx_in_flight();
        if (fl && inflight_guard) { uint8_t b = 1; send_frame(CMD_CAD_RESP, &b, 1); break; }
        if (fl) LOG("channel check aborts a reception in flight (guard off)");
        int r = radio_cad(); radio_rx();
        if (r < 0) { send_err(ERR_CAD_FAILED); break; }
        uint8_t b = (uint8_t)r; send_frame(CMD_CAD_RESP, &b, 1); DBG("CAD %s", r ? "busy" : "clear"); break; }
    case CMD_SET_CAD_PARAMS:
        if (n >= 3) { cad.sym_code = pl[0]; cad.peak = pl[1]; cad.min = pl[2]; }
        send_frame(CMD_CAD_PARAMS_RESP, pl, n < 3 ? n : 3); break;
    case CMD_NOISE_REQ: { int16_t nf = (int16_t)(noise_acc * 10); send_frame(CMD_NOISE_RESP, &nf, 2); break; }
    case CMD_STATUS_REQ: {
        uint8_t p[23]; uint32_t up = (now_ms() - t_start) / 1000; int16_t nf = (int16_t)(noise_acc * 10);
        memcpy(p, &up, 4); memcpy(p + 4, &st_rx, 4); memcpy(p + 8, &st_tx, 4); memcpy(p + 12, &st_crc, 4);
        memcpy(p + 16, &last_rssi, 2); memcpy(p + 18, &last_snr10, 2); memcpy(p + 20, &nf, 2); p[22] = 0; /* temp */
        /* radio_state, openHop's encoding: 0 idle/rx, 1 tx, 2 error. Read from the chip itself (GetStatus
           chip mode, bits 6:4: 5 = RX, 6 = TX). 0.1 always sent 0. TX is blocking in this single-threaded
           loop, so 1 only shows if the chip is still keyed when a status request is served; 2 means the
           chip is not in the mode the loop believes it is in (or the status read failed). */
        int sb = status_byte(), mode = sb < 0 ? -1 : (sb >> 4) & 7, rs_state = 0;
        if (mode == 6) rs_state = 1;
        else if (mode < 0 || (rstate == RS_RX && mode != 5)) rs_state = 2;
        DBG("status: chip status 0x%02x mode %d, loop state %d -> radio_state %d, noise %.1f dBm (%d samples)",
            sb, mode, (int)rstate, rs_state, noise_acc, nf_cnt);
        uint8_t q[24]; memcpy(q, p, 23); q[23] = (uint8_t)rs_state;
        send_frame(CMD_STATUS_RESP, q, 24); break; }
    default: send_err(ERR_INVALID_CMD);
    }
}

static void parse_client(void) {
    int i = 0;
    while (1) {
        while (i < rxlen && rxbuf[i] != 0xAA) i++;
        if (rxlen - i < 6) break;
        int n = rxbuf[i + 2] | (rxbuf[i + 3] << 8);
        if (n > 1024) { i++; continue; }
        if (rxlen - i < 6 + n) break;
        uint16_t c = crc16(rxbuf + i + 1, 3 + n), got = rxbuf[i + 4 + n] | (rxbuf[i + 5 + n] << 8);
        if (c != got) { send_err(ERR_CRC_MISMATCH); i++; continue; }
        handle(rxbuf[i + 1], rxbuf + i + 4, n);
        i += 6 + n;
    }
    memmove(rxbuf, rxbuf + i, rxlen - i); rxlen -= i;
}

/* Receive IRQ bits that can stand with no RxDone after them: PreambleDetected 0x04, SyncWordValid 0x08,
 * HeaderValid 0x10. If the chip loses the packet after any of them, nothing clears the flag. */
#define IRQ_PARTIAL 0x001C
static double partial_rssi = 0;                     /* channel RSSI when the partial-only IRQ was first seen */
/* Frequency error of the last received packet, Hz (sender minus this receiver as the chip estimates it).
 * Registers 0x076B..0x076D, 20-bit two's complement, Hz = raw * BW * 155 / 160e6: RadioLib's
 * SX126x::getFrequencyError. Semtech's datasheet does not document this use. Returns 9e9 on a failed read. */
static double freq_err_hz(void) {
    int b0 = reg_rd(0x076B), b1 = reg_rd(0x076C), b2 = reg_rd(0x076D);
    if (b0 < 0 || b1 < 0 || b2 < 0) return 9e9;
    int32_t raw = ((b0 << 16) | (b1 << 8) | b2) & 0x0FFFFF;
    if (raw & 0x80000) raw -= 0x100000;
    return raw * (double)cfg.bw * 155.0 / 160000000.0;
}
static uint64_t partial_since = 0;                   /* when the current partial-only IRQ was first seen */
static uint32_t st_stuck_irq = 0, st_rx_recover = 0;
/* How long a partial-only IRQ may stand: a 255-byte packet's airtime at the current modulation plus
 * 500 ms, never under 1 s (SF11/250 kHz: about 2.7 s; SF7/62.5 kHz: about 1.4 s). */
static uint64_t stuck_bound_ms(void) { uint64_t b = toa_us(255) / 1000 + 500; return b < 1000 ? 1000 : b; }

static void poll_rx(void) {
    if (rstate != RS_RX) return;
    uint16_t irq = irq_get();
    if (irq == 0xFFFF) return;                       /* failed or all-ones read: already counted */
    if (!irq) { partial_since = 0; return; }
    last_activity_ms = now_ms();
    if (!(irq & ~IRQ_PARTIAL)) {
        uint64_t t = now_ms();
        if (!partial_since) { partial_since = t; partial_rssi = rssi_inst(); }
        else if (t - partial_since > stuck_bound_ms()) {
            st_stuck_irq++;
            /* Not a stuck receiver: the flag is latched and the chip went back to searching. */
            LOG("detection with no packet: IRQ 0x%04x (%s), channel %.1f dBm when first seen, flag cleared after %llu ms (%u so far)",
                irq, (irq & 0x0010) ? "header valid" : (irq & 0x0008) ? "sync word" : "preamble only", partial_rssi,
                (unsigned long long)(t - partial_since), st_stuck_irq);
            irq_clear(); partial_since = 0;
        }
        return;
    }
    /* det: ms since the first detection flag of this reception was seen (0: none seen before the
     * packet finished). Longer than the packet's own airtime means an earlier false detection, and
     * that the chip went on searching after it. */
    unsigned long long det = partial_since ? (unsigned long long)(now_ms() - partial_since) : 0;
    partial_since = 0;
    if (irq & 0x0002) {
        uint8_t bs[3] = {0}; cmd_r(0x13, NULL, 0, bs, 3);
        uint8_t len = bs[1], start = bs[2];
        uint8_t ps[4] = {0}; cmd_r(0x14, NULL, 0, ps, 4);
        int crc_err = irq & 0x0040;
        double ferr = freq_err_hz();
        irq_clear();
        if (crc_err) {
            st_crc++;
            LOG("RX CRC error: %dB rssi %d snr %.1f sig %d ferr %+.0f Hz det %llu ms (%u so far)", len, -ps[1] / 2, (int8_t)ps[2] / 4.0, -ps[3] / 2, ferr, det, st_crc);
            return;
        }
        uint8_t off[1] = { start }, data[260] = {0};
        cmd_r(0x1E, off, 1, data, len + 1);
        int16_t rssi = -ps[1] / 2, snr10 = (int16_t)((int8_t)ps[2] * 10 / 4), srssi = -ps[3] / 2;
        last_rssi = rssi; last_snr10 = snr10; st_rx++;
        LOG("RX %3dB rssi %d snr %.1f sig %d ferr %+.0f Hz det %llu ms: %02x%02x...", len, rssi, snr10 / 10.0, srssi, ferr, det, data[1], len > 1 ? data[2] : 0);
        uint8_t pl[262]; memcpy(pl, &rssi, 2); memcpy(pl + 2, &snr10, 2); memcpy(pl + 4, &srssi, 2); memcpy(pl + 6, data + 1, len);
        send_frame(CMD_RX_PACKET, pl, 6 + len);
    } else if (irq & 0x0060) { st_crc++; irq_clear(); LOG("RX header or CRC error with no RxDone: IRQ 0x%04x (%u so far)", irq, st_crc); }
    else if (irq & ~IRQ_PARTIAL) irq_clear();
}

/* Is a packet being received right now? Header-valid flag: yes, for as long as a packet can last.
 * Preamble flag alone: yes for PREAMBLE_HOLD_MS (preamble, sync word, and header take about 95 ms at
 * SF7/62.5 kHz with a 32-symbol preamble; scaled by symbol time), after that it is a false detection.
 * A finished packet waiting to be read (RxDone) is delivered first and is not "in flight". */
static uint32_t st_inflight = 0;
static int rx_in_flight(void) {
    if (rstate != RS_RX) return 0;
    uint16_t irq = irq_get();
    if (irq == 0xFFFF || !irq) return 0;
    if (irq & ~IRQ_PARTIAL) { poll_rx(); return 0; }
    uint64_t t = now_ms();
    if (!partial_since) { partial_since = t; partial_rssi = rssi_inst(); }
    uint64_t age = t - partial_since;
    uint64_t sym_us = ((uint64_t)1000000 << cfg.sf) / (cfg.bw ? cfg.bw : 62500);
    uint64_t hold = (uint64_t)(cfg.preamble + 13 + 24) * sym_us / 1000 + 50;
    if ((irq & 0x0010) ? age > stuck_bound_ms() : age > hold) return 0;
    st_inflight++;
    LOG("channel check during a reception (IRQ 0x%04x, %llu ms in, channel %.1f dBm): answered busy, receive kept (%u so far)",
        irq, (unsigned long long)age, partial_rssi, st_inflight);
    return 1;
}

/* Idle check: the loop believes the chip is in RX; read its mode. If it is not, log it, re-apply the
 * radio config, re-enter RX, and count the event. Three in a row (the chip never stays in RX) is fatal. */
#define RX_CHECK_MS 1500
#define RX_RECOVER_LIMIT 3
static int recover_streak = 0;
static void rx_check(void) {
    int sb = status_byte();
    if (sb < 0) return;                                  /* failed read, already counted */
    int mode = (sb >> 4) & 7;
    if (mode == 5) { recover_streak = 0; return; }
    st_rx_recover++; recover_streak++;
    LOG("radio left RX: chip status 0x%02x (mode %d), loop thinks RX; re-applying config and re-entering RX (event %u, %d in a row)",
        sb, mode, st_rx_recover, recover_streak);
    if (recover_streak >= RX_RECOVER_LIMIT) die("radio would not stay in RX after %d recoveries", recover_streak);
    if (radio_apply_config() != 0) die("radio config failed during RX recovery");
    if (fe_rx() != 0) die("front end: PA_EN not confirmed low during RX recovery");
    radio_rx(); partial_since = 0;
}

/* Gain watch. Seen at one site on 2026-11-24: within seconds of entering receive the idle level rose to a
 * fixed value far above the floor and the unit stopped hearing weak stations until its own next
 * transmit re-entered receive. So: learn the floor as the lowest level that three idle samples in a
 * row stayed at or under; when six idle samples in a row (3 s, no packet activity) sit GAIN_HIGH_DB
 * above it, log the level and the gain-control registers and, in mode 2, re-enter receive. If the level
 * is still high afterwards the wait before the next attempt doubles, up to 60 s. */
#define GAIN_HIGH_DB 12.0
static double floor_min = 0.0, w3[3] = { 0, 0, 0 }, last_idle = 0.0;   /* 0.0 = not learned yet */
static int w3n = 0, hi_run = 0, after_restart = 0;
static uint32_t st_gain_events = 0, st_gain_restarts = 0;
static uint64_t gain_wait_ms = 5000, last_gain_restart = 0;
static void gain_watch(double rs) {
    if (rs > 0.0 || rs < -127.5) return;
    last_idle = rs;
    w3[w3n % 3] = rs; w3n++;
    if (floor_reset) { floor_reset = 0; floor_min = 0.0; w3n = 0; }
    if (w3n >= 3) { double m = w3[0]; if (w3[1] > m) m = w3[1]; if (w3[2] > m) m = w3[2]; if (floor_min == 0.0 || m < floor_min) floor_min = m; }
    if (floor_min == 0.0) return;
    int high = rs > floor_min + GAIN_HIGH_DB;
    if (after_restart) {
        after_restart = 0;
        LOG("gain watch: after re-entering receive the idle level is %.1f dBm (floor %.1f): %s", rs, floor_min, high ? "still high" : "back to normal");
        gain_wait_ms = high ? (gain_wait_ms * 2 > 60000 ? 60000 : gain_wait_ms * 2) : 5000;
    }
    if (!high) { hi_run = 0; return; }
    if (++hi_run < 6 || agc_watch_mode == 0) return;
    if (now_ms() - last_gain_restart < gain_wait_ms) return;
    hi_run = 0; st_gain_events++; last_gain_restart = now_ms();
    LOG("gain watch: idle level %.1f dBm for 3 s against a floor of %.1f dBm; registers 0x08BF 0x%02x, 0x08C0 0x%02x, 0x08B5 0x%02x; %s (event %u)",
        rs, floor_min, reg_rd(0x08BF) & 0xFF, reg_rd(0x08C0) & 0xFF, reg_rd(0x08B5) & 0xFF,
        agc_watch_mode == 2 ? "re-entering receive" : "logging only", st_gain_events);
    if (agc_watch_mode == 2) { radio_rx(); partial_since = 0; st_gain_restarts++; after_restart = 1; }
}

static volatile int stop = 0;
static void on_sig(int s) { (void)s; stop = 1; }

int main(int argc, char **argv) {
    int port = 5055, opt;
#ifdef M2_TEST_DRIVE
    while ((opt = getopt(argc, argv, "d:R:P:p:nvTA:F:O:")) != -1) {
#else
    while ((opt = getopt(argc, argv, "d:R:P:p:nv")) != -1) {
#endif
        switch (opt) {
        case 'd': spi_path = optarg; break;
        case 'R': reset_script = optarg; break;
        case 'P': max_power = atoi(optarg); break;
        case 'p': port = atoi(optarg); break;
        case 'n': rx_only = 1; break;
        case 'v': verbose = 1; break;
#ifdef M2_TEST_DRIVE
        case 'T': test_drive_mode = 1; break;
        case 'A': test_pa = atoi(optarg); if (test_pa < 0 || test_pa > 3) { fprintf(stderr, "-A must be 0..3\n"); return 2; } break;
        case 'O': test_ocp = atoi(optarg); break;
        case 'F': test_fault = atoi(optarg); break;
#endif
        default:
            fprintf(stderr, "usage: %s [-d spidev] [-R reset_script] [-P 12..27] [-p port] [-n] [-v]\n", argv[0]);
            return 2;
        }
    }
    if (max_power < 12 || max_power > CAL_MAX_DBM) { fprintf(stderr, "-P must be 12..%d (the calibrated range)\n", CAL_MAX_DBM); return 2; }
    if (port < 1024 || port > 65535) { fprintf(stderr, "-p must be 1024..65535\n"); return 2; }
    { /* no SA_RESTART, so a blocked read/write returns EINTR and the loop sees stop;
         SIGHUP (ssh session dropped) and SIGQUIT also go through cleanup */
      struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_sig; sigemptyset(&sa.sa_mask);
      sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
      sigaction(SIGHUP, &sa, NULL); sigaction(SIGQUIT, &sa, NULL);
      signal(SIGPIPE, SIG_IGN); }
    t_start = now_ms();
    LOG("%s: SPI %s, reset %s, TX cap %d dBm%s", VERSION, spi_path, reset_script, max_power, rx_only ? ", RECEIVE ONLY" : "");

    if (run_reset("") != 0) die("concentrator reset failed");
    if (lgw_connect(LGW_COM_SPI, spi_path) != LGW_REG_SUCCESS) die("SX1302 connect failed on %s", spi_path);
    hal_connected = 1;
    { int32_t v = 0;
      CHECK_HAL(lgw_reg_r(SX1302_REG_COMMON_VERSION_VERSION, &v));
      if (v != 0x10) die("SX1302 chip version 0x%02x, expected 0x10", (int)v);
      LOG("SX1302 chip version 0x%02x", (int)v); }

    /* Radio B stays in normal I/Q receive mode and clocks the SX1302 core (its BUSY pin is the
       32 MHz clock output into RADIO_B_CLK_I). Without that clock the front-end control
       registers ignore writes. */
    CHECK_HAL(sx1302_radio_reset(1, LGW_RADIO_TYPE_SX1250));
    CHECK_HAL(sx1302_radio_set_mode(1, LGW_RADIO_TYPE_SX1250));
    CHECK_HAL(sx1302_radio_host_ctrl(true));
    CHECK_HAL(sx1250_setup(1, cfg.freq - 300000, false));
    CHECK_HAL(sx1302_radio_clock_select(1));
    CHECK_HAL(sx1302_radio_reset(0, LGW_RADIO_TYPE_SX1250));
    CHECK_HAL(sx1302_radio_set_mode(0, LGW_RADIO_TYPE_SX1250));
    CHECK_HAL(sx1302_radio_host_ctrl(true));
    CHECK_HAL(lgw_reg_w(SX1302_REG_AGC_MCU_LUT_TABLE_A_PA_LUT, 0x04));
    CHECK_HAL(lgw_reg_w(SX1302_REG_AGC_MCU_LUT_TABLE_A_LNA_LUT, 0x02));
    CHECK_HAL(lgw_reg_w(SX1302_REG_AGC_MCU_CTRL_FORCE_HOST_FE_CTRL, 1));
    if (fe_rx() != 0) die("front end: PA_EN not confirmed low at startup");
    { int32_t v = 0;
      CHECK_HAL(lgw_reg_r(SX1302_REG_AGC_MCU_LUT_TABLE_A_PA_LUT, &v));
      if (v != 0x04) die("front-end control registers not responding (PA LUT reads 0x%x, wrote 0x04): SX1302 not clocked from radio B", (int)v);
      LOG("SX1302 clocked from radio B; front-end control OK"); }

    spi_fd = open(spi_path, O_RDWR);
    if (spi_fd < 0) die("open %s: %s", spi_path, strerror(errno));
    uint8_t mode = SPI_MODE_0, bits = 8; uint32_t speed = 2000000;
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0 || ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) die("SPI setup on %s: %s", spi_path, strerror(errno));

    uint8_t a[2];
    a[0] = 0x00; cmd_w(0x80, a, 1); msleep(10);                              /* STDBY_RC */
    a[0] = 0x7F; cmd_w(0x89, a, 1); msleep(20);                              /* calibrate all */
    a[0] = 0x01; cmd_w(0x80, a, 1); msleep(10);                              /* STDBY_XOSC */
    reg_w(0x0587, 0x00);                                                     /* leave FPGA (I/Q) mode */
    { int s = status_byte();
      if (s < 0 || ((s >> 4) & 7) != 3) die("radio A not in STDBY_XOSC after leaving I/Q mode (status 0x%02x)", s & 0xFF);
      LOG("radio A in STDBY_XOSC (status 0x%02x)", s); }
    if (radio_apply_config() != 0) die("initial radio config failed");
    radio_rx();

    ls = socket(AF_INET, SOCK_STREAM, 0); int one = 1;
    if (ls < 0) die("socket: %s", strerror(errno));
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) || listen(ls, 1)) die("bind/listen on 127.0.0.1:%d: %s", port, strerror(errno));
    LOG("listening on 127.0.0.1:%d (radio A)", port);

    uint64_t last_noise = 0, last_rxchk = now_ms(), last_knobs = 0, last_health = now_ms() - 240000;
    while (!stop) {
        fd_set fs; FD_ZERO(&fs); FD_SET(ls, &fs); int mx = ls;
        if (client >= 0) { FD_SET(client, &fs); if (client > mx) mx = client; }
        struct timeval tv = { 0, 5000 };
        int r = select(mx + 1, &fs, NULL, NULL, &tv);
        if (r > 0 && FD_ISSET(ls, &fs)) {
            int c = accept(ls, NULL, NULL);
            if (c >= 0) {
                if (client >= 0) { LOG("replacing existing client"); close(client); }
                client = c; rxlen = 0; setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
                { struct timeval st = { 1, 0 }; setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &st, sizeof st); } /* a stuck client can't block the loop */
                LOG("client connected");
            }
        }
        if (r > 0 && client >= 0 && FD_ISSET(client, &fs)) {
            int n = read(client, rxbuf + rxlen, sizeof rxbuf - rxlen);
            if (n <= 0) { LOG("client disconnected"); close(client); client = -1; rxlen = 0; }
            else { rxlen += n; parse_client(); }
        }
#ifdef M2_TEST_DRIVE
        if (test_fault && now_ms() - t_start > 15000) {
            if (test_fault == 1) { LOG("TEST fault 1: chip to standby behind the loop's back"); standby(); }
            if (test_fault == 2) { LOG("TEST fault 2: SPI transfers now fail"); inject_spi_fail = 1; }
            test_fault = 0;
        }
#endif
        poll_rx();
        if (rstate == RS_RX && now_ms() - last_rxchk > RX_CHECK_MS) { rx_check(); last_rxchk = now_ms(); }
        if (rstate == RS_RX && now_ms() - last_noise > 500 && now_ms() - last_activity_ms > 500) {
            /* one idle RSSI sample every 0.5 s, not within 0.5 s of any packet or error */
            double rs = rssi_inst();
            noise_sample(rs); gain_watch(rs); last_noise = now_ms();
        }
        if (rstate == RS_RX && now_ms() - last_knobs > 5000) {
            last_knobs = now_ms();
            if (rx_knobs_read()) agcfix_pending = 1;       /* applied below at the next idle moment */
        }
        if (agcfix_pending && rstate == RS_RX && now_ms() - last_activity_ms > 500) {
            agcfix_pending = 0; standby(); apply_agcfix(); radio_rx(); partial_since = 0;
        }
        if (rstate == RS_RX && now_ms() - last_health > 300000 && now_ms() - last_activity_ms > 500) {
            last_health = now_ms();
            LOG("rx health: idle %.1f dBm, floor %.1f, running mean %.1f; registers 0x08BF 0x%02x, 0x08C0 0x%02x, 0x08B5 0x%02x; gain events %u, receive re-entries %u",
                last_idle, floor_min, noise_acc, reg_rd(0x08BF) & 0xFF, reg_rd(0x08C0) & 0xFF, reg_rd(0x08B5) & 0xFF, st_gain_events, st_gain_restarts);
        }
    }
    LOG("stopping: rx %u, crc errors %u, tx %u, detections with no packet %u, RX recoveries %u, SPI failures %u, gain events %u, receive re-entries for gain %u",
        st_rx, st_crc, st_tx, st_stuck_irq, st_rx_recover, spi_fail_total, st_gain_events, st_gain_restarts);
    cleanup();
    return 0;
}
