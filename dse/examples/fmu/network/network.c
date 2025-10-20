// Copyright 2025 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <dse/fmu/fmu.h>
#include <dse/ncodec/codec.h>
#include <dse/ncodec/interface/pdu.h>

typedef struct {
    double  counter;
    NCODEC* pdu_rx;
    NCODEC* pdu_tx;
} VarTable;

FmuInstanceData* fmu_create(FmuInstanceData* fmu)
{
    VarTable* v = malloc(sizeof(VarTable));
    *v = (VarTable){
        .counter = fmu_register_var(fmu, 1, false, offsetof(VarTable, counter)),
        .pdu_rx = fmu_lookup_ncodec(fmu, 2, true),
        .pdu_tx = fmu_lookup_ncodec(fmu, 3, false),
    };
    fmu_register_var_table(fmu, v);

    if (v->pdu_rx == NULL) {
        fmu_log(fmu, FmiLogError, "Error", "PDU RX not configured (VR 2)");
    }
    if (v->pdu_tx == NULL) {
        fmu_log(fmu, FmiLogError, "Error", "PDU TX not configured (VR 3)");
    }
    return NULL;
}

int fmu_init(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

int fmu_step(FmuInstanceData* fmu, double CommunicationPoint, double stepSize)
{
    UNUSED(CommunicationPoint);
    UNUSED(stepSize);
    VarTable* v = fmu_var_table(fmu);

    /* Consume PDUs from the network. */
    while (1) {
        NCodecPdu pdu = {};
        int       len = ncodec_read(v->pdu_rx, &pdu);
        if (len < 0) break;
        fmu_log(fmu, FmiLogOk, "Debug", "RX (%08x): %s", pdu.id, pdu.payload);
    }

    /* Increment the counter. */
    v->counter += 1;

    /* Send a PDU over the network. */
    char msg[100] = {};
    snprintf(msg, sizeof(msg), "Counter is %d", (int)v->counter);
    struct NCodecPdu tx_msg = {
        .id = v->counter + 1000, /* Simple frame_id: 1001, 1002 ... */
        .payload = (uint8_t*)msg,
        .payload_len = strlen(msg) + 1, /* Capture the NULL terminator. */
        .swc_id = 42,  /* Set swc_id to bypass Rx filtering. */
        // Transport: IP
        .transport_type = NCodecPduTransportTypeIp,
        .transport.ip_message = {
            // Ethernet
            .eth_dst_mac = 0x0000123456789ABC,
            .eth_src_mac = 0x0000CBA987654321,
            .eth_ethertype = 1,
            .eth_tci_pcp = 2,
            .eth_tci_dei = 3,
            .eth_tci_vid = 4,
            // IP: IPv6
            .ip_protocol = NCodecPduIpProtocolUdp,
            .ip_addr_type = NCodecPduIpAddrIPv6,
            .ip_addr.ip_v6 = {
                .src_addr = {1, 2, 3, 4, 5, 6, 7, 8},
                .dst_addr = {2, 2, 4, 4, 6, 6, 8, 8},
            },
            .ip_src_port = 4242,
            .ip_dst_port = 2424,
            // Socket Adapter: DoIP
            .so_ad_type = NCodecPduSoAdDoIP,
            .so_ad.do_ip = {
                .protocol_version = 1,
                .payload_type = 2,
            },
        },
    };
    ncodec_write(v->pdu_tx, &tx_msg);
    ncodec_flush(v->pdu_tx);

    return 0;
}

int fmu_destroy(FmuInstanceData* fmu)
{
    UNUSED(fmu);
    return 0;
}

void fmu_reset_binary_signals(FmuInstanceData* fmu)
{
    UNUSED(fmu);
}
