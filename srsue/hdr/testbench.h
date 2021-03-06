#ifndef SRSUE_TESTBENCH_H
#define SRSUE_TESTBENCH_H

#include "srslte/common/log_filter.h"
#include "srslte/interfaces/ue_interfaces.h"
#include "srslte/common/security.h"
#include <bitset>
#include <mutex>
#include <map>
#include <string>

namespace srsue {

typedef struct summary_s {
  // TODO PCAPs and alike for easy summary printing

  // insecure ciphers from SMC: either NULL, or SPARE (due to bugs in BS implementation)
  bool insecure_nas_eea_choice = false;
  bool insecure_nas_eia_choice = false;
  bool insecure_rrc_eea_choice = false;
  bool insecure_rrc_eia_choice = false;
  bool rrc_sec_cap_mismatch = false;
  bool nas_sec_cap_mismatch = false;
  bool spare_values = false;
  // just any kind of reaction from the network?
  bool success = false;
  bool is_interesting = false;
} summary_t;

/* testbench: collect the results of all testcases
 */
class testbench : public testbench_interface_stack, public testbench_interface_nas, public testbench_interface_rrc
{
protected:
  /* testcase: results of an individual testcase
   */
  class testcase
  {
    uint id;
    /* starts with ExA0. indices 4 to 7 are spare bits */
    bool eia_caps[8] = {};
    bool eea_caps[8] = {};

    uint8_t nas_eia = 0;
    uint8_t nas_eea = 0;
    uint8_t rrc_eia = 0;
    uint8_t rrc_eea = 0;

    uint8_t k_rrc_enc[32] = {};
    uint8_t k_rrc_int[32] = {};
    uint8_t k_up_enc[32]  = {};

    std::string nas_pcap;
    std::string mac_pcap;

    srslte::log_filter* log;

    summary_t summary;
    void update_summary();

    /* NAS */
    bool got_nas_security_mode_command = false;
    bool got_attach_accept             = false;
    bool got_attach_reject             = false;

    /* RRC */
    bool got_rrc_security_mode_command = false;

  public:
    testcase(srslte::log_filter* _log, uint _id, uint8_t _eia_mask, uint8_t _eea_mask);

    std::string get_summary();

    void set_pcap(std::string nas_pcap, std::string mac_pcap);

    /* helpers */
    std::string mme_cause_str(uint cause);

    /* stack */
    bool is_finished();
    bool is_interesting();
    bool is_connected();

    /* NAS */
    void report_nas();
    void report_attach_accept();
    void report_attach_reject(uint8_t _cause);
    void report_nas_security_mode_command(uint8_t eia, uint8_t eea);

    /* RRC */
    void report_rrc_key(key_type _type, uint8_t* _k);
    void report_rrc_security_mode_command(uint8_t _eia, uint8_t _eea);
  };

  uint                                        current_testcase_id;
  std::map<uint /* testcase id */, testcase*> testcases;
  std::mutex testcases_mutex;

  srslte::log_filter* log;

public:
  testbench(srslte::log_filter* _log);
  ~testbench();

  /* registers new testcase; returns testcase ID */
  uint start_testcase(uint8_t _eia_mask, uint8_t _eea_mask);
  bool set_pcap(std::string nas_pcap, std::string mac_pcap);

  std::string get_summary();
  std::string get_current_tc_summary();

  /* stack / main() interface */
  bool get_result();
  bool is_finished();
  bool is_connected();

  /* NAS interface */
  void report_nas();
  void report_attach_accept();
  void report_attach_reject(uint8_t _cause);
  void report_nas_security_mode_command(uint8_t eia, uint8_t eea);

  /* RRC interface */
  void report_rrc_key(key_type _type, uint8_t* _k);
  void report_rrc_security_mode_command(uint8_t _eia, uint8_t _eea);
};

} // namespace srsue

#endif // SRSUE_TESTBENCH_H