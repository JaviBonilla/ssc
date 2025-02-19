#include "lib_battery_dispatch.h"
#include "lib_battery_powerflow.h"
#include "lib_power_electronics.h"
#include "lib_shared_inverter.h"

BatteryPower::BatteryPower(double dtHour) :
		dtHour(dtHour),
		powerPV(0),
		powerPVThroughSharedInverter(0),
		powerLoad(0),
		powerBatteryDC(0),
		powerBatteryAC(0),
		powerBatteryTarget(0),
		powerGrid(0),
		powerGeneratedBySystem(0),
		powerPVToLoad(0),
		powerPVToBattery(0),
		powerPVToGrid(0),
		powerPVClipped(0),
		powerClippedToBattery(0),
		powerGridToBattery(0),
		powerGridToLoad(0),
		powerBatteryToLoad(0),
		powerBatteryToGrid(0),
		powerFuelCell(0),
		powerFuelCellToGrid(0),
		powerFuelCellToLoad(0),
		powerFuelCellToBattery(0),
		powerPVInverterDraw(0),
		powerBatteryChargeMaxDC(0),
		powerBatteryDischargeMaxDC(0),
		powerBatteryChargeMaxAC(0),
		powerBatteryDischargeMaxAC(0),
		powerSystemLoss(0),
		powerConversionLoss(0),
		connectionMode(0),
		singlePointEfficiencyACToDC(0.96),
		singlePointEfficiencyDCToAC(0.96), 
		singlePointEfficiencyDCToDC(0.99),
		inverterEfficiencyCutoff(5),
		canPVCharge(false),
		canClipCharge(false),
		canGridCharge(false),
		canDischarge(false),
		canFuelCellCharge(false),
		stateOfChargeMax(1),
		stateOfChargeMin(0),
		depthOfDischargeMax(1),
		tolerance(0.001){}

BatteryPower::BatteryPower(const BatteryPower& orig) {
    sharedInverter = orig.sharedInverter;
    dtHour = orig.dtHour;
    powerPV = orig.powerPV;
    powerPVThroughSharedInverter = orig.powerPVThroughSharedInverter;
    powerLoad = orig.powerLoad;
    powerBatteryDC = orig.powerBatteryDC;
    powerBatteryAC = orig.powerBatteryAC;
    powerBatteryTarget = orig.powerBatteryTarget;
    powerGrid = orig.powerGrid;
    powerGeneratedBySystem = orig.powerGeneratedBySystem;
    powerPVToLoad = orig.powerPVToLoad;
    powerPVToBattery = orig.powerPVToBattery;
    powerPVToGrid = orig.powerPVToGrid;
    powerPVClipped = orig.powerPVClipped;
    powerClippedToBattery = orig.powerClippedToBattery;
    powerGridToBattery = orig.powerGridToBattery;
    powerGridToLoad = orig.powerGridToLoad;
    powerBatteryToLoad = orig.powerBatteryToLoad;
    powerBatteryToGrid = orig.powerBatteryToGrid;
    powerFuelCell = orig.powerFuelCell;
    powerFuelCellToGrid = orig.powerFuelCellToGrid;
    powerFuelCellToLoad = orig.powerFuelCellToLoad;
    powerFuelCellToBattery = orig.powerFuelCellToBattery;
    powerPVInverterDraw = orig.powerPVInverterDraw;
    powerBatteryChargeMaxDC = orig.powerBatteryChargeMaxDC;
    powerBatteryDischargeMaxDC = orig.powerBatteryDischargeMaxDC;
    powerBatteryChargeMaxAC = orig.powerBatteryChargeMaxAC;
    powerBatteryDischargeMaxAC = orig.powerBatteryDischargeMaxAC;
    powerSystemLoss = orig.powerSystemLoss;
    powerConversionLoss = orig.powerConversionLoss;
    connectionMode = orig.connectionMode;
    singlePointEfficiencyACToDC = orig.singlePointEfficiencyACToDC;
    singlePointEfficiencyDCToAC = orig.singlePointEfficiencyDCToAC;
    singlePointEfficiencyDCToDC = orig.singlePointEfficiencyDCToDC;
    canPVCharge = orig.canPVCharge;
    canClipCharge = orig.canClipCharge;
    canGridCharge = orig.canGridCharge;
    canDischarge = orig.canDischarge;
    canFuelCellCharge = orig.canFuelCellCharge;
    stateOfChargeMax = orig.stateOfChargeMax;
    stateOfChargeMin = orig.stateOfChargeMin;
    depthOfDischargeMax = orig.depthOfDischargeMax;
    tolerance = orig.tolerance;
}

void BatteryPower::setSharedInverter(SharedInverter * a_sharedInverter) {
	sharedInverter = a_sharedInverter;
}

void BatteryPower::reset()
{
	powerFuelCell = 0;
	powerFuelCellToGrid = 0;
	powerFuelCellToLoad = 0;
	powerFuelCellToBattery = 0;
	powerBatteryDC = 0;
	powerBatteryAC = 0;
	powerBatteryTarget = 0;
	powerBatteryToGrid = 0;
	powerBatteryToLoad = 0;
	powerClippedToBattery = 0;
	powerConversionLoss = 0;
	powerGeneratedBySystem = 0;
	powerGrid = 0;
	powerGridToBattery = 0;
	powerGridToLoad = 0;
	powerLoad = 0;
	powerPV = 0;
	powerPVThroughSharedInverter = 0;
	powerPVClipped = 0;
	powerPVInverterDraw = 0;
	powerPVToBattery = 0;
	powerPVToGrid = 0;
	powerPVToLoad = 0;
	voltageSystem = 0;
}

BatteryPowerFlow::BatteryPowerFlow(double dtHour)
{
	std::unique_ptr<BatteryPower> tmp(new BatteryPower(dtHour));
	m_BatteryPower = std::move(tmp);
}
BatteryPowerFlow::BatteryPowerFlow(const BatteryPowerFlow& powerFlow)
{
	std::unique_ptr<BatteryPower> tmp(new BatteryPower(*powerFlow.m_BatteryPower));
	m_BatteryPower = std::move(tmp);
}
BatteryPower * BatteryPowerFlow::getBatteryPower()
{
	return m_BatteryPower.get();
}
void BatteryPowerFlow::calculate()
{
	if (m_BatteryPower->connectionMode == ChargeController::AC_CONNECTED) {
		calculateACConnected();
	}
	else if (m_BatteryPower->connectionMode == ChargeController::DC_CONNECTED) {
		calculateDCConnected();
	}
}
void BatteryPowerFlow::initialize(double stateOfCharge)
{
	// If the battery is allowed to discharge, do so
	if (m_BatteryPower->canDischarge && stateOfCharge > m_BatteryPower->stateOfChargeMin + 1.0 &&
		(m_BatteryPower->powerPV < m_BatteryPower->powerLoad || m_BatteryPower->meterPosition == dispatch_t::FRONT))
	{
		// try to discharge full amount.  Will only use what battery can provide
		m_BatteryPower->powerBatteryDC = m_BatteryPower->powerBatteryDischargeMaxDC;
	}
	// Is there extra power from system
	else if ((m_BatteryPower->powerPV > m_BatteryPower->powerLoad && m_BatteryPower->canPVCharge) || m_BatteryPower->canGridCharge)
	{
		if (m_BatteryPower->canPVCharge)
		{
			// use all power available, it will only use what it can handle
			m_BatteryPower->powerBatteryDC = -(m_BatteryPower->powerPV - m_BatteryPower->powerLoad);
		}
		// if we want to charge from grid in addition to, or without array, we can always charge at max power
		if (m_BatteryPower->canGridCharge) {
			m_BatteryPower->powerBatteryDC = -m_BatteryPower->powerBatteryChargeMaxDC;
		}
	}
	m_BatteryPower->powerBatteryTarget = m_BatteryPower->powerBatteryDC;
}

void BatteryPowerFlow::reset()
{
	m_BatteryPower->reset();
}


void BatteryPowerFlow::calculateACConnected()
{
	// The battery power is initially a DC power, which must be converted to AC for powerflow
	double P_battery_dc = m_BatteryPower->powerBatteryDC;

	// These quantities are all AC quantities in KW unless otherwise specified
	double P_pv_ac = m_BatteryPower->powerPV;
	double P_fuelcell_ac = m_BatteryPower->powerFuelCell;
	double P_inverter_draw_ac = m_BatteryPower->powerPVInverterDraw;
	double P_load_ac = m_BatteryPower->powerLoad;
	double P_system_loss_ac = m_BatteryPower->powerSystemLoss;
	double P_pv_to_batt_ac, P_grid_to_batt_ac, P_batt_to_load_ac, P_grid_to_load_ac, P_pv_to_load_ac, P_pv_to_grid_ac, P_batt_to_grid_ac, P_gen_ac, P_grid_ac, P_grid_to_batt_loss_ac, P_batt_to_load_loss_ac, P_batt_to_grid_loss_ac,  P_pv_to_batt_loss_ac, P_fuelcell_to_batt_ac, P_fuelcell_to_load_ac, P_fuelcell_to_grid_ac;
    P_pv_to_batt_ac = P_grid_to_batt_ac = P_batt_to_load_ac = P_grid_to_load_ac = P_pv_to_load_ac = P_pv_to_grid_ac = P_batt_to_grid_ac = P_gen_ac = P_grid_ac = P_grid_to_batt_loss_ac = P_batt_to_load_loss_ac = P_batt_to_grid_loss_ac = P_pv_to_batt_loss_ac = P_fuelcell_to_batt_ac = P_fuelcell_to_load_ac = P_fuelcell_to_grid_ac = 0;

	// convert the calculated DC power to AC, considering the microinverter efficiences
	double P_battery_ac = 0;
	if (P_battery_dc < 0)
		P_battery_ac = P_battery_dc / m_BatteryPower->singlePointEfficiencyACToDC;
	else if (P_battery_dc > 0)
		P_battery_ac = P_battery_dc * m_BatteryPower->singlePointEfficiencyDCToAC;


	// charging 
	if (P_battery_ac <= 0)
	{
        // Test if battery is charging erroneously
        if (!(m_BatteryPower->canPVCharge || m_BatteryPower->canGridCharge || m_BatteryPower->canFuelCellCharge) && P_battery_ac < 0) {
            P_pv_to_batt_ac = P_grid_to_batt_ac = P_fuelcell_to_batt_ac = 0;
            P_battery_ac = 0;
        }
		// PV always goes to load first
		P_pv_to_load_ac = P_pv_ac;
		if (P_pv_to_load_ac > P_load_ac) {
			P_pv_to_load_ac = P_load_ac;
		}
		// Fuel cell goes to load next
		P_fuelcell_to_load_ac = std::fmin(P_load_ac - P_pv_to_load_ac, P_fuelcell_ac);

		// Excess PV can go to battery
		if (m_BatteryPower->canPVCharge){
			P_pv_to_batt_ac = fabs(P_battery_ac);
			if (P_pv_to_batt_ac > P_pv_ac - P_pv_to_load_ac)
			{
				P_pv_to_batt_ac = P_pv_ac - P_pv_to_load_ac;
			}
		}
		// Fuelcell can also charge battery
		if (m_BatteryPower->canFuelCellCharge) {
			P_fuelcell_to_batt_ac = std::fmin(std::fmax(0, fabs(P_battery_ac) - P_pv_to_batt_ac), P_fuelcell_ac - P_fuelcell_to_load_ac);
		}
		// Grid can also charge battery
		if (m_BatteryPower->canGridCharge){
			P_grid_to_batt_ac = std::fmax(0, fabs(P_battery_ac) - P_pv_to_batt_ac - P_fuelcell_to_batt_ac);
		}

		P_pv_to_grid_ac = P_pv_ac - P_pv_to_batt_ac - P_pv_to_load_ac;
		P_fuelcell_to_grid_ac = P_fuelcell_ac - P_fuelcell_to_load_ac - P_fuelcell_to_batt_ac;
	}
	else
	{
		// Test if battery is discharging erroneously
		if (!m_BatteryPower->canDischarge && P_battery_ac > 0) {
			P_batt_to_grid_ac = P_batt_to_load_ac = 0;
			P_battery_ac = 0;
		}
		P_pv_to_load_ac = P_pv_ac;

		// Excess PV production, no other component meets load
		if (P_pv_ac >= P_load_ac)
		{
			P_pv_to_load_ac = P_load_ac;
			P_fuelcell_to_load_ac = 0;
			P_batt_to_load_ac = 0;

			// discharging to grid
			P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;
			P_fuelcell_to_grid_ac = P_fuelcell_ac;
		}
		else {
			P_fuelcell_to_load_ac = std::fmin(P_fuelcell_ac, P_load_ac - P_pv_to_load_ac);
			P_batt_to_load_ac = std::fmin(P_battery_ac, P_load_ac - P_pv_to_load_ac - P_fuelcell_to_load_ac);
		}
		P_batt_to_grid_ac = P_battery_ac - P_batt_to_load_ac;
		P_fuelcell_to_grid_ac = P_fuelcell_ac - P_fuelcell_to_load_ac;
	}

	// compute losses
	P_pv_to_batt_loss_ac = P_pv_to_batt_ac * (1 - m_BatteryPower->singlePointEfficiencyACToDC);
	P_grid_to_batt_loss_ac = P_grid_to_batt_ac *(1 - m_BatteryPower->singlePointEfficiencyACToDC);
	P_batt_to_load_loss_ac = P_batt_to_load_ac * (1 / m_BatteryPower->singlePointEfficiencyDCToAC - 1);
	P_batt_to_grid_loss_ac = P_batt_to_grid_ac * (1 / m_BatteryPower->singlePointEfficiencyDCToAC - 1);

	// Compute total system output and grid power flow
	P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac - P_fuelcell_to_load_ac;
	P_gen_ac = P_pv_ac + P_fuelcell_ac + P_inverter_draw_ac + P_battery_ac - P_system_loss_ac;

    // Grid charging loss accounted for in P_battery_ac
	P_grid_ac = P_gen_ac - P_load_ac;

	// Error checking trying to charge from grid when not allowed
	if (!m_BatteryPower->canGridCharge && P_battery_ac < -tolerance){
	    if ((fabs(P_grid_ac - P_grid_to_load_ac) > tolerance) && (-P_grid_ac > P_grid_to_load_ac)) {
            P_battery_ac = P_pv_ac - P_pv_to_grid_ac - P_pv_to_load_ac;
            P_battery_ac = P_battery_ac > 0 ? P_battery_ac : 0; // Don't swap from charging to discharging
            m_BatteryPower->powerBatteryDC = -P_battery_ac * m_BatteryPower->singlePointEfficiencyACToDC;
            return calculateACConnected();
        }
	}

	// check tolerances
	if (fabs(P_grid_to_load_ac) < m_BatteryPower->tolerance)
		P_grid_to_load_ac = 0;
	if (fabs(P_grid_to_batt_ac) <  m_BatteryPower->tolerance)
		P_grid_to_batt_ac = 0;
	if (fabs(P_grid_ac) <  m_BatteryPower->tolerance)
		P_grid_ac = 0;

	// assign outputs
	m_BatteryPower->powerBatteryAC = P_battery_ac;
	m_BatteryPower->powerGrid = P_grid_ac;
	m_BatteryPower->powerGeneratedBySystem = P_gen_ac;
	m_BatteryPower->powerPVToLoad = P_pv_to_load_ac;
	m_BatteryPower->powerPVToBattery = P_pv_to_batt_ac;
	m_BatteryPower->powerPVToGrid = P_pv_to_grid_ac;
	m_BatteryPower->powerGridToBattery = P_grid_to_batt_ac;
	m_BatteryPower->powerGridToLoad = P_grid_to_load_ac;
	m_BatteryPower->powerBatteryToLoad = P_batt_to_load_ac;
	m_BatteryPower->powerBatteryToGrid = P_batt_to_grid_ac;
	m_BatteryPower->powerFuelCellToBattery = P_fuelcell_to_batt_ac;
	m_BatteryPower->powerFuelCellToLoad= P_fuelcell_to_load_ac;
	m_BatteryPower->powerFuelCellToGrid = P_fuelcell_to_grid_ac;
	m_BatteryPower->powerConversionLoss = P_batt_to_load_loss_ac + P_batt_to_grid_loss_ac + P_grid_to_batt_loss_ac + P_pv_to_batt_loss_ac;
}

void BatteryPowerFlow::calculateDCConnected()
{
	// Quantities are AC in KW unless otherwise specified
	double P_load_ac = m_BatteryPower->powerLoad;
	double P_system_loss_ac = m_BatteryPower->powerSystemLoss;
	double P_battery_ac, P_pv_ac, P_gen_ac, P_pv_to_batt_ac, P_grid_to_batt_ac, P_batt_to_load_ac, P_grid_to_load_ac, P_pv_to_load_ac, P_pv_to_grid_ac, P_batt_to_grid_ac, P_grid_ac, P_conversion_loss_ac;
	P_battery_ac = P_pv_ac = P_pv_to_batt_ac = P_grid_to_batt_ac = P_batt_to_load_ac = P_grid_to_load_ac = P_pv_to_load_ac = P_pv_to_grid_ac = P_batt_to_grid_ac = P_gen_ac = P_grid_ac = P_conversion_loss_ac = 0;
	
	// Quantities are DC in KW unless otherwise specified
	double P_pv_to_batt_dc, P_grid_to_batt_dc, P_pv_to_inverter_dc;
	P_pv_to_batt_dc = P_grid_to_batt_dc = P_pv_to_inverter_dc = 0;

	// The battery power and PV power are initially DC, which must be converted to AC for powerflow
	double P_battery_dc_pre_bms = m_BatteryPower->powerBatteryDC;
	double P_battery_dc = m_BatteryPower->powerBatteryDC;
	double P_pv_dc = m_BatteryPower->powerPV;

	// convert the calculated DC power to DC at the PV system voltage
	if (P_battery_dc_pre_bms < 0)
		P_battery_dc = P_battery_dc_pre_bms / m_BatteryPower->singlePointEfficiencyDCToDC;
	else if (P_battery_dc > 0)
		P_battery_dc = P_battery_dc_pre_bms * m_BatteryPower->singlePointEfficiencyDCToDC;

	double P_gen_dc = P_pv_dc + P_battery_dc;

	// in the event that PV system isn't operating, assume battery BMS converts battery voltage to nominal inverter input at the weighted efficiency
	double voltage = m_BatteryPower->voltageSystem;
	double efficiencyDCAC = m_BatteryPower->sharedInverter->efficiencyAC * 0.01;
	if (voltage <= 0) {
		voltage = m_BatteryPower->sharedInverter->getInverterDCNominalVoltage();
	}
	if (std::isnan(efficiencyDCAC) || m_BatteryPower->sharedInverter->efficiencyAC <= 0) {
		efficiencyDCAC = m_BatteryPower->sharedInverter->getMaxPowerEfficiency() * 0.01;
	}


	// charging 
	if (P_battery_dc < 0)
	{
		// First check whether battery charging came from PV.  
		// Assumes that if battery is charging and can charge from PV, that it will charge from PV before using the grid
		if (m_BatteryPower->canPVCharge || m_BatteryPower->canClipCharge) {
			P_pv_to_batt_dc = fabs(P_battery_dc);
			if (P_pv_to_batt_dc > P_pv_dc) {
				P_pv_to_batt_dc = P_pv_dc;
			}
		}
		P_pv_to_inverter_dc = P_pv_dc - P_pv_to_batt_dc;

		// Any remaining charge comes from grid if allowed
		P_grid_to_batt_dc = fabs(P_battery_dc) - P_pv_to_batt_dc;
        if (!m_BatteryPower->canGridCharge && P_grid_to_batt_dc > tolerance){
            m_BatteryPower->powerBatteryDC = -P_pv_to_batt_dc * m_BatteryPower->singlePointEfficiencyDCToDC;
            return calculateDCConnected();
        }
		
		// Assume inverter only "sees" the net flow in one direction, though practically
		// there should never be case where P_pv_dc - P_pv_to_batt_dc > 0 and P_grid_to_batt_dc > 0 simultaneously
		double P_gen_dc_inverter = P_pv_to_inverter_dc - P_grid_to_batt_dc;

		// convert the DC power to AC
		m_BatteryPower->sharedInverter->calculateACPower(P_gen_dc_inverter, voltage, m_BatteryPower->sharedInverter->Tdry_C);
		efficiencyDCAC = m_BatteryPower->sharedInverter->efficiencyAC * 0.01;


		// Restrict low efficiency so don't get infinites
		if (efficiencyDCAC <= 0.05 && (P_grid_to_batt_dc > 0 || P_pv_to_inverter_dc > 0)) {
			efficiencyDCAC = 0.05;
		}
		// This is a traditional DC/AC efficiency loss
		if (P_gen_dc_inverter > 0) {
			m_BatteryPower->sharedInverter->powerAC_kW = P_gen_dc_inverter * efficiencyDCAC;
		}
		// if we are charging from grid, then we actually care about the amount of grid power it took to achieve the DC value
		else {
			m_BatteryPower->sharedInverter->powerAC_kW = P_gen_dc_inverter / efficiencyDCAC;
		}
		m_BatteryPower->sharedInverter->efficiencyAC = efficiencyDCAC * 100;

		// Compute the AC quantities
		P_gen_ac = m_BatteryPower->sharedInverter->powerAC_kW;
		P_grid_to_batt_ac = P_grid_to_batt_dc / efficiencyDCAC;
		if (std::isnan(P_gen_ac) && m_BatteryPower->sharedInverter->powerDC_kW == 0){
		    P_gen_ac = 0;
            P_grid_to_batt_ac = 0;
		}
		P_pv_ac = P_pv_to_inverter_dc * efficiencyDCAC;
		P_pv_to_load_ac = P_load_ac;
		if (P_pv_to_load_ac > P_pv_ac) {
			P_pv_to_load_ac = P_pv_ac;
		}
		P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac;
		P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;

		// In this case, we have a combo of Battery DC power from the PV array, and potentially AC power from the grid
		if (P_pv_to_batt_dc + P_grid_to_batt_ac > 0) {
			P_battery_ac = -(P_pv_to_batt_dc + P_grid_to_batt_ac);
		}
		
		// Assign this as AC values, even though they are fully DC
		P_pv_to_batt_ac = P_pv_to_batt_dc;
	}
	else
	{
		// convert the DC power to AC
		m_BatteryPower->sharedInverter->calculateACPower(P_gen_dc, voltage, m_BatteryPower->sharedInverter->Tdry_C);
		efficiencyDCAC = m_BatteryPower->sharedInverter->efficiencyAC * 0.01;
		P_gen_ac = m_BatteryPower->sharedInverter->powerAC_kW;

		P_battery_ac = P_battery_dc * efficiencyDCAC;
		P_pv_ac = P_pv_dc * efficiencyDCAC;

		// Test if battery is discharging erroneously
		if (!m_BatteryPower->canDischarge && P_battery_ac > 0) {
			P_batt_to_grid_ac = P_batt_to_load_ac = 0;
			P_battery_ac = 0;
		}

		P_pv_to_load_ac = P_pv_ac;
		if (P_pv_ac >= P_load_ac)
		{
			P_pv_to_load_ac = P_load_ac;
			P_batt_to_load_ac = 0;

			// discharging to grid
			P_pv_to_grid_ac = P_pv_ac - P_pv_to_load_ac;
		}
		else {
			P_batt_to_load_ac = std::fmin(P_battery_ac, P_load_ac - P_pv_to_load_ac);
		}
		P_batt_to_grid_ac = P_battery_ac - P_batt_to_load_ac; 
	}

	// compute losses
	P_conversion_loss_ac = P_gen_dc - P_gen_ac + P_battery_dc_pre_bms - P_battery_dc;
	
	// Compute total system output and grid power flow, inverter draw is built into P_pv_ac
	P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac;
	P_gen_ac -= P_system_loss_ac;

	// Grid charging loss accounted for in P_battery_ac 
	P_grid_ac = P_gen_ac - P_load_ac;

	// Error checking for power to load
	if (P_pv_to_load_ac + P_grid_to_load_ac + P_batt_to_load_ac != P_load_ac)
		P_grid_to_load_ac = P_load_ac - P_pv_to_load_ac - P_batt_to_load_ac;

	// check tolerances
	if (fabs(P_grid_to_load_ac) < m_BatteryPower->tolerance)
		P_grid_to_load_ac = 0;
	if (fabs(P_grid_to_batt_ac) <  m_BatteryPower->tolerance)
		P_grid_to_batt_ac = 0;
	if (fabs(P_grid_ac) <  m_BatteryPower->tolerance)
		P_grid_ac = 0;

	// assign outputs
	m_BatteryPower->powerBatteryAC = P_battery_ac;
	m_BatteryPower->powerGrid = P_grid_ac;
	m_BatteryPower->powerGeneratedBySystem = P_gen_ac;
	m_BatteryPower->powerPVToLoad = P_pv_to_load_ac;
	m_BatteryPower->powerPVToBattery = P_pv_to_batt_ac;
	m_BatteryPower->powerPVToGrid = P_pv_to_grid_ac;
	m_BatteryPower->powerGridToBattery = P_grid_to_batt_ac;
	m_BatteryPower->powerGridToLoad = P_grid_to_load_ac;
	m_BatteryPower->powerBatteryToLoad = P_batt_to_load_ac;
	m_BatteryPower->powerBatteryToGrid = P_batt_to_grid_ac;
	m_BatteryPower->powerConversionLoss = P_conversion_loss_ac;
}

