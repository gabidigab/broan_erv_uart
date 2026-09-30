#include "broan.h"

namespace esphome {
namespace broan {

// Fields to write to enter a fan mode, in the order the wall controller sends them.
// Empty if the mode can't be set (Override, unknown).
std::vector<BroanField_t> BroanComponent::fanModeFields( const std::string &mode, uint8_t nSpeed )
{
	std::vector<BroanField_t> vecFields;
	uint8_t value;

	if( mode == FAN_MODE_AIR_EXCHANGE )
		value = g_rgAirExchangeModes[nSpeed];
	else if( mode == FAN_MODE_RECIRCULATE )
		value = g_rgRecirculateModes[nSpeed];
	else if( mode == FAN_MODE_INTERMITTENT_RECIRCULATE )
	{
		vecFields.push_back( m_vecFields[IntRecirculate].copyForUpdate( (uint8_t)0x01 ) );
		vecFields.push_back( m_vecFields[IntSpeed].copyForUpdate( g_rgIntSpeeds[nSpeed] ) );
		value = BroanFanMode::Intermittent;
	}
	else if( mode == FAN_MODE_INTERMITTENT )
	{
		vecFields.push_back( m_vecFields[IntRecirculate].copyForUpdate( (uint8_t)0x00 ) );
		vecFields.push_back( m_vecFields[IntSpeed].copyForUpdate( (uint8_t)0x00 ) );
		value = BroanFanMode::Intermittent;
	}
	else if( mode == FAN_MODE_TURBO )
		value = BroanFanMode::Turbo;
	else if( mode == FAN_MODE_HUMIDITY )
		value = BroanFanMode::Humidity;
	else if( mode == FAN_MODE_SMART )
		value = BroanFanMode::Smart;
	else if( mode == FAN_MODE_OFF )
		value = BroanFanMode::Off;
	else
		return vecFields;

	pushFanMode( vecFields, value );
	return vecFields;
}

// Like the wall controller: every FanMode write is followed by 08:20 = 0x00.
void BroanComponent::pushFanMode( std::vector<BroanField_t> &vecFields, uint8_t nMode )
{
	// Air exchange medium runs at the MED setpoints, force them first if configured.
	if( nMode == BroanFanMode::Manual && !std::isnan( m_flAirExchangeMediumCFM ) )
	{
		vecFields.push_back( m_vecFields[CFMIn_Medium].copyForUpdate( m_flAirExchangeMediumCFM ) );
		vecFields.push_back( m_vecFields[CFMOut_Medium].copyForUpdate( m_flAirExchangeMediumCFM ) );
	}

	vecFields.push_back( m_vecFields[FanMode].copyForUpdate( nMode ) );
	vecFields.push_back( m_vecFields[FanModeCommit].copyForUpdate( (uint8_t)0x00 ) );
}

void BroanComponent::setFanMode( const std::string &mode )
{
	std::vector<BroanField_t> vecFields = fanModeFields( mode, m_nFanSpeed );
	if( vecFields.empty() )
	{
		ESP_LOGW("broan","Fan mode '%s' can't be set", mode.c_str());
		// Put the select back on the ERV's actual mode
		publishFanState();
		return;
	}

	for( const BroanField_t &field : vecFields )
		lookupField( field.m_nOpcodeHigh, field.m_nOpcodeLow )->markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setFanSpeed( const std::string &speed )
{
	uint8_t nSpeed = BroanFanSpeed::MAX_FAN_SPEEDS;
	for( uint8_t i=0; i<BroanFanSpeed::MAX_FAN_SPEEDS; i++ )
	{
		if( speed == g_rgFanSpeedNames[i] )
			nSpeed = i;
	}

	if( nSpeed == BroanFanSpeed::MAX_FAN_SPEEDS )
	{
		ESP_LOGW("broan","Unknown fan speed '%s'", speed.c_str());
		return;
	}

	storeFanSpeed( nSpeed );

	// Only apply the speed to modes that use it, otherwise keep it for later.
	std::vector<BroanField_t> vecFields;
	uint8_t nMode = m_vecFields[FanMode].m_value.m_chValue;
	if( nMode >= BroanFanMode::RecirculateMin && nMode <= BroanFanMode::RecirculateMed )
		pushFanMode( vecFields, g_rgRecirculateModes[nSpeed] );
	else if( nMode >= BroanFanMode::Min && nMode <= BroanFanMode::Manual )
		pushFanMode( vecFields, g_rgAirExchangeModes[nSpeed] );
	else if( nMode == BroanFanMode::Intermittent && m_vecFields[IntRecirculate].m_value.m_chValue == 0x01 )
		vecFields.push_back( m_vecFields[IntSpeed].copyForUpdate( g_rgIntSpeeds[nSpeed] ) );

	if( vecFields.empty() )
	{
#ifdef USE_SELECT
		if( fan_speed_select_ )
			fan_speed_select_->publish_state( g_rgFanSpeedNames[m_nFanSpeed] );
#endif
		return;
	}

	for( const BroanField_t &field : vecFields )
		lookupField( field.m_nOpcodeHigh, field.m_nOpcodeLow )->markDirty();

	writeRegisters( vecFields );
}

// Put the MED setpoints back to air_exchange_medium_cfm if the ERV reports
// something else while in air exchange medium (eg: after a power cycle).
void BroanComponent::enforceAirExchangeMediumCFM()
{
	if( std::isnan( m_flAirExchangeMediumCFM ) || (uint8_t)m_vecFields[FanMode].m_value.m_chValue != BroanFanMode::Manual )
		return;

	float flIn = m_vecFields[CFMIn_Medium].m_value.m_flValue;
	float flOut = m_vecFields[CFMOut_Medium].m_value.m_flValue;
	if( flIn == 0.f || flOut == 0.f )
		return; // Not read yet

	if( fabsf( flIn - m_flAirExchangeMediumCFM ) < 0.5f && fabsf( flOut - m_flAirExchangeMediumCFM ) < 0.5f )
		return;

	// Don't hammer the ERV if it refuses the value
	uint32_t unNow = millis();
	if( m_unLastMediumCFMFix != 0 && unNow - m_unLastMediumCFMFix < 60000 )
		return;
	m_unLastMediumCFMFix = unNow;

	ESP_LOGW("broan","Air exchange medium at %.0f / %.0f CFM, setting %.0f", flIn, flOut, m_flAirExchangeMediumCFM);

	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[CFMIn_Medium].copyForUpdate( m_flAirExchangeMediumCFM ) );
	vecFields.push_back( m_vecFields[CFMOut_Medium].copyForUpdate( m_flAirExchangeMediumCFM ) );
	m_vecFields[CFMIn_Medium].markDirty();
	m_vecFields[CFMOut_Medium].markDirty();
	writeRegisters( vecFields );
}

void BroanComponent::storeFanSpeed( uint8_t nSpeed )
{
	if( nSpeed == m_nFanSpeed )
		return;

	m_nFanSpeed = nSpeed;
	m_prefFanSpeed.save( &m_nFanSpeed );
}

// Split the ERV state (FanMode, IntRecirculate, IntSpeed) into the mode and speed selects.
void BroanComponent::publishFanState()
{
	uint8_t nMode = m_vecFields[FanMode].m_value.m_chValue;
	if( nMode == 0 )
		return; // Not read yet

	const char *pszMode = nullptr;
	int nSpeed = -1;

	switch( nMode )
	{
		case BroanFanMode::Off: pszMode = FAN_MODE_OFF; break;
		case BroanFanMode::Ovr: pszMode = FAN_MODE_OVERRIDE; break;
		case BroanFanMode::Turbo: pszMode = FAN_MODE_TURBO; break;
		case BroanFanMode::Humidity: pszMode = FAN_MODE_HUMIDITY; break;
		case BroanFanMode::Smart: pszMode = FAN_MODE_SMART; break;

		case BroanFanMode::Intermittent:
		{
			uint8_t nRecirculate = m_vecFields[IntRecirculate].m_value.m_chValue;
			uint8_t nIntSpeed = m_vecFields[IntSpeed].m_value.m_chValue;

			if( nRecirculate == 0x00 )
			{
				pszMode = FAN_MODE_INTERMITTENT;
				break;
			}

			if( nRecirculate != 0x01 )
			{
				ESP_LOGW("broan","Unknown intermittent recirculation value %02X", nRecirculate);
				break;
			}

			pszMode = FAN_MODE_INTERMITTENT_RECIRCULATE;
			for( int i=0; i<BroanFanSpeed::MAX_FAN_SPEEDS; i++ )
			{
				if( g_rgIntSpeeds[i] == nIntSpeed )
					nSpeed = i;
			}

			if( nSpeed < 0 )
				ESP_LOGW("broan","Unknown intermittent speed %02X", nIntSpeed);
		}
		break;

		default:
			for( int i=0; i<BroanFanSpeed::MAX_FAN_SPEEDS; i++ )
			{
				if( g_rgAirExchangeModes[i] == nMode )
				{
					pszMode = FAN_MODE_AIR_EXCHANGE;
					nSpeed = i;
				}
				else if( g_rgRecirculateModes[i] == nMode )
				{
					pszMode = FAN_MODE_RECIRCULATE;
					nSpeed = i;
				}
			}

			if( !pszMode )
				ESP_LOGW("broan","Unknown fan mode %02X", nMode);
		break;
	}

	if( nSpeed >= 0 )
		storeFanSpeed( nSpeed );

#ifdef USE_SELECT
	if( pszMode && fan_mode_select_ )
		fan_mode_select_->publish_state( pszMode );

	if( fan_speed_select_ )
		fan_speed_select_->publish_state( g_rgFanSpeedNames[m_nFanSpeed] );
#endif
}

void BroanComponent::setFanSpeedCFM( BroanFanMode mode, BroanCFMMode direction, float flTargetCFM )
{
	std::vector<BroanField_t> vecFields;


	switch( mode )
	{
		case BroanFanMode::Max:
		{
			if( ( direction & BroanCFMMode::Input ) != 0 )
				vecFields.push_back( m_vecFields[CFMIn_Max].copyForUpdate( flTargetCFM ) );
			if( ( direction & BroanCFMMode::Output ) != 0 )
				vecFields.push_back( m_vecFields[CFMOut_Max].copyForUpdate( flTargetCFM ) );
		}
		break;

		case BroanFanMode::Min:
		{
			if( ( direction & BroanCFMMode::Input ) != 0 )
				vecFields.push_back( m_vecFields[CFMIn_Max].copyForUpdate( flTargetCFM ) );
			if( ( direction & BroanCFMMode::Output ) != 0 )
				vecFields.push_back( m_vecFields[CFMOut_Max].copyForUpdate( flTargetCFM ) );
		}
		break;


		default:
			ESP_LOGW("broan","Unhandled: Setting fan speed limits for  mode %02X", mode );

	}

	writeRegisters( vecFields );
}

// Sends new filter life to ERV in three steps (to mimic wall controller)
// 1. Write new filter life with FilterLifeStage (09:30)
// 2. Write FilterReset=1 + filter life in (08:30)
// 3. Write new filter life again, alone
// Not sure why new filter life needs to be repeated three times
// and if all this is necessary.
void BroanComponent::resetFilter()
{
	uint32_t unNewFilterLife = FILTER_LIFE_MAX;

	std::vector<BroanField_t> vecStage;
	vecStage.push_back( m_vecFields[FilterLifeStage].copyForUpdate( unNewFilterLife ) );
	writeRegisters( vecStage );

	std::vector<BroanField_t> vecFields;
	uint8_t unFilterReset = 1;

	vecFields.push_back( m_vecFields[FilterLife].copyForUpdate( unNewFilterLife ) );
	vecFields.push_back( m_vecFields[FilterReset].copyForUpdate( unFilterReset ) );

	m_vecFields[FilterReset].markDirty();
	m_vecFields[FilterLife].markDirty();

	writeRegisters( vecFields );

	std::vector<BroanField_t> vecConfirm;
	vecConfirm.push_back( m_vecFields[FilterLife].copyForUpdate( unNewFilterLife ) );
	m_vecFields[FilterLife].markDirty();
	writeRegisters( vecConfirm );
}

void BroanComponent::setHumidityControl( bool enable ) {
	std::vector<BroanField_t> vecFields;

	uint8_t value = 0;

	if (enable) {
		value = 0x01;
	}

	vecFields.push_back( m_vecFields[HumidityControl].copyForUpdate( value ) );

	m_vecFields[HumidityControl].markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setHumiditySetpoint( float humidity ) {
	std::vector<BroanField_t> vecFields;

	vecFields.push_back( m_vecFields[TargetHumidityA].copyForUpdate( humidity ) );
	vecFields.push_back( m_vecFields[TargetHumidityB].copyForUpdate( humidity ) );

	m_vecFields[TargetHumidityA].markDirty();
	m_vecFields[TargetHumidityB].markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setCurrentHumidity( float humidity ) {
	std::vector<BroanField_t> vecFields;
  
	ESP_LOGI("broan_control", "Set current humidity: %0.1f%%", humidity);

	vecFields.push_back( m_vecFields[ControllerHumidity].copyForUpdate( humidity ) );
	m_vecFields[ControllerHumidity].markDirty();

	writeRegisters( vecFields );
}

// period: seconds ON per hour, as stored in 02:22
void BroanComponent::setIntermittentPeriod( uint32_t period ) {
	std::vector<BroanField_t> vecFields;

	ESP_LOGI("broan_control", "Set int period: %us", (unsigned)period);

	vecFields.push_back( m_vecFields[IntModeDuration].copyForUpdate( period ) );
	m_vecFields[IntModeDuration].markDirty();

	writeRegisters( vecFields );
}

}  // namespace broan
}  // namespace esphome
