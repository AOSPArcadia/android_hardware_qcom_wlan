/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided ?with the distribution.
 *
 * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wifi_hal.h"
#include "nan_i.h"
#include "nancommand.h"

#ifdef WPA_PASN_LIB
#ifdef __cplusplus
extern "C" {
#endif
#include "rsn_supp/pmksa_cache.h"
#ifdef __cplusplus
}
#endif

/*
 * Note: Wi-Fi Aware device can act as PASN initiator with one peer and as
 * PASN responder with other peer, so they maintain separate PMKSA cache
 * for each role. This wrapper functions helps to initialise struct
 * rsn_pmksa_cache which is different for initiator and responder.
 */
struct rsn_pmksa_cache * nan_pairing_initiator_pmksa_cache_init(void)
{
    return pmksa_cache_init(NULL, NULL, NULL, NULL, NULL);
}


void nan_pairing_initiator_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
    return pmksa_cache_deinit(pmksa);
}


int nan_pairing_initiator_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
                                          u8 *bssid, u8 *pmkid)
{
    struct rsn_pmksa_cache_entry *entry;

    entry = pmksa_cache_get(pmksa, bssid, NULL, NULL, NULL, 0);
    if (entry) {
          os_memcpy(pmkid, entry->pmkid, PMKID_LEN);
          return 0;
    }
    return -1;
}


/*  Function to trigger pairing setup or verification request */
wifi_error nan_pairing_request(transaction_id id,
                               wifi_interface_handle iface,
                               NanPairingRequest* msg)
{
    int ret;
    int akmp, cipher;
    unsigned int group = 19;
    u8 pmkid[PMKID_LEN] = {0};
    struct pasn_data *pasn;
    struct wpa_secure_nan *secure_nan;
    NanCommand *nanCommand = NULL;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    struct nan_pairing_peer_info *peer;

    if (!info || !info->secure_nan) {
        ALOGE("%s: Error hal_info NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (!msg || !msg->peer_disc_mac_addr) {
        ALOGE("%s: msg req invalid", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    nanCommand = NanCommand::instance(wifiHandle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    secure_nan = info->secure_nan;
    if (msg->nan_pairing_request_type == NAN_PAIRING_SETUP) {
        peer = nan_pairing_add_peer_to_list(secure_nan,
                                            msg->peer_disc_mac_addr);
        if (peer)
            peer->peer_role = SECURE_NAN_PAIRING_RESPONDER;
    } else {
        peer = nan_pairing_get_peer_from_list(secure_nan,
                                              msg->peer_disc_mac_addr);
    }
    if (!peer) {
        ALOGE("%s: Peer not present", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pasn = &peer->pasn;
    cipher = WPA_CIPHER_CCMP;

    if (memcmp(secure_nan->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN) != 0) {
        memcpy(secure_nan->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN);
        // Update NIRA when src mac address changed
        if (info->secure_nan->dev_nik)
            nan_pairing_set_nik_nira(info->secure_nan);
    }

    memcpy(secure_nan->cluster_addr, nanCommand->getClusterAddr(), NAN_MAC_ADDR_LEN);
    memcpy(pasn->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN);
    memcpy(pasn->bssid, nanCommand->getClusterAddr(), NAN_MAC_ADDR_LEN);
    os_memcpy(pasn->peer_addr, (u8 *)msg->peer_disc_mac_addr, NAN_MAC_ADDR_LEN);

    if (msg->is_opportunistic)
        akmp = WPA_KEY_MGMT_PASN;
    else
        akmp = WPA_KEY_MGMT_SAE;

    pasn->derive_kdk = true;
    pasn->kdk_len = WPA_KDK_MAX_LEN;
    pasn->pmksa = (struct rsn_pmksa_cache *)secure_nan->initiator_pmksa;
    peer->peer_role = SECURE_NAN_PAIRING_RESPONDER;

    ALOGI("%s: src_addr=" MACSTR ",addr=" MACSTR ",akmp=%d type=%d auth=%d supported bootstrap = %d",
           __FUNCTION__,
           MAC2STR(nanCommand->getNmi()), MAC2STR(msg->peer_disc_mac_addr),
           akmp, msg->nan_pairing_request_type, msg->is_opportunistic,
           secure_nan->supported_bootstrap);

    if (akmp == WPA_KEY_MGMT_SAE)
        pasn->rsnxe_capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);

    if (msg->nan_pairing_request_type == NAN_PAIRING_SETUP) {
        if (!msg->is_opportunistic)
            nan_pairing_set_password(peer,
                                     msg->key_info.body.passphrase_info.passphrase,
                                     msg->key_info.body.passphrase_info.passphrase_len);

        // construct wrapped data for dcea, csia, npba
        nan_pairing_add_setup_ies(secure_nan, pasn, peer->peer_role);
        ret = wpas_pasn_start(pasn, nanCommand->getNmi(), msg->peer_disc_mac_addr,
                              nanCommand->getClusterAddr(), akmp,
                              cipher, group, 0, NULL, 0, NULL, 0, NULL);
        if (ret) {
            ALOGE("wpas_pasn_start failed, ret = %d", ret);
            return WIFI_ERROR_UNKNOWN;
        }
    } else if (msg->nan_pairing_request_type == NAN_PAIRING_VERIFICATION) {
        // construct wrapped data for csia, nira
        nan_pairing_add_verification_ies(secure_nan, pasn, peer->peer_role);

        if (!secure_nan->dev_nik) {
            ALOGE("%s: Device NIK NULL", __FUNCTION__);
            return WIFI_ERROR_UNKNOWN;
        }
        os_memcpy(pmkid, secure_nan->dev_nik->nira_nonce,
                  secure_nan->dev_nik->nira_nonce_len);
        os_memcpy(&pmkid[secure_nan->dev_nik->nira_nonce_len],
                  secure_nan->dev_nik->nira_tag,
                  secure_nan->dev_nik->nira_tag_len);
        pasn->custom_pmkid_valid = true;
        os_memcpy(pasn->custom_pmkid, pmkid, PMKID_LEN);

        ret = wpa_pasn_verify(pasn, nanCommand->getNmi(), msg->peer_disc_mac_addr,
                              nanCommand->getClusterAddr(), akmp,
                              cipher, group, 0, NULL, 0, NULL, 0, NULL);
        if (ret) {
            ALOGE("wpas_pasn_verify failed, ret = %d", ret);
            return WIFI_ERROR_UNKNOWN;
        }
    }
    return WIFI_SUCCESS;
}

#else  /* WPA_PASN_LIB */

wifi_error nan_pairing_request(transaction_id id,
                               wifi_interface_handle iface,
                               NanPairingRequest* msg)
{
    ALOGE(" nan pairing not supported");
    return WIFI_ERROR_NOT_SUPPORTED;
}
#endif /* WPA_PASN_LIB */
