#pragma once

#include "esphome.h"
#include <deque>
#include "esphome/core/component.h"

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include "esphome/components/uart/uart.h"


namespace esphome {
namespace broan {

#define CONTROL_TIMEOUT 5000
#define UPDATE_RATE 1000
#define HEARTBEAT_RATE 10000

#define UPDATE_RATE_FAST 10000 // 10 seconds
#define UPDATE_RATE_SLOW 60000 // 1 minute
#define UPDATE_RATE_ONCE 0xFFFFFFFE
#define UPDATE_RATE_NEVER 0xFFFFFFFF

#define MAX_REQUEST_SIZE 10
#define INVALID_FIELD 0xFFFFFF

#define FILTER_LIFE_MAX 7884000

//#define SCAN_UNKNOWN 1
//#define LISTEN_ONLY 1

template<typename T>
concept BroanFieldTypes = 	std::is_same_v<T, float> ||
							std::is_same_v<T, uint8_t> ||
							std::is_same_v<T, uint32_t>;

enum BroanFieldType
{
	Float,
	Int,
	Byte,
	Void,
	String,
};

enum BroanCFMMode
{
	Input = 1 << 0,
	Output = 1 << 1,
	Both = BroanCFMMode::Input | BroanCFMMode::Output,
};

// Values of the FanMode register (00:20). Continuous air exchange and
// recirculation encode their speed in the mode itself, in min / max / med
// order. Recirculation = air exchange - 4.
enum BroanFanMode
{
	Off = 0x01,
	Ovr = 0x02, // Set by the auxiliary (dry contact) remotes. Read only.
	RecirculateMin = 0x05,
	RecirculateMax = 0x06,
	RecirculateMed = 0x07,
	Intermittent = 0x08, // With or without recirculation, see IntRecirculate. Speed in IntSpeed.
	Min = 0x09,
	Max = 0x0a,
	Manual = 0x0b, // Medium speed air exchange
	Turbo = 0x0c,
	Humidity = 0x0d,
	Away = 0x0F, // "OTH", no idea what this actually does?
	Smart = 0x11,
};

// Values of the DefrostMode register (12:50), from the AI series manual.
enum BroanDefrostMode
{
	DefrostPlus = 0x01,       // Extended defrost for colder regions
	DefrostDiscretion = 0x02, // Defrost without fan speed change, factory setting
};

#define DEFROST_MODE_DISCRETION "Discretion"
#define DEFROST_MODE_PLUS "Plus"

// Speed choice exposed by the fan speed select. Applies to air exchange,
// recirculation and intermittent + recirculation.
enum BroanFanSpeed
{
	Minimum = 0,
	Medium,
	High,

	MAX_FAN_SPEEDS,
};

// Select options. Keep in sync with select/__init__.py
#define FAN_MODE_OFF "Off"
#define FAN_MODE_AIR_EXCHANGE "Air Exchange"
#define FAN_MODE_INTERMITTENT "Intermittent"
#define FAN_MODE_INTERMITTENT_RECIRCULATE "Intermittent + Recirculate"
#define FAN_MODE_TURBO "Turbo"
#define FAN_MODE_HUMIDITY "Humidity"
#define FAN_MODE_RECIRCULATE "Recirculate"
#define FAN_MODE_SMART "Smart"
#define FAN_MODE_OVERRIDE "Override"

inline constexpr const char *g_rgFanSpeedNames[BroanFanSpeed::MAX_FAN_SPEEDS] = { "Minimum", "Medium", "High" };

// Register values for each BroanFanSpeed
inline constexpr uint8_t g_rgAirExchangeModes[BroanFanSpeed::MAX_FAN_SPEEDS] = { BroanFanMode::Min, BroanFanMode::Manual, BroanFanMode::Max };
inline constexpr uint8_t g_rgRecirculateModes[BroanFanSpeed::MAX_FAN_SPEEDS] = { BroanFanMode::RecirculateMin, BroanFanMode::RecirculateMed, BroanFanMode::RecirculateMax };
inline constexpr uint8_t g_rgIntSpeeds[BroanFanSpeed::MAX_FAN_SPEEDS] = { 0x00, 0x02, 0x01 }; // IntSpeed (0E:22)

enum BroanField
{
	// Control
	FanMode = 0,
	IntRecirculate, // Keep right after FanMode so they are polled together
	IntSpeed,
	FanModeCommit, // Written right after FanMode, never polled
	DefrostMode,

	// Flow limits (installer menu), read only
	FlowLimitLow,
	FlowLimitHigh,
	MaxSupplyCFM,
	MaxExhaustCFM,
	HumidityControl,
	IntModeDuration,
	TargetHumidityA, // Set both to same value per VTSPEEDW
	TargetHumidityB,

	// Info
	Uptime, // In seconds?
	Wattage,
	TemperatureIn,
	TemperatureOut,
	SupplyCFM,
	ExhaustCFM,
	SupplyRPM,
	ExhaustRPM,

	// Speeds
	CFMIn_Medium,
	CFMOut_Medium,
	CFMIn_Max,
	CFMOut_Max,
	CFMIn_Min,
	CFMOut_Min,

	// Input
	Heartbeat, // Weird void value that controllers ping every 10s
	ControllerHumidity,
	ControllerTemperature,

	// Maintenance
	FilterReset, // Set to 1 to reset
	FilterLife, // default 7884000 / 3 months
	FilterLifeStage, // 09:30. Must be preloaded with the desired FilterLife value before FilterReset is set, otherwise the ERV ignores the reset.

	// Diagnostic
	WarningCode,
	FaultCode,

	// State
	BaseMode,
	ActiveMode,

	// Info 
	Firmware,
	Model,
	FirmwareVersion,
	HardwareRevision,

	MAX_FIELDS,
};

enum BroanFlowSide
{
	Supply = 0, // In
	Exhaust,    // Out

	MAX_FLOW_SIDES,
};

// Flow setpoint registers (installer menu), per BroanFanSpeed and BroanFlowSide.
// The wall controller writes both sides of a speed together, supply first.
inline constexpr uint32_t g_rgFlowFields[BroanFanSpeed::MAX_FAN_SPEEDS][BroanFlowSide::MAX_FLOW_SIDES] = {
	{ BroanField::CFMIn_Min, BroanField::CFMOut_Min },       // 0A:50 / 0B:50
	{ BroanField::CFMIn_Medium, BroanField::CFMOut_Medium }, // 06:22 / 08:22
	{ BroanField::CFMIn_Max, BroanField::CFMOut_Max },       // 0E:50 / 0F:50
};

struct BroanField_t
{
	uint8_t m_nOpcodeHigh;
	uint8_t m_nOpcodeLow;

	uint8_t m_nType;

	union {
		char m_rgBytes[4];
		float m_flValue;
		uint32_t m_nValue;
		uint8_t m_chValue;
	} m_value;

	uint32_t m_unPollRate = UPDATE_RATE_SLOW;
	uint32_t m_unLastUpdate = 0;

	// Totally safe blind copy of the incoming value.
	BroanField_t copyForUpdate(BroanFieldTypes auto const &newVal) const
	{
		BroanField_t copy = *this;

		size_t len = (m_nType == static_cast<uint8_t>(BroanFieldType::Byte)) ? 1 : 4;
		std::memcpy(copy.m_value.m_rgBytes, &newVal, len);

		return copy;
	}

	void markDirty()
	{
		m_unLastUpdate = millis() - m_unPollRate;
	}

};

class BroanComponent : public Component, public uart::UARTDevice
{

#ifdef USE_SENSOR
	SUB_SENSOR(power)
	SUB_SENSOR(temperature)
	SUB_SENSOR(temperature_out)
	SUB_SENSOR(filter_life)
	SUB_SENSOR(supply_cfm)
	SUB_SENSOR(exhaust_cfm)
	SUB_SENSOR(supply_rpm)
	SUB_SENSOR(exhaust_rpm)
	SUB_SENSOR(max_supply_cfm)
	SUB_SENSOR(max_exhaust_cfm)
#endif

#ifdef USE_SELECT
	SUB_SELECT(fan_mode)
	SUB_SELECT(fan_speed)
	SUB_SELECT(defrost_mode)
#endif

#ifdef USE_NUMBER
public:
	void set_flow_setpoint_number( uint8_t nSpeed, uint8_t nSide, number::Number *pNumber ) { m_rgFlowNumbers[nSpeed][nSide] = pNumber; }
protected:
	number::Number *m_rgFlowNumbers[BroanFanSpeed::MAX_FAN_SPEEDS][BroanFlowSide::MAX_FLOW_SIDES] = {};

	SUB_NUMBER(humidity_setpoint)
	SUB_NUMBER(intermittent_period)
#endif

#ifdef USE_BUTTON
  SUB_BUTTON(filter_reset)
#endif

#ifdef USE_SWITCH
  SUB_SWITCH(humidity_control)
#endif

#ifdef USE_TEXT_SENSOR
	SUB_TEXT_SENSOR(model)
	SUB_TEXT_SENSOR(firmware)
	SUB_TEXT_SENSOR(firmware_version)
	SUB_TEXT_SENSOR(hardware_revision)
	SUB_TEXT_SENSOR(fault_code)
	SUB_TEXT_SENSOR(warning_code)
	SUB_TEXT_SENSOR(active_mode)
	SUB_TEXT_SENSOR(base_mode)
#endif

public:
	const uint8_t m_nServerAddress = 0x10;
	const uint8_t m_nClientAddress = 0x12;

	bool m_bWaitForRemote = false;

	BroanField_t m_vecFields[BroanField::MAX_FIELDS] = {
		// Known fields
		// Control
		{ 0x00, 0x20, BroanFieldType::Byte, {0}, UPDATE_RATE_FAST }, // FanMode
		{ 0x03, 0x22, BroanFieldType::Byte, {0}, UPDATE_RATE_FAST }, // INT mode: recirculate during the off period. 0x00 = off, 0x01 = on
		{ 0x0E, 0x22, BroanFieldType::Byte, {0}, UPDATE_RATE_FAST }, // INT mode speed. 0x00 = min, 0x01 = max, 0x02 = med. With recirculation (03:22 = 0x01) it applies to both the exchange and recirculation phases (confirmed on a VanEE V180H75RT). Without recirculation the wall controller offers no speed and writes 0x00; we send the selected speed anyway, experimental, to validate on the ERV.
		{ 0x08, 0x20, BroanFieldType::Byte, {0}, UPDATE_RATE_NEVER }, // Meaning unknown. A VanEE V180H75RT wall controller writes 0x00 right after 00:20 on every mode change (air exchange, recirculation, intermittent). Without it, air exchange medium (0x0B) runs slower than minimum, so we mimic it.
		{ 0x12, 0x50, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Defrost mode (installer menu), see BroanDefrostMode. The wall controller writes it alone.

		// Flow limits, read by the wall controller when opening the installer menu (VanEE V180H75RT). Never written.
		{ 0x0C, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Lowest flow setpoint allowed. 65.0 (datasheet minimum). The wall controller accepts 65 and refuses below.
		{ 0x11, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Highest flow setpoint allowed. 152.52, same as 17:10. The wall controller accepts 152 and refuses above.
		{ 0x16, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Probably the highest supply flow reachable, measured by auto balancing (Virtuo). 163.37. Same group as 05:10 / 06:10.
		{ 0x17, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Probably the highest exhaust flow reachable, measured by auto balancing. 152.52.
		{ 0x0F, 0x22, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Humidity control on/off
		{ 0x02, 0x22, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // INT mode on time (seconds, OFF time will be what remains of an hour)
		{ 0x0C, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Target humidity?
		{ 0x0A, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Target humidity? (These are set together)


		// Info
		{ 0x14, 0x00, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // Uptime (Seconds)
		{ 0x23, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Power draw (Watts)
		{ 0x01, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Temperature sensor (In)
		{ 0x03, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Temperature sensor (Out)
		{ 0x05, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Intake CFM
		{ 0x06, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Exhaust CFM
		{ 0x03, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Intake RPM
		{ 0x04, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Exhaust RPM

		// Speeds
		{ 0x06, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // MED target CFM in.
		{ 0x08, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // MED target CFM out.
		{ 0x0E, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MAX target CFM in.
		{ 0x0F, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MAX target CFM out.
		{ 0x0A, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MIN target CFM in.
		{ 0x0B, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MIN target CFM out.

		//Input
		{ 0x00, 0x50, BroanFieldType::Void, {0}, UPDATE_RATE_NEVER }, // Unknown. Controllers regularly write this. Some kind of heartbeat maybe?
		{ 0x04, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Controller Humidity (Write only)
		{ 0x05, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Controller temperature (Write only)

		// Maintenance
		{ 0x01, 0x30, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Set to 0x01 to reset filter
		{ 0x08, 0x30, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // Number of seconds until filter needs reset. Set along side reset byte
		{ 0x09, 0x30, BroanFieldType::Int, {0}, UPDATE_RATE_NEVER }, // FilterLifeStage. Wall controller writes the desired FilterLife here first, before FilterReset.

		// Diagnostic
		{ 0x1A, 0x00, BroanFieldType::Int, {0}, UPDATE_RATE_FAST }, // Warning code -1 = OK, if multiple warnings, will cycle through on each read.
		{ 0x17, 0x00, BroanFieldType::Int, {0}, UPDATE_RATE_FAST }, // Fault code, -1 = OK
		{ 0x02, 0x20, BroanFieldType::Int, {0}, UPDATE_RATE_FAST }, // Base mode.
		{ 0x07, 0x20, BroanFieldType::Int, {0}, UPDATE_RATE_FAST }, // Active mode.

		// Info
		{ 0x02, 0x00, BroanFieldType::String,  {0}, UPDATE_RATE_ONCE }, // Firmware
		{ 0x02, 0x60, BroanFieldType::String,  {0}, UPDATE_RATE_ONCE }, // Model
		{ 0x01, 0x00, BroanFieldType::String,  {0}, UPDATE_RATE_ONCE }, // Firmware Version
		{ 0x01, 0x60, BroanFieldType::String,  {0}, UPDATE_RATE_ONCE }, // Hardware Revision
/*
		// Also read by the wall controller in the installer menu (VanEE V180H75RT), role unknown, never written:
		{ 0x0D, 0x50, BroanFieldType::Float, {0} }, // 140.0 then 142.0: follows the maximum setpoint (0E:50) - 10.
		{ 0x10, 0x50, BroanFieldType::Float, {0} }, // 90.0, equal to the minimum setpoint (0A:50). To confirm.
		{ 0x17, 0x50, BroanFieldType::Float, {0} }, // 150.0.
		{ 0x18, 0x50, BroanFieldType::Float, {0} }, // 150.0.
		{ 0x04, 0x22, BroanFieldType::Float, {0} }, // 114.8.
		{ 0x16, 0x50, BroanFieldType::Byte, {0} }, // 0x00.

		// Unknown fields scanned by the VTSPEEDW
		{ 0x02, 0x30, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Unknown. 1. Set to 0 in TURBO mode
		{ 0x0A, 0x22, BroanFieldType::Float, {0} }, // Unknown. 40 / 00002042
		{ 0x0E, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0C, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0B, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0A, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x09, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x08, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x07, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x06, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x05, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x04, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x00, 0x30, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x00, 0x22, BroanFieldType::Int, {0} }, // Unknown. 14400 / 40380000
		{ 0x07, 0x50, BroanFieldType::Int, {0} }, // Unknown. VTSPEEDW often sets this to -1
		{ 0x03, 0x20, BroanFieldType::Byte, {0} }, // Unknown. Set to 0 when entering INT mode

		// Airstream humidity? Needs verification. Broan wiring and parts diagrams for b150e75nt do not list humidity sensors, only the single j7a thermistor. 
		// May be specific to certain models, this model DOES report changing values on these registers, so I'm suspicious.
		{ 0x08, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Unknown. Seems to change a lot. 38.943115 / 1109116352 (Does not correlate with fan speed)
		{ 0x09, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Unknown. Seems to change a lot. 36.360962 / 1108439456 (Same as above)

*/
	};

	// uart overrides
	void setup() override;
	void loop() override;
	void dump_config() override;
	float get_setup_priority() const override;

public:
	// Setup
	void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

	// Control API
	void setFanMode( const std::string &mode );
	void setFanSpeed( const std::string &speed );
	void setFanSpeedCFM( BroanFanMode mode, BroanCFMMode direction, float flTargetCFM );
	void setFlowSetpoint( uint8_t nSpeed, uint8_t nSide, float flCFM );
	void setDefrostMode( const std::string &mode );
	void resetFilter();
	void setHumidityControl( bool enable );
	void setHumiditySetpoint( float humidity );
	void setCurrentHumidity( float humidity );
	void setIntermittentPeriod( uint32_t period );

private:

	uint32_t m_nLastHadControl = 0;
	uint32_t m_unLastHeartbeat = 0; // Next time to send heartbeat

	bool m_bERVReady = false;

#ifdef SCAN_UNKNOWN
	// Field scanner
	uint32_t m_nNextScan = 0;
	uint8_t m_nFieldCursor = 0;
	uint8_t m_nGroupCursor = 0x20;
	std::map<uint16_t, BroanField_t> m_vecFieldData;
#endif

	std::map<uint16_t, std::string> m_mapStrings;
	std::vector<uint8_t> m_vecHeader;
	bool m_bHaveHeader = false;

	bool m_bHaveControl = false;
	bool m_bExpectingReply = false;
	bool m_bHaveSentMessage = false;

	std::deque<std::vector<uint8_t>> m_vecSendQueue;


private:
	// Internal
	bool readHeader();
	bool readMessage();
	void handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message);
	void send(const std::vector<uint8_t>& msg);
	uint8_t calculateChecksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message);
	void replyIfAllowed();
	void runTasks();
	void parseBroanFields(const std::vector<uint8_t>& message);
#ifdef LISTEN_ONLY
	void logSniffedFields(const std::vector<uint8_t>& message, const char *pszWhat, bool bOnlyChanges);
	std::map<uint16_t, std::vector<uint8_t>> m_mapSniffedReads; // Last value logged per register
#endif
	void writeRegisters( const std::vector<BroanField_t> &values );

	float remap(float flIn, float flInMin, float flInMax, float flOutMin, float flOutMax) {
  		return (flIn - flInMin) * (flOutMax - flOutMin) / (flInMax - flInMin) + flOutMin;
	}

	BroanField_t* lookupField( uint8_t opcodeHigh, uint8_t opcodeLow );
	uint32_t lookupFieldIndex( uint8_t opcodeHigh, uint8_t opcodeLow );
	void handleUnknownField(uint32_t nOpcodeHigh, uint32_t nOpcodeLow, uint8_t len, uint32_t i, const std::vector<uint8_t>& message );

	void queueMessage(std::vector<uint8_t>& message);
	std::string activeModeToString( int code );

	// Fan mode / speed
	void publishFanState();
	void storeFanSpeed( uint8_t nSpeed );
	std::vector<BroanField_t> fanModeFields( const std::string &mode, uint8_t nSpeed );
	void pushFanMode( std::vector<BroanField_t> &vecFields, uint8_t nMode );

	uint8_t m_nFanSpeed = BroanFanSpeed::Medium; // Last speed chosen or reported by the ERV
	ESPPreferenceObject m_prefFanSpeed;


protected:
	// esphome glue
	std::string fan_mode_{};
	
	float fan_speed_{0.f};
	float power_{0.f};
	float temperature_{0.f};
	float temperature_out_{0.f};
	float supply_cfm_{0.f};
	float exhaust_cfm_{0.f};
	float supply_rpm_{0.f};
	float exhaust_rpm_{0.f};

	uint32_t filter_life_{0};

	GPIOPin *flow_control_pin_{nullptr};
};

}  // namespace broan
}  // namespace esphome
