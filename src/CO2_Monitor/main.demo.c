#include "hw/azure_sphere_learning_path.h"

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"

#include "buzzer_alert.h"
#include "co2_manager.h"
#include "connected_status.h"
//#include "hvac_status.h"
#include "location_from_ip.h"

#include "applibs_versions.h"
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define JSON_MESSAGE_BYTES 256 // Number of bytes to allocate for the JSON telemetry message for IoT Central
#define OneMS 1000000		// used to simplify timer defn.

 // Forward signatures
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer);

DX_USER_CONFIG dx_config;
struct location_info* locInfo = NULL;
float co2_ppm = NAN, temperature = NAN, relative_humidity = NAN;
static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

// Define the message to be sent to Azure IoT Hub
static const char* MsgTemplate = "{ \"CO2\": %3.2f, \"Temperature\": %3.2f, \"Humidity\": %3.1f, \"Pressure\": %3.1f, \"Longitude\": %lf, \"Latitude\":%lf }";


// Declare timers


// Declare Azure IoT Device Twins


// Declare message properties


// Declare timer event handlers


/// <summary>
/// Generic Device Twin Handler to acknowledge device twin received
/// </summary>
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	dx_deviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState);
	dx_deviceTwinAckDesiredState(deviceTwinBinding, deviceTwinBinding->twinState, DX_DEVICE_TWIN_COMPLETED);
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void) {
	dx_azureInitialize(dx_config.scopeId, NULL);
	InitializeSdc30();
	scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity);
	dx_deviceTwinSetOpen(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
	dx_timerSetStart(timerSet, NELEMS(timerSet));
	CO2AlertBuzzerInitialize();
	ConnectedStatusInitialise();
}

/// <summary>
///     Close PeripheralGpios and handlers.
/// </summary>
static void ClosePeripheralGpiosAndHandlers(void) {
	dx_timerSetStop(timerSet, NELEMS(timerSet));
	dx_azureToDeviceStop();
	dx_deviceTwinSetClose();
	scd30_stop_periodic_measurement();
	dx_timerEventLoopStop();
}

int main(int argc, char* argv[]) {
	dx_registerTerminationHandler();
	dx_configParseCmdLineArguments(argc, argv, &dx_config);
	if (!dx_configValidate(&dx_config)) {
		return dx_getTerminationExitCode();
	}
	InitPeripheralGpiosAndHandlers();

	// Main loop
	while (!dx_isTerminationRequired()) {
		int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
		// Continue if interrupted by signal, e.g. due to breakpoint being set.
		if (result == -1 && errno != EINTR) {
			dx_terminate(DX_ExitCode_Main_EventLoopFail);
		}
	}

	ClosePeripheralGpiosAndHandlers();
	return dx_getTerminationExitCode();
}