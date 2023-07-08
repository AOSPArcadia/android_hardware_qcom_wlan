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

#ifdef WPA_PASN_LIB

#include "nan_i.h"
#include "nancommand.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "ap/pmksa_cache_auth.h"
#ifdef __cplusplus
}
#endif

/*
 * Note: Wi-Fi Aware device can act as PASN initiator with one peer and as
 * PASN responder with other peer, so they maintain separate PMKSA cache
 * for each role. This wrapper functions helps to initialise struct
 * rsn_pmksa_cache which is different for initiator and responder.
 */
struct rsn_pmksa_cache * nan_pairing_responder_pmksa_cache_init(void)
{
   return pmksa_cache_auth_init(NULL, NULL);
}


void nan_pairing_responder_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
   return pmksa_cache_auth_deinit(pmksa);
}
#endif /* WPA_PASN_LIB */
