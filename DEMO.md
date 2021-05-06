# Azure Sphere CO2 Sensor Demo

## Demo overview

- [both] intro
- [Mike] The anatomy of a secure purpose built IoT App
- [mike] show simple app that shows the value of app_manifest. Use CO2 app for consistancy
- [dave] Build out CO2 demo Seeed Studio Mini
- [dave] talk about IoT Hub, routing, enrichment
- [mike] use use CO2 demo + Avnet, show telemetry in IoT Explorer, 
- [mike] layer in the Event Grid, message filtering (based on value threshold), and Teams messages (can be any WebHook endpoint)
- [dave] add CO2, and PowerBI
- [both] close with call to action [Azure Sphere Developer Learning Path](http://aka.ms/azure-sphere-developer-learning-path)

## Introduction

Introduce hello world demo project structure

1. Avnet Start and Seeed Studio Mini with Grove shield
2. Azure DPS and IoT Hub
3. app_manifest.json
4. [Azure Sphere DevX library](https://github.com/microsoft/Azure-Sphere-DevX)
5. [Azure Sphere Developer Learning Path](http://aka.ms/azure-sphere-developer-learning-path)

---

## Set up

1. Clone the demo repo: 

    ```bash
    git clone --recurse-submodules gloveboxes/azure_sphere_co2_monitor (github.com)
    ```
    
1. Open Azure Portal
1. Start Azure Portal Command Shell
1. Start IoT Hub monitor

	```bash
	az iot hub monitor-events --hub-name <hub-name> --properties all --device-id <device_id>
	```

---

## Demo flow

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

1. event driven programming architecture driver architecture 
1. Standard section for includes, and variables
1. InitPeripheralGpiosAndHandlers
1. ClosePeripheralGpiosAndHandlers
1. main and eventloop

---

## Timers

```c
static DX_TIMER measureSensorTimer = { .period = {4, 0}, .name = "measureSensorTimer", .handler = MeasureSensorHandler };
static DX_TIMER publishTelemetryTimer = { .period = {4, 0}, .name = "publishTelemetryTimer", .handler = PublishTelemetryHandler };

DX_TIMER* timerSet[] = { &measureSensorTimer, &publishTelemetryTimer };
```

---

## Device Twins

```c
// Azure IoT Device Twins
DX_DEVICE_TWIN_BINDING desiredCO2AlertLevel = { .twinProperty = "DesiredCO2AlertLevel", .twinType = DX_TYPE_INT, .handler = DeviceTwinGenericHandler };
DX_DEVICE_TWIN_BINDING reportedCO2Level = { .twinProperty = "ReportedCO2Level", .twinType = DX_TYPE_FLOAT };

// Initialize Sets
DX_DEVICE_TWIN_BINDING* deviceTwinBindingSet[] = { &desiredCO2AlertLevel, &reportedCO2Level };
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
}
```

---

## Publish Telemetry Handler

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

## CO2 Demo Telemetry

```json
{
    "event": {
        "origin": "4d660122b831da08fb32b3c70795e09b9a50abd7a556fe035b771b8aba5c195bf43daa5227983b993101fac76e7a5a4c1295a7571896dbfeda66ad5bfa6ac954",
        "payload": {
            "CO2": 969.71,
            "Temperature": 31.26,
            "Humidity": 45.0,
            "Pressure": 1100.0,
            "Longitude": 151.2122,
            "Latitude": -33.8563,
            "MsgId": -1
        },
        "annotations": {
            "iothub-connection-device-id": "4d660122b831da08fb32b3c70795e09b9a50abd7a556fe035b771b8aba5c195bf43daa5227983b993101fac76e7a5a4c1295a7571896dbfeda66ad5bfa6ac954",
            "iothub-connection-auth-method": "{\"scope\":\"device\",\"type\":\"x509Certificate\",\"issuer\":\"external\",\"acceptingIpFilterRule\":null}",
            "iothub-connection-auth-generation-id": "637547758448037735",
            "iothub-enqueuedtime": 1620021375309,
            "iothub-message-source": "Telemetry",
            "x-opt-sequence-number": 1156752,
            "x-opt-offset": "420915998736",
            "x-opt-enqueued-time": 1620021375403
        },
        "properties": {
            "system": {
                "content_encoding": "utf-8",
                "content_type": "application/json"
            },
            "application": {
                "appid": "co2monitor",
                "type": "telemetry",
                "schema": "1",
                "name": "Sydney Reactor CO2 Monitor",
                "latitude": "-33.8767984",
                "weather-location": "Sydney, AU",
                "longitude": "151.2083053"
            }
        }
    }
}
```

