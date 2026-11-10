#pragma once

#include <cmath>
#include "slave_external_change.h"
#include "slave_esp_calibration.h"
#include "slave_central_control_address.h"
#include "slave_pipe_temp_poll.h"
#include "slave_pipe_temp_discovery.h"
#include "slave_type_a_control_preflight.h"
#include "lg_capability_refresh.h"
#include "lg_sleep_reservation.h"
#include "lg_slave_sleep_sync.h"
#include "lg_master_sleep.h"
#include "floor_console_decode.h"
#include "floor_console_wire.h"
#include "lg_vane_mapping.h"
#include "lg_filter_time.h"
#include "floor_console_flow.h"

#include "esphome.h"
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#include <cstring>
#include <functional>
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"

static const char* const TAG = "lg-controller";

namespace esphome::lg_controller {

static constexpr size_t MIN_TEMP_SETPOINT = 16;
static constexpr size_t MAX_TEMP_SETPOINT = 30;

bool float_equal(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}

class LgSwitch final : public switch_::Switch {
public:
    // Only optional controls with a protocol-specific preflight use this.
    // Reporting a genuine indoor state via publish_state() is not gated.
    void set_write_guard(std::function<bool(bool)> guard) { write_guard_ = guard; }
    // For commands that need genuine LG readback, do not optimistically
    // publish the requested state. Only publish_state() on observed confirmation.
    void set_request_handler(std::function<void(bool)> handler) { request_handler_ = handler; }

    void restore_and_set_mode(switch_::SwitchRestoreMode mode) {
        set_restore_mode(mode);
        if (auto state = get_initial_state_with_restore_mode()) {
            write_state(*state);
        }
    }

protected:
    void write_state(bool value) override {
        if (write_guard_ && !write_guard_(value)) return;
        if (request_handler_) { request_handler_(value); return; }
        publish_state(value);
    }

private:
    std::function<bool(bool)> write_guard_;
    std::function<void(bool)> request_handler_;
};

class LgSelect final : public select::Select {
public:
    void set_control_guard(std::function<bool()> guard) { control_guard_ = guard; }
    void set_request_handler(std::function<void(const std::string &)> handler) {
        request_handler_ = handler;
    }

protected:
    void control(const std::string& value) override {
        if (this->current_option() != value) {
            if (control_guard_ && !control_guard_()) return;
            if (request_handler_) { request_handler_(value); return; }
            this->publish_state(value);
        }
    }

private:
    std::function<bool()> control_guard_;
    std::function<void(const std::string &)> request_handler_;
};

class LgNumber final : public number::Number {
    void control(float value) override {
        if (!float_equal(this->state, value)) {
            this->publish_state(value); 
        }
    }
};

// The LG protocol always uses Celsius. The HA/ESPHome climate component internally
// converts between Fahrenheit and Celsius. Values from the Home Assistant room temperature sensor
// are not converted automatically so can be Celsius or Fahrenheit.
//
// Unfortunately LG uses their own Fahrenheit/Celsius mapping that's different from what you'd
// expect. For example, 78F is ~25.5C, but LG controllers will send 26C for 78F. A value of 25.5C
// would be interpreted by the AC as 77F.
//
// This class has some functions to convert between Fahrenheit, Celsius and "LG-Celsius" (values
// we send to or receive from the unit). This ensures Home Assistant and the LG unit always agree
// on the setpoint in Fahrenheit.
//
// These conversions are only used in Fahrenheit mode.
class TempConversion {
private:
    static constexpr int8_t FahToLGCel[] = {
        0  /* 32  */, 1  /* 33  */, 2  /* 34  */, 3  /* 35  */, 4  /* 36  */,
        5  /* 37  */, 6  /* 38  */, 7  /* 39  */, 8  /* 40  */, 10 /* 41  */,
        12 /* 42  */, 13 /* 43  */, 14 /* 44  */, 15 /* 45  */, 16 /* 46  */,
        17 /* 47  */, 18 /* 48  */, 19 /* 49  */, 20 /* 50  */, 21 /* 51  */,
        22 /* 52  */, 23 /* 53  */, 24 /* 54  */, 25 /* 55  */, 26 /* 56  */,
        27 /* 57  */, 28 /* 58  */, 30 /* 59  */, 32 /* 60  */, 33 /* 61  */,
        34 /* 62  */, 35 /* 63  */, 36 /* 64  */, 37 /* 65  */, 38 /* 66  */,
        39 /* 67  */, 40 /* 68  */, 41 /* 69  */, 42 /* 70  */, 43 /* 71  */,
        44 /* 72  */, 45 /* 73  */, 46 /* 74  */, 47 /* 75  */, 48 /* 76  */,
        50 /* 77  */, 52 /* 78  */, 53 /* 79  */, 54 /* 80  */, 55 /* 81  */,
        56 /* 82  */, 57 /* 83  */, 58 /* 84  */, 59 /* 85  */, 60 /* 86  */,
        61 /* 87  */, 62 /* 88  */, 63 /* 89  */, 64 /* 90  */, 65 /* 91  */,
        66 /* 92  */, 67 /* 93  */, 68 /* 94  */, 70 /* 95  */, 72 /* 96  */,
        73 /* 97  */, 74 /* 98  */, 75 /* 99  */, 76 /* 100 */, 77 /* 101 */,
        78 /* 102 */, 79 /* 103 */, 80 /* 104 */
    };
    static constexpr int8_t LGCelToCelAdjustment[] = {
         0 /* 0    */,  0 /* 0.5  */,  0 /* 1.0  */,  0 /* 1.5  */,  0 /* 2.0  */,
         1 /* 2.5  */,  1 /* 3.0  */,  1 /* 3.5  */,  1 /* 4.0  */,  0 /* 4.5  */,
         0 /* 5.0  */, -1 /* 5.5  */, -1 /* 6.0  */, -1 /* 6.5  */, -1 /* 7.0  */,
        -1 /* 7.5  */,  0 /* 8.0  */,  0 /* 8.5  */,  0 /* 9.0  */,  0 /* 9.5  */,
         0 /* 10.0 */,  0 /* 10.5 */,  0 /* 11.0 */,  0 /* 11.5 */,  0 /* 12.0 */,
         1 /* 12.5 */,  1 /* 13.0 */,  1 /* 13.5 */,  1 /* 14.0 */,  0 /* 14.5 */,
         0 /* 15.0 */, -1 /* 15.5 */, -1 /* 16.0 */, -1 /* 16.5 */, -1 /* 17.0 */,
        -1 /* 17.5 */,  0 /* 18.0 */,  0 /* 18.5 */,  0 /* 19.0 */,  0 /* 19.5 */,
         0 /* 20.0 */,  0 /* 20.5 */,  0 /* 21.0 */,  0 /* 21.5 */,  0 /* 22.0 */,
         1 /* 22.5 */,  1 /* 23.0 */,  1 /* 23.5 */,  1 /* 24.0 */,  0 /* 24.5 */,
         0 /* 25.0 */, -1 /* 25.5 */, -1 /* 26.0 */, -1 /* 26.5 */, -1 /* 27.0 */,
        -1 /* 27.5 */,  0 /* 28.0 */,  0 /* 28.5 */,  0 /* 29.0 */,  0 /* 29.5 */,
         0 /* 30.0 */,  0 /* 30.5 */,  0 /* 31.0 */,  0 /* 31.5 */,  0 /* 32.0 */,
         1 /* 32.5 */,  1 /* 33.0 */,  1 /* 33.5 */,  1 /* 34.0 */,  0 /* 34.5 */,
         0 /* 35.0 */, -1 /* 35.5 */, -1 /* 36.0 */, -1 /* 36.5 */, -1 /* 37.0 */,
        -1 /* 37.5 */,  0 /* 38.0 */,  0 /* 38.5 */,  0 /* 39.0 */,  0 /* 39.5 */,
         0 /* 40.0 */
    };

public:
    // Convert from Fahrenheit to LG-Celsius (using the LG-compatible conversion).
    static float fahrenheit_to_lgcelsius(float temp) {
        int temp_int = int(round(temp));
        if (temp_int < 32 || temp_int > 104) {
            return esphome::fahrenheit_to_celsius(temp);
        }
        int8_t val = FahToLGCel[temp_int - 32];
        return float(val / 2) + ((val & 1) ? 0.5f : 0.0f);
    }
    // Convert an LG-Celsius value to Celsius. This is done to ensure the LG unit and HA agree
    // on the value in Fahrenheit. For example, the unit sends 78F as 26C (LG Celsius), but HA
    // would convert this to 78.8F => 79F. To work around this, we adjust 26C to 25.5C because
    // this maps to 78F in HA.
    static float lgcelsius_to_celsius(float temp) {
        int index = int(temp * 2);
        if (index < 0 || index >= sizeof(LGCelToCelAdjustment)) {
            return temp;
        }
        int8_t adjustment = LGCelToCelAdjustment[index];
        if (adjustment == -1) {
            return temp - 0.5;
        }
        if (adjustment == 1) {
            return temp + 0.5;
        }
        return temp;
    }
    static float celsius_to_lgcelsius(float temp) {
        float fahrenheit = esphome::celsius_to_fahrenheit(temp);
        return fahrenheit_to_lgcelsius(fahrenheit);
    }
};
constexpr int8_t TempConversion::FahToLGCel[];
constexpr int8_t TempConversion::LGCelToCelAdjustment[];

class LgController final : public climate::Climate, public uart::UARTDevice, public Component {
    static constexpr size_t MsgLen = 13;

    climate::ClimateTraits supported_traits_{};

    InternalGPIOPin& rx_pin_;
    esphome::sensor::Sensor* temperature_sensor_;

    LgSelect& vane_select_1_;
    LgSelect& vane_select_2_;
    LgSelect& vane_select_3_;
    LgSelect& vane_select_4_;
    LgSelect& overheating_select_;

    LgNumber& fan_speed_slow_;
    LgNumber& fan_speed_low_;
    LgNumber& fan_speed_medium_;
    LgNumber& fan_speed_high_;

    LgNumber& sleep_timer_;

    esphome::sensor::Sensor& error_code_;
    esphome::sensor::Sensor& pipe_temp_in_;
    esphome::sensor::Sensor& pipe_temp_mid_;
    esphome::sensor::Sensor& pipe_temp_out_;
    esphome::sensor::Sensor* power_consumption_;
    esphome::sensor::Sensor* current_power_;
    esphome::sensor::Sensor* humidity_;
    esphome::sensor::Sensor* fan_operation_time_;
    esphome::sensor::Sensor* indoor_unit_operation_time_;
    esphome::sensor::Sensor* precise_room_temperature_;
    LgSelect* overcooling_select_ = nullptr;
    LgSelect* cooling_thermal_off_fan_select_ = nullptr;
    uint8_t cooling_thermal_off_fan_ = 0;
    LgSelect* temperature_control_step_select_ = nullptr;
    uint8_t temperature_control_step_ = 0;
    float target_temperature_step_ = 0.5f;
    esphome::sensor::Sensor* static_pressure_step_ = nullptr;
    LgSwitch* continuous_fan_switch_ = nullptr;
    LgSwitch* temperature_based_fan_auto_switch_ = nullptr;
    esphome::text_sensor::TextSensor* advanced_settings_raw_ = nullptr;
    esphome::text_sensor::TextSensor* extended_settings_raw_ = nullptr;

    // Optional floor console controls; distinct from read-only monitoring.
    bool floor_console_monitoring_ = false;
    bool floor_console_features_ = false;
    bool floor_console_reversed_airflow_ = false;
    bool floor_console_passive_readback_ = false; // V2.0.2 passive experiment (disable for this test)
    bool floor_console_selective_cd_probe_ = false; // opt-in, unverified Type-D 0x80 wire experiment
    // V2.0.4: optional fast discharge writes with one delayed, coalesced
    // genuine CD verification. Local echo != acceptance; the UI is optimistic.
    bool floor_console_fast_discharge_ = false;
    bool floor_fast_verify_pending_ = false;
    bool floor_fast_verify_sent_ = false;
    bool floor_fast_verify_echo_seen_ = false;
    uint8_t floor_fast_verify_target_ = 0;
    lg_floor_console::FastCDVerifier floor_fast_cd_verifier_;
    uint32_t floor_fast_verify_queued_at_ = 0;
    uint32_t floor_fast_verify_sent_at_ = 0;
    sensor::Sensor *filter_time_remaining_ = nullptr;
    sensor::Sensor *filter_remaining_percent_ = nullptr;
    uint16_t filter_nominal_life_hours_ = 0;
    LgFilterTimeRemaining filter_time_remaining_decoder_;

    uint8_t vane_option_to_wire_(size_t i) const {
        return LgVaneMapping::option_to_wire(static_cast<uint8_t>(i), floor_console_reversed_airflow_);
    }
    uint8_t vane_wire_to_option_(uint8_t raw) const {
        return LgVaneMapping::wire_to_option(raw, floor_console_reversed_airflow_);
    }
    LgSwitch *floor_console_comfort_control_ = nullptr;
    LgSelect *floor_console_direction_control_ = nullptr;
    LgSelect *floor_console_step_control_ = nullptr;
    text_sensor::TextSensor *floor_console_command_status_ = nullptr;

    // Distinct source templates for pages 4/5 and page-6 subpage 0x10.
    // [page index 0..2][0: Master, 1: indoor], no speculative zero frames.
    uint8_t floor_console_templates_[3][2][MsgLen] = {};
    uint32_t floor_console_template_at_[3][2] = {};
    bool floor_console_template_valid_[3][2] = {};
    struct FloorConsoleTransaction {
        bool active = false;
        lg_floor_console::Page page = lg_floor_console::Page::Comfort;
        uint8_t target = 0;
        uint8_t transmissions = 0;
        uint8_t master_matches = 0;
        uint8_t indoor_matches = 0;
        uint32_t requested_at = 0;
        uint32_t transmitted_at = 0;
        bool echo_seen = false;
        bool superseded = false;  // newest HA selection replaces this request
        bool readback_queued = false;
        uint32_t readback_queued_at = 0;
        uint8_t readback_attempts = 0; // at most once for each 2D/2C/2E write
        uint32_t readback_sent_at = 0;
        uint8_t template_src = 0; // 0 genuine Master, 1 genuine indoor unit
        bool awaiting_template = false;
        uint8_t template_refresh_attempts = 0;
        uint32_t template_refresh_sent_at = 0;
        uint32_t template_wait_last_log_at = 0;
        bool template_refresh_echo_seen = false;
        // One candidate per requested discharge write. Not a normal settings
        // retry; a failed probe falls back to the proven full 28 readback.
        uint8_t last_wire_write[MsgLen] = {};
        bool selective_probe_attempted = false;
        bool selective_probe_fallback_ready = false;
        uint32_t selective_probe_sent_at = 0;
    } floor_console_tx_;
    struct FloorConsoleDeferredRequest {
        bool valid = false;
        lg_floor_console::Page page = lg_floor_console::Page::Discharge;
        uint8_t target = 0;
        uint32_t queued_at = 0;
    } floor_console_deferred_;
    bool floor_console_send_queued_ = false;
    uint32_t floor_console_last_finish_at_ = 0;
    static constexpr uint32_t FLOOR_TEMPLATE_MAX_AGE = 30u * 60u * 1000u;
    // Page-5/6 configuration can be sent only once on the indoor unit's
    // startup burst. Accept an observed indoor template for up to six hours
    // within this boot; we never persist raw frames across ESP32 reboots.
    static constexpr uint32_t FLOOR_INDOOR_STATIC_MAX_AGE = 6u * 60u * 60u * 1000u;
    // A genuine 28 settings refresh can take 20-60s before the indoor CD/CE.10
    // arrives. Wait for readback before the second (and final) write.
    static constexpr uint32_t FLOOR_READBACK_SETTLE_MS = 2500u;
    static constexpr uint32_t FLOOR_PASSIVE_DISCHARGE_WINDOW_MS = 12000u;
    static constexpr uint32_t FLOOR_SELECTIVE_CD_TIMEOUT_MS = 10000u;
    // Wait until the user has stopped choosing outlets before flooding the
    // 100-baud bus with a full-settings readback. At most one query per burst.
    static constexpr uint32_t FLOOR_FAST_QUIET_MS = 45000u;
    static constexpr uint32_t FLOOR_FAST_VERIFY_TIMEOUT_MS = 90000u;
    static constexpr uint32_t FLOOR_FAST_VERIFY_MAX_PENDING_MS = 180000u;
    static constexpr uint32_t FLOOR_READBACK_TIMEOUT_MS = 60000u;
    static constexpr uint32_t FLOOR_READBACK_GUARD_WAIT_MS = 20000u;
    static constexpr uint32_t FLOOR_TX_REPEAT_MS = 65000u;
    static constexpr uint32_t FLOOR_TX_DEADLINE_MS = 185000u;
    static constexpr uint32_t FLOOR_DEFER_TIMEOUT_MS = 185000u;
    // At most two read-only Type-0 requests, then fail without ever writing
    // a guessed page. The indoor full-settings batch may take 60+ seconds.
    static constexpr uint32_t FLOOR_TEMPLATE_REFRESH_RETRY_MS = 75000u;
    static constexpr uint8_t FLOOR_TEMPLATE_REFRESH_MAX_ATTEMPTS = 2u;

    text_sensor::TextSensor *floor_console_comfort_master_ = nullptr;
    text_sensor::TextSensor *floor_console_comfort_indoor_ = nullptr;
    text_sensor::TextSensor *floor_console_direction_master_ = nullptr;
    text_sensor::TextSensor *floor_console_direction_indoor_ = nullptr;
    text_sensor::TextSensor *floor_console_step_master_ = nullptr;
    text_sensor::TextSensor *floor_console_step_indoor_ = nullptr;
    text_sensor::TextSensor *floor_console_comfort_capability_ = nullptr;

    // Slave-only read-only diagnostics. All are populated from observed unit packets.
    // Type-A CA installer setting 02 (central-control/unit address), HEX / decimal text.
    text_sensor::TextSensor* installer_central_control_address_status_ = nullptr;
    SlaveCentralControlAddress slave_central_control_address_;
    text_sensor::TextSensor* installer_overheating_status_ = nullptr;
    text_sensor::TextSensor* installer_overcooling_status_ = nullptr;
    text_sensor::TextSensor* installer_cooling_thermal_off_fan_status_ = nullptr;
    text_sensor::TextSensor* installer_temperature_control_step_status_ = nullptr;
    text_sensor::TextSensor* installer_continuous_fan_status_ = nullptr;
    text_sensor::TextSensor* installer_temperature_based_fan_auto_status_ = nullptr;
    sensor::Sensor* installer_fan_speed_slow_status_ = nullptr;
    sensor::Sensor* installer_fan_speed_low_status_ = nullptr;
    sensor::Sensor* installer_fan_speed_medium_status_ = nullptr;
    sensor::Sensor* installer_fan_speed_high_status_ = nullptr;

    // Last 0xCE.00/10/11/12 sub-frames from the unit, for extended_settings_raw.
    uint8_t last_recv_type_e_settings_[4][MsgLen] = {};
    esphome::binary_sensor::BinarySensor& defrost_;
    esphome::binary_sensor::BinarySensor& preheat_;
    esphome::binary_sensor::BinarySensor& outdoor_;
    esphome::binary_sensor::BinarySensor& auto_dry_active_;
    uint32_t last_outdoor_change_millis_ = 0;

    LgSwitch& purifier_;
    LgSwitch& internal_thermistor_;
    LgSwitch& auto_dry_;

    uint8_t recv_buf_[MsgLen] = {};
    uint32_t recv_buf_len_ = 0;
    uint32_t last_recv_millis_ = 0;

    // Last received 0xC8 message.
    uint8_t last_recv_status_[MsgLen] = {};

    // The genuine wired Master is the authoritative source for temperature
    // source, measured room temperature, and unknown configuration bits.
    // C8 messages can contain different thermistor/source fields.
    uint8_t last_master_status_[MsgLen] = {};
    bool have_master_status_ = false;
    // Retain genuine Master minute-form A8 because the controller alternates
    // it with an A8 status/flag form that cannot be used for read-only query.
    uint8_t last_master_duration_status_[MsgLen] = {};
    bool have_master_duration_status_ = false;
    uint32_t last_master_duration_status_millis_ = 0;
    // V2 default: live-validated on Porch and Downstairs Bedroom with active
    // Master sleep; per-device override remains available for diagnostics.
    bool slave_sleep_safe_discovery_ = true;
    // Passive, post-query Master observation; does not send any timer command.
    bool slave_diagnostic_sleep_watch_ = false;
    uint16_t slave_diagnostic_sleep_minutes_ = 0;
    uint32_t slave_diagnostic_sleep_sent_at_ = 0;

    // Last received 0xCA message.
    uint8_t last_recv_type_a_settings_[MsgLen] = {};

    // Last received 0xCB message.
    uint8_t last_recv_type_b_settings_[MsgLen] = {};

    // V1.6: read-only/telemetry-only Type-B poll for Slave operation.
    // Require a CURRENT valid AB (genuine Master) or CB (unit) packet as
    // a template. Never invent installer settings or use a historical preset.
    SlavePipeTempPoll slave_pipe_temp_poll_;
    // A manual read uses the existing timed Type-B scheduler and never writes
    // installer settings. Rate-limit presses without changing the 10min timer.
    static constexpr uint32_t MANUAL_PIPE_REFRESH_COOLDOWN_MS = 30u * 1000u;
    bool manual_pipe_refresh_ever_requested_ = false;
    uint32_t last_manual_pipe_refresh_millis_ = 0;
    // V1.7: one-off *status* settings-batch discovery (never a guessed 2B).
    // The first valid CB then activates the unchanged V1.6 2B.80 poller.
    uint8_t last_unit_status_for_pipe_[MsgLen] = {};
    uint32_t last_master_status_for_pipe_millis_ = 0;
    uint32_t last_unit_status_for_pipe_millis_ = 0;
    bool have_unit_status_for_pipe_ = false;
    bool pending_slave_pipe_discovery_ = false;
    bool slave_pipe_discovery_attempted_ = false;
    bool slave_pipe_discovery_waiting_cb_ = false;
    uint32_t slave_pipe_discovery_sent_millis_ = 0;

    // Slave-only startup diagnostics: after a stable Master/IDU exchange,
    // request the full settings batch once if the indoor unit has not yet
    // supplied all three frames needed for the read-only diagnostics.
    // The existing ten-minute Type-B telemetry policy remains unchanged.
    static constexpr uint32_t SLAVE_DIAGNOSTICS_BOOT_SETTLE_MS = 60u * 1000u;
    static constexpr uint32_t SLAVE_DIAGNOSTICS_PAIR_SETTLE_MS = 10u * 1000u;
    uint32_t slave_startup_millis_ = 0;
    uint32_t slave_status_pair_first_seen_millis_ = 0;
    bool slave_unit_ca_seen_ = false;

    bool slave_installer_diagnostics_complete_() const {
        return slave_unit_ca_seen_ &&
               last_recv_type_b_settings_[0] == 0xCB &&
               last_recv_type_e_settings_[1][0] == 0xCE;  // CE.10
    }

    // Do not accept a Home Assistant Type-A request before a REAL indoor CA.
    // The previous optimistic UI behaviour could show Auto Dry ON even after
    // the firmware discarded the unsendable 2A settings write.
    // We deliberately reject rather than replay an old request after a later
    // settings refresh, when it might override a newer Master decision.
    bool slave_type_a_control_ready_() const {
        return SlaveTypeAControlPreflight::can_accept_user_write(
            slave_, slave_unit_ca_seen_, last_recv_type_a_settings_);
    }

    // Track repeated status messages to handle edge case with
    // corrupted messages.
    uint8_t repeated_status_[MsgLen] = {};
    uint8_t repeated_status_count_ = 0;
    static const uint8_t repeated_status_threshold_ = 2;

    uint8_t send_buf_[MsgLen] = {};
    uint32_t last_sent_status_millis_ = 0;
    uint32_t last_sent_recv_type_b_millis_ = 0;
    uint32_t last_write_millis_ = 0;
    uint32_t last_line_busy_millis_ = 0;

    enum class PendingSendKind : uint8_t { None, Status, TypeA, TypeB, TypeBPipePoll, TypeBDiscovery, TypeE, TypeAReadback, FloorConsole, FloorConsoleReadback, FloorConsoleTemplateRefresh, FloorConsoleSelectiveCDProbe };
    PendingSendKind pending_send_ = PendingSendKind::None;

    // Master-native Type-3 cancellation and indoor C8 confirmation.
    bool master_sleep_cancel_pending_ = false;
    bool master_sleep_command_waiting_ = false;
    bool master_sleep_command_was_cancel_ = false;
    uint32_t master_sleep_last_tx_millis_ = 0;

    bool pending_status_change_ = false;
    bool pending_type_a_settings_change_ = false;
    bool pending_type_b_settings_change_ = false;
    bool pending_type_e_settings_change_ = false;
    bool pending_type_b_timed_message_ = false;
    bool operation_mode_changed_ = false;
    bool user_changed_settings_ = false;

    // Experimental slave transaction tracker. UART echo != application acceptance.
    // Per-byte masks describe *only* settings explicitly requested by HA.
    // A confirmed *new* indoor-unit settings change supersedes a stale HA
    // request. The detector is limited to mode/fan/setpoint/swing fields and
    // requires a changed-bit C8 plus two consistent reports. It deliberately
    // ignores unrelated preheat/defrost/room-temperature fluctuations.
    SlaveExternalChangeDetector slave_external_change_;
    bool slave_external_takeover_this_frame_ = false;
    bool slave_ack_pending_ = false;
    uint32_t slave_ack_started_millis_ = 0;
    uint8_t slave_ack_attempts_ = 0;
    uint32_t slave_command_queued_millis_ = 0;
    bool slave_master_match_seen_ = false;
    uint8_t slave_expected_[MsgLen] = {};
    uint8_t slave_mask_1_ = 0, slave_mask_2_ = 0, slave_mask_3_ = 0;
    uint8_t slave_mask_5_ = 0, slave_mask_6_ = 0;
    // A short settle window coalesces rapid HA calls into one latest-state frame.
    // It does not change the 500 ms shared-bus idle guard or retry deadline.
    static constexpr uint32_t SLAVE_COALESCE_MS = 300;
    uint32_t slave_send_not_before_millis_ = 0;
    uint32_t slave_request_id_ = 0;
    uint8_t slave_matching_unit_frames_ = 0;
    // Monitor briefly for subsequent indoor-unit reversions after confirmation.
    bool slave_recent_confirm_watch_ = false;
    uint32_t slave_recent_confirm_millis_ = 0;
    uint8_t slave_recent_confirm_expected_[MsgLen] = {};
    uint8_t slave_recent_mask_1_ = 0, slave_recent_mask_2_ = 0;
    uint8_t slave_recent_mask_3_ = 0, slave_recent_mask_5_ = 0, slave_recent_mask_6_ = 0;
    uint8_t slave_recent_mismatch_count_ = 0;
    uint8_t slave_type_e_dirty_mask_ = 0;
    static constexpr uint32_t SLAVE_POST_CONFIRM_WATCH_MS = 30000;
    static constexpr uint32_t SLAVE_CONFIRM_TIMEOUT_MS = 70000;
    static constexpr uint8_t SLAVE_MAX_ATTEMPTS = 3;

    bool is_initializing_ = true;
    bool waiting_for_first_status = true;

    uint8_t vane_position_[4] = {0,0,0,0};
    uint8_t fan_speed_[4] = {0,0,0,0};
    // Experimental installer ESP calibration writes: one explicit byte at a
    // time, based exclusively on a fresh indoor CA. Echo is not acceptance:
    // read back through the existing safe 28 full-settings request and two
    // genuine indoor CA reports. Never retry a calibration write blindly.
    bool slave_fan_cal_pending_ = false;
    bool slave_fan_cal_write_sent_ = false;
    bool slave_fan_cal_readback_queued_ = false;
    uint8_t slave_fan_cal_index_ = 0;
    uint8_t slave_fan_cal_value_ = 0;
    uint8_t slave_fan_cal_old_ = 0;
    uint8_t slave_fan_cal_matching_ca_ = 0;
    uint8_t slave_fan_cal_mismatching_ca_ = 0;
    uint32_t slave_fan_cal_queued_millis_ = 0;
    uint32_t slave_fan_cal_readback_millis_ = 0;
    static constexpr uint32_t SLAVE_FAN_CAL_TIMEOUT_MS = 110000u;
    // CA can be stale; require recent unit CA AND a safe fresh status pair.
    uint32_t slave_unit_ca_millis_ = 0;

    void slave_fan_cal_finish_(bool success, const char* reason) {
        if (!slave_fan_cal_pending_) return;
        uint8_t index = slave_fan_cal_index_;
        uint8_t value = slave_fan_cal_value_;
        if (success) {
            fan_speed_[index] = value;
            ESP_LOGI(TAG, "SLAVE ESP CAL CONFIRMED: %s=%u (two indoor CA readback reports)",
                     index==0?"Slow":index==1?"Low":index==2?"Medium":"High", unsigned(value));
        } else {
            uint8_t actual = last_recv_type_a_settings_[0] == 0xCA
                             ? last_recv_type_a_settings_[2 + index] : slave_fan_cal_old_;
            fan_speed_[index] = actual;
            ESP_LOGW(TAG, "SLAVE ESP CAL NOT CONFIRMED: %s requested %u; %s; restoring observed value %u",
                     index==0?"Slow":index==1?"Low":index==2?"Medium":"High",
                     unsigned(value), reason, unsigned(actual));
        }
        slave_fan_cal_pending_ = false;
        slave_fan_cal_write_sent_ = false;
        slave_fan_cal_readback_queued_ = false;
        slave_fan_cal_matching_ca_ = slave_fan_cal_mismatching_ca_ = 0;
        slave_fan_cal_readback_millis_ = 0;
        switch (index) {
          case 0: fan_speed_slow_.publish_state(fan_speed_[index]); break;
          case 1: fan_speed_low_.publish_state(fan_speed_[index]); break;
          case 2: fan_speed_medium_.publish_state(fan_speed_[index]); break;
          case 3: fan_speed_high_.publish_state(fan_speed_[index]); break;
        }
    }
    uint8_t overheating_ = 0;
    uint8_t overcooling_ = 0;

    // Installer setting 27 (over cooling) to write, or -1 to echo the unit's value. If the
    // unit doesn't take it, give up after a few attempts rather than resend forever.
    int8_t overcooling_target_ = -1;
    uint8_t overcooling_write_attempts_ = 0;

    // Fan values 5 (low-medium) and 6 (medium-high) have no standard HA fan
    // mode. They are shown as the next lower standard mode, and the raw value
    // is kept here so that our next status message sends it back unchanged
    // instead of overwriting a speed picked at the wall remote. 0xFF = none.
    uint8_t unit_fan_raw_ = 0xFF;
    climate::ClimateFanMode unit_fan_mapped_ = climate::CLIMATE_FAN_MEDIUM;

    optional<uint32_t> sleep_timer_target_millis_{};
    bool active_reservation_ = false;
    // V1.8: Sleep is an independent, Master-authoritative reservation.
    // Its remaining duration is ONLY decoded from genuine Master A8 (Slave)
    // or indoor C8 (Master). Never infer a duration from reservation flags.
    bool slave_sleep_pending_ = false;
    uint16_t slave_sleep_requested_minutes_ = 0;
    bool slave_sleep_master_confirmed_ = false;
    // V1.8.1: once Master accepted our reservation, a *new* consistent
    // contradictory Master state supersedes pending indoor confirmation.
    bool slave_sleep_master_ever_confirmed_ = false;
    LgSlaveSleepSync::OverrideDetector slave_sleep_override_;
    LgSlaveSleepSync::PowerState slave_sleep_power_state_;
    bool slave_sleep_master_known_ = false;
    bool slave_sleep_master_active_ = false;
    bool slave_sleep_master_duration_known_ = false;
    uint16_t slave_sleep_master_minutes_ = 0;
    uint32_t slave_sleep_master_seen_millis_ = 0;
    bool slave_sleep_awaiting_master_logged_ = false;
    bool ignore_callbacks_ = false;

    uint32_t NVS_STORAGE_VERSION = 2843654U; // Change version if the NVSStorage struct changes
    struct NVSStorage {
        uint8_t capabilities_message[13] = {};
    };
    NVSStorage nvs_storage_;
    LgCapabilityRefresh capability_refresh_;

    const bool fahrenheit_;

    // Whether a message came from the HVAC unit, a master controller, or a slave controller.
    enum class MessageSender : uint8_t { Unit, Master, Slave };
    // Set if this controller is configured as slave controller.
    const bool slave_;

    enum class LgCapability {
        FAN_AUTO,
        FAN_SLOW,
        FAN_LOW,
        FAN_LOW_MEDIUM,
        FAN_MEDIUM,
        FAN_MEDIUM_HIGH,
        FAN_HIGH,
        FAN_POWER,
        MODE_HEATING,
        MODE_FAN,
        MODE_AUTO,
        MODE_DEHUMIDIFY,
        VERTICAL_SWING,
        HORIZONTAL_SWING,
    };

    bool parse_capability(LgCapability capability) {
        switch (capability) {
            case LgCapability::FAN_AUTO:
                return (nvs_storage_.capabilities_message[3] & 0x01) != 0;
            case LgCapability::FAN_SLOW:
                return (nvs_storage_.capabilities_message[3] & 0x20) != 0;
            case LgCapability::FAN_LOW:
                return (nvs_storage_.capabilities_message[3] & 0x10) != 0;
            case LgCapability::FAN_LOW_MEDIUM:
                return (nvs_storage_.capabilities_message[6] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM:
                return (nvs_storage_.capabilities_message[3] & 0x08) != 0;
            case LgCapability::FAN_MEDIUM_HIGH:
                return (nvs_storage_.capabilities_message[6] & 0x10) != 0;
            case LgCapability::FAN_HIGH:
                return true;
            case LgCapability::FAN_POWER:
                return (nvs_storage_.capabilities_message[3] & 0x02) != 0;
            case LgCapability::MODE_HEATING:
                return (nvs_storage_.capabilities_message[2] & 0x40) != 0;
            case LgCapability::MODE_FAN:
                return (nvs_storage_.capabilities_message[2] & 0x80) != 0;
            case LgCapability::MODE_AUTO:
                return (nvs_storage_.capabilities_message[2] & 0x08) != 0;
            case LgCapability::MODE_DEHUMIDIFY:
                return (nvs_storage_.capabilities_message[2] & 0x80) != 0;
            case LgCapability::VERTICAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x80) != 0;
            case LgCapability::HORIZONTAL_SWING:
                return (nvs_storage_.capabilities_message[1] & 0x40) != 0;
        }
        return false;
    }

    void configure_capabilities() {
        // Default traits
        climate::ClimateModeMask device_modes;
        device_modes.insert(climate::CLIMATE_MODE_OFF);
        device_modes.insert(climate::CLIMATE_MODE_COOL);
        device_modes.insert(climate::CLIMATE_MODE_HEAT);
        device_modes.insert(climate::CLIMATE_MODE_DRY);
        device_modes.insert(climate::CLIMATE_MODE_FAN_ONLY);
        device_modes.insert(climate::CLIMATE_MODE_HEAT_COOL);
        
        climate::ClimateFanModeMask fan_modes;
        fan_modes.insert(climate::CLIMATE_FAN_LOW);
        fan_modes.insert(climate::CLIMATE_FAN_MEDIUM);
        fan_modes.insert(climate::CLIMATE_FAN_HIGH);
        fan_modes.insert(climate::CLIMATE_FAN_AUTO);
        fan_modes.insert(climate::CLIMATE_FAN_FOCUS);
        
        climate::ClimateSwingModeMask swing_modes;
        swing_modes.insert(climate::CLIMATE_SWING_OFF);
        swing_modes.insert(climate::CLIMATE_SWING_BOTH);
        swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
        swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);

        supported_traits_.set_supported_modes(device_modes);
        supported_traits_.set_supported_fan_modes(fan_modes);
        supported_traits_.set_supported_swing_modes(swing_modes);
        supported_traits_.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
        supported_traits_.set_visual_min_temperature(MIN_TEMP_SETPOINT);
        supported_traits_.set_visual_max_temperature(MAX_TEMP_SETPOINT);
        supported_traits_.set_visual_current_temperature_step(fahrenheit_ ? 1 : 0.5);
        supported_traits_.set_visual_target_temperature_step(fahrenheit_ ? 1.0f : target_temperature_step_);

        // Only override defaults if the capabilities are known
        if (nvs_storage_.capabilities_message[0] != 0) {
            // Configure the climate traits
            climate::ClimateModeMask override_device_modes;
            override_device_modes.insert(climate::CLIMATE_MODE_OFF);
            override_device_modes.insert(climate::CLIMATE_MODE_COOL);
            if (parse_capability(LgCapability::MODE_HEATING))
                override_device_modes.insert(climate::CLIMATE_MODE_HEAT);
            if (parse_capability(LgCapability::MODE_FAN))
                override_device_modes.insert(climate::CLIMATE_MODE_FAN_ONLY);
            if (parse_capability(LgCapability::MODE_AUTO))
                override_device_modes.insert(climate::CLIMATE_MODE_HEAT_COOL);
            if (parse_capability(LgCapability::MODE_DEHUMIDIFY))
                override_device_modes.insert(climate::CLIMATE_MODE_DRY);
            supported_traits_.set_supported_modes(override_device_modes);

            climate::ClimateFanModeMask override_fan_modes;
            if (parse_capability(LgCapability::FAN_AUTO))
                override_fan_modes.insert(climate::CLIMATE_FAN_AUTO);
            if (parse_capability(LgCapability::FAN_SLOW))
                override_fan_modes.insert(climate::CLIMATE_FAN_QUIET);
            if (parse_capability(LgCapability::FAN_LOW))
                override_fan_modes.insert(climate::CLIMATE_FAN_LOW);
            if (parse_capability(LgCapability::FAN_MEDIUM))
                override_fan_modes.insert(climate::CLIMATE_FAN_MEDIUM);
            if (parse_capability(LgCapability::FAN_HIGH))
                override_fan_modes.insert(climate::CLIMATE_FAN_HIGH);
            if (parse_capability(LgCapability::FAN_POWER))
                override_fan_modes.insert(climate::CLIMATE_FAN_FOCUS);
            supported_traits_.set_supported_fan_modes(override_fan_modes);

            climate::ClimateSwingModeMask override_swing_modes;
            override_swing_modes.insert(climate::CLIMATE_SWING_OFF);
            if (parse_capability(LgCapability::VERTICAL_SWING) && parse_capability(LgCapability::HORIZONTAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_BOTH);
            if (parse_capability(LgCapability::VERTICAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_VERTICAL);
            if (parse_capability(LgCapability::HORIZONTAL_SWING))
                override_swing_modes.insert(climate::CLIMATE_SWING_HORIZONTAL);
            supported_traits_.set_supported_swing_modes(override_swing_modes);
        }
    }

public:
    // Exposed through the Slave-only HA diagnostic button. Request is queued;
    // bus arbitration remains owned by the existing send scheduler.
    void request_pipe_temperature_refresh() {
        const uint32_t now = millis();
        if (manual_pipe_refresh_ever_requested_ &&
            static_cast<uint32_t>(now - last_manual_pipe_refresh_millis_) <
                MANUAL_PIPE_REFRESH_COOLDOWN_MS) {
            ESP_LOGI(TAG, "SLAVE PIPE MANUAL: refresh ignored (30-second press cooldown)");
            return;
        }
        if (pending_type_b_timed_message_) {
            ESP_LOGI(TAG, "SLAVE PIPE MANUAL: a Type-B refresh is already queued");
            return;
        }
        if (slave_) {
            if (is_initializing_ || !have_master_status_ || !have_unit_status_for_pipe_) {
                ESP_LOGW(TAG, "SLAVE PIPE MANUAL: waiting for Master and indoor-unit status; no request sent");
                return;
            }
            if (!slave_pipe_temp_poll_.has_fresh_template(now)) {
                ESP_LOGW(TAG, "SLAVE PIPE MANUAL: no recent real AB/CB template; refusing to guess installer fields");
                return;
            }
            if (slave_pipe_temp_poll_.awaiting_response(now)) {
                ESP_LOGI(TAG, "SLAVE PIPE MANUAL: previous 2B request still awaiting indoor CB");
                return;
            }
        } else if (last_recv_type_b_settings_[0] != 0xAB &&
                   last_recv_type_b_settings_[0] != 0xCB) {
            ESP_LOGW(TAG, "PIPE MANUAL: no Type-B settings available yet; request not queued");
            return;
        }

        // Same read-only path as the 10-minute timer. Existing status and
        // reservation handling retains priority over telemetry in loop().
        pending_type_b_timed_message_ = true;
        last_manual_pipe_refresh_millis_ = now;
        manual_pipe_refresh_ever_requested_ = true;
        ESP_LOGI(TAG, "SLAVE PIPE MANUAL: refresh queued; awaiting safe bus send slot");
    }

    void set_overcooling_target(int value) {
        overcooling_target_ = value;
    }
    void set_target_temperature_step(float step) {
        target_temperature_step_ = step;
        supported_traits_.set_visual_target_temperature_step(fahrenheit_ ? 1.0f : step);
    }
    void set_temperature_control_step_select(LgSelect* s) {
        temperature_control_step_select_ = s;
        if (temperature_control_step_select_ != nullptr) {
            temperature_control_step_select_->add_on_state_callback([this](size_t index) {
                set_temperature_control_step(index);
            });
        }
    }

    void set_temperature_control_step(int value) {
        if (slave_) return;  // Installer setting: physical Master only.
        if (value < 0 || value > 1) {
            ESP_LOGE(TAG, "Unexpected temperature control step value: %d", value);
            return;
        }

        if (temperature_control_step_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting temperature control step installer setting: %d", value);

        temperature_control_step_ = value;
        if (!is_initializing_) {
            pending_type_b_settings_change_ = true;
        }
    }
    void set_static_pressure_step_sensor(sensor::Sensor* s) { static_pressure_step_ = s; }
    void set_continuous_fan_switch(LgSwitch* s) {
        continuous_fan_switch_ = s;
        if (continuous_fan_switch_ != nullptr) {
            continuous_fan_switch_->add_on_state_callback([this](bool) {
                if (slave_ || ignore_callbacks_) return;
                if (!is_initializing_) {
                    if (slave_) slave_type_e_dirty_mask_ |= 0x02;
                    pending_type_e_settings_change_ = true;
                }
            });
        }
    }
    void set_temperature_based_fan_auto_switch(LgSwitch* s) {
        temperature_based_fan_auto_switch_ = s;
        if (temperature_based_fan_auto_switch_ != nullptr) {
            temperature_based_fan_auto_switch_->add_on_state_callback([this](bool) {
                if (slave_ || ignore_callbacks_) return;
                if (!is_initializing_) {
                    if (slave_) slave_type_e_dirty_mask_ |= 0x04;
                    pending_type_e_settings_change_ = true;
                }
            });
        }
    }
    void set_floor_console_monitoring(bool value) { floor_console_monitoring_ = value; }
    void set_floor_console_features(bool value) {
        floor_console_features_ = value;
        if (value) floor_console_monitoring_ = true;
    }
    void set_floor_console_reversed_airflow(bool v) { floor_console_reversed_airflow_ = v; }
    void set_floor_console_passive_readback(bool v) { floor_console_passive_readback_ = v; }
    void set_floor_console_fast_discharge(bool v) { floor_console_fast_discharge_ = v; }
    void set_floor_console_selective_cd_probe(bool v) { floor_console_selective_cd_probe_ = v; }
    void set_slave_sleep_safe_discovery(bool v) { slave_sleep_safe_discovery_ = v; }
    void set_filter_time_remaining_sensor(sensor::Sensor *s) { filter_time_remaining_ = s; }
    void set_filter_remaining_percent_sensor(sensor::Sensor *s) { filter_remaining_percent_ = s; }
    void set_filter_nominal_life_hours(uint16_t hours) { filter_nominal_life_hours_ = hours; }
    void set_floor_console_command_status_text_sensor(text_sensor::TextSensor *s) { floor_console_command_status_ = s; }
    void set_floor_console_controls(LgSwitch *comfort, LgSelect *direction, LgSelect *step) {
        floor_console_comfort_control_ = comfort;
        floor_console_direction_control_ = direction;
        floor_console_step_control_ = step;
        if (comfort != nullptr) comfort->set_request_handler([this](bool value) {
            floor_console_request_(lg_floor_console::Page::Comfort, value ? 1 : 0);
        });
        if (direction != nullptr) direction->set_request_handler([this, direction](const std::string &option) {
            for (size_t i = 0; i < 3; ++i) {
                if (direction->at(i).has_value() && *direction->at(i) == option) {
                    floor_console_request_(lg_floor_console::Page::Discharge, static_cast<uint8_t>(i));
                    return;
                }
            }
            floor_console_report_("REJECTED: unknown discharge selection");
        });
        if (step != nullptr) step->set_request_handler([this, step](const std::string &option) {
            for (size_t i = 0; i < 3; ++i) {
                if (step->at(i).has_value() && *step->at(i) == option) {
                    floor_console_request_(lg_floor_console::Page::Step, static_cast<uint8_t>(i));
                    return;
                }
            }
            floor_console_report_("REJECTED: unknown comfort step selection");
        });
    }
    void set_floor_console_comfort_master_text_sensor(text_sensor::TextSensor *s) { floor_console_comfort_master_ = s; }
    void set_floor_console_comfort_indoor_text_sensor(text_sensor::TextSensor *s) { floor_console_comfort_indoor_ = s; }
    void set_floor_console_direction_master_text_sensor(text_sensor::TextSensor *s) { floor_console_direction_master_ = s; }
    void set_floor_console_direction_indoor_text_sensor(text_sensor::TextSensor *s) { floor_console_direction_indoor_ = s; }
    void set_floor_console_step_master_text_sensor(text_sensor::TextSensor *s) { floor_console_step_master_ = s; }
    void set_floor_console_step_indoor_text_sensor(text_sensor::TextSensor *s) { floor_console_step_indoor_ = s; }
    void set_floor_console_comfort_capability_text_sensor(text_sensor::TextSensor *s) { floor_console_comfort_capability_ = s; }
    void set_advanced_settings_raw_text_sensor(text_sensor::TextSensor* s) { advanced_settings_raw_ = s; }
    void set_extended_settings_raw_text_sensor(text_sensor::TextSensor* s) { extended_settings_raw_ = s; }

    void set_installer_central_control_address_status_text_sensor(text_sensor::TextSensor* s) { installer_central_control_address_status_ = s; }
    void set_installer_overheating_status_text_sensor(text_sensor::TextSensor* s) { installer_overheating_status_ = s; }
    void set_installer_overcooling_status_text_sensor(text_sensor::TextSensor* s) { installer_overcooling_status_ = s; }
    void set_installer_cooling_thermal_off_fan_status_text_sensor(text_sensor::TextSensor* s) { installer_cooling_thermal_off_fan_status_ = s; }
    void set_installer_temperature_control_step_status_text_sensor(text_sensor::TextSensor* s) { installer_temperature_control_step_status_ = s; }
    void set_installer_continuous_fan_status_text_sensor(text_sensor::TextSensor* s) { installer_continuous_fan_status_ = s; }
    void set_installer_temperature_based_fan_auto_status_text_sensor(text_sensor::TextSensor* s) { installer_temperature_based_fan_auto_status_ = s; }
    void set_installer_fan_speed_slow_status_sensor(sensor::Sensor* s) { installer_fan_speed_slow_status_ = s; }
    void set_installer_fan_speed_low_status_sensor(sensor::Sensor* s) { installer_fan_speed_low_status_ = s; }
    void set_installer_fan_speed_medium_status_sensor(sensor::Sensor* s) { installer_fan_speed_medium_status_ = s; }
    void set_installer_fan_speed_high_status_sensor(sensor::Sensor* s) { installer_fan_speed_high_status_ = s; }

    LgController(InternalGPIOPin* rx_pin,
                 sensor::Sensor* temperature_sensor,
                 LgSelect* vane_select_1,
                 LgSelect* vane_select_2,
                 LgSelect* vane_select_3,
                 LgSelect* vane_select_4,
                 LgSelect* overheating_select,
                 LgNumber* fan_speed_slow,
                 LgNumber* fan_speed_low,
                 LgNumber* fan_speed_medium,
                 LgNumber* fan_speed_high,
                 LgNumber* sleep_timer,
                 sensor::Sensor* error_code,
                 sensor::Sensor* pipe_temp_in,
                 sensor::Sensor* pipe_temp_mid,
                 sensor::Sensor* pipe_temp_out,
                 sensor::Sensor* power_consumption,
                 sensor::Sensor* current_power,
                 sensor::Sensor* humidity,
                 sensor::Sensor* fan_operation_time,
                 sensor::Sensor* indoor_unit_operation_time,
                 sensor::Sensor* precise_room_temperature,
                 LgSelect* overcooling,
                 LgSelect* cooling_thermal_off_fan,
                 binary_sensor::BinarySensor* defrost,
                 binary_sensor::BinarySensor* preheat,
                 binary_sensor::BinarySensor* outdoor,
                 binary_sensor::BinarySensor* auto_dry_active,
                 LgSwitch* purifier,
                 LgSwitch* internal_thermistor,
                 LgSwitch* auto_dry,
                 bool fahrenheit, bool is_slave_controller)
      : rx_pin_(*rx_pin),
        temperature_sensor_(temperature_sensor),
        vane_select_1_(*vane_select_1),
        vane_select_2_(*vane_select_2),
        vane_select_3_(*vane_select_3),
        vane_select_4_(*vane_select_4),
        overheating_select_(*overheating_select),
        fan_speed_slow_(*fan_speed_slow),
        fan_speed_low_(*fan_speed_low),
        fan_speed_medium_(*fan_speed_medium),
        fan_speed_high_(*fan_speed_high),
        sleep_timer_(*sleep_timer),
        error_code_(*error_code),
        pipe_temp_in_(*pipe_temp_in),
        pipe_temp_mid_(*pipe_temp_mid),
        pipe_temp_out_(*pipe_temp_out),
        power_consumption_(power_consumption),
        current_power_(current_power),
        humidity_(humidity),
        fan_operation_time_(fan_operation_time),
        indoor_unit_operation_time_(indoor_unit_operation_time),
        precise_room_temperature_(precise_room_temperature),
        overcooling_select_(overcooling),
        cooling_thermal_off_fan_select_(cooling_thermal_off_fan),
        defrost_(*defrost),
        preheat_(*preheat),
        outdoor_(*outdoor),
        auto_dry_active_(*auto_dry_active),
        purifier_(*purifier),
        internal_thermistor_(*internal_thermistor),
        auto_dry_(*auto_dry),
        fahrenheit_(fahrenheit),
        slave_(is_slave_controller)
    {
        // V2: gate Auto Dry and vane *user writes* BEFORE they publish an
        // optimistic Home Assistant state. Genuine CA/AA readback still uses
        // publish_state() directly and is unaffected by these controls.
        const auto vane_preflight = [this]() -> bool {
            if (slave_type_a_control_ready_()) return true;
            ESP_LOGW(TAG, "SLAVE TYPE-A V2: vane request rejected before genuine indoor CA; "
                          "settings discovery will populate the available controls (no TX)");
            return false;
        };
        vane_select_1_.set_control_guard(vane_preflight);
        vane_select_2_.set_control_guard(vane_preflight);
        vane_select_3_.set_control_guard(vane_preflight);
        vane_select_4_.set_control_guard(vane_preflight);
        auto_dry_.set_write_guard([this](bool requested) -> bool {
            if (slave_type_a_control_ready_()) return true;
            ESP_LOGW(TAG, "SLAVE AUTO DRY V2: %s request rejected until genuine indoor CA is available; "
                          "no settings packet sent and no stale command queued",
                     requested ? "ON" : "OFF");
            return false;
        });

        vane_select_1_.add_on_state_callback([this](size_t index) {
            set_vane_position(1, vane_option_to_wire_(index));
        });
        vane_select_2_.add_on_state_callback([this](size_t index) {
            set_vane_position(2, vane_option_to_wire_(index));
        });
        vane_select_3_.add_on_state_callback([this](size_t index) {
            set_vane_position(3, vane_option_to_wire_(index));
        });
        vane_select_4_.add_on_state_callback([this](size_t index) {
            set_vane_position(4, vane_option_to_wire_(index));
        });
        overheating_select_.add_on_state_callback([this](size_t index) {
            set_overheating(index);
        });
        if (overcooling_select_ != nullptr) {
            overcooling_select_->add_on_state_callback([this](size_t index) {
                set_overcooling(index);
            });
        }
        if (cooling_thermal_off_fan_select_ != nullptr) {
            cooling_thermal_off_fan_select_->add_on_state_callback([this](size_t index) {
                set_cooling_thermal_off_fan(index);
            });
        }

        fan_speed_slow_.add_on_state_callback([this](float v) {
            set_fan_speed(0, v);
        });
        fan_speed_low_.add_on_state_callback([this](float v) {
            set_fan_speed(1, v);
        });
        fan_speed_medium_.add_on_state_callback([this](float v) {
            set_fan_speed(2, v);
        });
        fan_speed_high_.add_on_state_callback([this](float v) {
            set_fan_speed(3, v);
        });
        sleep_timer_.add_on_state_callback([this](float v) {
            set_sleep_timer(v);
        });

        purifier_.add_on_state_callback([this](bool) {
            if (ignore_callbacks_) {
                return;
            }
            if (slave_) {
                slave_mask_2_ |= 0x04;
                ++slave_request_id_;
                slave_ack_pending_ = true;
                slave_ack_started_millis_ = 0;
                slave_ack_attempts_ = 0;
                slave_matching_unit_frames_ = 0;
                slave_master_match_seen_ = false;
                slave_recent_confirm_watch_ = false;
                // Snapshot the actual state BEFORE transmitting this request;
                // this lets us distinguish a new ThinQ/Master change from
                // ordinary old C8 frames and partial adoption of our command.
                slave_external_change_.arm(last_recv_status_, last_recv_status_[0] != 0,
                                           last_master_status_, have_master_status_);
                slave_command_queued_millis_ = millis();
                slave_send_not_before_millis_ = slave_command_queued_millis_ + SLAVE_COALESCE_MS;
            }
            user_changed_settings_ = true;
            pending_status_change_ = true;
        });
        internal_thermistor_.add_on_state_callback([this](bool) {
            if (ignore_callbacks_) {
                return;
            }
            user_changed_settings_ = true;
            pending_status_change_ = true;
        });
        auto_dry_.add_on_state_callback([this](bool) {
            if (ignore_callbacks_) {
                return;
            }
            pending_type_a_settings_change_ = true;
        });
    }

    float get_setup_priority() const override {
        return esphome::setup_priority::BUS;
    }

    // Return the value after a roundtrip through the LG system so we
    // store the value we expect to hear back.
    float get_roundtrip_target_temperature(float target) {
        if (fahrenheit_) {
            target = TempConversion::lgcelsius_to_celsius
                (TempConversion::celsius_to_lgcelsius(target));
        } else {
            target = round(target * 2) / 2;
        }
        return target;
    }

    void setup() override {
        // Load our custom NVS storage to get the capabilities message
        ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
        pref.load(&nvs_storage_);

        auto restore = this->restore_state_();
        if (restore.has_value()) {
            restore->apply(this);
            this->target_temperature = get_roundtrip_target_temperature
                (this->target_temperature);
        } else {
            this->mode = climate::CLIMATE_MODE_OFF;
            this->target_temperature = 20;
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
            this->swing_mode = climate::CLIMATE_SWING_OFF;
            this->publish_state();
        }

        internal_thermistor_.restore_and_set_mode(esphome::switch_::SWITCH_RESTORE_DEFAULT_OFF);

        // Restoring a freshly booted Master must not trigger a phantom
        // 0-minute sleep cancellation through the number callback.
        ignore_callbacks_ = true;
        sleep_timer_.publish_state(0);
        ignore_callbacks_ = false;

        // Configure climate traits and entities based on the capabilities message (if available)
        ESP_LOGI(TAG, "LG CONTROLLER V2.0.1 candidate: V2 baseline with floor-console template recovery; "
                      "sleep-safe standard-unit discovery and Type-A preflight retained");
        configure_capabilities();
        if (floor_console_features_) {
            ESP_LOGI(TAG, "FLOOR CONSOLE V2.0.1: guarded read-before-write template recovery; "
                          "confirmed-state Home Assistant controls; bounded readbacks and writes");
        }

        while (UARTDevice::available() > 0) {
            uint8_t b;
            UARTDevice::read_byte(&b);
        }
        // A Slave must first adopt the Master's / indoor unit's status.
        // The original boot-time pending send is meant for a Master, not a Slave.
        // Clear state potentially raised by restored entities during setup;
        // later explicit Home Assistant commands will set it again.
        if (slave_) {
            slave_startup_millis_ = millis();
            pending_status_change_ = false;
            user_changed_settings_ = false;
            // Publishing the initial sleep_timer_=0 can invoke its callback,
            // which otherwise creates a phantom reservation-write request.
            slave_ack_pending_ = false;
            slave_sleep_pending_ = false;
            slave_sleep_master_confirmed_ = false;
            slave_sleep_master_ever_confirmed_ = false;
            slave_sleep_override_.reset();
            slave_ack_started_millis_ = 0;
            slave_ack_attempts_ = 0;
            slave_matching_unit_frames_ = 0;
            slave_type_e_dirty_mask_ = 0;
            slave_recent_confirm_watch_ = false;
            slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
            slave_mask_5_ = slave_mask_6_ = 0;
            ESP_LOGI(TAG, "SLAVE startup: waiting for first Master/IDU status; no unsolicited boot transmit");
            ESP_LOGI(TAG, "SLAVE SLEEP V1.8.1: Master A8 duration/flag decoder, explicit type-3 cancellation, bounded confirmation; no independent Slave power-off");
        } else {
            pending_status_change_ = true;
        }

        // Call `update` every 6 seconds, but first wait 10 seconds.
        set_timeout("initial_send", 10000, [this]() {
            set_interval("update", 6000, [this]() { update(); });
        });
    }

    // Process changes from HA.
    void control(const climate::ClimateCall &call) override {
        if (call.get_mode().has_value()) {
            this->mode = *call.get_mode();
            operation_mode_changed_ = true;
            if (this->mode == climate::CLIMATE_MODE_OFF) {
                this->action = climate::CLIMATE_ACTION_OFF;
            } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
                this->action = climate::CLIMATE_ACTION_FAN;
            } else if (this->action == climate::CLIMATE_ACTION_OFF) {
                this->action = climate::CLIMATE_ACTION_IDLE;
            }
        }
        if (!slave_ && call.get_mode().has_value() &&
            this->mode == climate::CLIMATE_MODE_OFF &&
            (active_reservation_ || sleep_timer_target_millis_.has_value())) {
            // Power OFF always clears a local Master sleep timer.
            sleep_timer_target_millis_.reset();
            active_reservation_ = false;
            master_sleep_cancel_pending_ = true;
            master_sleep_command_waiting_ = false;
            ignore_callbacks_ = true;
            sleep_timer_.publish_state(0);
            ignore_callbacks_ = false;
            ESP_LOGI(TAG, "MASTER SLEEP: HA power OFF clears reservation; explicit type-3 cancellation queued");
        }
        if (call.get_target_temperature().has_value()) {
            this->target_temperature = get_roundtrip_target_temperature
                (*call.get_target_temperature());
        }
        if (call.get_fan_mode().has_value()) {
            this->fan_mode = *call.get_fan_mode();
            unit_fan_raw_ = 0xFF;
        }
        if (call.get_swing_mode().has_value()) {
            set_swing_mode(*call.get_swing_mode());
        }
        if (slave_) {
            // Only request confirmation for fields the user actually changed.
            if (call.get_mode().has_value()) slave_mask_1_ |= 0x1E; // power + mode
            if (call.get_fan_mode().has_value()) slave_mask_1_ |= 0xE0;
            if (call.get_target_temperature().has_value()) {
                slave_mask_5_ |= 0x01; // half-degree part
                slave_mask_6_ |= 0x0F; // whole-degree setpoint
            }
            if (call.get_swing_mode().has_value()) slave_mask_2_ |= 0xC0;
            if (slave_mask_1_ || slave_mask_2_ || slave_mask_3_ ||
                slave_mask_5_ || slave_mask_6_) {
                if (slave_ack_pending_ && slave_ack_started_millis_ != 0) {
                    ESP_LOGI(TAG, "SLAVE request supersedes unconfirmed request #%u (latest state wins)",
                             unsigned(slave_request_id_));
                }
                ++slave_request_id_;
                slave_ack_pending_ = true;
                slave_ack_started_millis_ = 0;
                slave_ack_attempts_ = 0;
                slave_matching_unit_frames_ = 0;
                slave_master_match_seen_ = false;
                slave_recent_confirm_watch_ = false;
                // Re-arm on EVERY climate request, including fan-only requests.
                // Snapshot the pre-request Master for A8/C8 external arbitration.
                slave_external_change_.arm(last_recv_status_, last_recv_status_[0] != 0,
                                           last_master_status_, have_master_status_);
                slave_command_queued_millis_ = millis();
                slave_send_not_before_millis_ = slave_command_queued_millis_ + SLAVE_COALESCE_MS;
                ESP_LOGI(TAG, "SLAVE command queued (mode=%u, fan=%u, temperature=%u, swing=%u)",
                         unsigned(call.get_mode().has_value()),
                         unsigned(call.get_fan_mode().has_value()),
                         unsigned(call.get_target_temperature().has_value()),
                         unsigned(call.get_swing_mode().has_value()));
            }
        }
        this->user_changed_settings_ = true;
        this->pending_status_change_ = true;
        this->publish_state();
    }

    void loop() override {
        uint32_t millis_now = millis();

        // Track if the RX pin is idle for at least 500 ms to avoid collisions on the bus as much
        // as possible. If there is still a collision, we'll likely both start sending at
        // approximately the same time and the message will hopefully be corrupt (and ignored)
        // anyway. Else the pending_send_/send_buf_ mechanism should catch it and we try again.
        //
        // Note: using digital_read is *much* better for this than using UARTDevice because that
        // interface has significant delays. It has to wait for a full byte to arrive and this
        // takes about 9-10 ms with our slow baud rate. There are also various buffers and
        // timeouts before incoming bytes reach us.
        if (UARTDevice::available() > 0 || !rx_pin_.digital_read()) {
            this->last_line_busy_millis_ = millis_now;
        }

        bool bytes_received = false;
        while (UARTDevice::available() > 0) {
            if (!UARTDevice::read_byte(&recv_buf_[recv_buf_len_])) {
                break;
            }
            bytes_received = true;
            recv_buf_len_++;
            if (recv_buf_len_ == MsgLen) {
                process_message(recv_buf_);
                recv_buf_len_ = 0;
            }
        }

        if (bytes_received) {
            last_recv_millis_ = millis_now;
        }

        if (recv_buf_len_ > 0) {
            // A byte takes about 96 milliseconds to transmit.
            if (millis_now - last_recv_millis_ > 150) {
                ESP_LOGE(TAG, "discarding incomplete data %s",
                         format_hex_pretty(recv_buf_, recv_buf_len_).c_str());
                recv_buf_len_ = 0;
            }
            return;
        }

        // If we did not receive the message we sent last time, try to send it again.
        // Ignore this when we're initializing because the unit then immediately responds by
        // sending a lot of messages and this introduces a delay.
        if (pending_send_ != PendingSendKind::None &&
            (millis_now - last_write_millis_ > 2000)) {
            ESP_LOGE(TAG, "did not receive message we just sent");
            // No UART echo: do not send indefinitely on a shared bus.
            if (slave_ && pending_send_ == PendingSendKind::Status &&
                slave_ack_pending_ && slave_ack_attempts_ >= SLAVE_MAX_ATTEMPTS) {
                ESP_LOGW(TAG, "SLAVE TX failed: no local UART echo after %u attempts",
                              unsigned(slave_ack_attempts_));
                slave_cancel_pending_sleep_("UART echo not received");
                slave_ack_pending_ = false;
                slave_external_change_.reset();
                slave_ack_started_millis_ = 0;
                slave_matching_unit_frames_ = 0;
                slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
                slave_mask_5_ = slave_mask_6_ = 0;
                user_changed_settings_ = false;
                slave_master_match_seen_ = false;
                slave_command_queued_millis_ = 0;
                pending_status_change_ = false;
                pending_send_ = PendingSendKind::None;
                return;
            }
            if (slave_ && slave_fan_cal_pending_ &&
                (pending_send_ == PendingSendKind::TypeA ||
                 pending_send_ == PendingSendKind::TypeAReadback)) {
                pending_send_ = PendingSendKind::None;
                pending_type_a_settings_change_ = false;
                slave_fan_cal_finish_(false, "no UART echo; not retrying installer write");
                return;
            }
            switch (pending_send_) {
                case PendingSendKind::Status:
                    pending_status_change_ = true;
                    break;
                case PendingSendKind::TypeA:
                    pending_type_a_settings_change_ = true;
                    break;
                case PendingSendKind::TypeB:
                    pending_type_b_settings_change_ = true;
                    break;
                case PendingSendKind::TypeBPipePoll:
                    // A failed read must NEVER turn into a Type-B installer
                    // settings write (the original TypeB retry does that).
                    ESP_LOGW(TAG, "SLAVE PIPE POLL: no UART echo; no settings-write retry");
                    break;
                case PendingSendKind::TypeBDiscovery:
                    ESP_LOGW(TAG, "SLAVE PIPE DISCOVERY: no UART echo for one-time 28 refresh; no retries");
                    break;
                case PendingSendKind::TypeAReadback:
                    ESP_LOGW(TAG, "SLAVE ESP CAL: no UART echo on readback refresh; no retries");
                    break;
                case PendingSendKind::TypeE:
                    pending_type_e_settings_change_ = true;
                    break;
                case PendingSendKind::FloorConsole:
                    // No echo: abort this experiment, not a blind retry.
                    floor_console_finish_("No UART echo; command not confirmed", false);
                    break;
                case PendingSendKind::FloorConsoleReadback:
                    ESP_LOGW(TAG, "FLOOR READBACK: 28 request had no UART echo; cannot verify acceptance");
                    floor_console_tx_.readback_queued = false;
                    break;
                case PendingSendKind::FloorConsoleTemplateRefresh:
                    ESP_LOGW(TAG, "FLOOR TEMPLATE: guarded 28 request had no UART echo; "
                                  "waiting for genuine AD/CD/CE.10 (no settings write)");
                    break;
                case PendingSendKind::FloorConsoleSelectiveCDProbe:
                    ESP_LOGW(TAG, "FLOOR SELECTIVE: no UART echo for unverified 2D.80 candidate; "
                                  "falling back to guarded 28 readback");
                    floor_console_tx_.selective_probe_fallback_ready = true;
                    break;
                case PendingSendKind::None:
                    ESP_LOGE(TAG, "unreachable");
                    break;
            }
            pending_send_ = PendingSendKind::None;
            return;
        }

        // The first 28 echo means the UART transmitted; it does not mean the
        // indoor unit accepted the request. Wait for a 0xC8 that matches the
        // explicitly requested fields. Retry at most three times, leaving
        // enough time for the unit's periodic status frame.
        if (slave_ && slave_ack_pending_ && slave_ack_started_millis_ != 0 &&
            pending_send_ == PendingSendKind::None && !pending_status_change_ &&
            (millis_now - slave_ack_started_millis_ >= SLAVE_CONFIRM_TIMEOUT_MS)) {
            if (slave_ack_attempts_ >= SLAVE_MAX_ATTEMPTS) {
                ESP_LOGW(TAG, "SLAVE TX NOT CONFIRMED by indoor unit after %u attempts; "
                              "returning control to authoritative status", slave_ack_attempts_);
                slave_cancel_pending_sleep_("indoor/Master acceptance not confirmed after bounded retries");
                slave_ack_pending_ = false;
                slave_external_change_.reset();
                slave_ack_started_millis_ = 0;
                slave_matching_unit_frames_ = 0;
                slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
                slave_mask_5_ = slave_mask_6_ = 0;
                user_changed_settings_ = false;
                slave_master_match_seen_ = false;
                slave_command_queued_millis_ = 0;
            } else {
                ESP_LOGW(TAG, "SLAVE TX still unconfirmed by indoor unit; retry %u/%u",
                              unsigned(slave_ack_attempts_ + 1),
                              unsigned(SLAVE_MAX_ATTEMPTS));
                pending_status_change_ = true;
                slave_ack_started_millis_ = 0;
                slave_matching_unit_frames_ = 0;
            }
        }
        if (slave_ && slave_fan_cal_pending_ &&
            millis_now - slave_fan_cal_queued_millis_ >= SLAVE_FAN_CAL_TIMEOUT_MS) {
            slave_fan_cal_finish_(false, "indoor CA confirmation timed out");
            pending_type_a_settings_change_ = false;
        }
        // Experimental floor console transactions share the exact existing
        // bus-idle check and pending_send echo handling with all other writes.
        floor_console_maintain_(millis_now);
        // Send if there's a pending message.
        if (pending_send_ == PendingSendKind::None &&
            (millis_now - last_line_busy_millis_ > 500) &&
            !(slave_ && is_initializing_) &&
            // Never interleave an optional Type-A/B/E write while a normal
            // Slave request is awaiting indoor-unit status. Retry the normal
            // status if queued; send optional writes after it finishes.
            !(slave_ && slave_ack_pending_ && !pending_status_change_) &&
            !(slave_ && pending_status_change_ &&
              int32_t(millis_now - slave_send_not_before_millis_) < 0)) {

            if (slave_ && pending_status_change_) {
                // Do not hold a requested power/fan/setpoint change behind
                // optional installer messages on a shared controller bus.
                send_status_message();
            } else if (slave_fan_cal_readback_queued_) {
                // Read back the ONE installer write before any other optional Type-A work.
                send_slave_esp_readback_();
            } else if (floor_console_tx_.active && !floor_console_tx_.superseded &&
                       floor_console_tx_.readback_queued &&
                       !pending_status_change_ && !slave_ack_pending_ &&
                       !slave_fan_cal_pending_ && !pending_type_a_settings_change_ &&
                       !pending_type_b_settings_change_ && !pending_type_b_timed_message_ &&
                       !pending_type_e_settings_change_) {
                send_floor_console_readback_();
            } else if (floor_console_tx_.active && !floor_console_tx_.superseded &&
                       floor_console_tx_.awaiting_template &&
                       !pending_status_change_ && !slave_ack_pending_ &&
                       !slave_fan_cal_pending_ &&
                       !pending_type_a_settings_change_ &&
                       !pending_type_b_settings_change_ && !pending_type_b_timed_message_ &&
                       !pending_type_e_settings_change_ &&
                       lg_floor_console::template_refresh_due(
                           floor_console_tx_.template_refresh_attempts,
                           uint32_t(millis_now - floor_console_tx_.template_refresh_sent_at),
                           FLOOR_TEMPLATE_REFRESH_RETRY_MS,
                           FLOOR_TEMPLATE_REFRESH_MAX_ATTEMPTS)) {
                send_floor_console_template_refresh_();
            } else if (floor_console_send_queued_ && floor_console_tx_.active &&
                       !floor_console_tx_.superseded && !pending_status_change_ &&
                       !slave_ack_pending_ && !slave_fan_cal_pending_ &&
                       !pending_type_a_settings_change_ &&
                       !pending_type_b_settings_change_ && !pending_type_b_timed_message_ &&
                       !pending_type_e_settings_change_) {
                send_floor_console_message_();
            } else if (pending_type_a_settings_change_ &&
                       !(slave_ && slave_fan_cal_pending_ && slave_fan_cal_write_sent_)) {
                send_type_a_settings_message();
            } else if (pending_type_b_settings_change_) {
                send_type_b_settings_message(/* timed = */ false);
            } else if (pending_type_b_timed_message_) {
                send_type_b_settings_message(/* timed = */ true);
                pending_type_b_timed_message_ = false;
            } else if (pending_type_e_settings_change_) {
                send_type_e_settings_message();
            } else if (floor_console_fast_discharge_ && lg_floor_console::fast_verify_due(
                           floor_fast_verify_pending_, floor_fast_verify_sent_,
                           floor_console_tx_.active, floor_console_deferred_.valid,
                           uint32_t(millis_now - floor_fast_verify_queued_at_), FLOOR_FAST_QUIET_MS) &&
                       !active_reservation_ && !slave_sleep_master_active_ &&
                       !sleep_timer_target_millis_.has_value()) {
                // Lowest-priority coalesced readback. Skip if we cannot build
                // a known-safe Type-0 settings request from recent A8/C8.
                send_floor_console_fast_verify_();
            } else if (pending_slave_pipe_discovery_) {
                // Optional telemetry discovery always has lowest priority.
                pending_slave_pipe_discovery_ = false;
                send_slave_pipe_discovery_message_();
            } else if (pending_status_change_) {
                send_status_message();
            }

        }
    }

    climate::ClimateTraits traits() override {
        auto t = supported_traits_;
        if (!fahrenheit_) {
            t.set_visual_target_temperature_step(target_temperature_step_);
        }
        return t;
    }

private:
    bool set_action_from_status_(bool outdoor_on, bool preheating, bool defrosting, uint8_t temperature_flags) {
        climate::ClimateAction action = climate::CLIMATE_ACTION_IDLE;
        if (this->mode == climate::CLIMATE_MODE_OFF) {
            action = climate::CLIMATE_ACTION_OFF;
        } else if (defrosting) {
            action = climate::CLIMATE_ACTION_DEFROSTING;
        } else if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
            action = climate::CLIMATE_ACTION_FAN;
        } else if (!outdoor_on || preheating) {
            action = climate::CLIMATE_ACTION_IDLE;
        } else {
            switch (this->mode) {
                case climate::CLIMATE_MODE_COOL:
                    action = climate::CLIMATE_ACTION_COOLING;
                    break;
                case climate::CLIMATE_MODE_DRY:
                    action = climate::CLIMATE_ACTION_DRYING;
                    break;
                case climate::CLIMATE_MODE_HEAT:
                    action = climate::CLIMATE_ACTION_HEATING;
                    break;
                case climate::CLIMATE_MODE_HEAT_COOL:
                    if (temperature_flags & 0x40) {
                        action = climate::CLIMATE_ACTION_COOLING;
                    } else if (temperature_flags & 0x80) {
                        action = climate::CLIMATE_ACTION_HEATING;
                    }
                    break;
                default:
                    break;
            }
        }

        if (this->action == action) {
            return false;
        }
        this->action = action;
        return true;
    }

    // Sets position of vane index (1-4) to position (0-6).
    void set_vane_position(int index, int position) {
        if (index < 1 || index > 4) {
            ESP_LOGE(TAG, "Unexpected vane index: %d", index);
            return;
        }
        if (position < 0 || position > 6) {
            ESP_LOGE(TAG, "Unexpected vane position: %d", position);
            return;
        }
        if (vane_position_[index-1] == position) {
            return;
        }
        ESP_LOGD(TAG, "Setting vane %d position: %d", index, position);
        vane_position_[index-1] = position;
        if (!is_initializing_) {
            pending_type_a_settings_change_ = true;
        }
    }

    // Sets installer setting fan speed index (0-3 for slow-high) to value (0-255), with "0" being the factory default
    void set_fan_speed(int index, int value) {
        if (slave_) {
            if (index < 0 || index > 3 || value < 0 || value > 255) return;
            const uint8_t observed = last_recv_type_a_settings_[0] == 0xCA
                ? last_recv_type_a_settings_[2 + index] : fan_speed_[index];
            if (fan_speed_[index] == value) return;
            uint8_t preflight[13] = {};
            uint32_t now = millis();
            bool safe_status = have_master_status_ && have_unit_status_for_pipe_ &&
                SlavePipeTempDiscovery::make_request(
                    preflight, last_master_status_, last_unit_status_for_pipe_,
                    static_cast<uint32_t>(now-last_master_status_for_pipe_millis_),
                    static_cast<uint32_t>(now-last_unit_status_for_pipe_millis_));
            if (is_initializing_ || slave_ack_pending_ || pending_status_change_ ||
                pending_send_ != PendingSendKind::None ||
                slave_fan_cal_pending_ || pending_type_a_settings_change_ ||
                !slave_unit_ca_seen_ || last_recv_type_a_settings_[0] != 0xCA ||
                now - slave_unit_ca_millis_ > 300000u || !safe_status) {
                ESP_LOGW(TAG, "SLAVE ESP CAL REFUSED: need recent indoor CA, recent matching A8/C8, "
                              "no active reservation or other pending changes");
                fan_speed_[index] = observed;
                switch (index) {
                  case 0: fan_speed_slow_.publish_state(observed); break;
                  case 1: fan_speed_low_.publish_state(observed); break;
                  case 2: fan_speed_medium_.publish_state(observed); break;
                  case 3: fan_speed_high_.publish_state(observed); break;
                }
                return;
            }
            slave_fan_cal_pending_ = true;
            slave_fan_cal_write_sent_ = false;
            slave_fan_cal_index_ = uint8_t(index);
            slave_fan_cal_value_ = uint8_t(value);
            slave_fan_cal_old_ = observed;
            slave_fan_cal_queued_millis_ = now;
            slave_fan_cal_matching_ca_ = slave_fan_cal_mismatching_ca_ = 0;
            slave_fan_cal_readback_millis_ = 0;
            slave_fan_cal_readback_queued_ = false;
            fan_speed_[index] = uint8_t(value);
            pending_type_a_settings_change_ = true;
            ESP_LOGW(TAG, "SLAVE ESP CAL EXPERIMENTAL: request %s %u -> %u (Type-A 2A; require indoor CA readback)",
                     index==0?"Slow":index==1?"Low":index==2?"Medium":"High",
                     unsigned(observed), unsigned(value));
            return;
        }
        if (index < 0 || index > 3) {
            ESP_LOGE(TAG, "Unexpected fan speed index: %d", index);
            return;
        }
        if (value<0 || value>255) {
            ESP_LOGE(TAG, "Unexpected fan speed: %d", value);
            return;
        }

        if (fan_speed_[index] == value) {
            return;
        }

        fan_speed_[index] = value;
        if (!is_initializing_) {
            pending_type_a_settings_change_ = true;
        }
    }

    // Set cooling thermal off fan installer setting 35 to 0-2.
    void set_cooling_thermal_off_fan(int value) {
        if (slave_) return;
        if (value < 0 || value > 2) {
            ESP_LOGE(TAG, "Unexpected cooling thermal off fan value: %d", value);
            return;
        }

        if (cooling_thermal_off_fan_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting cooling thermal off fan installer setting: %d", value);

        cooling_thermal_off_fan_ = value;
        if (!is_initializing_) {
            pending_type_b_settings_change_ = true;
        }
    }

    // Set overcooling installer setting 27 to 0-3.
    void set_overcooling(int value) {
        if (slave_) return;
        if (value < 0 || value > 3) {
            ESP_LOGE(TAG, "Unexpected overcooling value: %d", value);
            return;
        }

        if (overcooling_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting overcooling installer setting: %d", value);

        overcooling_ = value;
        if (!is_initializing_) {
            pending_type_b_settings_change_ = true;
        }
    }

    // Set overheating installer setting 15 to 0-4.
    void set_overheating(int value) {
        if (slave_) return;
        if (value < 0 || value > 4) {
            ESP_LOGE(TAG, "Unexpected overheating value: %d", value);
            return;
        }

        if (overheating_ == value) {
            return;
        }

        ESP_LOGD(TAG, "Setting overheating installer setting: %d", value);

        overheating_ = value;
        if (!is_initializing_) {
            pending_type_b_settings_change_ = true;
        }
    }

    // Restore only a verified Master-derived sleep state. Never let an
    // unconfirmed local sleep request start its own independent power-off.
    void slave_restore_master_sleep_() {
        if (!slave_) return;
        if (slave_sleep_master_known_ && slave_sleep_master_active_ &&
            slave_sleep_master_duration_known_) {
            // Use the age of the last actual Master duration report so that
            // command failure does not restart an older countdown.
            const uint32_t age_ms = millis() - slave_sleep_master_seen_millis_;
            const uint32_t total_ms = uint32_t(slave_sleep_master_minutes_) * 60000u;
            if (age_ms < total_ms) {
                sleep_timer_target_millis_ = millis() + (total_ms - age_ms);
            } else {
                sleep_timer_target_millis_.reset();
            }
        } else {
            sleep_timer_target_millis_.reset();
        }
        active_reservation_ = slave_sleep_master_known_ && slave_sleep_master_active_;
        const auto remaining = get_sleep_timer_minutes();
        const float reported = remaining.has_value() ? float(*remaining) : 0.0f;
        if (!float_equal(sleep_timer_.state, reported)) {
            ignore_callbacks_ = true;
            sleep_timer_.publish_state(reported);
            ignore_callbacks_ = false;
        }
    }

    void slave_cancel_pending_sleep_(const char* why) {
        if (!slave_ || !slave_sleep_pending_) return;
        ESP_LOGW(TAG, "SLAVE SLEEP: %s; restoring last observed Master state", why);
        slave_sleep_pending_ = false;
        slave_sleep_master_confirmed_ = false;
        slave_sleep_master_ever_confirmed_ = false;
        slave_sleep_override_.reset();
        slave_sleep_awaiting_master_logged_ = false;
        slave_restore_master_sleep_();
    }

    // Master already acknowledged our requested timer, then twice reported
    // a different explicit sleep state before the indoor C8 completed.
    // Stop retries rather than overriding a wall-controller change.
    void slave_yield_to_newer_master_sleep_(
        const char* reason = "newer confirmed Master sleep change superseded pending request") {
        if (!slave_ || !slave_sleep_pending_ || !slave_ack_pending_) return;
        slave_cancel_pending_sleep_(reason);
        slave_ack_pending_ = false;
        slave_ack_started_millis_ = 0;
        slave_ack_attempts_ = 0;
        slave_matching_unit_frames_ = 0;
        slave_master_match_seen_ = false;
        slave_recent_confirm_watch_ = false;
        slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
        slave_mask_5_ = slave_mask_6_ = 0;
        slave_command_queued_millis_ = 0;
        pending_status_change_ = false;
        if (pending_send_ == PendingSendKind::Status) pending_send_ = PendingSendKind::None;
        operation_mode_changed_ = false;
        user_changed_settings_ = false;
        slave_external_change_.reset();
    }

    // Actual LG power OFF cancels its sleep reservation (even when the
    // power/status frame does not carry a reservation-cancellation payload).
    // Sync only local state/HA; do not transmit a second cancellation or OFF.
    void slave_clear_sleep_on_power_off_(const char* source) {
        if (!slave_) return;
        const bool had_sleep = sleep_timer_target_millis_.has_value() || active_reservation_ ||
            slave_sleep_pending_ || slave_sleep_master_active_ ||
            (sleep_timer_.state > 0.0f);
        slave_sleep_master_known_ = true;
        slave_sleep_master_active_ = false;
        slave_sleep_master_duration_known_ = false;
        slave_sleep_master_minutes_ = 0;
        slave_sleep_master_seen_millis_ = millis();
        sleep_timer_target_millis_.reset();
        active_reservation_ = false;
        // An in-flight sleep SET must not retry after the genuine controller
        // has switched the unit off. This path is never used for unrelated
        // climate/pipe-temperature operations.
        if (slave_sleep_pending_) {
            if (slave_ack_pending_) {
                slave_yield_to_newer_master_sleep_("LG power OFF cancelled sleep reservation");
            } else {
                slave_cancel_pending_sleep_("LG power OFF cancelled sleep reservation");
            }
        }
        if (!float_equal(sleep_timer_.state, 0.0f)) {
            ignore_callbacks_ = true;
            sleep_timer_.publish_state(0.0f);
            ignore_callbacks_ = false;
        }
        if (had_sleep) {
            ESP_LOGI(TAG, "SLAVE SLEEP: %s confirms AC OFF; clearing local sleep countdown and HA timer (0 min)", source);
        }
    }

    // Record the Master's real reservation state, not the C8 reservation
    // active-bit or the local UART echo. A8 may alternate a valid duration
    // with the 0x80 flag format. Never overwrite 240 minutes with 0x10=16.
    void slave_observe_master_sleep_(const uint8_t* buffer) {
        if (!slave_) return;
        const auto r = LgSleepReservation::decode(buffer);
        // Byte 3's active-reservation bit is NOT specific to sleep. Only
        // a type-3 duration or the 0x80 sleep-flag report identifies sleep.
        if (r.active && r.form == LgSleepReservation::Form::Minutes &&
            r.minutes > 0 && r.minutes <= 480 &&
            (!slave_sleep_master_duration_known_ || r.minutes != slave_sleep_master_minutes_)) {
            ESP_LOGI(TAG, "SLAVE SLEEP: genuine Master reports %u minutes remaining",
                     unsigned(r.minutes));
        } else if (!r.active && slave_sleep_master_known_ && slave_sleep_master_active_) {
            ESP_LOGI(TAG, "SLAVE SLEEP: genuine Master reports reservation cleared");
        }
        if (!r.active || r.form == LgSleepReservation::Form::Flags ||
            r.form == LgSleepReservation::Form::Minutes) {
            slave_sleep_master_known_ = true;
            slave_sleep_master_active_ = r.active &&
                ((r.form == LgSleepReservation::Form::Flags && r.sleep_flag) ||
                 (r.form == LgSleepReservation::Form::Minutes && r.minutes > 0));
            if (!slave_sleep_master_active_) {
                slave_sleep_master_duration_known_ = false;
                slave_sleep_master_minutes_ = 0;
            } else if (r.form == LgSleepReservation::Form::Minutes && r.minutes <= 480) {
                slave_sleep_master_duration_known_ = true;
                slave_sleep_master_minutes_ = r.minutes;
                slave_sleep_master_seen_millis_ = millis();
            }
        }
        if (slave_sleep_pending_ && slave_ack_started_millis_ != 0) {
            // Do not treat Master A8 packets sent before the first successful
            // duration confirmation as external override (normal propagation
            // can take 20+ seconds). After a confirmed match, two matching
            // contradictory explicit states mean the wall controller changed
            // its mind; yield rather than issuing a stale retry.
            if (slave_sleep_override_.observe(
                    buffer, slave_sleep_requested_minutes_,
                    millis() - slave_command_queued_millis_,
                    slave_sleep_master_ever_confirmed_)) {
                ESP_LOGW(TAG, "SLAVE SLEEP V1.8.1: two contradictory genuine Master A8 reports after confirmation; yielding to wall controller");
                slave_yield_to_newer_master_sleep_();
                return;
            }
            const bool matches = LgSleepReservation::master_matches(
                slave_sleep_requested_minutes_, buffer,
                millis() - slave_command_queued_millis_);
            if (matches) {
                if (!slave_sleep_master_confirmed_) {
                    ESP_LOGI(TAG, "SLAVE SLEEP: genuine Master A8 confirms %s (%u min requested)",
                             slave_sleep_requested_minutes_ ? "sleep duration" : "cancellation",
                             unsigned(slave_sleep_requested_minutes_));
                }
                slave_sleep_master_confirmed_ = true;
                slave_sleep_master_ever_confirmed_ = true;
            } else if (r.form == LgSleepReservation::Form::Minutes || !r.active ||
                       (r.form == LgSleepReservation::Form::Flags && !r.sleep_flag)) {
                // A more recent contradictory explicit Master reservation
                // state revokes an earlier tentative duration confirmation.
                slave_sleep_master_confirmed_ = false;
            }
        }
        if (!slave_sleep_pending_) {
            // The actual wall controller may set/cancel a reservation at any
            // time; HA must follow instead of keeping an old locally set one.
            slave_restore_master_sleep_();
        }
    }

    void master_clear_sleep_on_off_(const char* source) {
        if (slave_) return;
        if (!active_reservation_ && !sleep_timer_target_millis_.has_value() &&
            !(sleep_timer_.state > 0.0f)) return;
        sleep_timer_target_millis_.reset();
        active_reservation_ = false;
        master_sleep_cancel_pending_ = false; // Indoor is already OFF.
        master_sleep_command_waiting_ = false;
        ignore_callbacks_ = true;
        sleep_timer_.publish_state(0);
        ignore_callbacks_ = false;
        ESP_LOGI(TAG, "MASTER SLEEP: %s reports OFF; cleared timer without additional command", source);
    }

    void set_sleep_timer(int minutes) {
        if (ignore_callbacks_) {
            return;
        }
        // The LG PREMTB100 encodes a reservation duration in 11 bits.
        // The genuine controller supports up to 8 hours (480 minutes).
        if (minutes < 0 || minutes > 8 * 60) {
            ESP_LOGE(TAG, "Ignoring invalid sleep timer value: %d minutes", minutes);
            return;
        }
        // A genuine LG sleep reservation only makes sense when the unit is
        // already ON. Do not queue a type-3 SET against an OFF indoor unit,
        // even if HA is optimistically displaying a newly requested ON state.
        // Zero (cancel) stays available, including when power is OFF.
        if (slave_ && minutes > 0 && !LgSlaveSleepSync::SleepSetGuard::can_set(
                have_master_status_, last_master_status_[1],
                have_unit_status_for_pipe_, last_unit_status_for_pipe_[1],
                this->mode != climate::CLIMATE_MODE_OFF,
                slave_sleep_power_state_.off_latched)) {
            ESP_LOGW(TAG, "SLAVE SLEEP: rejecting %d-minute reservation; Master and indoor unit "
                          "must both confirm power ON; resetting HA timer to 0 (no TX)", minutes);
            // A rejected SET is not a pending command, so do not touch any
            // climate transaction or the Slave retry scheduler.
            sleep_timer_target_millis_.reset();
            active_reservation_ = false;
            ignore_callbacks_ = true;
            sleep_timer_.publish_state(0.0f);
            ignore_callbacks_ = false;
            return;
        }
        if (!slave_ && minutes > 0 && this->mode == climate::CLIMATE_MODE_OFF) {
            ESP_LOGW(TAG, "MASTER SLEEP: refusing reservation while AC is OFF (no TX)");
            ignore_callbacks_ = true;
            sleep_timer_.publish_state(0);
            ignore_callbacks_ = false;
            return;
        }
        ESP_LOGD(TAG, "Setting sleep timer: %d minutes", minutes);
        if (minutes > 0) {
            sleep_timer_target_millis_ = millis() + unsigned(minutes) * 60 * 1000;
            active_reservation_ = true;
        } else {
            sleep_timer_target_millis_.reset();
            active_reservation_ = false;
        }
        if (!slave_) {
            master_sleep_cancel_pending_ = (minutes == 0);
            master_sleep_command_waiting_ = true;
            master_sleep_command_was_cancel_ = (minutes == 0);
            master_sleep_last_tx_millis_ = 0; // ignore any stale C8 before TX
            ESP_LOGI(TAG, "MASTER SLEEP: %s type-3 reservation (%d minutes)",
                     minutes ? "queuing" : "cancelling", minutes);
        }
        if (slave_) {
            slave_sleep_pending_ = true;
            slave_sleep_requested_minutes_ = uint16_t(minutes);
            slave_sleep_master_confirmed_ = false;
            slave_sleep_master_ever_confirmed_ = false;
            slave_sleep_override_.reset();
            slave_sleep_awaiting_master_logged_ = false;
            slave_mask_3_ |= 0x10; // reservation active flag
            ++slave_request_id_;
            slave_ack_pending_ = true;
            slave_ack_started_millis_ = 0;
            slave_ack_attempts_ = 0;
            slave_matching_unit_frames_ = 0;
            slave_master_match_seen_ = false;
            slave_recent_confirm_watch_ = false;
            slave_external_change_.arm(last_recv_status_, last_recv_status_[0] != 0,
                                           last_master_status_, have_master_status_);
            slave_command_queued_millis_ = millis();
            slave_send_not_before_millis_ = slave_command_queued_millis_ + SLAVE_COALESCE_MS;
            ESP_LOGI(TAG, "SLAVE sleep reservation request queued (%d minutes)", minutes);
        }
        user_changed_settings_ = true;
        pending_status_change_ = true;
    }

    optional<float> get_room_temp() const {
        if (temperature_sensor_ == nullptr) {
            return {};
        }

        float temp = temperature_sensor_->get_state();
        if (std::isnan(temp) || temp == 0) {
            return {};
        }
        if (fahrenheit_) {
            temp = TempConversion::fahrenheit_to_lgcelsius(temp);
        }
        if (temp < 11) {
            return 11;
        }
        if (temp > 35) {
            return 35;
        }
        // Round to nearest 0.5 degrees.
        return round(temp * 2) / 2;
    }

    optional<uint32_t> get_sleep_timer_minutes() const {
        if (!sleep_timer_target_millis_.has_value()) {
            return {};
        }
        int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis());
        if (diff <= 0) {
            return {};
        }
        if (slave_) {
            // Master A8 already reports whole remaining minutes. V1.8's
            // unconditional +1 displayed 420 immediately after an A8 419,
            // then 419 one second later. Use exact ceil in Slave mode.
            return LgSlaveSleepSync::ceil_minutes(uint32_t(diff));
        }
        // Exact ceiling, shared with Slave. The former +1 returned 3
        // immediately after setting a 2-minute Master timer at precisely
        // 120000 ms, which could transmit an unintended extra minute.
        return LgSlaveSleepSync::ceil_minutes(uint32_t(diff));
    }

    static uint8_t calc_checksum(const uint8_t* buffer) {
        size_t result = 0;
        for (size_t i = 0; i < 12; i++) {
            result += buffer[i];
        }
        return (result & 0xff) ^ 0x55;
    }

    void set_swing_mode(climate::ClimateSwingMode mode) {
        if (this->swing_mode != mode) {
            // If vertical swing is off, send a 0xAA message to restore the vane position.
            if (!slave_ &&
                (mode == climate::CLIMATE_SWING_OFF || mode == climate::CLIMATE_SWING_HORIZONTAL)) {
                pending_type_a_settings_change_ = true;
            }
        }
        this->swing_mode = mode;
    }

    void send_status_message() {
        // In Slave mode, copy preserved information from the genuine Master,
        // NOT the last C8 frame. Master and unit can report different
        // temperature source and room-temperature bytes on the same bus.
        const uint8_t* status_template =
            (slave_ && have_master_status_) ? last_master_status_ : last_recv_status_;

        // Byte 0: message type.
        send_buf_[0] = slave_ ? 0x28 : 0xA8;

        // Byte 1: changed flag (0x1), power on (0x2), mode (0x1C), fan speed (0x70).
        uint8_t b = 0;
        if (user_changed_settings_) {
            b |= 0x1;

            // Prevent an incoming status message received shortly
            // after sending this outgoing message from immediately
            // taking effect if it is outdated.
            repeated_status_count_ = 0;
            memset(repeated_status_, 0, MsgLen);
        }
        switch (this->mode) {
            case climate::CLIMATE_MODE_COOL:
                b |= (0 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_DRY:
                b |= (1 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_FAN_ONLY:
                b |= (2 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT_COOL:
                b |= (3 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_HEAT:
                b |= (4 << 2) | 0x2;
                break;
            case climate::CLIMATE_MODE_OFF:
                // Don't set power-on flag, but preserve previous operation mode.
                b |= (status_template[1] & 0x1C);
                break;
            default:
                ESP_LOGE(TAG, "unknown operation mode, turning off");
                b |= (2 << 2);
                break;
        }
        
        // Fix: Check if fan_mode has a value before dereferencing
        if (this->fan_mode.has_value() && unit_fan_raw_ != 0xFF &&
            this->fan_mode.value() == unit_fan_mapped_) {
            b |= unit_fan_raw_ << 5;
        } else if (this->fan_mode.has_value()) {
            switch (this->fan_mode.value()) {
                case climate::CLIMATE_FAN_LOW:
                    b |= 0 << 5;
                    break;
                case climate::CLIMATE_FAN_MEDIUM:
                    b |= 1 << 5;
                    break;
                case climate::CLIMATE_FAN_HIGH:
                    b |= 2 << 5;
                    break;
                case climate::CLIMATE_FAN_AUTO:
                    b |= 3 << 5;
                    break;
                case climate::CLIMATE_FAN_QUIET:
                    b |= 4 << 5;
                    break;
                case climate::CLIMATE_FAN_FOCUS:
                    b |= 7 << 5;
                    break;
                default:
                    ESP_LOGE(TAG, "unknown fan mode, using Medium");
                    b |= 1 << 5;
                    break;
            }
        } else {
            // Default to Medium if no fan mode is set
            ESP_LOGD(TAG, "no fan mode set, using Medium as default");
            b |= 1 << 5;
        }
        send_buf_[1] = b;

        // Byte 2: swing mode and purifier/plasma setting. Preserve the other bits.
        b = status_template[2] & ~(0x4|0x40|0x80);
        if (purifier_.state) {
            b |= 0x4;
        }
        switch (this->swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                b |= 0x40;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                b |= 0x80;
                break;
            case climate::CLIMATE_SWING_BOTH:
                b |= 0x40 | 0x80;
                break;
            default:
                ESP_LOGE(TAG, "unknown swing mode");
                break;
        }
        send_buf_[2] = b;

        // Byte 3.
        send_buf_[3] = status_template[3];
        if (!slave_ || slave_sleep_pending_) {
            if (active_reservation_) send_buf_[3] |= 0x10;
            else send_buf_[3] &= ~0x10;
        }  // Otherwise preserve genuine Master reservation bit.

        // Byte 4.
        send_buf_[4] = status_template[4];

        float target = this->target_temperature;
        if (fahrenheit_) {
            target = TempConversion::celsius_to_lgcelsius(target);
        }
        if (target < MIN_TEMP_SETPOINT) {
            target = MIN_TEMP_SETPOINT;
        } else if (target > MAX_TEMP_SETPOINT) {
            target = MAX_TEMP_SETPOINT;
        }

        // Byte 5. Unchanged except for the low bit which indicates the target temperature has a
        // 0.5 fractional part.
        send_buf_[5] = status_template[5] & ~0x1;
        if (float_equal(target - uint8_t(target), 0.5)) {
            send_buf_[5] |= 0x1;
        }

        // Byte 6: thermistor setting and target temperature (fractional part in byte 5).
        // Byte 7: room temperature. Preserve the (unknown) upper two bits.
        enum ThermistorSetting { Unit = 0, Controller = 1, TwoTH = 2 };
        ThermistorSetting thermistor =
            internal_thermistor_.state ? ThermistorSetting::Unit : ThermistorSetting::Controller;
        float temp;
        if (auto maybe_temp = get_room_temp()) {
            temp = *maybe_temp;
        } else {
            // Room temperature isn't available. Use the unit's thermistor and send something
            // reasonable.
            thermistor = ThermistorSetting::Unit;
            temp = 20;
        }
        if (slave_) {
            // Only update the setpoint nibble.  Preserve the Master's
            // thermistor and dual-thermistor configuration bits (e.g. 0x97).
            send_buf_[6] = (status_template[6] & 0xF0) | ((uint8_t(target) - 15) & 0x0F);
            // The genuine wired Master decides the room temperature.
            // Do not manufacture 20 C because HA's sensor is unavailable.
            send_buf_[7] = status_template[7];
        } else {
            send_buf_[6] = (thermistor << 4) | ((uint8_t(target) - 15) & 0x0F);
            send_buf_[7] = (status_template[7] & 0xC0) | uint8_t((temp - 10) * 2);
        }

        // Bytes 8-10. Initialize bytes 8-9 to 0 to not echo back timer settings set by the AC.
        send_buf_[8] = 0;
        send_buf_[9] = 0;
        send_buf_[10] = status_template[10];

        if (is_initializing_) {
            // Request settings when controller turns on.
            send_buf_[8] |= 0x40;
            // Set bit 0x80 of byte 10 to use byte 9 for the Fahrenheit setting flag (0x40).
            if (fahrenheit_) {
                send_buf_[9] |= 0x40;
            }
            send_buf_[10] = 0x80;
        } else if (slave_ && slave_sleep_pending_) {
            // A deliberate sleep command, INCLUDING cancellation (type 3,
            // zero minutes). Do not send a zeroed/no-type status and assume
            // the genuine Master will interpret it as cancellation.
            // 0x80 in byte 10 makes byte 9 a FLAG mask, not a duration.
            const uint16_t minutes = slave_sleep_requested_minutes_;
            LgSleepReservation::encode_slave_request(send_buf_, minutes);
            ESP_LOGI(TAG, "SLAVE SLEEP: transmitting explicit type-3 %s (%u min); waiting for Master duration + indoor state",
                     minutes ? "set" : "cancel", unsigned(minutes));
        } else if (!slave_) {
            // Use native Type-3 reservation and explicitly cancel once.
            // Do not inherit byte-10 flag-mask from C8 when setting minutes.
            if (master_sleep_cancel_pending_) {
                LgMasterSleep::encode(send_buf_, 0);
                ESP_LOGI(TAG, "MASTER SLEEP: transmitting explicit type-3 zero-minute cancel");
                master_sleep_cancel_pending_ = false;
            } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
                LgMasterSleep::encode(send_buf_, uint16_t(*minutes));
            }
        }

        // Byte 11.
        send_buf_[11] = status_template[11];

        // Byte 12.
        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();

        pending_status_change_ = false;
        pending_send_ = PendingSendKind::Status;
        last_sent_status_millis_ = last_write_millis_;
        if (!slave_ && master_sleep_command_waiting_ && master_sleep_last_tx_millis_ == 0) {
            // Deadline is measured from the FIRST status transmission,
            // not reset by normal 20-second Master status reports.
            master_sleep_last_tx_millis_ = last_write_millis_;
        }
        if (slave_ && slave_ack_pending_) {
            memcpy(slave_expected_, send_buf_, MsgLen);
            slave_ack_started_millis_ = last_write_millis_;
            ++slave_ack_attempts_;
            if (slave_command_queued_millis_ != 0 && slave_ack_attempts_ == 1) {
                ESP_LOGI(TAG, "SLAVE first transmission after %u ms from queued command",
                         unsigned(last_write_millis_ - slave_command_queued_millis_));
            }
            ESP_LOGI(TAG, "SLAVE TX attempt %u/%u (awaiting indoor 0xC8, not UART echo)",
                     unsigned(slave_ack_attempts_), unsigned(SLAVE_MAX_ATTEMPTS));
        }

        // If we sent an updated temperature to the AC, update temperature in HA too.
        // Slave controller temperature sensor is ignored.
        if (!slave_ && thermistor == ThermistorSetting::Controller) {
            float ha_temp = temp;
            if (fahrenheit_) {
                ha_temp = TempConversion::lgcelsius_to_celsius(ha_temp);
            }
            if (!float_equal(this->current_temperature, ha_temp)) {
                this->current_temperature = ha_temp;
                publish_state();
            }
        }
    }

    void send_type_a_settings_message() {
        if (!slave_type_a_control_ready_() ||
            (last_recv_type_a_settings_[0] != 0xCA && last_recv_type_a_settings_[0] != 0xAA)) {
            // Guard again at the actual bus send time: the original CA might
            // have been replaced by a Master AA between the HA callback and TX.
            if (slave_) {
                ESP_LOGW(TAG, "SLAVE TYPE-A V2: settings write not transmitted: "
                              "genuine indoor CA template unavailable (no guessed settings)");
            } else {
                ESP_LOGW(TAG, "Skipping Type-A settings write: no CA/AA template received");
            }
            pending_type_a_settings_change_ = false;
            if (slave_ && slave_fan_cal_pending_) {
                slave_fan_cal_finish_(false, "genuine indoor CA template unavailable at send time");
            }
            return;
        }

        // Copy settings from the CA/AA message we received.
        memcpy(send_buf_, last_recv_type_a_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2A : 0xAA;

        // Explicit experimental Slave ESP change: modify ONE calibration
        // byte only, using all other installer fields verbatim from indoor CA.
        if (slave_ && slave_fan_cal_pending_ && slave_fan_cal_readback_millis_ == 0) {
            if (last_recv_type_a_settings_[0] != 0xCA) {
                pending_type_a_settings_change_ = false;
                slave_fan_cal_finish_(false, "indoor CA template unavailable");
                return;
            }
            if (!SlaveEspCalibration::make_write(
                    send_buf_, last_recv_type_a_settings_,
                    slave_fan_cal_index_, slave_fan_cal_value_)) {
                pending_type_a_settings_change_ = false;
                slave_fan_cal_finish_(false, "cannot construct safe one-byte CA write");
                return;
            }
        } else if (!slave_) {
            send_buf_[2] = fan_speed_[0];
            send_buf_[3] = fan_speed_[1];
            send_buf_[4] = fan_speed_[2];
            send_buf_[5] = fan_speed_[3];
        }

        if (!(slave_ && slave_fan_cal_pending_)) {
            // Normal Type-A vane and auto-dry operations; do not piggyback
            // unrelated settings onto an experimental installer write.
            send_buf_[7] = (send_buf_[7] & 0xf0) | (vane_position_[0] & 0x0f);
            send_buf_[7] = (send_buf_[7] & 0x0f) | ((vane_position_[1] & 0x0f) << 4);
            send_buf_[8] = (send_buf_[8] & 0xf0) | (vane_position_[2] & 0x0f);
            send_buf_[8] = (send_buf_[8] & 0x0f) | ((vane_position_[3] & 0x0f) << 4);
            uint8_t b = send_buf_[11] & ~0x8;
            if (auto_dry_.state) b |= 0x8;
            send_buf_[11] = b;
        }

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();

        pending_type_a_settings_change_ = false;
        if (slave_ && slave_fan_cal_pending_) slave_fan_cal_write_sent_ = true;
        pending_send_ = PendingSendKind::TypeA;
    }

    // Experimental read-only bootstrap for *active* Master sleep.
    // All other 28 uses (including fan-calibration and floor-direction
    // readback) retain the original stricter no-reservation policy.
    bool prepare_slave_diagnostic_query_(uint8_t* out, uint32_t now) const {
        if (!have_master_status_ || !have_unit_status_for_pipe_) return false;
        // Current genuine Master must still report sleep active; a stale
        // historical minutes frame is NEVER sufficient by itself.
        const auto current_reservation =
            LgSleepReservation::decode(last_master_status_);
        if (!current_reservation.active) {
            return SlavePipeTempDiscovery::make_request(
                out, last_master_status_, last_unit_status_for_pipe_,
                uint32_t(now - last_master_status_for_pipe_millis_),
                uint32_t(now - last_unit_status_for_pipe_millis_));
        }
        // V2 rollout scope: active-sleep bootstrap has been verified on two
        // ordinary units, not the Garden Office floor console hardware.
        // Preserve the previous floor console policy when sleep is active;
        // a read-only no-reservation discovery is still permitted there.
        if (floor_console_features_) return false;
        // Do not mistake an On/Off schedule or unknown active reservation
        // for a type-3 sleep reservation. The alternating flag report must
        // explicitly carry its sleep flag, and the status must be settled.
        if (current_reservation.form == LgSleepReservation::Form::Other ||
            (current_reservation.form == LgSleepReservation::Form::Flags &&
             !current_reservation.sleep_flag) ||
            (last_master_status_[1] & 0x01u)) return false;
        if (!slave_sleep_safe_discovery_ || !have_master_duration_status_ ||
            uint32_t(now - last_master_duration_status_millis_) >
                SlavePipeTempDiscovery::STATUS_MAX_AGE_MS ||
            uint32_t(now - last_master_status_for_pipe_millis_) >
                SlavePipeTempDiscovery::STATUS_MAX_AGE_MS)
            return false;
        return SlavePipeTempDiscovery::make_request(
            out, last_master_duration_status_, last_unit_status_for_pipe_,
            uint32_t(now - last_master_duration_status_millis_),
            uint32_t(now - last_unit_status_for_pipe_millis_), true);
    }

    // V1.7 one-shot request for the unit's *full settings batch*.
    // LG protocol byte 8 bit 0x40 requests C9/CA/CB/... without requiring
    // any Type-B installer template. Only use a recent, consistent Master/IDU
    // operating state, and never during a climate write or active reservation.
    void send_slave_esp_readback_() {
        if (!slave_ || !slave_fan_cal_pending_) {
            slave_fan_cal_readback_queued_ = false;
            return;
        }
        uint32_t now = millis();
        if (!have_master_status_ || !have_unit_status_for_pipe_ ||
            !SlavePipeTempDiscovery::make_request(
                send_buf_, last_master_status_, last_unit_status_for_pipe_,
                static_cast<uint32_t>(now-last_master_status_for_pipe_millis_),
                static_cast<uint32_t>(now-last_unit_status_for_pipe_millis_))) {
            // No guessed status frames; wait for a safe observed Master/IDU pair.
            return;
        }
        send_buf_[12] = calc_checksum(send_buf_);
        ESP_LOGI(TAG, "SLAVE ESP CAL READBACK: requesting fresh indoor CA (28 full-settings refresh)");
        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = now;
        slave_fan_cal_readback_millis_ = now;
        slave_fan_cal_matching_ca_ = slave_fan_cal_mismatching_ca_ = 0;
        slave_fan_cal_readback_queued_ = false;
        pending_send_ = PendingSendKind::TypeAReadback;
    }

    void send_slave_pipe_discovery_message_() {
        if (!slave_ || slave_pipe_discovery_attempted_ ||
            slave_installer_diagnostics_complete_()) return;
        if (is_initializing_ || pending_status_change_ || slave_ack_pending_ ||
            user_changed_settings_) return;
        uint32_t now = millis();
        if (!prepare_slave_diagnostic_query_(send_buf_, now)) {
            ESP_LOGW(TAG, "SLAVE PIPE DISCOVERY: waiting for safe matching A8/C8; "
                          "active-sleep diagnostic query %s; no packet sent",
                          slave_sleep_safe_discovery_ ? "enabled (V2 default)" : "disabled by override");
            last_sent_recv_type_b_millis_ = now;
            return;
        }
        send_buf_[12] = calc_checksum(send_buf_);
        const bool preserved_sleep = (last_master_status_[3] & 0x10u) != 0;
        if (preserved_sleep) {
            const auto before = LgSleepReservation::decode(last_master_duration_status_);
            ESP_LOGI(TAG, "SLAVE PIPE DISCOVERY V2: active sleep remains Master-owned "
                          "(%u min observed); request has no timer selector/duration, no changed bit; "
                          "monitoring genuine Master readback", unsigned(before.minutes));
            slave_diagnostic_sleep_watch_ = true;
            slave_diagnostic_sleep_minutes_ = before.minutes;
            slave_diagnostic_sleep_sent_at_ = now;
        } else {
            ESP_LOGI(TAG, "SLAVE PIPE DISCOVERY: one-time 28 full-settings refresh "
                          "(byte8=0x40); no installer-settings bytes guessed");
        }
        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();
        last_sent_recv_type_b_millis_ = last_write_millis_;
        slave_pipe_discovery_sent_millis_ = last_write_millis_;
        slave_pipe_discovery_waiting_cb_ = true;
        slave_pipe_discovery_attempted_ = true;
        pending_send_ = PendingSendKind::TypeBDiscovery;
    }

    void send_type_b_settings_message(bool timed) {
        if (slave_ && !timed) {
            // Never send installer configuration writes when the physical controller is Master.
            ESP_LOGW(TAG, "SLAVE: blocked installer Type-B write (pipe polling unaffected)");
            pending_type_b_settings_change_ = false;
            return;
        }
        if (slave_ && timed) {
            // Dedicated Slave *query*. In contrast to a Type-B settings
            // write below, the only changes to the known live AB/CB template
            // are sender 0x2B, request bit 0x80, and checksum. We do not
            // touch overheat, cooling, group control, temperature resolution,
            // static pressure or any unknown installer fields.
            uint32_t now = millis();
            if (!slave_pipe_temp_poll_.make_request(send_buf_, now)) {
                ESP_LOGW(TAG, "SLAVE PIPE POLL: no fresh live AB/CB template; refusing to send guessed installer values");
                last_sent_recv_type_b_millis_ = now;
                return;
            }
            send_buf_[12] = calc_checksum(send_buf_);
            ESP_LOGI(TAG, "SLAVE PIPE POLL: sending 2B.80 read request from observed %s template",
                     slave_pipe_temp_poll_.template_source());
            ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
            UARTDevice::write_array(send_buf_, MsgLen);
            last_write_millis_ = millis();
            last_sent_recv_type_b_millis_ = last_write_millis_;
            slave_pipe_temp_poll_.mark_sent(last_write_millis_);
            pending_send_ = PendingSendKind::TypeBPipePoll;
            return;
        }
        if (timed) {
            ESP_LOGD(TAG, "sending timed AB message");
        }
        if (last_recv_type_b_settings_[0] != 0xCB && last_recv_type_b_settings_[0] != 0xAB) {
            ESP_LOGE(TAG, "Unexpected missing previous CB/AB message");
            pending_type_b_settings_change_ = false;
            // Don't try to send another message immediately after.
            last_sent_recv_type_b_millis_ = millis();
            return;
        }

        // Copy settings from the CB/AB message we received.
        memcpy(send_buf_, last_recv_type_b_settings_, MsgLen);
        send_buf_[0] = slave_ ? 0x2B : 0xAB;

        // Set the high bit of the second byte to request a CB message from the unit.
        if (timed) {
            send_buf_[1] |= 0x80;
        } else {
            send_buf_[1] &= ~0x80;
        }

        // Byte 2 stores installer setting 15.
        send_buf_[2] = (send_buf_[2] & 0xC7) | (overheating_ << 3);

        // Byte 2 bits 6-7 store installer setting 27.
        if (overcooling_select_ != nullptr) {
            send_buf_[2] = (send_buf_[2] & 0x3F) | (overcooling_ << 6);
        } else if (overcooling_target_ >= 0) {
            send_buf_[2] = (send_buf_[2] & 0x3F) | (overcooling_target_ << 6);
        }

        // Byte 6 bit 2 = centigrade control (installer setting 17): 0 = 1 °C, 1 = 0.5 °C.
        if (temperature_control_step_select_ != nullptr) {
            send_buf_[6] = (send_buf_[6] & ~0x04) | (temperature_control_step_ << 2);
        }

        // Byte 9 bits 6-7 = fan in cooling thermal off (installer setting 35).
        if (cooling_thermal_off_fan_select_ != nullptr) {
            send_buf_[9] = (send_buf_[9] & 0x3F) | (cooling_thermal_off_fan_ << 6);
        }

        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();

        pending_type_b_settings_change_ = false;
        pending_send_ = PendingSendKind::TypeB;
        last_sent_recv_type_b_millis_ = last_write_millis_;
    }

    void send_type_e_settings_message() {
        if (slave_) {
            ESP_LOGW(TAG, "SLAVE: blocked installer Type-E write");
            pending_type_e_settings_change_ = false;
            slave_type_e_dirty_mask_ = 0;
            return;
        }
        memset(send_buf_, 0, MsgLen);
        if (last_recv_type_e_settings_[1][0] != 0) {
            memcpy(send_buf_, last_recv_type_e_settings_[1], MsgLen);
        }
        send_buf_[0] = slave_ ? 0x2E : 0xAE;
        send_buf_[1] = 0x10;
        if (continuous_fan_switch_ != nullptr &&
            (!slave_ || (slave_type_e_dirty_mask_ & 0x02))) {
            if (continuous_fan_switch_->state) {
                send_buf_[4] |= 0x02;
            } else {
                send_buf_[4] &= ~0x02;
            }
        }
        if (temperature_based_fan_auto_switch_ != nullptr &&
            (!slave_ || (slave_type_e_dirty_mask_ & 0x04))) {
            if (temperature_based_fan_auto_switch_->state) {
                send_buf_[4] |= 0x04;
            } else {
                send_buf_[4] &= ~0x04;
            }
        }
        if (slave_ && last_recv_type_e_settings_[1][0] == 0) {
            ESP_LOGW(TAG, "SLAVE extended setting requested without CE.10 template; writing only explicitly requested bits");
        }
        send_buf_[12] = calc_checksum(send_buf_);

        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();

        pending_type_e_settings_change_ = false;
        slave_type_e_dirty_mask_ = 0;
        pending_send_ = PendingSendKind::TypeE;
    }

    void process_message(const uint8_t* buffer) {
        ESP_LOGD(TAG, "received %s", format_hex_pretty(buffer, MsgLen).c_str());

        if (calc_checksum(buffer) != buffer[12]) {
            // When initializing, the unit sends an all-zeroes message as padding between
            // messages. Ignore those false checksum failures.
            auto is_zero = [](uint8_t b) { return b == 0; };
            if (std::all_of(buffer, buffer + MsgLen, is_zero)) {
                ESP_LOGD(TAG, "Ignoring padding message sent by unit");
                return;
            }
            ESP_LOGE(TAG, "invalid checksum %s", format_hex_pretty(buffer, MsgLen).c_str());
            return;
        }

        if (pending_send_ != PendingSendKind::None && memcmp(send_buf_, buffer, MsgLen) == 0) {
            ESP_LOGD(TAG, "verified send");

            // Queue a Type A message after verifying sending the
            // status message because some units set the vane position
            // to the default setting after changing swing mode or
            // operation mode.
            if (pending_send_ == PendingSendKind::FloorConsole) {
                floor_console_tx_.echo_seen = true;
                ESP_LOGI(TAG, "FLOOR TX: UART echo verified; not an indoor acceptance");
                if (floor_console_fast_discharge_ && slave_ && floor_console_tx_.active &&
                    !floor_console_tx_.superseded &&
                    floor_console_tx_.page == lg_floor_console::Page::Discharge) {
                    floor_console_fast_complete_();
                }
            }
            if (pending_send_ == PendingSendKind::FloorConsoleReadback) {
                if (floor_fast_verify_pending_ && floor_fast_verify_sent_)
                    floor_fast_verify_echo_seen_ = true;
                ESP_LOGI(TAG, "FLOOR READBACK: 28 UART echo verified; awaiting genuine CD/CC/CE.10");
            }
            if (pending_send_ == PendingSendKind::FloorConsoleSelectiveCDProbe) {
                ESP_LOGI(TAG, "FLOOR SELECTIVE: 2D.80 UART echo verified, NOT proof of selective read or indoor acceptance; awaiting CD");
            }
            if (pending_send_ == PendingSendKind::FloorConsoleTemplateRefresh) {
                floor_console_tx_.template_refresh_echo_seen = true;
                ESP_LOGI(TAG, "FLOOR TEMPLATE: guarded 28 UART echo verified; awaiting genuine page (not confirmation of control)");
            }
            if (pending_send_ == PendingSendKind::TypeBPipePoll) {
                ESP_LOGD(TAG, "SLAVE PIPE POLL: UART echo verified; awaiting indoor CB (not proof of response)");
            }
            if (pending_send_ == PendingSendKind::TypeBDiscovery) {
                ESP_LOGD(TAG, "SLAVE PIPE DISCOVERY: UART echo verified, waiting for indoor CB");
            }
            if (pending_send_ == PendingSendKind::TypeA && slave_ && slave_fan_cal_pending_) {
                slave_fan_cal_readback_queued_ = true;
                ESP_LOGI(TAG, "SLAVE ESP CAL: 2A UART echo received; queuing verified indoor CA readback (not accepted yet)");
            }
            if (pending_send_ == PendingSendKind::TypeAReadback) {
                ESP_LOGI(TAG, "SLAVE ESP CAL: readback 28 UART echo received; awaiting indoor CA");
            }
            if (pending_send_ == PendingSendKind::Status) {
                if (operation_mode_changed_) {
                    // For a genuine Slave, the main 0x28 frame already
                    // carries the requested operating mode. A 0x2A message
                    // requires a prior AA/CA template, which some IDUs never
                    // emit. Do not queue one just because the mode changed.
                    if (!slave_) pending_type_a_settings_change_ = true;
                    operation_mode_changed_ = false;
                }
                if (!(slave_ && slave_ack_pending_)) {
                    user_changed_settings_ = false;
                } else {
                    ESP_LOGD(TAG, "SLAVE UART echo received; still awaiting indoor acceptance");
                }
            }
            pending_send_ = PendingSendKind::None;
            return;
        }

        // Determine message type.
        optional<MessageSender> sender;
        switch (buffer[0] & 0xf8) {
            case 0xC8:
                sender = MessageSender::Unit;
                break;
            case 0xA8:
                if (!slave_) {
                    // Ignore (our own?) master controller messages.
                    return;
                }
                sender = MessageSender::Master;
                break;
            case 0x28:
                if (slave_) {
                    // Ignore (our own?) slave controller messages.
                    return;
                }
                sender = MessageSender::Slave;
                break;
            default:
                return; // Unknown message sender. Ignore.
        }

        switch (buffer[0] & 0b111) {
            case 0: // 0xC8/A8/28
                process_status_message(*sender, buffer);
                break;
            case 1: // 0xC9
                process_capabilities_message(*sender, buffer);
                break;
            case 2: // 0xCA/AA/2A
                process_type_a_settings_message(*sender, buffer);
                break;
            case 3: // 0xCB/AB/2B
                process_type_b_settings_message(*sender, buffer);
                break;
            case 4: // 0xCC/AC/2C
                process_type_c_status_message(*sender, buffer);
                break;
            case 5: // 0xCD/AD/2D
                process_type_d_settings_message(*sender, buffer);
                break;
            case 6: // 0xCE/AE/2E
                process_type_e_status_message(*sender, buffer);
                break;
            case 7: // 0xCF/AF/2F
                process_type_f_status_message(*sender, buffer);
                break;
            default:
                return;
        }
    }

    // Check only the fields that were explicitly requested by HA. The
    // indoor unit need not echo every status byte exactly (for example,
    // it reports different thermistor bits than the Master).
    static bool slave_fields_match_(const uint8_t* received, const uint8_t* expected,
                                    uint8_t mask1, uint8_t mask2, uint8_t mask3,
                                    uint8_t mask5, uint8_t mask6) {
        return (((received[1] ^ expected[1]) & mask1) == 0) &&
               (((received[2] ^ expected[2]) & mask2) == 0) &&
               (((received[3] ^ expected[3]) & mask3) == 0) &&
               (((received[5] ^ expected[5]) & mask5) == 0) &&
               (((received[6] ^ expected[6]) & mask6) == 0);
    }

    // Only C8 from the indoor unit can conclude an outgoing Slave transaction.
    // Require two matching consecutive C8 reports to avoid declaring a single,
    // transient frame as durable acceptance. After confirmation, watch briefly
    // for a reversion but do NOT fight manual changes made at the wall control.
    void slave_check_confirmation_(MessageSender sender, const uint8_t* buffer) {
        if (!slave_ || sender == MessageSender::Slave) return;
        if (!slave_ack_pending_ || slave_ack_started_millis_ == 0) {
            if (sender == MessageSender::Unit && slave_recent_confirm_watch_) {
                if (millis() - slave_recent_confirm_millis_ > SLAVE_POST_CONFIRM_WATCH_MS) {
                    slave_recent_confirm_watch_ = false;
                } else if (!slave_fields_match_(buffer, slave_recent_confirm_expected_,
                           slave_recent_mask_1_, slave_recent_mask_2_, slave_recent_mask_3_,
                           slave_recent_mask_5_, slave_recent_mask_6_)) {
                    if (++slave_recent_mismatch_count_ >= 2) {
                        ESP_LOGW(TAG, "SLAVE confirmed setting reverted on indoor unit within 30 s; no automatic override");
                        slave_recent_confirm_watch_ = false;
                    }
                } else {
                    slave_recent_mismatch_count_ = 0;
                }
            }
            return;
        }
        bool matches = slave_fields_match_(buffer, slave_expected_,
                        slave_mask_1_, slave_mask_2_, slave_mask_3_,
                        slave_mask_5_, slave_mask_6_);
        // Do not keep retrying a stale request after an independently changed
        // indoor-unit state. Never conclude external takeover from an A8 frame,
        // a UART echo, or an unmarked/isolated C8 mismatch.
        const bool external_takeover = (sender == MessageSender::Unit)
            ? slave_external_change_.observe(buffer, slave_expected_, matches, millis())
            : slave_external_change_.observe_master(buffer, matches, millis());
        if (external_takeover) {
            ESP_LOGW(TAG, "SLAVE EXTERNAL CHANGE: observed changed indoor state / Master corroboration; "
                          "cancelling request #%u, no stale retries", unsigned(slave_request_id_));
            slave_external_takeover_this_frame_ = true;
            slave_cancel_pending_sleep_("newer external climate change superseded request");
            slave_ack_pending_ = false;
            slave_ack_started_millis_ = 0;
            slave_ack_attempts_ = 0;
            slave_matching_unit_frames_ = 0;
            slave_master_match_seen_ = false;
            slave_recent_confirm_watch_ = false;
            slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
            slave_mask_5_ = slave_mask_6_ = 0;
            slave_command_queued_millis_ = 0;
            pending_status_change_ = false;
            if (pending_send_ == PendingSendKind::Status) pending_send_ = PendingSendKind::None;
            operation_mode_changed_ = false;
            user_changed_settings_ = false;
            slave_external_change_.reset();
            return;
        }
        if (!matches) {
            if (sender == MessageSender::Unit) {
                slave_matching_unit_frames_ = 0;
                ESP_LOGI(TAG, "SLAVE indoor 0xC8 does not yet reflect requested settings");
            } else {
                if (slave_master_match_seen_) {
                    ESP_LOGW(TAG, "SLAVE Master 0xA8 reverted before indoor confirmation; awaiting IDU / bounded retry");
                    slave_master_match_seen_ = false;
                } else {
                    ESP_LOGD(TAG, "SLAVE Master 0xA8 does not yet reflect requested settings");
                }
            }
            return;
        }
        if (sender == MessageSender::Master) {
            if (!slave_master_match_seen_) {
                slave_master_match_seen_ = true;
                ESP_LOGI(TAG, "SLAVE Master 0xA8 now reflects request (%u ms since last TX); awaiting indoor 0xC8",
                         unsigned(millis() - last_write_millis_));
            }
            return;
        }
        if (slave_matching_unit_frames_ < 2) ++slave_matching_unit_frames_;
        if (slave_matching_unit_frames_ < 2) {
            ESP_LOGI(TAG, "SLAVE first matching indoor 0xC8; waiting for second consistent 0xC8");
            return;
        }
        if (slave_sleep_pending_ && !slave_sleep_master_confirmed_) {
            if (!slave_sleep_awaiting_master_logged_) {
                ESP_LOGI(TAG, "SLAVE SLEEP: C8 confirms reservation active bit only; awaiting genuine Master A8 duration/cancellation before acceptance");
                slave_sleep_awaiting_master_logged_ = true;
            }
            return;
        }
        if (slave_sleep_pending_) {
            if (slave_sleep_requested_minutes_ == 0) {
                ESP_LOGI(TAG, "SLAVE SLEEP CANCEL CONFIRMED: reservation cleared (Master A8 and two consistent indoor C8 status reports)");
            } else {
                ESP_LOGI(TAG, "SLAVE SLEEP SET CONFIRMED: %u-minute request accepted (Master A8 and two consistent indoor C8 status reports)",
                         unsigned(slave_sleep_requested_minutes_));
            }
            slave_sleep_pending_ = false;
            slave_sleep_master_ever_confirmed_ = false;
            slave_sleep_override_.reset();
            slave_sleep_awaiting_master_logged_ = false;
            slave_restore_master_sleep_();
        }
        ESP_LOGI(TAG, "SLAVE TX CONFIRMED by two indoor 0xC8 reports after %u attempt(s) (%u ms after TX)",
                 unsigned(slave_ack_attempts_), unsigned(millis() - last_write_millis_));
        memcpy(slave_recent_confirm_expected_, slave_expected_, MsgLen);
        slave_recent_mask_1_ = slave_mask_1_;
        slave_recent_mask_2_ = slave_mask_2_;
        slave_recent_mask_3_ = slave_mask_3_;
        slave_recent_mask_5_ = slave_mask_5_;
        slave_recent_mask_6_ = slave_mask_6_;
        slave_recent_confirm_watch_ = true;
        slave_recent_confirm_millis_ = millis();
        slave_recent_mismatch_count_ = 0;
        slave_ack_pending_ = false;
        slave_external_change_.reset();
        slave_ack_started_millis_ = 0;
        slave_ack_attempts_ = 0;
        slave_matching_unit_frames_ = 0;
        slave_master_match_seen_ = false;
        slave_command_queued_millis_ = 0;
        slave_mask_1_ = slave_mask_2_ = slave_mask_3_ = 0;
        slave_mask_5_ = slave_mask_6_ = 0;
        user_changed_settings_ = false;
    }
    void process_status_message(MessageSender sender, const uint8_t* buffer) {
        if (slave_ && sender == MessageSender::Master) {
            memcpy(last_master_status_, buffer, MsgLen);
            have_master_status_ = true;
            last_master_status_for_pipe_millis_ = millis();
            const auto reservation = LgSleepReservation::decode(buffer);
            if (slave_diagnostic_sleep_watch_) {
                const uint32_t elapsed = uint32_t(millis() - slave_diagnostic_sleep_sent_at_);
                if (elapsed <= 120000u &&
                    (!reservation.active ||
                     reservation.form == LgSleepReservation::Form::Minutes)) {
                    if (!reservation.active) {
                        ESP_LOGW(TAG, "SLAVE DIAGNOSTIC SLEEP CHECK: Master reports reservation CLEARED "
                                      "after experimental query; check HA automation and controller logs");
                    } else if (reservation.minutes > slave_diagnostic_sleep_minutes_ + 1u) {
                        ESP_LOGW(TAG, "SLAVE DIAGNOSTIC SLEEP CHECK: Master reports %u min, "
                                      "previous snapshot %u min; possible external timer reset",
                                      unsigned(reservation.minutes), unsigned(slave_diagnostic_sleep_minutes_));
                    } else {
                        ESP_LOGI(TAG, "SLAVE DIAGNOSTIC SLEEP CHECK: genuine Master STILL reports "
                                      "active reservation (%u min); no ESP sleep command issued",
                                      unsigned(reservation.minutes));
                    }
                    slave_diagnostic_sleep_watch_ = false;
                }
            }
            if (reservation.active &&
                reservation.form == LgSleepReservation::Form::Minutes &&
                reservation.minutes > 0 && reservation.minutes <= 480 &&
                !(buffer[8] & 0xC0u)) {
                memcpy(last_master_duration_status_, buffer, MsgLen);
                have_master_duration_status_ = true;
                last_master_duration_status_millis_ = millis();
            }
        }
        if (slave_ && sender == MessageSender::Unit) {
            memcpy(last_unit_status_for_pipe_, buffer, MsgLen);
            have_unit_status_for_pipe_ = true;
            last_unit_status_for_pipe_millis_ = millis();
        }
        slave_external_takeover_this_frame_ = false;
        if (slave_ && sender != MessageSender::Slave) {
            const auto power_change = slave_sleep_power_state_.observe(buffer);
            if (power_change == LgSlaveSleepSync::PowerState::Change::TurnedOff) {
                slave_clear_sleep_on_power_off_(sender == MessageSender::Master ? "Master A8" : "indoor C8");
            } else if (power_change == LgSlaveSleepSync::PowerState::Change::TurnedOn) {
                ESP_LOGI(TAG, "SLAVE SLEEP: Master and indoor unit both report AC ON; accepting new sleep reservations again");
            }
        }
        if (slave_ && sender == MessageSender::Master &&
            !slave_sleep_power_state_.off_latched) {
            // Ignore stale reservation data while real LG power is OFF.
            slave_observe_master_sleep_(buffer);
        }
        slave_check_confirmation_(sender, buffer);
        // Track instances of repeated status messages.  If the same
        // status message arrives enough times without the changed
        // settings bit set, accept it anyway, since a previous status
        // message with the changed settings bit set may have been
        // corrupted.
        if (memcmp(repeated_status_, buffer, MsgLen) == 0) {
            if (repeated_status_count_ < 255) {
                repeated_status_count_++;
            };
        } else {
            memcpy(repeated_status_, buffer, MsgLen);
            repeated_status_count_ = 1;
        }

        // To protect against corrupted messages, only allow settings
        // changes if this is the first status message received from
        // the LG unit, or if the settings changed bit is set.  If
        // settings are unexpectedly changed, ignore the entire
        // message.  However, if the same message without the settings
        // changed bit arrives sufficiently many times, accept it
        // anyway since the original change message may have been
        // corrupted.
        bool repeated_status_override =
            repeated_status_count_ >= repeated_status_threshold_;
        bool settings_changed = (buffer[1] & 0x01) != 0;
        bool changes_allowed = waiting_for_first_status || settings_changed ||
            repeated_status_override || slave_external_takeover_this_frame_;

        uint8_t b = buffer[1];
        climate::ClimateMode new_mode;
        if ((b & 0x2) == 0) {
            new_mode = climate::CLIMATE_MODE_OFF;
        } else {
            uint8_t mode_val = (b >> 2) & 0b111;
            switch (mode_val) {
                case 0:
                    new_mode = climate::CLIMATE_MODE_COOL;
                    break;
                case 1:
                    new_mode = climate::CLIMATE_MODE_DRY;
                    break;
                case 2:
                    new_mode = climate::CLIMATE_MODE_FAN_ONLY;
                    break;
                case 3:
                    new_mode = climate::CLIMATE_MODE_HEAT_COOL;
                    break;
                case 4:
                    new_mode = climate::CLIMATE_MODE_HEAT;
                    break;
                default:
                    ESP_LOGE(TAG, "received invalid operation mode from AC (%u)", mode_val);
                    return;
            }
        }

        uint8_t fan_val = b >> 5;
        climate::ClimateFanMode new_fan_mode;
        switch (fan_val) {
            case 0:
                new_fan_mode = climate::CLIMATE_FAN_LOW;
                break;
            case 1:
                new_fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;
            case 2:
                new_fan_mode = climate::CLIMATE_FAN_HIGH;
                break;
            case 3:
                new_fan_mode = climate::CLIMATE_FAN_AUTO;
                break;
            case 4:
                new_fan_mode = climate::CLIMATE_FAN_QUIET;
                break;
            case 5: // low-medium, see unit_fan_raw_
                new_fan_mode = climate::CLIMATE_FAN_LOW;
                break;
            case 6: // medium-high, see unit_fan_raw_
                new_fan_mode = climate::CLIMATE_FAN_MEDIUM;
                break;
            case 7:
                new_fan_mode = climate::CLIMATE_FAN_FOCUS;
                break;
            default:
                ESP_LOGE(TAG, "received unexpected fan mode from AC (%u)", fan_val);
                return;
        }

        bool horiz_swing = buffer[2] & 0x40;
        bool vert_swing = buffer[2] & 0x80;
        climate::ClimateSwingMode new_swing_mode;
        if (horiz_swing && vert_swing) {
            new_swing_mode = climate::CLIMATE_SWING_BOTH;
        } else if (horiz_swing) {
            new_swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        } else if (vert_swing) {
            new_swing_mode = climate::CLIMATE_SWING_VERTICAL;
        } else {
            new_swing_mode = climate::CLIMATE_SWING_OFF;
        }

        float target = float((buffer[6] & 0xf) + 15);
        if (buffer[5] & 0x1) {
            target += 0.5;
        }
        if (fahrenheit_) {
            target = TempConversion::lgcelsius_to_celsius(target);
        }

        bool new_purifier = buffer[2] & 0x4;
        bool new_active_reservation = buffer[3] & 0x10;

        // Make all the changes if changes are allowed.
        bool mode_changed = this->mode != new_mode;
        bool fan_mode_changed = this->fan_mode != new_fan_mode;
        bool swing_mode_changed = this->swing_mode != new_swing_mode;
        bool set_temperature_changed =
            !float_equal(this->target_temperature, target);
        bool purifier_changed = purifier_.state != new_purifier;
        bool reservation_changed = active_reservation_ != new_active_reservation;
        // Don't update our settings if we have a pending change/send,
        // because else we overwrite changes we still have to send (or
        // are sending) to the AC.
        if (user_changed_settings_ &&
            (pending_status_change_ ||
             pending_send_ == PendingSendKind::Status ||
             (slave_ && slave_ack_pending_))) {
            // An HA command can arrive before the first bus status. Do not
            // overwrite that request, but do release the initialization gate;
            // otherwise the pending command can never transmit (deadlock).
            if (slave_ && is_initializing_ && waiting_for_first_status) {
                // Store the first authoritative frame as the template for
                // preserved fields in outgoing status frames. We must not
                // overwrite the HA-requested climate values on this path.
                memcpy(last_recv_status_, buffer, MsgLen);
                is_initializing_ = false;
                waiting_for_first_status = false;
                ESP_LOGI(TAG, "SLAVE startup: first status received; preserving queued user command");
            }
            ESP_LOGD(TAG, "ignoring incoming settings changes because "
                     "of an outgoing pending change");
            return;
        }

        if (changes_allowed) {
            if (mode_changed || fan_mode_changed || swing_mode_changed ||
                set_temperature_changed || purifier_changed ||
                reservation_changed) {

                if (!settings_changed && repeated_status_override) {
                    ESP_LOGW(TAG, "accepting settings change without "
                             "changed bit set after the same status "
                             "message has been received consecutively "
                             "%d times", repeated_status_count_);
                }

                this->mode = new_mode;
                this->fan_mode = new_fan_mode;
                if (fan_val == 5 || fan_val == 6) {
                    unit_fan_raw_ = fan_val;
                    unit_fan_mapped_ = new_fan_mode;
                } else {
                    unit_fan_raw_ = 0xFF;
                }
                if (swing_mode_changed) {
                    // Avoid calling set_swing_mode unless the swing
                    // mode has actually changed, since it sends a
                    // 0xAA message.
                    set_swing_mode(new_swing_mode);
                }
                this->target_temperature = target;
                ignore_callbacks_ = true;
                purifier_.publish_state(buffer[2] & 0x4);
                ignore_callbacks_ = false;
                if (!slave_ || sender == MessageSender::Master)
                    active_reservation_ = (buffer[3] & 0x10) != 0 &&
                        !(slave_ && slave_sleep_power_state_.off_latched);
            }
        } else {
            std::vector<std::string> changes;
            if (mode_changed) {
                changes.push_back("mode");
            }
            if (fan_mode_changed) {
                changes.push_back("fan mode");
            }
            if (swing_mode_changed) {
                changes.push_back("swing mode");
            }
            if (set_temperature_changed) {
                changes.push_back("set temperature");
            }
            if (purifier_changed) {
                changes.push_back("purifier");
            }
            if (reservation_changed) {
                changes.push_back("timer reservation");
            }

            if (changes.size() > 0) {
                std::string change_str;
                bool first = true;
                for (const std::string& change : changes) {
                    if (first) {
                        first = false;
                    } else {
                        change_str += ", ";
                    }
                    change_str += change;
                }

                const char* times = "time";
                if (repeated_status_count_ > 1) {
                    times = "times";
                }
                ESP_LOGE(TAG, "ignoring status message with unexpected "
                         "changes without changed bit set: %s; "
                         "exact message received consecutively %d %s "
                         "(will be accepted after %d times)",
                         change_str.c_str(), repeated_status_count_,
                         times, repeated_status_threshold_);
                return;
            }
        }

        // Consider slave controller initialized if we received a
        // status message from the other controller or the unit.
        if (slave_ && is_initializing_) {
            is_initializing_ = false;
            ESP_LOGI(TAG, "SLAVE startup: synchronized with first Master/IDU status");
        }
        waiting_for_first_status = false;

        defrost_.publish_state(buffer[3] & 0x4);
        bool preheating = buffer[3] & 0x8;
        preheat_.publish_state(preheating);

        if (sender == MessageSender::Unit) {
            error_code_.publish_state(buffer[11]);
        }

        // When turning on the outdoor unit, the AC sometimes reports ON => OFF => ON within a
        // few seconds. No big deal but it causes noisy state changes in HA. Only report OFF if
        // the last state change was at least 8 seconds ago.
        bool outdoor_on = buffer[5] & 0x4;
        bool outdoor_changed = outdoor_.state != outdoor_on;
        if (outdoor_on) {
            outdoor_.publish_state(true);
        } else if (millis() - last_outdoor_change_millis_ > 8000) {
            outdoor_.publish_state(false);
        }
        if (outdoor_changed) {
            last_outdoor_change_millis_ = millis();
        }

        if (sender == MessageSender::Unit && !auto_dry_.is_internal()) {
            bool unit_off = (buffer[1] & 0x2) == 0;
            bool drying = (buffer[10] & 0x10) && unit_off;
            auto_dry_active_.publish_state(drying);
        }

        bool read_temp = false;
        if (slave_) {
            // Let the slave controller report the temperature from the master.
            read_temp = (sender == MessageSender::Master);
        } else {
            // Report the unit's room temperature only if we're using the internal thermistor.
            // With an external temperature sensor, some units report the temperature we sent and
            // others always send the internal temperature.
            read_temp = (sender == MessageSender::Unit && internal_thermistor_.state);
        }
        if (read_temp) {
            float room_temp = float(buffer[7] & 0x3F) / 2 + 10;
            if (fahrenheit_) {
                room_temp = TempConversion::lgcelsius_to_celsius(room_temp);
            }
            if (!float_equal(this->current_temperature, room_temp)) {
                this->current_temperature = room_temp;
                publish_state();
            }
        }

        // NOTE: PR #172 (upstream) had early "ignoring because pending
        // change/send" returns here (each still updating the action
        // telemetry before bailing), matching the pre-#168 control flow.
        // #168's rearchitecture already removed the equivalent guard from
        // this location -- the same "don't overwrite a pending outgoing
        // change" concern is now handled earlier in this function via
        // user_changed_settings_. Reintroducing pending_status_change_/
        // pending_send_ checks here would be redundant against that and
        // would double-guard against #168's now-different semantics for
        // those flags. Net effect of dropping this hunk: action telemetry
        // simply updates on the next status message instead of this one,
        // in the narrow case where this one was a self-echo of a pending
        // change -- not lost, just deferred by one cycle. The normal-path
        // set_action_from_status_() call below (unconflicted, from #172)
        // still runs every time.
        if (sender != MessageSender::Slave) {
            memcpy(last_recv_status_, buffer, MsgLen);
        }

        if (!slave_ && sender == MessageSender::Unit) {
            if (new_mode == climate::CLIMATE_MODE_OFF && changes_allowed) {
                master_clear_sleep_on_off_("indoor C8");
            }
            if (master_sleep_command_waiting_ && master_sleep_last_tx_millis_ != 0) {
                // A genuine C8 after transmission is useful evidence,
                // unlike UART echo or status which predates the request. Do not
                // claim an accepted cancel until C8 drops its sleep flag.
                bool flag = (buffer[3] & 0x10) != 0;
                if ((master_sleep_command_was_cancel_ && !flag) ||
                    (!master_sleep_command_was_cancel_ && flag)) {
                    ESP_LOGI(TAG, "MASTER SLEEP: indoor C8 reflects %s reservation",
                             flag ? "active" : "cancelled");
                    master_sleep_command_waiting_ = false;
                } else if (master_sleep_last_tx_millis_ &&
                           uint32_t(millis() - master_sleep_last_tx_millis_) > 90000u) {
                    ESP_LOGW(TAG, "MASTER SLEEP: no matching indoor reservation flag observed within 90 seconds");
                    master_sleep_command_waiting_ = false;
                }
            }
        }

        // Sleep reservation decoding: only a real minutes-form payload has
        // a remaining duration. 18.10.84 is a flag saying sleep is enabled,
        // NOT a timer reading of 16 minutes.
        if (!slave_) {
            const auto sleep = LgSleepReservation::decode(buffer);
            ignore_callbacks_ = true;
            if (!active_reservation_ && !master_sleep_command_waiting_)
                sleep_timer_.publish_state(0);
            else if (active_reservation_ && sleep.form == LgSleepReservation::Form::Minutes &&
                     sleep.minutes <= 480 && !master_sleep_command_waiting_)
                sleep_timer_.publish_state(sleep.minutes);
            ignore_callbacks_ = false;
        }
        // Slave is updated ONLY from the actual Master A8, by
        // slave_observe_master_sleep_(), not from arbitrary indoor C8 frames.

        set_action_from_status_(outdoor_.state, preheating, defrost_.state, buffer[7]);
        publish_state();
    }

    // Home Assistant learns supported climate controls from the API entity-info
    // message on connect, not from publish_state(). Refresh just the HA API
    // connection when our advertised traits change; NEVER restart the ESP32.
    // Other clients (e.g., ESPHome live logs) are not disconnected.
    void schedule_home_assistant_capability_refresh_() {
#ifdef USE_API
        // Named timeout coalesces repeated changes; the C9 decoder itself has
        // already required two consecutive identical unit frames.
        this->set_timeout("lg_capabilities_ha_refresh", 5000, [this]() {
            if (api::global_api_server == nullptr) return;
            uint8_t count = 0;
            for (const auto &client : api::global_api_server->active_clients()) {
                const char *name = client->get_name();
                if (name != nullptr && std::strstr(name, "Home Assistant") != nullptr) {
                    // Mark only HA's socket for closure. The API server will
                    // safely remove it in its loop; HA will reconnect and
                    // re-request the climate entity information.
                    client->on_fatal_error();
                    ++count;
                }
            }
            if (count != 0)
                ESP_LOGI(TAG, "LG capabilities: refreshing Home Assistant API connection (%u client(s)); ESP32 remains running", static_cast<unsigned>(count));
        });
#endif
    }

    void process_capabilities_message(MessageSender sender, const uint8_t* buffer) {
        if (sender != MessageSender::Unit) {
            ESP_LOGE(TAG, "ignoring capabilities message not from unit");
            return;
        }
        if (floor_console_monitoring_) {
            const char *value = lg_floor_console::supports_comfort_cooling(buffer)
                ? "Advertised in C9" : "Not advertised in C9";
            floor_console_publish_observation_(floor_console_comfort_capability_,
                "Indoor C9 comfort capability", value, buffer);
        }

        const auto result = capability_refresh_.observe(buffer, nvs_storage_.capabilities_message);
        if (result == LgCapabilityRefresh::Result::WAITING_FOR_REPEAT) {
            ESP_LOGV(TAG, "LG capabilities: candidate C9 received; awaiting matching repeat");
        } else if (result == LgCapabilityRefresh::Result::ACCEPTED_WITHOUT_TRAIT_CHANGE ||
                   result == LgCapabilityRefresh::Result::ACCEPTED_WITH_TRAIT_CHANGE) {
            // Persist every confirmed C9 change, including differences in bits
            // which don't affect the HA controls. No changes to NVS format/key.
            ESPPreferenceObject pref = global_preferences->make_preference<NVSStorage>(
                this->get_object_id_hash() ^ NVS_STORAGE_VERSION);
            if (!pref.save(&nvs_storage_)) {
                ESP_LOGW(TAG, "LG capabilities: failed to save confirmed C9 to preferences");
            } else {
                global_preferences->sync();
            }

            if (result == LgCapabilityRefresh::Result::ACCEPTED_WITH_TRAIT_CHANGE) {
                // Live trait application. No UART command or ESP reboot.
                configure_capabilities();
                publish_state();
                ESP_LOGI(TAG, "LG capabilities: updated supported climate controls live; Home Assistant API rediscovery scheduled (no ESP reboot)");
                schedule_home_assistant_capability_refresh_();
            } else {
                ESP_LOGD(TAG, "LG capabilities: saved confirmed C9 update; advertised controls unchanged");
            }
        }

        is_initializing_ = false;
    }

    void process_type_a_settings_message(MessageSender sender, const uint8_t* buffer) {
        // Only a genuine indoor CA can confirm a calibration change.
        if (slave_ && sender == MessageSender::Unit && buffer[0] == 0xCA) {
            // The true indoor CA carries the two-digit HEX Central Control
            // Address at byte 1 (installer setting 02). Never use Master AA,
            // Slave echo or inferred LGAP zone order as evidence.
            // This only observes incoming traffic, requiring two agreeing CA
            // reports before publishing. No new polling/writes are scheduled.
            if (slave_central_control_address_.observe(buffer[1])) {
                // One read-only HA text diagnostic: e.g. 0A / 10.
                // Keep the two-genuine-indoor-CA confirmation and bus traffic unchanged.
                char address_text[9];
                SlaveCentralControlAddress::format_display(slave_central_control_address_.address(), address_text);
                publish_if_changed_(installer_central_control_address_status_, address_text);
                ESP_LOGI(TAG, "SLAVE INSTALLER: Central Control Address confirmed: %s", address_text);
            }
            slave_unit_ca_seen_ = true;
            slave_unit_ca_millis_ = millis();
            if (slave_fan_cal_pending_ && slave_fan_cal_readback_millis_ != 0 &&
                static_cast<int32_t>(millis() - slave_fan_cal_readback_millis_) >= 0) {
                if (buffer[2 + slave_fan_cal_index_] == slave_fan_cal_value_) {
                    slave_fan_cal_mismatching_ca_ = 0;
                    if (++slave_fan_cal_matching_ca_ >= 2)
                        slave_fan_cal_finish_(true, "confirmed");
                } else {
                    slave_fan_cal_matching_ca_ = 0;
                    if (++slave_fan_cal_mismatching_ca_ >= 2)
                        slave_fan_cal_finish_(false, "indoor CA kept a different value");
                }
            }
        }
        // Send settings the first time we receive a 0xCA message.
        if (sender != MessageSender::Slave) {
            bool first_time = last_recv_type_a_settings_[0] == 0;
            memcpy(last_recv_type_a_settings_, buffer, MsgLen);
            if (first_time && !slave_) {
                pending_type_a_settings_change_ = true;
            }
        }

        // Handle vane 1 position change
        uint8_t vane1 = buffer[7] & 0x0F;
        if (vane1 <= 6) {
            vane_position_[0] = vane1;
            vane_select_1_.publish_state(*vane_select_1_.at(vane_wire_to_option_(vane1)));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 1 position: %u", vane1);
        }

        // Handle vane 2 position change
        uint8_t vane2 = (buffer[7] >> 4) & 0x0F;
        if (vane2 <= 6) {
            vane_position_[1] = vane2;
            vane_select_2_.publish_state(*vane_select_2_.at(vane_wire_to_option_(vane2)));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 2 position: %u", vane2);
        }

        // Handle vane 3 position change
        uint8_t vane3 = buffer[8] & 0x0F;
        if (vane3 <= 6) {
            vane_position_[2] = vane3;
            vane_select_3_.publish_state(*vane_select_3_.at(vane_wire_to_option_(vane3)));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 3 position: %u", vane3);
        }

        // Handle vane 4 position change
        uint8_t vane4 = (buffer[8] >> 4) & 0x0F;
        if (vane4 <= 6) {
            vane_position_[3] = vane4;
            vane_select_4_.publish_state(*vane_select_4_.at(vane_wire_to_option_(vane4)));
        } else {
            ESP_LOGE(TAG, "Unexpected vane 4 position: %u", vane4);
        }

        ignore_callbacks_ = true;
        auto_dry_.publish_state(buffer[11] & 0x8);
        ignore_callbacks_ = false;

        if (sender != MessageSender::Slave) {
            // Handle fan speed 0 (slow) change
            if (!slave_fan_cal_pending_ || slave_fan_cal_index_ != 0) {
                fan_speed_[0] = buffer[2];
                fan_speed_slow_.publish_state(fan_speed_[0]);
            }
            if (!slave_fan_cal_pending_ || slave_fan_cal_index_ != 1) {
                fan_speed_[1] = buffer[3];
                fan_speed_low_.publish_state(fan_speed_[1]);
            }
            if (!slave_fan_cal_pending_ || slave_fan_cal_index_ != 2) {
                fan_speed_[2] = buffer[4];
                fan_speed_medium_.publish_state(fan_speed_[2]);
            }
            if (!slave_fan_cal_pending_ || slave_fan_cal_index_ != 3) {
                fan_speed_[3] = buffer[5];
                fan_speed_high_.publish_state(fan_speed_[3]);
            }

            if (slave_ && sender == MessageSender::Unit) {
                if (installer_fan_speed_slow_status_ != nullptr) installer_fan_speed_slow_status_->publish_state(buffer[2]);
                if (installer_fan_speed_low_status_ != nullptr) installer_fan_speed_low_status_->publish_state(buffer[3]);
                if (installer_fan_speed_medium_status_ != nullptr) installer_fan_speed_medium_status_->publish_state(buffer[4]);
                if (installer_fan_speed_high_status_ != nullptr) installer_fan_speed_high_status_->publish_state(buffer[5]);
            }
        }
    }

    void process_type_b_settings_message(MessageSender sender, const uint8_t* buffer) {
        // Preserve real, checksum-verified AB/CB fields as a Type-B template.
        // The genuine Master AB is NOT treated as an indoor-unit reading.
        if (slave_ && sender == MessageSender::Master && buffer[0] == 0xAB) {
            bool first_template = !slave_pipe_temp_poll_.has_template();
            slave_pipe_temp_poll_.observe(buffer, millis());
            last_sent_recv_type_b_millis_ = millis();
            if (first_template) {
                ESP_LOGI(TAG, "SLAVE PIPE POLL: learned live Type-B template from genuine Master AB");
            }
        }
        if (sender != MessageSender::Unit) {
            return;
        }
        if (slave_ && slave_pipe_discovery_waiting_cb_) {
            if (static_cast<uint32_t>(millis() - slave_pipe_discovery_sent_millis_) <=
                SlavePipeTempDiscovery::RESPONSE_WINDOW_MS) {
                ESP_LOGI(TAG, "SLAVE PIPE DISCOVERY: indoor CB received following one-time settings refresh");
            }
            slave_pipe_discovery_waiting_cb_ = false;
        }
        if (slave_) {
            bool first_template = !slave_pipe_temp_poll_.has_template();
            bool solicited_response = slave_pipe_temp_poll_.observe(buffer, millis());
            if (solicited_response) {
                ESP_LOGI(TAG, "SLAVE PIPE POLL: indoor CB response received; updating pipe temperatures");
            } else if (first_template) {
                ESP_LOGI(TAG, "SLAVE PIPE POLL: learned live Type-B template from indoor CB");
            }
        }

        // Send installer settings the first time we receive a 0xCB message.
        bool first_time = last_recv_type_b_settings_[0] == 0;
        memcpy(last_recv_type_b_settings_, buffer, MsgLen);
        if (first_time && !slave_) {
            pending_type_b_settings_change_ = true;
        }

        last_sent_recv_type_b_millis_ = millis();

        uint8_t overheating = (buffer[2] >> 3) & 0b111;
        if (overheating <= 4) {
            overheating_ = overheating;
            overheating_select_.publish_state(*overheating_select_.at(overheating));
        } else {
            ESP_LOGE(TAG, "Unexpected overheating value: %u", overheating);
        }

        if (slave_ && overheating <= 4) {
            static const char* const NAMES[] = {"0 (Default)", "1 (+4C/+6C)", "2 (+2C/+4C)",
                                                "3 (-1C/+1C)", "4 (-0.5C/+0.5C)"};
            publish_if_changed_(installer_overheating_status_, NAMES[overheating]);
        }

        // Byte 2 bits 6-7 = over cooling (installer setting 27)
        uint8_t overcooling = (buffer[2] >> 6) & 0b11;
        overcooling_ = overcooling;
        if (slave_) {
            static const char* const NAMES[] = {"0 (Default: +0.5C/-0.5C)", "1 (+6C/+4C)",
                                                "2 (+4C/+2C)", "3 (+1C/-1C)"};
            publish_if_changed_(installer_overcooling_status_, NAMES[overcooling]);
        }
        if (overcooling_select_ != nullptr) {
            ignore_callbacks_ = true;
            overcooling_select_->publish_state(*overcooling_select_->at(overcooling));
            ignore_callbacks_ = false;
        } else if (!slave_ && overcooling_target_ >= 0 && overcooling != overcooling_target_) {
            if (overcooling_write_attempts_ < 3) {
                overcooling_write_attempts_++;
                ESP_LOGD(TAG, "Setting over cooling installer setting: %u -> %d (attempt %u)",
                         overcooling, overcooling_target_, overcooling_write_attempts_);
                pending_type_b_settings_change_ = true;
            } else if (overcooling_write_attempts_ == 3) {
                overcooling_write_attempts_++;
                ESP_LOGE(TAG, "Unit did not accept over cooling %d (still %u)", overcooling_target_, overcooling);
            }
        }
        // Byte 9 bits 6-7 = fan in cooling thermal off (installer setting 35).
        uint8_t thermal_off_fan = (buffer[9] >> 6) & 0b11;
        if (thermal_off_fan <= 2) {
            cooling_thermal_off_fan_ = thermal_off_fan;
            if (slave_) {
                static const char* const NAMES[] = {"0 (Fan Low)", "1 (Fan Off)", "2 (Fan Setting)"};
                publish_if_changed_(installer_cooling_thermal_off_fan_status_, NAMES[thermal_off_fan]);
            }
            if (cooling_thermal_off_fan_select_ != nullptr) {
                ignore_callbacks_ = true;
                cooling_thermal_off_fan_select_->publish_state(*cooling_thermal_off_fan_select_->at(thermal_off_fan));
                ignore_callbacks_ = false;
            }
        }
        // Byte 6 bit 2 = centigrade control (installer setting 17): 0 = 1 °C, 1 = 0.5 °C.
        uint8_t step_half = (buffer[6] >> 2) & 0b1;
        temperature_control_step_ = step_half;
        if (slave_) publish_if_changed_(installer_temperature_control_step_status_,
                                       step_half ? "1 (0.5C)" : "0 (1C)");
        if (temperature_control_step_select_ != nullptr) {
            ignore_callbacks_ = true;
            temperature_control_step_select_->publish_state(*temperature_control_step_select_->at(step_half));
            ignore_callbacks_ = false;
        }
        // Byte 10 bits 0-3 = static pressure step (installer setting 32).
        if (static_pressure_step_ != nullptr) {
            static_pressure_step_->publish_state(buffer[10] & 0x0F);
        }

        // Table mapping a byte value to degrees Celsius based on values displayed by PREMTB100.
        // INT8_MIN indicates an invalid value.
        static constexpr int8_t PipeTempTable[] = {
            /* 0x00 */ INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, 108, 104, 101, 100, 98, 95,
            /* 0x10 */ 93, 91, 89, 87, 85, 84, 82, 81, 79, 78, 76, 75, 74, 73, 72, 71,
            /* 0x20 */ 70, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 60, 59, 58, 57, 57,
            /* 0x30 */ 56, 55, 55, 54, 53, 53, 52, 52, 51, 50, 50, 49, 49, 48, 47, 47,
            /* 0x40 */ 46, 46, 45, 45, 44, 44, 43, 43, 42, 42, 41, 41, 40, 40, 39, 39,
            /* 0x50 */ 39, 38, 38, 37, 37, 36, 36, 36, 35, 35, 34, 34, 33, 33, 33, 32,
            /* 0x60 */ 32, 31, 31, 31, 30, 30, 30, 29, 29, 29, 28, 28, 27, 27, 27, 26,
            /* 0x70 */ 26, 26, 25, 25, 24, 24, 24, 23, 23, 23, 22, 22, 22, 21, 21, 21,
            /* 0x80 */ 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15,
            /* 0x90 */ 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 10, 10,
            /* 0xa0 */ 10, 9, 9, 9, 8, 8, 8, 7, 7, 6, 6, 6, 5, 5, 5, 4,
            /* 0xb0 */ 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0, -1,
            /* 0xc0 */ -1, -2, -2, -2, -3, -3, -4, -4, -5, -5, -5, -6, -6, -7, -7, -8,
            /* 0xd0 */ -8, -9, -9, -9, -10, -10, -11, -11, -12, -12, -13, -14, -14, -15, -15, -16,
            /* 0xe0 */ -16, -17, -18, -18, -19, -20, -20, -21, -22, -22, -23, -24, -25, -26, -27,
                       -28,
            /* 0xf0 */ -29, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN,
                       INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN, INT8_MIN
        };
        static_assert(sizeof(PipeTempTable) == 256);
        static_assert(PipeTempTable[UINT8_MAX] == INT8_MIN);

        int8_t pipe_temp_in = PipeTempTable[buffer[3]];
        if (pipe_temp_in != INT8_MIN) {
            pipe_temp_in_.publish_state(pipe_temp_in);
        }

        int8_t pipe_temp_out = PipeTempTable[buffer[4]];
        if (pipe_temp_out != INT8_MIN) {
            pipe_temp_out_.publish_state(pipe_temp_out);
        }

        int8_t pipe_temp_mid = PipeTempTable[buffer[5]];
        if (pipe_temp_mid != INT8_MIN) {
            pipe_temp_mid_.publish_state(pipe_temp_mid);
        }
    }

    static float decode_power_uint24_(const uint8_t* buffer, size_t offset, float divisor) {
        // Plain big-endian 24-bit integers, per the #170 review: CF.02.00.02.29 = 553 W matched
        // the LG app (0.55 kW). The earlier nibble-decimal decode made the 0xCC counter go
        // backwards when a low nibble rolled F->0 (0x02CF -> 0x02D0 read as 33.5 -> 33.0 kWh).
        uint32_t result = (uint32_t(buffer[offset]) << 16) | (uint32_t(buffer[offset + 1]) << 8) |
                          buffer[offset + 2];
        return result / divisor;
    }

    void process_type_c_status_message(MessageSender sender, const uint8_t* buffer) {
        floor_console_observe_page_(sender, 4, buffer);
        floor_console_observe_write_(sender, lg_floor_console::Page::Comfort, buffer);
        if (sender != MessageSender::Unit) {
            return;
        }
        // Verified indoor CC filter remaining hours (12-bit): byte 1 plus
        // low nibble of byte 2. The upper nibble is NOT part of filter life.
        // Preserve the existing energy telemetry below without modification.
        if (filter_time_remaining_decoder_.observe_indoor_cc(buffer)) {
            const uint16_t hours = filter_time_remaining_decoder_.hours();
            if (filter_time_remaining_ != nullptr)
                filter_time_remaining_->publish_state(static_cast<float>(hours));
            if (filter_remaining_percent_ != nullptr && filter_nominal_life_hours_ > 0) {
                float pct = (100.0f * static_cast<float>(hours)) / static_cast<float>(filter_nominal_life_hours_);
                if (pct > 100.0f) pct = 100.0f;
                filter_remaining_percent_->publish_state(pct);
            }
            ESP_LOGI(TAG, "LG FILTER: confirmed genuine indoor CC remaining time: %u hours", unsigned(hours));
        }
        if (power_consumption_ != nullptr) {
            power_consumption_->publish_state(decode_power_uint24_(buffer, 3, 10.0f));
        }
    }

    void process_type_f_status_message(MessageSender sender, const uint8_t* buffer) {
        if (sender != MessageSender::Unit) {
            return;
        }
        if (current_power_ != nullptr) {
            current_power_->publish_state(decode_power_uint24_(buffer, 2, 1000.0f));
        }
    }

    static uint16_t decode_uint16_(const uint8_t* buffer, size_t offset) {
        return (uint16_t(buffer[offset]) << 8) | buffer[offset + 1];
    }

    static unsigned floor_page_index_(lg_floor_console::Page page) {
        return static_cast<unsigned>(page) - 4u;
    }

    void floor_console_report_(const std::string &message) {
        publish_if_changed_(floor_console_command_status_, message);
        ESP_LOGI(TAG, "FLOOR TX: %s", message.c_str());
    }

    void floor_console_publish_control_(lg_floor_console::Page page, uint8_t code) {
        if (!lg_floor_console::valid_value(page, code)) return;
        ignore_callbacks_ = true;
        switch (page) {
            case lg_floor_console::Page::Comfort:
                if (floor_console_comfort_control_ != nullptr)
                    floor_console_comfort_control_->publish_state(code != 0);
                break;
            case lg_floor_console::Page::Discharge:
                if (floor_console_direction_control_ != nullptr)
                    floor_console_direction_control_->publish_state(*floor_console_direction_control_->at(code));
                break;
            case lg_floor_console::Page::Step:
                if (floor_console_step_control_ != nullptr)
                    floor_console_step_control_->publish_state(*floor_console_step_control_->at(code));
                break;
        }
        ignore_callbacks_ = false;
    }

    // -1: no usable source; 0: Master; 1: indoor unit.
    // Prefer the freshest valid page. The physical Master may only emit AD
    // when a user operates its menu; CD after a forced readback is newer.
    int floor_console_source_index_(lg_floor_console::Page page, uint32_t now) const {
        const unsigned p = floor_page_index_(page);
        const bool master_fresh = slave_ && floor_console_template_valid_[p][0] &&
            uint32_t(now - floor_console_template_at_[p][0]) <= FLOOR_TEMPLATE_MAX_AGE;
        const uint32_t indoor_limit = page == lg_floor_console::Page::Comfort
            ? FLOOR_TEMPLATE_MAX_AGE : FLOOR_INDOOR_STATIC_MAX_AGE;
        const bool indoor_fresh = floor_console_template_valid_[p][1] &&
            uint32_t(now - floor_console_template_at_[p][1]) <= indoor_limit;
        if (master_fresh && indoor_fresh)
            return lg_floor_console::frame_is_newer(floor_console_template_at_[p][0], floor_console_template_at_[p][1]) ? 0 : 1;
        if (master_fresh) return 0;
        if (indoor_fresh) return 1;
        return -1;
    }

    // The most recently heard *real* Master or indoor report is the best
    // available UI fallback. It does not imply acceptance of our Slave write.
    void floor_console_restore_observed_(lg_floor_console::Page page) {
        const int src = floor_console_source_index_(page, millis());
        if (src < 0) return;
        floor_console_publish_control_(page,
            lg_floor_console::value(floor_console_templates_[floor_page_index_(page)][src], page));
    }

    bool floor_console_busy_for_start_() const {
        return is_initializing_ || floor_console_tx_.active ||
            pending_send_ != PendingSendKind::None || pending_status_change_ ||
            slave_ack_pending_ || slave_fan_cal_pending_ ||
            pending_type_a_settings_change_ || pending_type_b_settings_change_ ||
            pending_type_e_settings_change_ || pending_type_b_timed_message_;
    }

    void floor_console_start_(lg_floor_console::Page page, uint8_t requested, uint32_t now) {
        if (floor_console_fast_discharge_ && slave_ && page == lg_floor_console::Page::Discharge) {
            // Last HA choice wins. Any prior delayed verification belongs to
            // an older request and must never confirm this newer selection.
            floor_fast_verify_pending_ = false;
            floor_fast_verify_sent_ = false;
            floor_fast_verify_echo_seen_ = false;
            floor_fast_cd_verifier_.reset(requested);
        }
        const unsigned p = floor_page_index_(page);
        const int src = floor_console_source_index_(page, now);
        if (src < 0 && !slave_) {
            floor_console_report_("BLOCKED: no genuine indoor page template available in Master mode; no packet sent");
            return;
        }
        // Never suppress a requested Both/Top/Bottom change based solely on
        // an older Master AD or an old indoor CD. Such shortcuts caused Both
        // to be silently skipped after a previous ESP32 adjustment.
        floor_console_tx_ = FloorConsoleTransaction{};
        floor_console_tx_.active = true;
        floor_console_tx_.page = page;
        floor_console_tx_.target = requested;
        floor_console_tx_.requested_at = now;
        if (src < 0) {
            floor_console_tx_.awaiting_template = true;
            floor_console_report_("WAITING FOR TEMPLATE: requesting genuine LG page before write; no control packet sent");
        } else {
            floor_console_tx_.template_src = static_cast<uint8_t>(src);
            floor_console_send_queued_ = true;
            floor_console_report_("QUEUED: awaiting free LG bus; indoor acceptance not yet checked");
        }
    }

    void floor_console_finish_(const char *why, bool success) {
        if (!floor_console_tx_.active) return;
        const auto page = floor_console_tx_.page;
        const auto target = floor_console_tx_.target;
        const unsigned repeats = floor_console_tx_.transmissions;
        const bool superseded = floor_console_tx_.superseded;
        floor_console_tx_ = FloorConsoleTransaction{};
        floor_console_send_queued_ = false;
        floor_console_last_finish_at_ = millis();
        if (pending_send_ == PendingSendKind::FloorConsole ||
            pending_send_ == PendingSendKind::FloorConsoleReadback ||
            pending_send_ == PendingSendKind::FloorConsoleSelectiveCDProbe)
            pending_send_ = PendingSendKind::None;
        if (superseded) {
            floor_console_report_("SUPERSEDED: newer HA selection waiting; stopped old retries");
            // Do NOT revert the UI to an old AD/CD while the newer choice
            // remains queued. The deferred request will execute when idle.
            return;
        }
        floor_console_report_(std::string(success ? "CONFIRMED: " : "UNVERIFIED: ") + why +
                              " (page " + std::to_string(static_cast<unsigned>(page)) +
                              ", " + std::to_string(repeats) + " TX)");
        if (success) floor_console_publish_control_(page, target);
        else floor_console_restore_observed_(page);
    }

    // After a verified local echo, release the bus and let further discharge
    // selections proceed without a 60-second settings burst. The visible
    // select is explicitly optimistic until a genuine CD appears later.
    void floor_console_fast_complete_() {
        const auto target = floor_console_tx_.target;
        floor_console_tx_ = FloorConsoleTransaction{};
        floor_console_send_queued_ = false;
        floor_console_last_finish_at_ = millis();
        floor_fast_verify_pending_ = true;
        floor_fast_verify_sent_ = false;
        floor_fast_verify_echo_seen_ = false;
        floor_fast_cd_verifier_.reset(target);
        floor_fast_verify_target_ = target;
        floor_fast_verify_queued_at_ = millis();
        floor_fast_verify_sent_at_ = 0;
        floor_console_publish_control_(lg_floor_console::Page::Discharge, target);
        floor_console_report_("SENT (UNVERIFIED): discharge command echoed; one coalesced indoor CD readback will follow after 45s quiet");
    }

    void send_floor_console_fast_verify_() {
        if (!slave_ || !floor_fast_verify_pending_ || floor_fast_verify_sent_ ||
            !have_master_status_ || !have_unit_status_for_pipe_ ||
            !SlavePipeTempDiscovery::make_request(
                send_buf_, last_master_status_, last_unit_status_for_pipe_,
                uint32_t(millis() - last_master_status_for_pipe_millis_),
                uint32_t(millis() - last_unit_status_for_pipe_millis_))) return;
        send_buf_[12] = calc_checksum(send_buf_);
        floor_fast_verify_sent_ = true;
        floor_fast_verify_echo_seen_ = false;
        floor_fast_verify_sent_at_ = millis();
        ESP_LOGI(TAG, "FLOOR FAST: one coalesced guarded 28 readback after 45s quiet; awaiting two indoor CD frames");
        ESP_LOGD(TAG, "sending %s", format_frame_(send_buf_).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = millis();
        pending_send_ = PendingSendKind::FloorConsoleReadback;
    }

    // Latest choice wins while an earlier 2D, 2C or 2E request is pending.
    // We never transmit two settings pages concurrently or unbounded retries.
    void floor_console_request_(lg_floor_console::Page page, uint8_t requested) {
        if (!floor_console_features_ || !lg_floor_console::valid_value(page, requested)) return;
        const uint32_t now = millis();
        if (floor_console_tx_.active || floor_console_busy_for_start_() ||
            floor_console_deferred_.valid) {
            if (floor_console_tx_.active && floor_console_tx_.page == page &&
                floor_console_tx_.target == requested && !floor_console_deferred_.valid) {
                floor_console_report_("IN PROGRESS: same setting already requested");
                return;
            }
            floor_console_deferred_ = FloorConsoleDeferredRequest{};
            floor_console_deferred_.valid = true;
            floor_console_deferred_.page = page;
            floor_console_deferred_.target = requested;
            floor_console_deferred_.queued_at = now;
            if (floor_console_tx_.active) {
                floor_console_tx_.superseded = true;
                floor_console_send_queued_ = false;
                floor_console_tx_.readback_queued = false;
            }
            floor_console_report_("DEFERRED: latest HA selection retained; waiting for safe bus slot");
            return;
        }
        floor_console_start_(page, requested, now);
    }

    void floor_console_maintain_(uint32_t now) {
        if (floor_fast_verify_pending_) {
            const uint32_t elapsed = floor_fast_verify_sent_
                ? uint32_t(now - floor_fast_verify_sent_at_)
                : uint32_t(now - floor_fast_verify_queued_at_);
            if ((floor_fast_verify_sent_ && elapsed >= FLOOR_FAST_VERIFY_TIMEOUT_MS) ||
                (!floor_fast_verify_sent_ && elapsed >= FLOOR_FAST_VERIFY_MAX_PENDING_MS)) {
                floor_fast_verify_pending_ = false;
                floor_fast_verify_sent_ = false;
                floor_console_report_("UNVERIFIED: no timely genuine indoor CD; optimistic discharge selection may be stale");
                // Do not restore a potentially stale pre-command CD as the
                // new physical state. Next genuine CD will resynchronise HA.
            }
        }
        if (floor_console_tx_.active) {
            auto &tx = floor_console_tx_;
            if (tx.superseded) {
                // Wait for an already transmitted frame's local UART echo.
                if (pending_send_ != PendingSendKind::FloorConsole &&
                    pending_send_ != PendingSendKind::FloorConsoleReadback &&
                    pending_send_ != PendingSendKind::FloorConsoleSelectiveCDProbe)
                    floor_console_finish_("superseded", false);
            } else if (uint32_t(now - tx.requested_at) > FLOOR_TX_DEADLINE_MS) {
                floor_console_finish_(tx.awaiting_template
                    ? "genuine page template not received before deadline; no settings write"
                    : "fresh indoor readback did not confirm selected value", false);
            } else if (tx.awaiting_template) {
                // An AD/CD/CE.10 from either genuine source is a prerequisite,
                // not evidence that the requested option was applied.
                const int src = floor_console_source_index_(tx.page, now);
                if (src >= 0) {
                    tx.awaiting_template = false;
                    tx.template_src = static_cast<uint8_t>(src);
                    floor_console_send_queued_ = true;
                    floor_console_report_("TEMPLATE READY: genuine page received; write queued, acceptance still unverified");
                } else if (uint32_t(now - tx.requested_at) >= 15000u &&
                           (tx.template_wait_last_log_at == 0 ||
                            uint32_t(now - tx.template_wait_last_log_at) >= 30000u)) {
                    tx.template_wait_last_log_at = now;
                    ESP_LOGW(TAG, "FLOOR TEMPLATE: waiting for safe Master/indoor status "
                                  "or genuine page %u; sleep-active or mismatched statuses block "
                                  "the read (attempts %u/%u)",
                             unsigned(tx.page), unsigned(tx.template_refresh_attempts),
                             unsigned(FLOOR_TEMPLATE_REFRESH_MAX_ATTEMPTS));
                }
            } else if (tx.transmissions > 0 && tx.echo_seen) {
                // The speculative Type-D probe gets only a short observation
                // window, then we request the known safe full settings page.
                if (tx.selective_probe_attempted &&
                    !tx.selective_probe_fallback_ready &&
                    uint32_t(now - tx.selective_probe_sent_at) >= FLOOR_SELECTIVE_CD_TIMEOUT_MS) {
                    tx.selective_probe_fallback_ready = true;
                    ESP_LOGW(TAG, "FLOOR SELECTIVE: no two matching indoor CD reports "
                                  "within 10s of 2D.80 candidate; trying guarded 28 fallback");
                }
                // A guarded readback must not block the queue indefinitely
                // if no safe synchronized Master/indoor Type-0 pair exists.
                if (tx.readback_queued &&
                    uint32_t(now - tx.readback_queued_at) > FLOOR_READBACK_GUARD_WAIT_MS) {
                    tx.readback_queued = false;
                    tx.readback_attempts = tx.transmissions; // skipped, never sent
                    ESP_LOGW(TAG, "FLOOR READBACK: no safe 28 request pair after 20s; skipping refresh (not accepted)");
                }
                // One guarded 28 request per write, rather than waiting for
                // a CD report the unit does not broadcast spontaneously.
                const uint32_t readback_settle = lg_floor_console::readback_settle_ms(
                    slave_, tx.page, floor_console_passive_readback_,
                    FLOOR_READBACK_SETTLE_MS, FLOOR_PASSIVE_DISCHARGE_WINDOW_MS);
                if ((!tx.selective_probe_attempted || tx.selective_probe_fallback_ready) &&
                    lg_floor_console::readback_due(slave_, tx.transmissions,
                    tx.readback_attempts, tx.echo_seen, tx.readback_queued,
                    uint32_t(now - tx.transmitted_at), readback_settle)) {
                    tx.readback_queued = true;
                    tx.readback_queued_at = now;
                    if (floor_console_passive_readback_ &&
                        tx.page == lg_floor_console::Page::Discharge)
                        ESP_LOGI(TAG, "FLOOR PASSIVE: no spontaneous two-CD confirmation "
                                      "within %us; queuing existing guarded 28 full-refresh fallback",
                                 unsigned(FLOOR_PASSIVE_DISCHARGE_WINDOW_MS / 1000u));
                }
                // A full settings batch may take a minute. Only retry after
                // allowing a complete readback, not automatically at 10 s.
                if (lg_floor_console::retry_due(tx.transmissions,
                    tx.readback_queued, floor_console_send_queued_,
                    uint32_t(now - tx.transmitted_at), FLOOR_TX_REPEAT_MS) &&
                    (tx.readback_sent_at == 0 ||
                     uint32_t(now - tx.readback_sent_at) >= FLOOR_READBACK_TIMEOUT_MS) &&
                    pending_send_ == PendingSendKind::None)
                    floor_console_send_queued_ = true;
            }
        }
        if (!floor_console_tx_.active && floor_console_deferred_.valid) {
            if (uint32_t(now - floor_console_deferred_.queued_at) > FLOOR_DEFER_TIMEOUT_MS) {
                const auto page = floor_console_deferred_.page;
                floor_console_deferred_ = FloorConsoleDeferredRequest{};
                floor_console_report_("DEFERRED EXPIRED: LG bus unavailable; no packet sent");
                floor_console_restore_observed_(page);
            } else if (!floor_console_busy_for_start_() &&
                       uint32_t(now - floor_console_last_finish_at_) >= 1500u) {
                const auto queued = floor_console_deferred_;
                floor_console_deferred_ = FloorConsoleDeferredRequest{};
                floor_console_start_(queued.page, queued.target, now);
            }
        }
    }

    // Read-before-write recovery. The requesting select/switch stays at its
    // last observed value until two matching indoor page reports confirm it.
    // Never fabricate a Type-5/6 page or turn a read query into a settings write.
    void send_floor_console_template_refresh_() {
        auto &tx = floor_console_tx_;
        if (!slave_ || !tx.active || tx.superseded || !tx.awaiting_template ||
            !lg_floor_console::template_refresh_due(
                tx.template_refresh_attempts,
                uint32_t(millis() - tx.template_refresh_sent_at),
                FLOOR_TEMPLATE_REFRESH_RETRY_MS, FLOOR_TEMPLATE_REFRESH_MAX_ATTEMPTS)) return;
        const uint32_t now = millis();
        // Floor consoles retain their existing no-active-reservation policy.
        // The validated settings query will refuse conflicting/stale A8/C8,
        // any active timer and unsafe transition frames.
        if (!have_master_status_ || !have_unit_status_for_pipe_ ||
            (last_master_status_[1] & 0x01u) ||  // no unsettled Master change
            !SlavePipeTempDiscovery::make_request(
                send_buf_, last_master_status_, last_unit_status_for_pipe_,
                uint32_t(now - last_master_status_for_pipe_millis_),
                uint32_t(now - last_unit_status_for_pipe_millis_))) return;
        send_buf_[12] = calc_checksum(send_buf_);
        ++tx.template_refresh_attempts;
        tx.template_refresh_sent_at = now;
        tx.template_refresh_echo_seen = false;
        ESP_LOGI(TAG, "FLOOR TEMPLATE: guarded 28 full-settings READ for page %u "
                      "(attempt %u/%u); awaiting genuine AD/CD/CE.10; no settings write",
                 unsigned(tx.page), unsigned(tx.template_refresh_attempts),
                 unsigned(FLOOR_TEMPLATE_REFRESH_MAX_ATTEMPTS));
        ESP_LOGD(TAG, "sending %s", format_frame_(send_buf_).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = now;
        pending_send_ = PendingSendKind::FloorConsoleTemplateRefresh;
    }

    // Type-0 0x40 request is a read-only full-settings query. No guessed
    // installer page: reuse exactly the same pair-validated helper as the
    // successful startup CA/CB/CD/CE.10 discovery. Never issue during a
    // normal climate command or a pending fan-calibration write.
    void send_floor_console_readback_() {
        auto &tx = floor_console_tx_;
        if (!slave_ || !tx.active || tx.superseded || !tx.readback_queued) return;
        const uint32_t now = millis();
        // Unverified Type-D page-5 selective read candidate. Only transmit
        // when explicitly enabled, once for an actual HA discharge change,
        // with a genuine checksum-valid 2D write already sent. Keep the
        // setting identical to the write in case this is not a read opcode.
        // Active sleep/reservations are excluded from this wire experiment.
        if (floor_console_selective_cd_probe_ && !floor_console_fast_discharge_ &&
            tx.page == lg_floor_console::Page::Discharge &&
            !tx.selective_probe_attempted &&
            !active_reservation_ && !slave_sleep_master_active_ &&
            !sleep_timer_target_millis_.has_value() &&
            !floor_console_passive_readback_ &&
            lg_floor_console::build_selective_cd_probe(send_buf_, tx.last_wire_write)) {
            tx.readback_queued = false;
            tx.selective_probe_attempted = true;
            tx.selective_probe_sent_at = now;
            ESP_LOGW(TAG, "FLOOR SELECTIVE EXPERIMENT: transmitting UNVERIFIED 2D.80 candidate "
                          "for page 5; this MAY be interpreted as a settings write; "
                          "no change to requested direction, one attempt only");
            ESP_LOGD(TAG, "sending %s", format_frame_(send_buf_).c_str());
            UARTDevice::write_array(send_buf_, MsgLen);
            last_write_millis_ = now;
            pending_send_ = PendingSendKind::FloorConsoleSelectiveCDProbe;
            return;
        }
        if (!have_master_status_ || !have_unit_status_for_pipe_ ||
            !SlavePipeTempDiscovery::make_request(
                send_buf_, last_master_status_, last_unit_status_for_pipe_,
                uint32_t(now - last_master_status_for_pipe_millis_),
                uint32_t(now - last_unit_status_for_pipe_millis_))) {
            // Guard may fail temporarily while operating states disagree.
            // Continue waiting; the transaction will expire with UNVERIFIED
            // rather than transmit a speculative Type-0 frame.
            return;
        }
        send_buf_[12] = calc_checksum(send_buf_);
        tx.readback_queued = false;
        ++tx.readback_attempts;
        tx.readback_sent_at = now;
        ESP_LOGI(TAG, "FLOOR READBACK: guarded 28 settings refresh after page %u attempt %u/2; awaiting genuine indoor page",
                 unsigned(tx.page), unsigned(tx.transmissions));
        ESP_LOGD(TAG, "sending %s", format_hex_pretty(send_buf_, MsgLen).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = now;
        pending_send_ = PendingSendKind::FloorConsoleReadback;
    }

    void send_floor_console_message_() {
        if (!floor_console_tx_.active || floor_console_tx_.superseded) {
            floor_console_send_queued_ = false;
            return;
        }
        auto &tx = floor_console_tx_;
        const unsigned p = floor_page_index_(tx.page);
        const unsigned src = tx.template_src;
        const uint32_t now = millis();
        const uint32_t age_limit = src == 1 && tx.page != lg_floor_console::Page::Comfort
            ? FLOOR_INDOOR_STATIC_MAX_AGE : FLOOR_TEMPLATE_MAX_AGE;
        if (!floor_console_template_valid_[p][src] ||
            uint32_t(now - floor_console_template_at_[p][src]) > age_limit ||
            !lg_floor_console::build(send_buf_, floor_console_templates_[p][src],
                                     tx.page, slave_, tx.target)) {
            floor_console_finish_("source frame unavailable or invalid", false);
            return;
        }
        floor_console_send_queued_ = false;
        ++tx.transmissions;
        tx.echo_seen = false;
        tx.readback_queued = false;
        tx.readback_queued_at = 0;
        tx.readback_sent_at = 0;
        tx.master_matches = 0;
        tx.indoor_matches = 0;
        tx.transmitted_at = now;
        // Snapshot the precise last command BEFORE send_buf_ is reused by
        // any subsequent status write or selective-readback request.
        memcpy(tx.last_wire_write, send_buf_, MsgLen);
        tx.selective_probe_attempted = false;
        tx.selective_probe_fallback_ready = false;
        tx.selective_probe_sent_at = 0;
        if (slave_ && floor_console_passive_readback_ && tx.page == lg_floor_console::Page::Discharge)
            ESP_LOGI(TAG, "FLOOR PASSIVE: observing for spontaneous genuine indoor CD "
                          "for up to %us before any full-refresh fallback; newer HA choices may supersede",
                     unsigned(FLOOR_PASSIVE_DISCHARGE_WINDOW_MS / 1000u));
        ESP_LOGI(TAG, "FLOOR TX: page %u, value %u, attempt %u/2, %s; template=%s (%us old), acceptance unverified",
                 unsigned(tx.page), unsigned(tx.target), unsigned(tx.transmissions),
                 slave_ ? "Slave" : "Master", src == 0 ? "genuine Master" : "genuine indoor",
                 unsigned((now - floor_console_template_at_[p][src]) / 1000u));
        ESP_LOGD(TAG, "sending %s", format_frame_(send_buf_).c_str());
        UARTDevice::write_array(send_buf_, MsgLen);
        last_write_millis_ = now;
        pending_send_ = PendingSendKind::FloorConsole;
    }

    void floor_console_observe_write_(MessageSender sender, lg_floor_console::Page page,
                                      const uint8_t *frame) {
        if (!floor_console_features_ || sender == MessageSender::Slave ||
            !lg_floor_console::recognized_frame(frame, page)) return;
        const unsigned p = floor_page_index_(page);
        const unsigned src = sender == MessageSender::Master ? 0 : 1;
        const uint32_t now = millis();
        const uint8_t observed = lg_floor_console::value(frame, page);
        const bool master_had_selection = floor_console_template_valid_[p][0];
        const bool new_master_selection = sender == MessageSender::Master &&
            (!master_had_selection ||
             lg_floor_console::value(floor_console_templates_[p][0], page) != observed);
        memcpy(floor_console_templates_[p][src], frame, MsgLen);
        floor_console_template_at_[p][src] = now;
        floor_console_template_valid_[p][src] = true;
        // A new physical Master selection takes precedence over any deferred
        // HA command for that same page, not just over an active write.
        if (sender == MessageSender::Master && master_had_selection && new_master_selection &&
            page == lg_floor_console::Page::Discharge && floor_fast_verify_pending_) {
            floor_fast_verify_pending_ = false;
            floor_fast_verify_sent_ = false;
            ESP_LOGI(TAG, "FLOOR FAST: physical wall-controller change supersedes unverified HA selection");
        }
        if (sender == MessageSender::Master && new_master_selection &&
            floor_console_deferred_.valid && floor_console_deferred_.page == page) {
            floor_console_deferred_ = FloorConsoleDeferredRequest{};
            ESP_LOGI(TAG, "FLOOR TX: dropped deferred HA request; physical Master changed page %u to %u",
                     unsigned(page), unsigned(observed));
        }
        if (floor_console_tx_.active && floor_console_tx_.page == page) {
            auto &tx = floor_console_tx_;
            if (sender == MessageSender::Master && master_had_selection &&
                new_master_selection && observed != tx.target) {
                // Real wall-control operation wins. Do not fight it with a
                // stale queued HA request or second 2D attempt.
                floor_console_deferred_ = FloorConsoleDeferredRequest{};
                floor_console_finish_("wall controller changed discharge/comfort setting", false);
                floor_console_publish_control_(page, observed);
                return;
            }
            if (tx.transmissions > 0 &&
                uint32_t(now - tx.transmitted_at) < FLOOR_TX_DEADLINE_MS) {
                uint8_t &matches = sender == MessageSender::Master ? tx.master_matches : tx.indoor_matches;
                if (observed == tx.target) {
                    if (matches < 2) ++matches;
                    ESP_LOGI(TAG, "FLOOR TX: %s reports requested page %u value %u (%u independent frame(s))",
                             sender == MessageSender::Master ? "Master" : "Indoor",
                             unsigned(page), unsigned(observed), unsigned(matches));
                } else {
                    matches = 0;
                    if (sender == MessageSender::Unit)
                        ESP_LOGI(TAG, "FLOOR READBACK: indoor page %u reports %u, requested %u (not confirmed)",
                                 unsigned(page), unsigned(observed), unsigned(tx.target));
                }
                if (tx.indoor_matches >= 2) {
                    floor_console_finish_("two matching genuine indoor-unit reports", true);
                    return;
                }
            }
            return;
        }
        // Deferred V2.0.4 verification uses the genuine indoor CD frames,
        // never the local echo or the speculative Type-D 0x80 packet.
        if (sender == MessageSender::Unit && page == lg_floor_console::Page::Discharge &&
            floor_fast_verify_pending_) {
            if (!floor_fast_verify_sent_ || !floor_fast_verify_echo_seen_) {
                // The requested value remains visibly optimistic until we
                // deliberately obtain a fresh status. A CD from a previous
                // in-flight full-settings burst might be stale.
                return;
            }
            const auto outcome = floor_fast_cd_verifier_.observe(observed);
            if (outcome == lg_floor_console::FastCDResult::Confirmed) {
                floor_fast_verify_pending_ = false;
                floor_fast_verify_sent_ = false;
                floor_console_report_("CONFIRMED: delayed genuine indoor CD matches requested discharge");
            } else if (outcome == lg_floor_console::FastCDResult::Different) {
                floor_fast_verify_pending_ = false;
                floor_fast_verify_sent_ = false;
                floor_console_report_("REJECTED/OVERRIDDEN: indoor CD differs from requested discharge; UI synchronised");
            } else {
                // Keep showing pending request until the second matching CD;
                // do not momentarily publish a stale intermediate sample.
                return;
            }
        }
        // When idle, accept genuine indoor status or a changed value from the
        // wall Master. Repeated unchanged AD must not overwrite a confirmed
        // indoor selection with an older cached Master choice.
        if (sender == MessageSender::Unit ||
            (slave_ && sender == MessageSender::Master && new_master_selection))
            floor_console_publish_control_(page, observed);
    }

    static std::string format_frame_(const uint8_t* buffer) {
        char hex[MsgLen * 3];
        for (size_t i = 0; i < MsgLen; i++) {
            snprintf(hex + i * 3, 4, i + 1 < MsgLen ? "%02X." : "%02X", buffer[i]);
        }
        return std::string(hex);
    }

    static void publish_if_changed_(text_sensor::TextSensor* sensor, const std::string& value) {
        if (sensor != nullptr && (!sensor->has_state() || sensor->get_state() != value)) {
            sensor->publish_state(value);
        }
    }

    // Publish only on a newly observed value so debug logs remain readable.
    void floor_console_publish_observation_(text_sensor::TextSensor *destination,
            const char *label, const char *value, const uint8_t *buffer) {
        if (destination == nullptr) return;
        if (!destination->has_state() || destination->get_state() != value) {
            destination->publish_state(value);
            ESP_LOGI(TAG, "FLOOR MONITOR: %s = %s | %s", label, value, format_frame_(buffer).c_str());
        }
    }

    void floor_console_observe_page_(MessageSender sender, uint8_t page, const uint8_t *buffer) {
        if (!floor_console_monitoring_ || sender == MessageSender::Slave) return;
        const bool master = sender == MessageSender::Master;
        if (page == 4) {
            floor_console_publish_observation_(master ? floor_console_comfort_master_ : floor_console_comfort_indoor_,
                master ? "Master AC comfort cooling" : "Indoor CC comfort cooling",
                lg_floor_console::comfort_cooling(buffer) ? "On" : "Off", buffer);
        } else if (page == 5) {
            floor_console_publish_observation_(master ? floor_console_direction_master_ : floor_console_direction_indoor_,
                master ? "Master AD discharge direction" : "Indoor CD discharge direction",
                lg_floor_console::discharge_name(lg_floor_console::discharge_code(buffer)), buffer);
        } else if (page == 6 && buffer[1] == 0x10) {
            floor_console_publish_observation_(master ? floor_console_step_master_ : floor_console_step_indoor_,
                master ? "Master AE.10 comfort step" : "Indoor CE.10 comfort step",
                lg_floor_console::comfort_step_name(lg_floor_console::comfort_step_code(buffer)), buffer);
        }
    }

    // Type 5 (0xCD): advanced installer settings, mostly undocumented. Raw only.
    void process_type_d_settings_message(MessageSender sender, const uint8_t* buffer) {
        floor_console_observe_page_(sender, 5, buffer);
        floor_console_observe_write_(sender, lg_floor_console::Page::Discharge, buffer);
        if (sender != MessageSender::Unit) {
            return;
        }
        publish_if_changed_(advanced_settings_raw_, format_frame_(buffer));
    }

    void process_type_e_settings_message(const uint8_t* buffer) {
        int slot;
        switch (buffer[1]) {
            case 0x00: slot = 0; break;
            case 0x10: slot = 1; break;
            case 0x11: slot = 2; break;
            case 0x12: slot = 3; break;
            default: return;
        }
        memcpy(last_recv_type_e_settings_[slot], buffer, MsgLen);

        // 0xCE.10 byte 4: bit 1 = continuous fan (installer setting 46), bit 2 =
        // temperature-based fan speed auto (51).
        if (buffer[1] == 0x10) {
            if (slave_) {
                publish_if_changed_(installer_continuous_fan_status_, (buffer[4] & 0x02) ? "On" : "Off");
                publish_if_changed_(installer_temperature_based_fan_auto_status_, (buffer[4] & 0x04) ? "On" : "Off");
            }
            ignore_callbacks_ = true;
            if (continuous_fan_switch_ != nullptr) {
                continuous_fan_switch_->publish_state((buffer[4] >> 1) & 0b1);
            }
            if (temperature_based_fan_auto_switch_ != nullptr) {
                temperature_based_fan_auto_switch_->publish_state((buffer[4] >> 2) & 0b1);
            }
            ignore_callbacks_ = false;
        }

        // One string for all four sub-frames, those not yet received left out.
        std::string all;
        for (auto& frame : last_recv_type_e_settings_) {
            if (frame[0] == 0) {
                continue;
            }
            if (!all.empty()) {
                all += " ";
            }
            all += format_frame_(frame);
        }
        publish_if_changed_(extended_settings_raw_, all);
    }

    void process_type_e_status_message(MessageSender sender, const uint8_t* buffer) {
        floor_console_observe_page_(sender, 6, buffer);
        if (buffer[1] == 0x10)
            floor_console_observe_write_(sender, lg_floor_console::Page::Step, buffer);
        if (sender == MessageSender::Unit && buffer[1] != 0x80) {
            process_type_e_settings_message(buffer);
            return;
        }
        if (sender == MessageSender::Slave || buffer[1] != 0x80) {
            return;
        }

        if (humidity_ != nullptr) {
            humidity_->publish_state(buffer[2]);
        }
        if (fan_operation_time_ != nullptr) {
            fan_operation_time_->publish_state(decode_uint16_(buffer, 3));
        }
        if (indoor_unit_operation_time_ != nullptr) {
            indoor_unit_operation_time_->publish_state(decode_uint16_(buffer, 6));
        }
        if (precise_room_temperature_ != nullptr) {
            const float reading = float(buffer[10]) + float(buffer[11]) / 10.0f;
            // Some indoor units emit CE.80 with zeroed unsupported fields.
            // Do not overwrite a previously valid AE.80 room temperature
            // with a physically implausible 0.0 C diagnostic reading.
            if (reading >= 5.0f && reading <= 45.0f)
                precise_room_temperature_->publish_state(reading);
        }
    }

    void update() {
        uint32_t millis_now = millis();

        // Handle sleep timer.
        if (sleep_timer_target_millis_.has_value()) {
            int32_t diff = int32_t(sleep_timer_target_millis_.value() - millis_now);
            if (diff <= 0) {
                sleep_timer_target_millis_.reset();
                ignore_callbacks_ = true;
                sleep_timer_.publish_state(0);
                ignore_callbacks_ = false;
                if (!slave_) {
                    // Master owns its own timer and power-off behaviour.
                    ESP_LOGD(TAG, "Turning off for sleep timer (Master)");
                    active_reservation_= false;
                    master_sleep_cancel_pending_ = true;
                    master_sleep_command_waiting_ = false;
                    user_changed_settings_ = true;
                    this->mode = climate::CLIMATE_MODE_OFF;
                    this->action = climate::CLIMATE_ACTION_OFF;
                    pending_status_change_ = true;
                    publish_state();
                } else {
                    // The genuine wired Master owns the reservation and the
                    // power-off. Never issue an extra independent OFF as Slave.
                    ESP_LOGI(TAG, "SLAVE SLEEP: local displayed countdown reached zero; waiting for genuine Master/indoor status, no independent power-off");
                }
            } else if (optional<uint32_t> minutes = get_sleep_timer_minutes()) {
                if (!float_equal(sleep_timer_.state, *minutes)) {
                    ignore_callbacks_ = true;
                    sleep_timer_.publish_state(*minutes);
                    ignore_callbacks_ = false;
                }
            }
        }

        if (slave_ && is_initializing_) {
            ESP_LOGD(TAG, "Not sending, waiting for other controller or unit to send first");
        }

        // Slave: a one-time full-settings request early after boot if the
        // read-only installer diagnostics have not all been received yet.
        // No blind startup transmit: require 60 seconds since ESPHome setup,
        // ten seconds since a valid Master A8 and indoor C8 pair was first
        // observed, fresh matching status, and no outstanding user command.
        // A unit that naturally sends CA, CB and CE.10 needs no request.
        if (slave_ && !slave_pipe_discovery_attempted_ &&
            !pending_slave_pipe_discovery_ &&
            !slave_installer_diagnostics_complete_() &&
            !is_initializing_ && !slave_ack_pending_ &&
            !pending_status_change_ && !user_changed_settings_ &&
            have_master_status_ && have_unit_status_for_pipe_) {
            if (slave_status_pair_first_seen_millis_ == 0) {
                slave_status_pair_first_seen_millis_ = millis_now;
            }
            if (static_cast<uint32_t>(millis_now - slave_startup_millis_) >=
                    SLAVE_DIAGNOSTICS_BOOT_SETTLE_MS &&
                static_cast<uint32_t>(millis_now - slave_status_pair_first_seen_millis_) >=
                    SLAVE_DIAGNOSTICS_PAIR_SETTLE_MS) {
                uint8_t safe_request[MsgLen];
                if (prepare_slave_diagnostic_query_(safe_request, millis_now)) {
                    pending_slave_pipe_discovery_ = true;
                    ESP_LOGI(TAG, "SLAVE STARTUP DIAGNOSTICS: missing CA/CB/CE.10; "
                                  "queuing one-time guarded full-settings refresh");
                }
            }
        }

        // Master: existing 10-minute AB polling, unchanged.
        // Slave: V1.6 Type-B polling with a current AB/CB template. If no
        // template has appeared, V1.7 also allows exactly ONE guarded
        // status-batch discovery using current Master A8 and matching C8.
        // The startup logic above may already have used that one attempt.
        if (millis_now - last_sent_recv_type_b_millis_ > SlavePipeTempPoll::POLL_INTERVAL_MS) {
            if (!slave_ || slave_pipe_temp_poll_.has_fresh_template(millis_now)) {
                pending_type_b_timed_message_ = true;
            } else if (slave_ && !slave_pipe_temp_poll_.has_template() &&
                       !slave_pipe_discovery_attempted_) {
                pending_slave_pipe_discovery_ = true;
                last_sent_recv_type_b_millis_ = millis_now;
            } else {
                ESP_LOGW(TAG, "SLAVE PIPE POLL: no usable Type-B template; one-time discovery %s",
                    slave_pipe_discovery_attempted_ ? "already attempted (no repeat)" : "waiting for valid state");
                last_sent_recv_type_b_millis_ = millis_now;
            }
        }
        if (slave_diagnostic_sleep_watch_ &&
            uint32_t(millis_now - slave_diagnostic_sleep_sent_at_) > 120000u) {
            slave_diagnostic_sleep_watch_ = false;
            ESP_LOGW(TAG, "SLAVE DIAGNOSTIC SLEEP CHECK: no suitable post-query Master "
                          "duration report within two minutes; outcome not verified");
        }
        if (slave_pipe_discovery_waiting_cb_ &&
            static_cast<uint32_t>(millis_now - slave_pipe_discovery_sent_millis_) >
            SlavePipeTempDiscovery::RESPONSE_WINDOW_MS) {
            ESP_LOGW(TAG, "SLAVE PIPE DISCOVERY: no indoor CB within 90 seconds; not repeating request");
            slave_pipe_discovery_waiting_cb_ = false;
        }

        // Send a status message every 20 seconds.
        // Slave controllers only send a status message when settings are changed.
        if (!slave_ && millis_now - last_sent_status_millis_ > 20 * 1000) {
            pending_status_change_ = true;
        }
    }
};

// A momentary API button, not a writable LG setting. It only asks the
// controller to enqueue its existing Type-B telemetry read.
class LgPipeTemperatureRefreshButton final : public button::Button {
public:
    explicit LgPipeTemperatureRefreshButton(LgController *controller)
        : controller_(controller) {}

protected:
    void press_action() override {
        if (controller_ != nullptr) controller_->request_pipe_temperature_refresh();
    }

private:
    LgController *controller_;
};

} // namespace esphome::lg_controller
