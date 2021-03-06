/*
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * This file is part of srsLTE.
 *
 * srsLTE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsLTE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#ifndef SRSENB_TEST_HELPERS_H
#define SRSENB_TEST_HELPERS_H

#include "srsenb/test/common/dummy_classes.h"
#include <srslte/common/log_filter.h>

using namespace srsenb;
using namespace asn1::rrc;

namespace argparse {

std::string            repository_dir;
srslte::LOG_LEVEL_ENUM log_level;

void usage(char* prog)
{
  printf("Usage: %s [v] -i repository_dir\n", prog);
  printf("\t-v [set srslte_verbose to debug, default none]\n");
}

void parse_args(int argc, char** argv)
{
  int opt;

  while ((opt = getopt(argc, argv, "i")) != -1) {
    switch (opt) {
      case 'i':
        repository_dir = argv[optind];
        break;
      case 'v':
        log_level = srslte::LOG_LEVEL_DEBUG;
        break;
      default:
        usage(argv[0]);
        exit(-1);
    }
  }
  if (repository_dir.empty()) {
    usage(argv[0]);
    exit(-1);
  }
}

} // namespace argparse

namespace test_dummies {

class s1ap_mobility_dummy : public s1ap_dummy
{
public:
  struct ho_req_data {
    uint16_t                     rnti;
    uint32_t                     target_eci;
    srslte::plmn_id_t            target_plmn;
    srslte::unique_byte_buffer_t rrc_container;
  } last_ho_required = {};
  struct enb_status_transfer_info {
    uint16_t                        rnti;
    std::vector<bearer_status_info> bearer_list;
  } last_enb_status = {};

  bool send_ho_required(uint16_t                     rnti,
                        uint32_t                     target_eci,
                        srslte::plmn_id_t            target_plmn,
                        srslte::unique_byte_buffer_t rrc_container) final
  {
    last_ho_required = ho_req_data{rnti, target_eci, target_plmn, std::move(rrc_container)};
    return true;
  }
};

class pdcp_mobility_dummy : public pdcp_dummy
{
public:
  struct last_sdu_t {
    uint16_t                     rnti;
    uint32_t                     lcid;
    srslte::unique_byte_buffer_t sdu;
  } last_sdu;

  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::unique_byte_buffer_t sdu) override
  {
    last_sdu.rnti = rnti;
    last_sdu.lcid = lcid;
    last_sdu.sdu  = std::move(sdu);
  }
};

} // namespace test_dummies

namespace test_helpers {

int parse_default_cfg(rrc_cfg_t* rrc_cfg, srsenb::all_args_t& args)
{
  args                      = {};
  *rrc_cfg                  = {};
  args.enb_files.sib_config = argparse::repository_dir + "/sib.conf.example";
  args.enb_files.rr_config  = argparse::repository_dir + "/rr.conf.example";
  args.enb_files.drb_config = argparse::repository_dir + "/drb.conf.example";
  srslte::logmap::get("TEST")->debug("sib file path=%s\n", args.enb_files.sib_config.c_str());

  args.enb.dl_earfcn = 3400;
  args.enb.n_prb     = 50;
  TESTASSERT(srslte::string_to_mcc("001", &args.stack.s1ap.mcc));
  TESTASSERT(srslte::string_to_mnc("01", &args.stack.s1ap.mnc));
  args.enb.transmission_mode = 1;
  args.enb.nof_ports         = 1;
  args.general.eia_pref_list = "EIA2, EIA1, EIA0";
  args.general.eea_pref_list = "EEA0, EEA2, EEA1";

  phy_cfg_t phy_cfg;

  return enb_conf_sections::parse_cfg_files(&args, rrc_cfg, &phy_cfg);
}

template <typename ASN1Type>
bool unpack_asn1(ASN1Type& asn1obj, const srslte::unique_byte_buffer_t& pdu)
{
  asn1::cbit_ref bref{pdu->msg, pdu->N_bytes};
  if (asn1obj.unpack(bref) != asn1::SRSASN_SUCCESS) {
    srslte::logmap::get("TEST")->error("Failed to unpack ASN1 type\n");
    return false;
  }
  return true;
}

void copy_msg_to_buffer(srslte::unique_byte_buffer_t& pdu, uint8_t* msg, size_t nof_bytes)
{
  srslte::byte_buffer_pool* pool = srslte::byte_buffer_pool::get_instance();
  pdu                            = srslte::allocate_unique_buffer(*pool, true);
  memcpy(pdu->msg, msg, nof_bytes);
  pdu->N_bytes = nof_bytes;
}

int bring_rrc_to_reconf_state(srsenb::rrc& rrc, srslte::timer_handler& timers, uint16_t rnti)
{
  srslte::unique_byte_buffer_t pdu;

  // Send RRCConnectionRequest
  uint8_t rrc_conn_request[] = {0x40, 0x12, 0xf6, 0xfb, 0xe2, 0xc6};
  copy_msg_to_buffer(pdu, rrc_conn_request, sizeof(rrc_conn_request));
  rrc.write_pdu(rnti, 0, std::move(pdu));
  timers.step_all();
  rrc.tti_clock();

  // Send RRCConnectionSetupComplete
  uint8_t rrc_conn_setup_complete[] = {0x20, 0x00, 0x40, 0x2e, 0x90, 0x50, 0x49, 0xe8, 0x06, 0x0e, 0x82, 0xa2,
                                       0x17, 0xec, 0x13, 0xe2, 0x0f, 0x00, 0x02, 0x02, 0x5e, 0xdf, 0x7c, 0x58,
                                       0x05, 0xc0, 0xc0, 0x00, 0x08, 0x04, 0x03, 0xa0, 0x23, 0x23, 0xc0};
  copy_msg_to_buffer(pdu, rrc_conn_setup_complete, sizeof(rrc_conn_setup_complete));
  rrc.write_pdu(rnti, 1, std::move(pdu));
  timers.step_all();
  rrc.tti_clock();

  // S1AP receives InitialContextSetupRequest and forwards it to RRC
  uint8_t s1ap_init_ctxt_setup_req[] = {
      0x00, 0x09, 0x00, 0x80, 0xc6, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x64, 0x00, 0x08, 0x00, 0x02, 0x00,
      0x01, 0x00, 0x42, 0x00, 0x0a, 0x18, 0x3b, 0x9a, 0xca, 0x00, 0x60, 0x3b, 0x9a, 0xca, 0x00, 0x00, 0x18, 0x00, 0x78,
      0x00, 0x00, 0x34, 0x00, 0x73, 0x45, 0x00, 0x09, 0x3c, 0x0f, 0x80, 0x0a, 0x00, 0x21, 0xf0, 0xb7, 0x36, 0x1c, 0x56,
      0x64, 0x27, 0x3e, 0x5b, 0x04, 0xb7, 0x02, 0x07, 0x42, 0x02, 0x3e, 0x06, 0x00, 0x09, 0xf1, 0x07, 0x00, 0x07, 0x00,
      0x37, 0x52, 0x66, 0xc1, 0x01, 0x09, 0x1b, 0x07, 0x74, 0x65, 0x73, 0x74, 0x31, 0x32, 0x33, 0x06, 0x6d, 0x6e, 0x63,
      0x30, 0x37, 0x30, 0x06, 0x6d, 0x63, 0x63, 0x39, 0x30, 0x31, 0x04, 0x67, 0x70, 0x72, 0x73, 0x05, 0x01, 0xc0, 0xa8,
      0x03, 0x02, 0x27, 0x0e, 0x80, 0x80, 0x21, 0x0a, 0x03, 0x00, 0x00, 0x0a, 0x81, 0x06, 0x08, 0x08, 0x08, 0x08, 0x50,
      0x0b, 0xf6, 0x09, 0xf1, 0x07, 0x80, 0x01, 0x01, 0xf6, 0x7e, 0x72, 0x69, 0x13, 0x09, 0xf1, 0x07, 0x00, 0x01, 0x23,
      0x05, 0xf4, 0xf6, 0x7e, 0x72, 0x69, 0x00, 0x6b, 0x00, 0x05, 0x18, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x49, 0x00, 0x20,
      0x45, 0x25, 0xe4, 0x9a, 0x77, 0xc8, 0xd5, 0xcf, 0x26, 0x33, 0x63, 0xeb, 0x5b, 0xb9, 0xc3, 0x43, 0x9b, 0x9e, 0xb3,
      0x86, 0x1f, 0xa8, 0xa7, 0xcf, 0x43, 0x54, 0x07, 0xae, 0x42, 0x2b, 0x63, 0xb9};
  asn1::s1ap::s1ap_pdu_c s1ap_pdu;
  srslte::byte_buffer_t  byte_buf;
  byte_buf.N_bytes = sizeof(s1ap_init_ctxt_setup_req);
  memcpy(byte_buf.msg, s1ap_init_ctxt_setup_req, byte_buf.N_bytes);
  asn1::cbit_ref bref(byte_buf.msg, byte_buf.N_bytes);
  TESTASSERT(s1ap_pdu.unpack(bref) == asn1::SRSASN_SUCCESS);
  rrc.setup_ue_ctxt(rnti, s1ap_pdu.init_msg().value.init_context_setup_request());
  timers.step_all();
  rrc.tti_clock();

  // Send SecurityModeComplete
  uint8_t sec_mode_complete[] = {0x28, 0x00};
  copy_msg_to_buffer(pdu, sec_mode_complete, sizeof(sec_mode_complete));
  rrc.write_pdu(rnti, 1, std::move(pdu));
  timers.step_all();
  rrc.tti_clock();

  // send UE cap info
  uint8_t ue_cap_info[] = {0x38, 0x01, 0x01, 0x0c, 0x98, 0x00, 0x00, 0x18, 0x00, 0x0f,
                           0x30, 0x20, 0x80, 0x00, 0x01, 0x00, 0x0e, 0x01, 0x00, 0x00};
  copy_msg_to_buffer(pdu, ue_cap_info, sizeof(ue_cap_info));
  rrc.write_pdu(rnti, 1, std::move(pdu));
  timers.step_all();
  rrc.tti_clock();

  return SRSLTE_SUCCESS;
}

} // namespace test_helpers

#endif // SRSENB_TEST_HELPERS_H
