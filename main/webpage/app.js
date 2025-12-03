/**
 * Globales
 */
var seconds = null;
var otaTimerVar = null;
var wifiConnectInterval = null;

/**
 * Inicialización al cargar la página
 */
$(document).ready(function () {
    //getUpdateStatus();   // si quieres mostrar versión de firmware al entrar

    // Empezar a leer el estado general del sistema
    startStatusInterval();

    // Botón de conectar WiFi
    $("#connect_wifi").on("click", function () {
        checkCredentials();
    });

    // Cargar configuración del modo manual (PWM guardado en NVS)
    loadManualConfig();

    // Cargar configuración del modo automático (tmin / tmax desde NVS)
    loadAutoConfig();

    // Evento del slider de modo manual
    $("#manual_pwm").on("input change", function () {
        onManualSliderChange(this);
    });
});

/* ============================================================
 *  OTA — Actualización de firmware
 * ============================================================
 */

function getFileInfo() {
    var x = document.getElementById("selected_file");
    var file = x.files[0];

    document.getElementById("file_info").innerHTML =
        "<h4>File: " + file.name + "<br>" +
        "Size: " + file.size + " bytes</h4>";
}

function updateFirmware() {
    var formData = new FormData();
    var fileSelect = document.getElementById("selected_file");

    if (fileSelect.files && fileSelect.files.length === 1) {
        var file = fileSelect.files[0];
        formData.set("file", file, file.name);
        document.getElementById("ota_update_status").innerHTML =
            "Uploading " + file.name + ", Firmware Update in Progress...";

        var request = new XMLHttpRequest();

        request.upload.addEventListener("progress", updateProgress);
        request.open("POST", "/OTAupdate");
        request.responseType = "blob";
        request.send(formData);
    } else {
        window.alert("Select A File First");
    }
}

function updateProgress(oEvent) {
    if (oEvent.lengthComputable) {
        getUpdateStatus();
    } else {
        window.alert("total size is unknown");
    }
}

function getUpdateStatus() {
    var xhr = new XMLHttpRequest();
    var requestURL = "/OTAstatus";
    xhr.open("POST", requestURL, false);
    xhr.send("ota_update_status");

    if (xhr.readyState === 4 && xhr.status === 200) {
        var response = JSON.parse(xhr.responseText);

        document.getElementById("latest_firmware").innerHTML =
            response.compile_date + " - " + response.compile_time;

        if (response.ota_update_status === 1) {
            seconds = 10;
            otaRebootTimer();
        } else if (response.ota_update_status === -1) {
            document.getElementById("ota_update_status").innerHTML =
                "!!! Upload Error !!!";
        }
    }
}

function otaRebootTimer() {
    document.getElementById("ota_update_status").innerHTML =
        "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " + seconds;

    if (--seconds === 0) {
        clearTimeout(otaTimerVar);
        window.location.reload();
    } else {
        otaTimerVar = setTimeout(otaRebootTimer, 1000);
    }
}

/* ============================================================
 *  BLOQUE 2: Estado general del sistema (/status.json)
 * ============================================================
 */

// Pedir estado actual al ESP
function getStatusValues() {
    $.getJSON("/status.json", function (data) {

        // Temperatura
        if (typeof data.temp !== "undefined") {
            var t = data.temp;
            if (typeof t === "number") {
                t = t.toFixed(1) + " °C";
            }
            $("#status_temp").text(t);
        }

        // PIR
        if (typeof data.pir !== "undefined") {
            $("#status_pir").text(data.pir ? "Presencia" : "Sin presencia");
        }

        // Modo actual
        if (typeof data.mode !== "undefined") {
            let modeText = "";
            if (data.mode === 0) modeText = "Manual";
            else if (data.mode === 1) modeText = "Automático";
            else if (data.mode === 2) modeText = "Programado";
            else modeText = "Desconocido";
            $("#status_mode").text(modeText);
        }

        // PWM actual
        if (typeof data.pwm !== "undefined") {
            $("#status_pwm").text(data.pwm + " %");
        }
    }).fail(function () {
        // Si aún no existe /status.json, simplemente no actualiza nada.
        // console.warn("No se pudo leer /status.json");
    });
}

// Intervalo periódico (cada 3 s)
function startStatusInterval() {
    setInterval(getStatusValues, 3000);
}

/* ============================================================
 *  Lectura de registros programados
 * ============================================================
 */

function getregValues() {
    $.getJSON("/read_regs.json", function (data) {
        $("#reg_1").text(data["reg1"]);
        $("#reg_2").text(data["reg2"]);
        $("#reg_3").text(data["reg3"]);
        $("#reg_4").text(data["reg4"]);
        $("#reg_5").text(data["reg5"]);
        $("#reg_6").text(data["reg6"]);
        $("#reg_7").text(data["reg7"]);
        $("#reg_8").text(data["reg8"]);
        $("#reg_9").text(data["reg9"]);
        $("#reg_10").text(data["reg10"]);
    });
}

/* ============================================================
 *  WiFi: estado de conexión
 * ============================================================
 */

function stopWifiConnectStatusInterval() {
    if (wifiConnectInterval != null) {
        clearInterval(wifiConnectInterval);
        wifiConnectInterval = null;
    }
}

function getWifiConnectStatus() {
    var xhr = new XMLHttpRequest();
    var requestURL = "/wifiConnectStatus";
    xhr.open("POST", requestURL, false);
    xhr.send("wifi_connect_status");

    if (xhr.readyState === 4 && xhr.status === 200) {
        var response = JSON.parse(xhr.responseText);

        document.getElementById("wifi_connect_status").innerHTML = "Connecting...";

        if (response.wifi_connect_status === 2) {
            document.getElementById("wifi_connect_status").innerHTML =
                "<h4 class='rd'>Failed to Connect. Please check your AP credentials and compatibility</h4>";
            stopWifiConnectStatusInterval();
        } else if (response.wifi_connect_status === 3) {
            document.getElementById("wifi_connect_status").innerHTML =
                "<h4 class='gr'>Connection Success!</h4>";
            stopWifiConnectStatusInterval();
        }
    }
}

function startWifiConnectStatusInterval() {
    wifiConnectInterval = setInterval(getWifiConnectStatus, 2800);
}

/* ============================================================
 *  WiFi: enviar credenciales
 * ============================================================
 */

function connectWifi() {
    var selectedSSID = $("#connect_ssid").val();
    var pwd = $("#connect_pass").val();

    var requestData = {
        "selectedSSID": selectedSSID,
        "pwd": pwd,
        "timestamp": Date.now()
    };

    var requestDataJSON = JSON.stringify(requestData);

    $.ajax({
        url: "/wifiConnect.json",
        dataType: "json",
        method: "POST",
        cache: false,
        data: requestDataJSON,
        contentType: "application/json",
        success: function (response) {
            console.log(response);
        },
        error: function (xhr, status, error) {
            console.error(xhr.responseText);
        }
    });

    //startWifiConnectStatusInterval();
}

function checkCredentials() {
    var errorList = "";
    var credsOk = true;

    var selectedSSID = $("#connect_ssid").val();
    var pwd = $("#connect_pass").val();

    if (selectedSSID === "") {
        errorList += "<h4 class='rd'>SSID cannot be empty!</h4>";
        credsOk = false;
    }
    if (pwd === "") {
        errorList += "<h4 class='rd'>Password cannot be empty!</h4>";
        credsOk = false;
    }

    if (!credsOk) {
        $("#wifi_connect_credentials_errors").html(errorList);
    } else {
        $("#wifi_connect_credentials_errors").html("");
        connectWifi();
    }
}

function showPassword() {
    var x = document.getElementById("connect_pass");
    if (x.type === "password") {
        x.type = "text";
    } else {
        x.type = "password";
    }
}

/* ============================================================
 *  Registros de programación
 * ============================================================
 */

function send_register() {
    var selectedNumber = $("#selectNumber").val();
    var hours = $("#hours").val();
    var minutes = $("#minutes").val();

    var selectedDays = [];
    if ($("#day_mon").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_tue").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_wed").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_thu").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_fri").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_sat").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");
    if ($("#day_sun").prop("checked")) selectedDays.push("1"); else selectedDays.push("0");

    var requestData = {
        "selectedNumber": selectedNumber,
        "hours": hours,
        "minutes": minutes,
        "selectedDays": selectedDays,
        "timestamp": Date.now()
    };

    var requestDataJSON = JSON.stringify(requestData);

    $.ajax({
        url: "/regchange.json",
        dataType: "json",
        method: "POST",
        cache: false,
        data: requestDataJSON,
        contentType: "application/json",
        success: function (response) {
            console.log(response);
        },
        error: function (xhr, status, error) {
            console.error(xhr.responseText);
        }
    });
}

function read_reg() {
    $.ajax({
        url: "/readreg.json",
        dataType: "json",
        method: "POST",
        cache: false
    });
}

function erase_register() {
    var selectedNumber = $("#selectNumber").val();

    var requestData = {
        "selectedNumber": selectedNumber,
        "timestamp": Date.now()
    };

    var requestDataJSON = JSON.stringify(requestData);

    $.ajax({
        url: "/regerase.json",   // <- coincide con http_server_register_erase_handler
        dataType: "json",
        method: "POST",
        cache: false,
        data: requestDataJSON,
        contentType: "application/json",
        success: function (response) {
            console.log(response);
        },
        error: function (xhr, status, error) {
            console.error(xhr.responseText);
        }
    });
}

/* ============================================================
 *  LED de debug (toggle)
 * ============================================================
 */

function toogle_led() {
    $.ajax({
        url: "/toogle_led.json",
        dataType: "json",
        method: "POST",
        cache: false
    });
}

function brigthness_up() {
    $.ajax({
        url: "/toogle_led.json",
        dataType: "json",
        method: "POST",
        cache: false
    });
}

/* ============================================================
 *  MODO MANUAL — Slider PWM
 * ============================================================
 */

// Cargar configuración al abrir la página
function loadManualConfig() {
    $.getJSON("/get_manual_config.json", function (cfg) {
        const pwm =
            cfg && typeof cfg.manual_pwm !== "undefined" ? cfg.manual_pwm : 0;
        $("#manual_pwm").val(pwm);
        $("#manualPwmLabel").text(pwm + "%");
    });
}

// Actualizar la etiqueta cuando se mueve el slider
function onManualSliderChange(el) {
    $("#manualPwmLabel").text(el.value + "%");
}

// Enviar PWM manual al ESP
function sendManualPwm() {
    const pwmVal = parseInt($("#manual_pwm").val(), 10) || 0;
    const status = $("#manual_pwm_status");

    status.text("Guardando y aplicando...");

    $.ajax({
        url: "/manual_pwm.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({ pwm: pwmVal }),
        success: function () {
            status.text("PWM aplicado: " + pwmVal + "%");
        },
        error: function () {
            status.text("Error enviando PWM");
        }
    });
}

/* ============================================================
 *  MODO AUTOMÁTICO — T_min / T_max
 * ============================================================
 */

function loadAutoConfig() {
    $.getJSON("/auto_config.json", function (data) {
        if (typeof data.tmin !== "undefined") {
            $("#auto_tmin").val(data.tmin);
        }
        if (typeof data.tmax !== "undefined") {
            $("#auto_tmax").val(data.tmax);
        }
    });
}

function saveAutoConfig() {
    let tmin = parseInt($("#auto_tmin").val());
    let tmax = parseInt($("#auto_tmax").val());

    $.ajax({
        url: "/set_auto_config.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({ tmin: tmin, tmax: tmax }),
        success: function () {
            $("#auto_config_status").text("Configuración guardada y modo automático activado.");
            alert("Guardado");
        },
        error: function () {
            $("#auto_config_status").text("Error al guardar configuración.");
        }
    });
}



    










    


