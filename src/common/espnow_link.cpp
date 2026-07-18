#include "espnow_link.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "config.h"
#include "protocol.h"

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool g_isBoat = false;

// Pairing state. The receive callback runs on the WiFi task, so it only copies
// the candidate MAC and raises a flag; espnowLinkService() (Arduino loop task)
// does the esp_now_add_peer / ACK send.
static volatile bool g_paired = false;
static uint8_t g_peerMac[6] = {0};
static volatile bool g_pairPending = false;
static uint8_t g_pendingMac[6] = {0};

static volatile uint32_t g_lastRxMs = 0;

// Boat: latest control values
static volatile int16_t g_ctrlX = 0, g_ctrlY = 0;
static volatile bool g_ctrlDirty = false;
static volatile uint32_t g_lastControlMs = 0;
static volatile uint8_t g_lastCtrlSeq = 0;
static volatile bool g_haveCtrlSeq = false;
static volatile uint16_t g_ctrlRecv = 0, g_ctrlLost = 0;
static volatile int8_t g_lastRxRssi = 0;

// TX: latest telemetry values
static volatile int8_t g_telemRssi = 0;
static volatile uint8_t g_telemLoss = 0;
static volatile bool g_telemDirty = false;

static uint8_t g_txSeq = 0;

static bool macEquals(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

static void requestPair(const uint8_t* mac) {
    memcpy(g_pendingMac, mac, 6);
    g_pairPending = true;
}

// ── Receive path ─────────────────────────────────────────────────────────────

static void handlePacket(const uint8_t* mac, const uint8_t* data, int len) {
    if (len < (int)sizeof(DiscoveryPacket)) return;
    const PacketHeader* hdr = (const PacketHeader*)data;
    if (hdr->magic != PROTO_MAGIC || hdr->version != PROTO_VERSION) return;
    if (proto_crc8(data, len - 1) != data[len - 1]) return;

    switch (hdr->type) {
        case PKT_CONTROL: {
            if (!g_isBoat || len != sizeof(ControlPacket)) return;
            const ControlPacket* p = (const ControlPacket*)data;
            // Implicit pairing: control from an unknown MAC means the boat
            // rebooted mid-session (peer table lost) or discovery ACK was lost.
            if (!g_paired || !macEquals(mac, g_peerMac)) requestPair(mac);
            if (g_haveCtrlSeq) g_ctrlLost += (uint8_t)(hdr->seq - g_lastCtrlSeq - 1);
            g_lastCtrlSeq = hdr->seq;
            g_haveCtrlSeq = true;
            g_ctrlRecv++;
            g_ctrlX = p->x;
            g_ctrlY = p->y;
            g_ctrlDirty = true;
            g_lastControlMs = millis();
            g_lastRxMs = g_lastControlMs;
            break;
        }
        case PKT_TELEMETRY: {
            if (g_isBoat || len != sizeof(TelemetryPacket)) return;
            if (!g_paired || !macEquals(mac, g_peerMac)) return;
            const TelemetryPacket* p = (const TelemetryPacket*)data;
            g_telemRssi = p->rssi;
            g_telemLoss = p->lossPct;
            g_telemDirty = true;
            g_lastRxMs = millis();
            break;
        }
        case PKT_DISCOVER:
            if (!g_isBoat || len != sizeof(DiscoveryPacket)) return;
            requestPair(mac);
            break;
        case PKT_DISCOVER_ACK:
            if (g_isBoat || len != sizeof(DiscoveryPacket)) return;
            requestPair(mac);
            g_lastRxMs = millis();
            break;
    }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (info->rx_ctrl) g_lastRxRssi = info->rx_ctrl->rssi;
    handlePacket(info->src_addr, data, len);
}
#else
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
    handlePacket(mac, data, len);
}

// Core 2.x esp_now recv callback carries no RSSI, so the boat snoops it from
// promiscuous mode: ESP-NOW frames are 802.11 action (mgmt) frames; the source
// MAC sits at payload bytes 10..15. Cosmetic path only (signal bars).
static void promiscuousRx(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* p = (const wifi_promiscuous_pkt_t*)buf;
    if (g_paired && memcmp(p->payload + 10, g_peerMac, 6) == 0)
        g_lastRxRssi = (int8_t)p->rx_ctrl.rssi;
}
#endif

// ── Send helpers ─────────────────────────────────────────────────────────────

static void fillHeader(PacketHeader* hdr, uint8_t type) {
    hdr->magic = PROTO_MAGIC;
    hdr->version = PROTO_VERSION;
    hdr->type = type;
    hdr->seq = ++g_txSeq;
}

static bool sendPacket(const uint8_t* mac, void* pkt, size_t len) {
    uint8_t* bytes = (uint8_t*)pkt;
    bytes[len - 1] = proto_crc8(bytes, len - 1);
    return esp_now_send(mac, bytes, len) == ESP_OK;
}

void espnowLinkSendDiscover() {
    DiscoveryPacket p;
    fillHeader(&p.hdr, PKT_DISCOVER);
    sendPacket(BROADCAST_MAC, &p, sizeof(p));
}

bool espnowLinkSendControl(int16_t x, int16_t y) {
    if (!g_paired) return false;
    ControlPacket p;
    fillHeader(&p.hdr, PKT_CONTROL);
    p.x = x;
    p.y = y;
    return sendPacket(g_peerMac, &p, sizeof(p));
}

bool espnowLinkSendTelemetry(int8_t rssi, uint8_t lossPct) {
    if (!g_paired) return false;
    TelemetryPacket p;
    fillHeader(&p.hdr, PKT_TELEMETRY);
    p.rssi = rssi;
    p.lossPct = lossPct;
    return sendPacket(g_peerMac, &p, sizeof(p));
}

// ── Pairing / lifecycle ──────────────────────────────────────────────────────

void espnowLinkService() {
    if (!g_pairPending) return;
    uint8_t mac[6];
    memcpy(mac, g_pendingMac, 6);
    g_pairPending = false;

    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = 0;   // current channel
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        if (esp_now_add_peer(&peer) != ESP_OK) {
            Serial.println("Link: esp_now_add_peer failed");
            return;
        }
    }
    bool wasPaired = g_paired && macEquals(mac, g_peerMac);
    memcpy(g_peerMac, mac, 6);
    g_paired = true;
    if (!wasPaired)
        Serial.printf("Link: paired with %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (g_isBoat) {
        DiscoveryPacket p;
        fillHeader(&p.hdr, PKT_DISCOVER_ACK);
        sendPacket(g_peerMac, &p, sizeof(p));
    }
}

bool espnowLinkPaired() { return g_paired; }

void espnowLinkResetPairing() {
    if (g_paired) esp_now_del_peer(g_peerMac);
    g_paired = false;
    g_telemDirty = false;
}

uint32_t espnowLinkLastRxMs() { return g_lastRxMs; }

// ── State accessors ──────────────────────────────────────────────────────────

bool espnowLinkGetControl(int16_t& x, int16_t& y) {
    x = g_ctrlX;
    y = g_ctrlY;
    bool fresh = g_ctrlDirty;
    g_ctrlDirty = false;
    return fresh;
}

uint32_t espnowLinkLastControlMs() { return g_lastControlMs; }

bool espnowLinkGetTelemetry(int8_t& rssi, uint8_t& lossPct) {
    rssi = g_telemRssi;
    lossPct = g_telemLoss;
    bool fresh = g_telemDirty;
    g_telemDirty = false;
    return fresh;
}

int8_t espnowLinkLastRxRssi() { return g_lastRxRssi; }

uint8_t espnowLinkTakeLossPct() {
    uint16_t recv = g_ctrlRecv, lost = g_ctrlLost;
    g_ctrlRecv = 0;
    g_ctrlLost = 0;
    uint16_t total = recv + lost;
    return total ? (uint8_t)((lost * 100) / total) : 0;
}

// ── Init ─────────────────────────────────────────────────────────────────────

void espnowLinkInit(bool isBoat) {
    g_isBoat = isBoat;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // TX runs BLE + WiFi on the C3's single radio: modem-sleep is required for
    // coexistence. The boat is WiFi-only, so take the lowest-latency setting.
    esp_wifi_set_ps(isBoat ? WIFI_PS_NONE : WIFI_PS_MIN_MODEM);
#if ESPNOW_USE_LR
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));
#endif
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(onRecv);

    // Broadcast peer is needed to send discovery packets.
    esp_now_peer_info_t bc = {};
    memcpy(bc.peer_addr, BROADCAST_MAC, 6);
    bc.channel = 0;
    bc.ifidx = WIFI_IF_STA;
    bc.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&bc));

#if ESP_ARDUINO_VERSION_MAJOR < 3
    if (isBoat) {
        static const wifi_promiscuous_filter_t filt = {
            .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT
        };
        esp_wifi_set_promiscuous_filter(&filt);
        esp_wifi_set_promiscuous_rx_cb(&promiscuousRx);
        esp_wifi_set_promiscuous(true);
    }
#endif

    Serial.printf("Link: %s ready, MAC %s, channel %d, protocol %s\n",
                  isBoat ? "boat" : "transmitter",
                  WiFi.macAddress().c_str(), ESPNOW_CHANNEL,
                  ESPNOW_USE_LR ? "LR" : "802.11bgn");
}
