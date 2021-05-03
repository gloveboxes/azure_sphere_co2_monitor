# Azure Sphere CO2 Sensor Demo


## Introduction

Introduce hello world demo project structure

1. Avnet Start and Seeed Studio Mini with Grove shield
2. Azure DPS and IoT Hub
3. app_manifest.json
4. [Azure Sphere DevX library](https://github.com/microsoft/Azure-Sphere-DevX)
5. [Azure Sphere Developer Learning Path](http://aka.ms/azure-sphere-developer-learning-path)

---

## Set up

1. Open Azure Portal
2. Start Azure Portal Command Shell
3. Start IoT Hub monitor

	```bash
	az iot hub monitor-events --hub-name <hub-name> --properties all --device-id <device_id>
	```
---

Demo flow

1. Really important concept for Azure Sphere - the platform is secure by default
2. Apps have no access to any resources unless explicitly declared - hardware and network endpoints 
3. app_manifest.json
   1. ID Scope
   2. Capabilities have to be explicitly declared
    	- peripherals
   		- allows network endpoints
1. Discuss main.c structure
1. Set up event timer and handler
1. Add peripheral
1. Add device twin
1. Add Direct method

## Discuss main.c layout

1. Timer event driver architecture 
1. Standard section for includes, and variables
1. InitPeripheralGpiosAndHandlers
1. ClosePeripheralGpiosAndHandlers
1. main and eventloop

---

## Timers

```c
static DX_TIMER measureSensorTimer = { .period = {20, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static DX_TIMER publishTelemetryTimer = { .period = {4, 0}, .name = "publishTelemetryTimer", .handler = PublishTelemetryHandler };

DX_TIMER* timerSet[] = { &measureSensorTimer, &publishTelemetryTimer };
```

---

## Device Twins

```c
DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel = { .twinProperty = "DesiredCO2AlertLevel", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING desiredTemperature = { .twinProperty = "DesiredTemperature", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING reportedCO2Level = { .twinProperty = "ReportedCO2Level", .twinType = DX_TYPE_FLOAT };
DX_DEVICE_TWIN_BINDING reportedCountryCode = { .twinProperty = "ReportedCountryCode",.twinType = DX_TYPE_STRING };
DX_DEVICE_TWIN_BINDING reportedLatitude = { .twinProperty = "ReportedLatitude",.twinType = DX_TYPE_DOUBLE };
DX_DEVICE_TWIN_BINDING reportedLongitude = { .twinProperty = "ReportedLongitude",.twinType = DX_TYPE_DOUBLE };

// Initialize Sets
DX_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = {
	&desiredCO2AlertLevel, &reportedCO2Level, &desiredTemperature, 
	&reportedLatitude, &reportedLongitude, &reportedCountryCode 
};
```

---

## Message Application Properties

```c
static DX_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(DX_MESSAGE_PROPERTY) { .key = "appid", .value = "co2monitor" },
	&(DX_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(DX_MESSAGE_PROPERTY) {.key = "schema", .value = "1" }
};
```

---

## Message System Properties

```c
static DX_MESSAGE_CONTENT_PROPERTIES telemetryContentProperties = {
	.contentEncoding = "utf-8",
	.contentType = "application/json"
};
```

---

## Message Sensor Handler

```c
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

	SetHvacStatusColour((int)temperature);
}
```

---

## Publish Telemtry Handler

```c
static void PublishTelemetryHandler(EventLoopTimer* eventLoopTimer) {
	const float pressure = 1100.0;

	if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
		dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
		return;
	}

	if (!isnan(co2_ppm) && locInfo != NULL) {
		if (snprintf(msgBuffer, JSON_MESSAGE_BYTES, MsgTemplate, co2_ppm, temperature, relative_humidity, pressure, locInfo->lng, locInfo->lat) > 0) {
			
			Log_Debug("%s\n", msgBuffer);

			dx_azureMsgSendWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties), &telemetryContentProperties);
		}
		dx_deviceTwinReportState(&reportedCO2Level, &co2_ppm);
	}
}
```

---

## Discuss Initialization

```c

```

