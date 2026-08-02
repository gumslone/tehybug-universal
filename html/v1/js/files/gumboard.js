;
var ipAddress = $(location).attr('hostname');
var pageName = 'dash';
var timeleft;
var rebootTimer;
var json;
var boardURL = 'https://tehybug.com/tehybug/v1/';

$(function () {
    // Active Menu Button select
    $('.nav-link').click(function () {
        $('.nav-link').removeClass('active');
        $(this).addClass('active');
    });

    ChangePage('main', '#page');

    setTimeout(function () {
        connectionStart();
    }, 1000);
});

var connection = null;

function connectionStart() {
    if (connection != null && connection.readyState != WebSocket.CLOSED) {
        connection.close();
    }

    var wsServer = ipAddress;
    let socketPageName = pageName;
    
    if (pageName == "cloud_settings" || pageName == "ha_settings") {
        socketPageName = "settings";
    }

    connection = new WebSocket('ws://' + wsServer + ':81/' + socketPageName);

    connection.onopen = function () {
        updateConnectionStatus(true);

        if (pageName == 'setConfig') {
            connection.send(json);
        }

        KeepAlive();
    };

    connection.onclose = function (e) {
        console.log('WebSocket connection closed');
        updateConnectionStatus(false);
    };

    connection.onerror = function (error) {
        console.log('WebSocket Error: ' + error);
        if (connection.readyState !== WebSocket.CLOSED) {
            connection.close();
        }
    };

    connection.onmessage = function (e) {
        console.log('WebSocket incoming message: ' + e.data);
        RefreshData(e.data);
    };

    function KeepAlive() {
        const timeout = 1000;
        if (connection.readyState == WebSocket.OPEN) {
            connection.send("KeepAlive");
        }
        setTimeout(KeepAlive, timeout);
    }
}

function updateConnectionStatus(isOnline) {
    const statusElement = $("#connectionStatus");
    if (isOnline) {
        statusElement.html('<span data-feather="wifi" style="width: 16px; height: 16px;"></span> Online')
            .removeClass("text-danger")
            .addClass("text-success");
    } else {
        statusElement.html('<span data-feather="wifi-off" style="width: 16px; height: 16px;"></span> Offline')
            .removeClass("text-success")
            .addClass("text-danger");
    }
    feather.replace();
}

const sensorMap = {
    'temp': { name: "Temperature", unit: "°C", url: '&t=%temp%', mqtt: ', "temp":"%temp%"' },
    'temp_imp': { name: "Temperature", unit: "°F", url: '', mqtt: ', "temp_imp":"%temp_imp%"' },
    'temp2': { name: "Temperature2", unit: "°C", url: '&t2=%temp2%', mqtt: ', "temp2":"%temp2%"' },
    'temp2_imp': { name: "Temperature2", unit: "°F", url: '', mqtt: ', "temp2_imp":"%temp2_imp%"' },
    'humi': { name: "Humidity", unit: "%RH", url: '&h=%humi%', mqtt: ', "humi":"%humi%"' },
    'humi2': { name: "Humidity2", unit: "%RH", url: '&h2=%humi2%', mqtt: ', "humi2":"%humi2%"' },
    'ah': { name: "Absolute humidity", unit: "g/m³", url: '&ah=%ah%', mqtt: ', "ah":"%ah%"' },
    'ah2': { name: "Absolute humidity2", unit: "g/m³", url: '', mqtt: ', "ah2":"%ah2%"' },
    'cr': { name: "Comfort ratio", unit: "%", url: '', mqtt: ', "cr":"%cr%"' },
    'cr2': { name: "Comfort ratio2", unit: "%", url: '', mqtt: ', "cr2":"%cr2%"' },
    'dew': { name: "Dew point", unit: "°C", url: '', mqtt: ', "dew":"%dew%"' },
    'dew_imp': { name: "Dew point", unit: "°F", url: '', mqtt: ', "dew_imp":"%dew_imp%"' },
    'hi': { name: "Heat index", unit: "°C", url: '', mqtt: ', "hi":"%hi%"' },
    'hi_imp': { name: "Heat index", unit: "°F", url: '', mqtt: ', "hi_imp":"%hi_imp%"' },
    'air': { name: "Gas resistance", unit: "kOhm", url: '&a=%air%', mqtt: ', "air":"%air%"' },
    'iaq': { name: "Indoor air quality", unit: "", url: '', mqtt: ', "iaq":"%iaq%"' },
    'qfe': { name: "Atmospheric pressure", unit: "hPa", url: '&p=%qfe%', mqtt: ', "qfe":"%qfe%"' },
    'alt': { name: "Altitude", unit: "m", url: '', mqtt: ', "alt":"%alt%"' },
    'eco2': { name: "CO2 equivalent", unit: "", url: '', mqtt: ', "eco2":"%eco2%"' },
    'bvoc': { name: "breath VOC equivalent", unit: "", url: '', mqtt: ', "bvoc":"%bvoc%"' },
    'uv': { name: "UV index", unit: "", url: '&u=%uv%', mqtt: ', "uv":"%uv%"' },
    'lux': { name: "Ambient light", unit: "Lux", url: '&l=%lux%', mqtt: ', "lux":"%lux%"' },
    'adc': { name: "ADC", unit: "ADC", url: '&x=%adc%', mqtt: ', "adc":"%adc%"' }
};

// Every sensor key the device has actually reported this session. This is
// what lets the portal guess a payload or query string that matches the
// hardware instead of a generic example.
const availableSensors = {};

// sensorMap order, filtered to what the device really has
function knownSensorKeys() {
    return Object.keys(sensorMap).filter(function (k) { return availableSensors[k]; });
}

// The device reports metric keys plus imperial siblings (temp_imp, dew_imp,
// hi_imp...) side by side. A suggestion should be one system, not both:
// metric takes the base keys, imperial swaps in the _imp sibling wherever the
// device computes one and keeps the base key where none exists (humidity,
// pressure, lux have no imperial variant).
function unitAdjustedKeys(units) {
    const keys = knownSensorKeys().filter(function (k) { return k.indexOf('_imp') < 0; });
    if (units !== 'imperial') {
        return keys;
    }
    return keys.map(function (k) { return availableSensors[k + '_imp'] ? k + '_imp' : k; });
}

// "bug_key=%key%&t=%temp%&h=%humi%..." from the device's own sensors. The
// imperial variant keeps the same short parameter names and only swaps the
// placeholder, so t= simply carries Fahrenheit.
function suggestedGetQuery(units) {
    const parts = unitAdjustedKeys(units).map(function (k) {
        const base = k.replace('_imp', '');
        return (sensorMap[base].url || '').replace('%' + base + '%', '%' + k + '%');
    });
    return 'bug_key=%key%' + parts.join('');
}

// '{"temp":"%temp%", "humi":"%humi%", ...}' from the device's own sensors
function suggestedJsonPayload(units) {
    const parts = unitAdjustedKeys(units).map(function (k) { return sensorMap[k].mqtt; }).join('');
    return '{' + parts.replace(/^, /, '') + '}';
}

// One-click apply into the real fields. The GET helper keeps whatever server
// the user already entered (everything before '?') and only regenerates the
// query string; with no usable URL in the field it falls back to the cloud
// default.
// True once at least one real reading has arrived; the links explain
// themselves instead of sitting disabled when it has not.
function haveSensorSuggestions() {
    if (knownSensorKeys().length > 0) {
        return true;
    }
    alert('No sensor readings received from the device yet - wait a moment and try again.');
    return false;
}

function applySuggestedGetUrl(fieldId, units) {
    if (!haveSensorSuggestions()) {
        return;
    }
    const current = $('#' + fieldId).val() || '';
    const base = current.indexOf('://') > 0 ? current.split('?')[0] : 'http://tehybug.com/track/';
    $('#' + fieldId).val(base + '?' + suggestedGetQuery(units));
}

function applySuggestedPayload(fieldId, units) {
    if (!haveSensorSuggestions()) {
        return;
    }
    $('#' + fieldId).val(suggestedJsonPayload(units));
}

function sensorData(key, value) {
    if (!sensorMap[key]) {
        return;
    }

    const sensor = sensorMap[key];
    availableSensors[key] = true;

    // Rebuild the reference strings from the full known set instead of
    // appending this key: the device pushes sensor data repeatedly, and
    // appending grew these with duplicates on every message.
    $("#url").text('http://tehybug.com/track/?' + suggestedGetQuery());
    $("#mqtt_message").text(suggestedJsonPayload().slice(1, -1) ? ', ' + suggestedJsonPayload().slice(1, -1) : '');

    if (pageName == 'main') {
        $("#sensor_data").append(`<tr><td>${sensor.name}</td><td>${value} ${sensor.unit}</td></tr>`);
    } else if (pageName == 'cloud_settings') {
        // idempotent for the same reason: regenerate rather than append
        $("#httpGetURL").val('http://tehybug.com/track/?' + suggestedGetQuery());
    } else {
        // dedupe the placeholder table too - repeated pushes re-listed every row
        if (!$("#sensor_data").find('[data-key="' + key + '"]').length) {
            $("#sensor_data").append(`<tr data-key="${key}"><td>${sensor.name}</td><td><code>%${key}%</code></td><td>${sensor.unit}</td></tr>`);
        }
    }
}

function RefreshData(input) {
    // Validate JSON
    if (!input.startsWith("{")) {
        return;
    }

    const jsonData = $.parseJSON(input);

    // Skip if only contains empty key
    if (Object.keys(jsonData).length === 1 && jsonData.key === "") {
        console.log('Skipping empty key message');
        return;
    }

    // Log JSON
    if (jsonData.log) {
        const logArea = $('#log');
        logArea.append(`${jsonData.log.function}: ${jsonData.log.message}\n`);
        logArea.scrollTop(logArea[0].scrollHeight);
        return;
    }

    // Only reset the dashboard sensor table when this message actually
    // carries sensor readings: on connect the device pushes info, sensor
    // data and config in sequence, and the trailing config message would
    // otherwise wipe the freshly rendered table.
    if (pageName == 'main' && Object.keys(jsonData).some(function (k) { return sensorMap[k]; })) {
        $("#sensor_data").html('');
    }

    $.each(jsonData, function (key, val) {
        // Config JSON
        const configPages = ['settings', 'ha_settings', 'cloud_settings', 'setsensor', 'scenarios', 'setsystem', 'datalog'];
        if (configPages.includes(pageName)) {
            if (typeof val === 'boolean') {
                $("#" + key).not(".dont-change").prop('checked', val);
            } else {
                $("#" + key).not(".dont-change").val(val.toString());
            }
        }

        // SystemInfo JSON
        const infoPages = ['main', 'firststart', 'cloud_settings'];
        if (infoPages.includes(pageName)) {
            if (key === 'key' && val.toString() === "") {
                return; // Do not update if key is empty
            }
            $("#" + key).html(val.toString());
            if (key == 'key') {
                $("." + key).html(val.toString());
            }
        }

        // Sensor data
        const sensorPages = ['settings', 'cloud_settings', 'main'];
        if (sensorPages.includes(pageName)) {
            sensorData(key, val.toString());
        }
    });
}

function SaveConfig() {
    const obj = {};

    // Read all inputs
    $("input").each(function () {
        console.log(`SaveConfig -> ID: ${this.id}, Val: ${this.type == 'checkbox' ? $(this).prop('checked') : $(this).val()}`);

        if (this.type == 'checkbox') {
            obj[this.id] = $(this).prop('checked');
        } else {
            obj[this.id] = $(this).val();
        }
    });

    // Read all selects
    $("select").each(function () {
        console.log(`SaveConfig -> ID: ${this.id}, Val: ${$(this).val()}`);
        obj[this.id] = $(this).val();
    });

    json = JSON.stringify(obj);
    console.log(json);
    pageName = "setConfig";

    connectionStart();

    // Restart Countdown
    const timeout = 12000;
    StartCountDown(timeout / 1000);

    setTimeout(function () {
        $("#popup").modal('hide');
    }, timeout);

    setTimeout(function () {
        location.reload();
    }, timeout + 500);
}

function ChangePage(_pageName, destination) {
    pageName = _pageName;
    $(destination).load(boardURL + 'html/' + _pageName + '.php');
}

function ChangeContent(element, _pageName, destination) {
    pageName = _pageName;
    $(destination).load(boardURL + 'html/' + _pageName + '.php');
    $('.nav-link').removeClass('active');
    $(element).addClass('active');
    // Update hash
    window.location.hash = _pageName;
}

function ChangeContentIframe(element, _pageName, destination) {
    pageName = _pageName;
        // Update hash
    $(destination).html(`<iframe src="/${pageName}" title="" width="100%" height="100%"></iframe>`);
    $('.nav-link').removeClass('active');
    $(element).addClass('active');
}

// Handle hashtag navigation
function handleHashNavigation() {
    const hash = window.location.hash.substring(1);
    
    if (hash && hash !== '') {
        const navLink = document.querySelector(`a[onclick*="'${hash}'"]`);
        
        if (navLink && typeof ChangeContent === 'function') {
            ChangeContent(navLink, hash, '#right-content');
        }
    }
}

function StartCountDown(_timeleft) {
    timeleft = _timeleft;
    rebootTimer = setInterval(function () {
        timeleft--;
        $("#countdowntimer").html(timeleft);
        if (timeleft <= 0) {
            clearInterval(rebootTimer);
        }
    }, 1000);
}

function openIpAddress(element) {
    const ip = element.innerText;
    const ipRegex = /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
    if (ipRegex.test(ip)) {
        window.open('http://' + ip, '_blank');
    }
}