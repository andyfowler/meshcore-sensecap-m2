/* m2modem: SenseCAP M2 (SX1302 + SX1250) as an openHop TCP modem.
 *
 * Drives SX1250 radio A directly as an SX126x LoRa transceiver (the SX1302 is only used as an
 * SPI pass-through), and serves the openHop Modem wire protocol on TCP so openhop_repeater
 * (radio_type: modem_tcp) can use it. Works at any LoRa BW the SX126x supports, incl. 62.5 kHz.
 *
 *   m2modem [-p port] [-r rf_chain] [-v]
 *
 * Frame: 0xAA | CMD | LEN(2 LE) | PAYLOAD | CRC16-CCITT-FALSE(2 LE) over CMD+LEN+PAYLOAD.
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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <linux/spi/spidev.h>
#include "loragw_hal.h"
#include "loragw_reg.h"
#include "loragw_com.h"
#include "loragw_sx1302.h"
#include "loragw_sx1250.h"

#define VERSION "m2modem 0.1"

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
static const char *reset_script = NULL;
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

/* ---------------- SX1250-as-SX126x over the SX1302 SPI mux ---------------- */
static int spi_fd = -1;
static uint8_t mux;

static int cmd_w(uint8_t op, const uint8_t *args, int n) {
    uint8_t buf[300]; buf[0] = mux; buf[1] = op; if (n) memcpy(buf + 2, args, n);
    struct spi_ioc_transfer k; memset(&k, 0, sizeof k);
    k.tx_buf = (unsigned long)buf; k.len = 2 + n;
    msleep(1); /* BUSY is not readable on this board: give the radio time between commands */
    return ioctl(spi_fd, SPI_IOC_MESSAGE(1), &k) == 2 + n ? 0 : -1;
}
static int cmd_r(uint8_t op, const uint8_t *args, int na, uint8_t *out, int n) {
    uint8_t tx[16]; tx[0] = mux; tx[1] = op; if (na) memcpy(tx + 2, args, na);
    struct spi_ioc_transfer k[2]; memset(k, 0, sizeof k);
    k[0].tx_buf = (unsigned long)tx; k[0].len = 2 + na;
    k[1].rx_buf = (unsigned long)out; k[1].len = n;
    msleep(1);
    return ioctl(spi_fd, SPI_IOC_MESSAGE(2), k) == 2 + na + n ? 0 : -1;
}
static void reg_w(uint16_t a, uint8_t v) { uint8_t b[3] = { a >> 8, a & 0xff, v }; cmd_w(0x0D, b, 3); }
static uint8_t status_byte(void) { uint8_t o[1] = {0}; cmd_r(0xC0, NULL, 0, o, 1); return o[0]; }
static uint16_t irq_get(void) { uint8_t o[3] = {0}; cmd_r(0x12, NULL, 0, o, 3); return (o[1] << 8) | o[2]; }
static void irq_clear(void) { uint8_t a[2] = { 0xFF, 0xFF }; cmd_w(0x02, a, 2); }
static int rssi_inst(void) { uint8_t r[2] = {0}; cmd_r(0x15, NULL, 0, r, 2); return -r[1] / 2; }

/* radio config (openHop RADIO_CONFIG "<IIBBbHB") */
static struct { uint32_t freq, bw; uint8_t sf, cr; int8_t power; uint16_t sync; uint8_t preamble; } cfg =
    { 910525000, 62500, 7, 5, 14, 0x12, 16 };
static struct { uint8_t sym_code, peak, min; } cad = { 0x01, 22, 10 };

/* Front-end (FEM) control. Only works while the SX1302 core is clocked (radio B in I/Q mode).
 * PA_EN high = FEM transmit path; LNA_EN high = FEM receive LNA. PA_GAIN selects FEM PA (0 bypass, 1 PA). */
static uint16_t reg_pa_en, reg_lna_en, reg_pa_gain;
static int max_power = 27;   /* dBm cap at the connector (-P to lower it); top of Seeed's table */
static void fe_rx(void) { lgw_reg_w(reg_pa_en, 0); lgw_reg_w(reg_lna_en, 1); }
static void fe_tx(void) { lgw_reg_w(reg_lna_en, 0); lgw_reg_w(reg_pa_en, 1); }

/* Seeed M2 US915 TX gain LUT (rf_power dBm -> FEM pa_gain, SX1250 pwr_idx); SX1250 drive dBm = pwr_idx - 4 */
static const struct { int8_t p; uint8_t pa, idx; } TXLUT[] = {
    {1,0,5},{2,0,6},{3,0,7},{4,0,8},{5,0,9},{6,0,10},{7,0,11},{8,0,12},{9,0,13},{10,0,14},{11,0,15},{12,0,16},
    {13,0,17},{14,0,18},{15,0,19},{16,1,3},{17,1,4},{18,1,5},{19,1,6},{20,1,7},{21,1,8},{22,1,9},{23,1,11},
    {24,1,12},{25,1,13},{26,1,14},{27,1,15}};
static int tx_pa_gain = 0, tx_drive = 0, tx_power_eff = 0;
static void power_lookup(int want) {
    if (want > max_power) want = max_power;
    int i = 0, n = sizeof TXLUT / sizeof TXLUT[0];
    while (i + 1 < n && TXLUT[i + 1].p <= want) i++;
    tx_pa_gain = TXLUT[i].pa; tx_drive = (int)TXLUT[i].idx - 4; tx_power_eff = TXLUT[i].p;
}

static int bw_code(uint32_t bw) {
    switch (bw) { case 7800: return 0x00; case 10400: return 0x08; case 15600: return 0x01; case 20800: return 0x09;
        case 31250: return 0x02; case 41700: return 0x0A; case 62500: return 0x03; case 125000: return 0x04;
        case 250000: return 0x05; case 500000: return 0x06; default: return -1; }
}
static int ldro_needed(void) { /* symbol time > 16.38 ms */ return ((1000000.0 * (1 << cfg.sf)) / cfg.bw) > 16380.0; }

enum { RS_RX, RS_TX, RS_CAD, RS_IDLE } rstate = RS_IDLE;

static void set_packet_params(uint8_t len) {
    uint8_t a[6] = { cfg.preamble >> 8, cfg.preamble & 0xff, 0x00 /* explicit */, len, 0x01 /* CRC */, 0x00 /* IQ std */ };
    a[0] = 0; a[1] = cfg.preamble; cmd_w(0x8C, a, 6);
}
static void standby(void) { uint8_t a[1] = { 0x01 }; cmd_w(0x80, a, 1); msleep(1); }

static int radio_apply_config(void) {
    int bwc = bw_code(cfg.bw);
    if (bwc < 0 || cfg.sf < 5 || cfg.sf > 12 || cfg.cr < 5 || cfg.cr > 8) return -1;
    standby();
    uint8_t a[8];
    a[0] = 0x01; cmd_w(0x8A, a, 1);                                         /* LoRa */
    uint32_t fr = (uint32_t)((uint64_t)cfg.freq * (1 << 25) / 32000000U);
    a[0] = fr >> 24; a[1] = fr >> 16; a[2] = fr >> 8; a[3] = fr; cmd_w(0x86, a, 4);
    a[0] = 0xE1; a[1] = 0xE9; cmd_w(0x98, a, 2); msleep(5);                  /* image cal 902-928 */
    a[0] = cfg.sf; a[1] = bwc; a[2] = cfg.cr - 4; a[3] = ldro_needed(); cmd_w(0x8B, a, 4);
    set_packet_params(0xFF);
    /* sync word: 0xXY -> 0xX4 0xY4 (RadioLib convention); full 16-bit values passed through */
    uint16_t sw = cfg.sync <= 0xFF ? (uint16_t)((((cfg.sync & 0xF0) | 0x04) << 8) | (((cfg.sync & 0x0F) << 4) | 0x04)) : cfg.sync;
    reg_w(0x0740, sw >> 8); reg_w(0x0741, sw & 0xff);
    a[0] = 0x04; a[1] = 0x07; a[2] = 0x00; a[3] = 0x01; cmd_w(0x95, a, 4);   /* HP PA */
    power_lookup(cfg.power);
    int p = tx_drive;
    a[0] = (uint8_t)(int8_t)p; a[1] = 0x04; cmd_w(0x8E, a, 2);               /* SX1250 drive, 200 us ramp */
    lgw_reg_w(reg_pa_gain, tx_pa_gain);
    a[0] = 0x00; a[1] = 0x00; cmd_w(0x8F, a, 2);                             /* buffer base */
    reg_w(0x08AC, 0x96);                                                     /* RX boosted gain */
    LOG("radio config: %.3f MHz BW%u SF%u CR4/%u %d dBm (FEM pa_gain %d, SX1250 %d dBm) sync 0x%04x pre %u ldro %d",
        cfg.freq / 1e6, cfg.bw, cfg.sf, cfg.cr, tx_power_eff, tx_pa_gain, p, sw, cfg.preamble, ldro_needed());
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

static int radio_tx(const uint8_t *data, int len) {
    uint8_t a[8], wb[260];
    standby();
    set_packet_params(len);
    a[0] = 0x00; a[1] = 0x00; cmd_w(0x8F, a, 2);
    wb[0] = 0; memcpy(wb + 1, data, len); cmd_w(0x0E, wb, len + 1);
    a[0] = 0x02; a[1] = 0x01; a[2] = a[3] = a[4] = a[5] = a[6] = a[7] = 0; cmd_w(0x08, a, 8);
    irq_clear();
    fe_tx();                                                                  /* FEM -> transmit */
    a[0] = 0; a[1] = 0; a[2] = 0; cmd_w(0x83, a, 3);
    rstate = RS_TX;
    uint64_t t0 = now_ms(), limit = toa_us(len) / 1000 + 2000;
    int done = -1;
    while (now_ms() - t0 < limit) {
        msleep(2);
        if (irq_get() & 0x0001) { irq_clear(); done = (int)(now_ms() - t0); break; }
    }
    fe_rx();                                                                  /* FEM -> receive */
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
        if (irq & 0x0080) { int busy = (irq & 0x0100) ? 1 : 0; irq_clear(); return busy; }
    }
    return -1;
}

/* ---------------- TCP framing ---------------- */
static int client = -1;
static uint8_t rxbuf[4096]; static int rxlen = 0;
static uint32_t st_rx = 0, st_tx = 0, st_crc = 0; static int16_t last_rssi = 0, last_snr10 = 0;
static double noise_acc = -120; static uint64_t t_start;

static uint16_t crc16(const uint8_t *d, int n) {
    uint16_t c = 0xFFFF;
    for (int i = 0; i < n; i++) { c ^= d[i] << 8; for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1; }
    return c;
}
static void send_frame(uint8_t cmd, const void *pl, int n) {
    if (client < 0) return;
    uint8_t f[300]; f[0] = 0xAA; f[1] = cmd; f[2] = n & 0xff; f[3] = n >> 8;
    if (n) memcpy(f + 4, pl, n);
    uint16_t c = crc16(f + 1, 3 + n); f[4 + n] = c & 0xff; f[5 + n] = c >> 8;
    if (write(client, f, 6 + n) != 6 + n) DBG("short write");
}
static void send_err(uint8_t code) { send_frame(CMD_ERROR, &code, 1); }

static void cfg_payload(uint8_t *p) {
    memcpy(p, &cfg.freq, 4); memcpy(p + 4, &cfg.bw, 4); p[8] = cfg.sf; p[9] = cfg.cr; p[10] = (uint8_t)cfg.power;
    memcpy(p + 11, &cfg.sync, 2); p[13] = cfg.preamble;
}

static void handle(uint8_t cmd, const uint8_t *pl, int n) {
    DBG("cmd 0x%02x len %d", cmd, n);
    switch (cmd) {
    case CMD_PING: send_frame(CMD_PING, NULL, 0); break;
    case CMD_AUTH: send_frame(CMD_AUTH_OK, NULL, 0); break;
    case CMD_GET_VERSION: send_frame(CMD_VERSION_RESP, VERSION, strlen(VERSION)); break;
    case CMD_SET_CONFIG: {
        if (n < 14) { send_err(ERR_INVALID_CONFIG); break; }
        typeof(cfg) old = cfg;
        memcpy(&cfg.freq, pl, 4); memcpy(&cfg.bw, pl + 4, 4); cfg.sf = pl[8]; cfg.cr = pl[9]; cfg.power = (int8_t)pl[10];
        memcpy(&cfg.sync, pl + 11, 2); cfg.preamble = pl[13];
        if (radio_apply_config() != 0) { cfg = old; radio_apply_config(); radio_rx(); send_err(ERR_INVALID_CONFIG); break; }
        radio_rx();
        uint8_t p[14]; cfg_payload(p); send_frame(CMD_CONFIG_RESP, p, 14); break; }
    case CMD_GET_CONFIG: { uint8_t p[14]; cfg_payload(p); send_frame(CMD_CONFIG_RESP, p, 14); break; }
    case CMD_RX_START: radio_rx(); send_frame(CMD_RX_STARTED, NULL, 0); break;
    case CMD_TX_REQUEST: {
        if (n < 1 || n > 255) { send_err(ERR_PAYLOAD_TOO_BIG); break; }
        int ms = radio_tx(pl, n);
        radio_rx();
        if (ms < 0) { LOG("TX %dB: timeout", n); send_frame(CMD_TX_FAIL, NULL, 0); break; }
        st_tx++; uint32_t air = toa_us(n);
        LOG("TX %3dB airtime %u ms (radio %d ms): %02x%02x...", n, air / 1000, ms, pl[0], n > 1 ? pl[1] : 0);
        send_frame(CMD_TX_DONE, &air, 4); break; }
    case CMD_CAD_REQUEST: {
        int r = radio_cad(); radio_rx();
        if (r < 0) { send_err(ERR_CAD_FAILED); break; }
        uint8_t b = (uint8_t)r; send_frame(CMD_CAD_RESP, &b, 1); DBG("CAD %s", r ? "busy" : "clear"); break; }
    case CMD_SET_CAD_PARAMS:
        if (n >= 3) { cad.sym_code = pl[0]; cad.peak = pl[1]; cad.min = pl[2]; }
        send_frame(CMD_CAD_PARAMS_RESP, pl, n); break;
    case CMD_NOISE_REQ: { int16_t nf = (int16_t)(noise_acc * 10); send_frame(CMD_NOISE_RESP, &nf, 2); break; }
    case CMD_STATUS_REQ: {
        uint8_t p[23]; uint32_t up = (now_ms() - t_start) / 1000; int16_t nf = (int16_t)(noise_acc * 10);
        memcpy(p, &up, 4); memcpy(p + 4, &st_rx, 4); memcpy(p + 8, &st_tx, 4); memcpy(p + 12, &st_crc, 4);
        memcpy(p + 16, &last_rssi, 2); memcpy(p + 18, &last_snr10, 2); memcpy(p + 20, &nf, 2); p[22] = 0; /* temp */
        uint8_t q[24]; memcpy(q, p, 23); q[23] = 0; /* radio_state idle/rx */
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

static void poll_rx(void) {
    if (rstate != RS_RX) return;
    uint16_t irq = irq_get();
    if (!irq) return;
    if (irq & 0x0002) {
        uint8_t bs[3] = {0}; cmd_r(0x13, NULL, 0, bs, 3);
        uint8_t len = bs[1], start = bs[2];
        uint8_t ps[4] = {0}; cmd_r(0x14, NULL, 0, ps, 4);
        int crc_err = irq & 0x0040;
        irq_clear();
        if (crc_err) { st_crc++; DBG("RX CRC error"); return; }
        uint8_t off[1] = { start }, data[260] = {0};
        cmd_r(0x1E, off, 1, data, len + 1);
        int16_t rssi = -ps[1] / 2, snr10 = (int16_t)((int8_t)ps[2] * 10 / 4), srssi = -ps[3] / 2;
        last_rssi = rssi; last_snr10 = snr10; st_rx++;
        LOG("RX %3dB rssi %d snr %.1f: %02x%02x...", len, rssi, snr10 / 10.0, data[1], len > 1 ? data[2] : 0);
        uint8_t pl[262]; memcpy(pl, &rssi, 2); memcpy(pl + 2, &snr10, 2); memcpy(pl + 4, &srssi, 2); memcpy(pl + 6, data + 1, len);
        send_frame(CMD_RX_PACKET, pl, 6 + len);
    } else if (irq & 0x0060) { st_crc++; irq_clear(); DBG("RX header/crc error irq 0x%04x", irq); }
    else if (irq & ~0x0004 & ~0x0008 & ~0x0010) irq_clear();
}

static volatile int stop = 0;
static void on_sig(int s) { (void)s; stop = 1; }

int main(int argc, char **argv) {
    int port = 5055, rf = 0, opt;
    while ((opt = getopt(argc, argv, "p:r:R:P:v")) != -1) {
        if (opt == 'P') { max_power = atoi(optarg); if (max_power > 27) max_power = 27; continue; }
        if (opt == 'R') reset_script = optarg;
        else if (opt == 'p') port = atoi(optarg); else if (opt == 'r') rf = atoi(optarg); else if (opt == 'v') verbose = 1;
    }
    signal(SIGINT, on_sig); signal(SIGTERM, on_sig); signal(SIGPIPE, SIG_IGN);
    mux = rf == 0 ? LGW_SPI_MUX_TARGET_RADIOA : LGW_SPI_MUX_TARGET_RADIOB;
    t_start = now_ms();

    if (!reset_script) reset_script = access("/usr/libexec/meshcore-m2/reset-sx1302", X_OK) == 0
                                     ? "/usr/libexec/meshcore-m2/reset-sx1302" : "/usr/bin/lora/reset_lgw.sh";
    { char c[256]; snprintf(c, sizeof c, "%s start >/dev/null 2>&1", reset_script); system(c); }
    if (lgw_connect(LGW_COM_SPI, "/dev/spidev0.1") != LGW_REG_SUCCESS) { LOG("SX1302 connect failed"); return 1; }
    /* The other radio stays in normal I/Q ("FPGA") RX mode and clocks the SX1302 core. Without that
       clock the SX1302's front-end control registers ignore writes and the FEM stays in receive. */
    int clk = rf ? 0 : 1;
    sx1302_radio_reset(clk, LGW_RADIO_TYPE_SX1250);
    sx1302_radio_set_mode(clk, LGW_RADIO_TYPE_SX1250);
    sx1302_radio_host_ctrl(true);
    if (sx1250_setup(clk, cfg.freq - 300000, false) != 0) LOG("warning: clock radio setup failed");
    sx1302_radio_clock_select(clk);
    sx1302_radio_reset(rf, LGW_RADIO_TYPE_SX1250);
    sx1302_radio_set_mode(rf, LGW_RADIO_TYPE_SX1250);
    sx1302_radio_host_ctrl(true);
    reg_pa_en = rf ? SX1302_REG_AGC_MCU_RF_EN_B_PA_EN : SX1302_REG_AGC_MCU_RF_EN_A_PA_EN;
    reg_lna_en = rf ? SX1302_REG_AGC_MCU_RF_EN_B_LNA_EN : SX1302_REG_AGC_MCU_RF_EN_A_LNA_EN;
    reg_pa_gain = rf ? SX1302_REG_AGC_MCU_PA_GAIN_PA_B_GAIN : SX1302_REG_AGC_MCU_PA_GAIN_PA_A_GAIN;
    lgw_reg_w(rf ? SX1302_REG_AGC_MCU_LUT_TABLE_B_PA_LUT : SX1302_REG_AGC_MCU_LUT_TABLE_A_PA_LUT, 0x04);
    lgw_reg_w(rf ? SX1302_REG_AGC_MCU_LUT_TABLE_B_LNA_LUT : SX1302_REG_AGC_MCU_LUT_TABLE_A_LNA_LUT, 0x02);
    lgw_reg_w(SX1302_REG_AGC_MCU_CTRL_FORCE_HOST_FE_CTRL, 1);
    fe_rx();
    { int32_t v = 0; lgw_reg_r(rf ? SX1302_REG_AGC_MCU_LUT_TABLE_B_PA_LUT : SX1302_REG_AGC_MCU_LUT_TABLE_A_PA_LUT, &v);
      if (v != 0x04) LOG("warning: FEM control registers not responding (LUT=0x%x) - TX will be weak", (int)v);
      else LOG("SX1302 clocked from radio %c; FEM control OK", clk ? 'B' : 'A'); }

    spi_fd = open("/dev/spidev0.1", O_RDWR);
    uint8_t mode = SPI_MODE_0, bits = 8; uint32_t speed = 2000000;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode); ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits); ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    uint8_t a[2];
    a[0] = 0x00; cmd_w(0x80, a, 1); msleep(10);
    a[0] = 0x7F; cmd_w(0x89, a, 1); msleep(20);
    a[0] = 0x01; cmd_w(0x80, a, 1); msleep(10);
    reg_w(0x0587, 0x00);                                                     /* leave FPGA/IQ mode */
    if (((status_byte() >> 4) & 7) != 3) LOG("warning: radio not in STDBY_XOSC (status 0x%02x)", status_byte());
    radio_apply_config();
    radio_rx();

    int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in sa = { .sin_family = AF_INET, .sin_port = htons(port), .sin_addr.s_addr = INADDR_ANY };
    if (bind(ls, (struct sockaddr *)&sa, sizeof sa) || listen(ls, 1)) { LOG("bind/listen failed: %s", strerror(errno)); return 1; }
    LOG("%s listening on tcp/%d (radio %c)", VERSION, port, rf ? 'B' : 'A');

    uint64_t last_noise = 0;
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
                LOG("client connected");
            }
        }
        if (r > 0 && client >= 0 && FD_ISSET(client, &fs)) {
            int n = read(client, rxbuf + rxlen, sizeof rxbuf - rxlen);
            if (n <= 0) { LOG("client disconnected"); close(client); client = -1; rxlen = 0; }
            else { rxlen += n; parse_client(); }
        }
        poll_rx();
        if (rstate == RS_RX && now_ms() - last_noise > 1000) {   /* slow EMA of idle RSSI as noise floor */
            int rs = rssi_inst(); if (rs < noise_acc + 15) noise_acc = noise_acc * 0.9 + rs * 0.1; last_noise = now_ms();
        }
    }
    LOG("stopping");
    standby();
    lgw_reg_w(SX1302_REG_AGC_MCU_CTRL_FORCE_HOST_FE_CTRL, 0);
    sx1302_radio_host_ctrl(false);
    lgw_disconnect();
    { char c[256]; snprintf(c, sizeof c, "%s stop >/dev/null 2>&1", reset_script); system(c); }
    return 0;
}
