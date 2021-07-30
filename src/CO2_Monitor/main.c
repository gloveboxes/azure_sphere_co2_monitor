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
#define NETWORK_INTERFACE "wlan0"

 // Forward signatures
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding);
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer);
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer);

DX_USER_CONFIG dx_config;
struct location_info* locInfo = NULL;

float co2_ppm = NAN, temperature = NAN, relative_humidity = NAN;

static char msgBuffer[JSON_MESSAGE_BYTES] = { 0 };

DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel = { .propertyName = "DesiredCO2AlertLevel", .twinType = DX_DEVICE_TWIN_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING reportedCO2Level = { .propertyName = "ReportedCO2Level", .twinType = DX_DEVICE_TWIN_FLOAT };

static DX_TIMER_BINDING measureSensorTimer = { .period = {4, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static DX_TIMER_BINDING publishTelemetryTimer = { .period = {60, 0}, .name = "publishTelemetryTimer", .handler = PublishTelemetryHandler };

// Initialize Sets
DX_TIMER_BINDING* timerSet[] = { &measureSensorTimer, &publishTelemetryTimer };
DX_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = { &desiredCO2AlertLevel, &reportedCO2Level };


// Declare message application properties
static DX_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(DX_MESSAGE_PROPERTY) { .key = "appid", .value = "co2monitor" },
	&(DX_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(DX_MESSAGE_PROPERTY) {.key = "schema", .value = "1" }
};


// Declare message system properties
static DX_MESSAGE_CONTENT_PROPERTIES telemetryContentProperties = {
	.contentEncoding = "utf-8",
	.contentType = "application/json"
};


// Declare timer event handlers

/// <summary>
/// Read sensor and send to Azure IoT
/// </summary>
static void MeasureSensorHandler(EventLoopTimer* eventLoopTimer) {
	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity) != STATUS_OK) {
		co2_ppm = NAN;
	}
}

static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer) {
	const float pressure = 1100.0;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (!isnan(co2_ppm) && locInfo != NULL) {

		// Serialize telemetry as JSON
		bool serialization_result = dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 6,
			DX_JSON_DOUBLE, "CO2", co2_ppm,
			DX_JSON_DOUBLE, "Temperature", temperature,
			DX_JSON_DOUBLE, "Humidity", relative_humidity,
			DX_JSON_DOUBLE, "Pressure", pressure,
			DX_JSON_DOUBLE, "Longitude", locInfo->lng,
			DX_JSON_DOUBLE, "Latitude", locInfo->lat
		);

		if (serialization_result) {
			Log_Debug("%s\n", msgBuffer);
			dx_azurePublish(msgBuffer, strlen(msgBuffer), telemetryMessageProperties, NELEMS(telemetryMessageProperties), &telemetryContentProperties);
		} else {
			Log_Debug("JSON Serialization failed: Buffer too small\n");
		}

		dx_deviceTwinReportValue(&reportedCO2Level, &co2_ppm);
	}
}


/// <summary>
/// Generic Device Twin Handler to acknowledge device twin received
/// </summary>
static void DeviceTwinGenericHandler(DX_DEVICE_TWIN_BINDING* deviceTwinBinding) {
	dx_deviceTwinReportValue(deviceTwinBinding, deviceTwinBinding->propertyValue);
	dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue, DX_DEVICE_TWIN_RESPONSE_COMPLETED);
}

/// <summary>
///  Initialize PeripheralGpios, device twins, direct methods, timers.
/// </summary>
/// <returns>0 on success, or -1 on failure</returns>
static void InitPeripheralGpiosAndHandlers(void) {
	dx_azureConnect(&dx_config, NETWORK_INTERFACE, NULL);
	InitializeSdc30();
	scd30_read_measurement(&co2_ppm, &temperature, &relative_humidity);
	dx_deviceTwinSubscribe(deviceTwinBindingSet, NELEMS(deviceTwinBindingSet));
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
	dx_deviceTwinUnsubscribe();
	scd30_stop_periodic_measurement();
	dx_timerEventLoopStop();
}

int main(int argc, char* argv[]) {
	dx_registerTerminationHandler();

	if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
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