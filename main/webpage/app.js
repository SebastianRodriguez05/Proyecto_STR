/**
 * ============================================
 *  VARIABLES GLOBALES
 * ============================================
 */
var seconds = null;
var otaTimerVar = null;
var currentMode = null; // 0=manual,1=auto,2=programado

/**
 * ============================================
 *  INICIALIZACIÓN AL CARGAR LA PÁGINA
 * ============================================
 */
$(document).ready(function () {

    // Primer refresh inmediato de estado
    getStatusValues();

    // Empezar a leer el estado general del sistema cada 2.5 s
    startStatusInterval();

    // Cargar configuración del modo manual
    loadManualConfig();

    // Cargar configuración del modo automático
    loadAutoConfig();

    // Cargar por defecto el registro 1 (si existe)
    $("#prog_id").val("1");
    loadProgramSlot();

    // Evento del slider de modo manual
    $("#manual_pwm").on("input change", function () {
        onManualSliderChange(this);
    });
});


/* ============================================================
 *                     OTA — FIRMWARE UPDATE
 * ============================================================ */

function getFileInfo() {
    const file = document.getElementById("selected_file").files[0];
    if (!file) return;

    document.getElementById("file_info").innerHTML =
        `<h4>File: ${file.name}<br>Size: ${file.size} bytes</h4>`;
}

function updateFirmware() {
    const fileSelect = document.getElementById("selected_file");

    if (!fileSelect.files || fileSelect.files.length !== 1) {
        alert("Selecciona un archivo primero");
        return;
    }

    const formData = new FormData();
    formData.set("file", fileSelect.files[0], fileSelect.files[0].name);

    document.getElementById("ota_update_status").innerHTML =
        "Uploading... Firmware Update in Progress...";

    var request = new XMLHttpRequest();
    request.upload.addEventListener("progress", updateProgress);
    request.open("POST", "/OTAupdate");
    request.responseType = "blob";
    request.send(formData);
}

function updateProgress(oEvent) {
    if (oEvent.lengthComputable) {
        getUpdateStatus();
    }
}

function getUpdateStatus() {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/OTAstatus", false);
    xhr.send("ota_update_status");

    if (xhr.readyState === 4 && xhr.status === 200) {
        const res = JSON.parse(xhr.responseText);

        document.getElementById("latest_firmware").innerHTML =
            `${res.compile_date} - ${res.compile_time}`;

        if (res.ota_update_status === 1) {
            seconds = 10;
            otaRebootTimer();
        } else if (res.ota_update_status === -1) {
            document.getElementById("ota_update_status").innerHTML = "!!! Upload Error !!!";
        }
    }
}

function otaRebootTimer() {
    document.getElementById("ota_update_status").innerHTML =
        `OTA complete. Rebooting in: ${seconds}`;

    if (--seconds === 0) {
        window.location.reload();
    } else {
        otaTimerVar = setTimeout(otaRebootTimer, 1000);
    }
}


/* ============================================================
 *                      STATUS DEL SISTEMA
 * ============================================================ */

function getStatusValues() {
    $.getJSON("/status.json", function (data) {

        // Temperatura
        if (typeof data.temp !== "undefined") {
            let t = data.temp;
            if (typeof t === "number") t = t.toFixed(1) + " °C";
            $("#status_temp").text(t);
        }

        // PIR
        if (typeof data.pir !== "undefined") {
            $("#status_pir").text(data.pir ? "Presencia" : "Sin presencia");
        }

        // Modo actual
        if (typeof data.mode !== "undefined") {
            let txt =
                data.mode === 0 ? "Manual" :
                data.mode === 1 ? "Automático" :
                data.mode === 2 ? "Programado" : "Desconocido";

            $("#status_mode").text(txt);

            // Sincronizar el selector de modo (radios) con el valor real
            $("input[name='mode_select'][value='" + data.mode + "']").prop("checked", true);

            // Mostrar/ocultar tarjetas según modo
            showModeCards(data.mode);
        }

        // PWM actual
        if (typeof data.pwm !== "undefined") {
            $("#status_pwm").text(data.pwm + " %");
        }
    });
}

function startStatusInterval() {
    setInterval(getStatusValues, 2500);
}


/* ============================================================
 *                     WIFI
 * ============================================================ */

function showPassword() {
    var x = document.getElementById("connect_pass");
    x.type = (x.type === "password") ? "text" : "password";
}

function checkCredentials() {
    const ssid = $("#connect_ssid").val();
    const pwd = $("#connect_pass").val();
    let error = "";

    if (ssid === "") error += "<h4 class='rd'>SSID no puede estar vacío</h4>";
    if (pwd === "") error += "<h4 class='rd'>Password no puede estar vacío</h4>";

    if (error) {
        $("#wifi_connect_credentials_errors").html(error);
    } else {
        $("#wifi_connect_credentials_errors").html("");
        connectWifi();
    }
}

function connectWifi() {
    const ssid = $("#connect_ssid").val();
    const pwd  = $("#connect_pass").val();

    $.ajax({
        url: "/wifiConnect.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({
            selectedSSID: ssid,
            pwd: pwd,
            timestamp: Date.now()
        }),
        success: function () {
            $("#wifi_connect_status").text("Conectando...");
        }
    });
}

function toogle_led() {
    $.ajax({
        url: "/toogle_led.json",
        method: "POST"
    });
}


/* ============================================================
 *                MODO MANUAL — PWM
 * ============================================================ */

function loadManualConfig() {
    $.getJSON("/get_manual_config.json", function (cfg) {
        const pwm = cfg && typeof cfg.manual_pwm !== "undefined" ? cfg.manual_pwm : 0;
        $("#manual_pwm").val(pwm);
        $("#manualPwmLabel").text(pwm + "%");
    });
}

function onManualSliderChange(el) {
    $("#manualPwmLabel").text(el.value + "%");
}

function sendManualPwm() {
    const pwmVal = parseInt($("#manual_pwm").val(), 10) || 0;

    $("#manual_pwm_status").text("Guardando...");

    $.ajax({
        url: "/manual_pwm.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({ pwm: pwmVal }),
        success: function () {
            $("#manual_pwm_status").text("PWM aplicado: " + pwmVal + "%");
        },
        error: function () {
            $("#manual_pwm_status").text("Error enviando PWM");
        }
    });
}


/* ============================================================
 *           MODO AUTOMÁTICO — T_min / T_max
 * ============================================================ */

function loadAutoConfig() {
    $.getJSON("/auto_config.json", function (data) {
        if (typeof data.tmin !== "undefined") $("#auto_tmin").val(data.tmin);
        if (typeof data.tmax !== "undefined") $("#auto_tmax").val(data.tmax);
    });
}

function saveAutoConfig() {
    let tmin = parseInt($("#auto_tmin").val(), 10);
    let tmax = parseInt($("#auto_tmax").val(), 10);

    $("#auto_config_status").text("Guardando...");

    $.ajax({
        url: "/set_auto_config.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({ tmin: tmin, tmax: tmax }),
        success: function () {
            $("#auto_config_status").text("Configuración guardada.");
        },
        error: function () {
            $("#auto_config_status").text("Error al guardar.");
        }
    });
}


/* ============================================================
 *            MODO PROGRAMADO — 3 REGISTROS
 *   (usa /get_program.json, /set_program.json, /erase_program.json)
 * ============================================================ */

function saveProgramSlot() {
    let id = parseInt($("#prog_id").val(), 10); // 1..3

    let obj = {
        id:      id,
        active:  $("#prog_active").is(":checked") ? 1 : 0,
        h_start: parseInt($("#prog_h_start").val(), 10),
        m_start: parseInt($("#prog_m_start").val(), 10),
        h_end:   parseInt($("#prog_h_end").val(), 10),
        m_end:   parseInt($("#prog_m_end").val(), 10),
        t0:      parseInt($("#prog_t0").val(), 10),
        t100:    parseInt($("#prog_t100").val(), 10)
    };

    $("#prog_status").text("Guardando...");

    $.ajax({
        url: "/set_program.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify(obj),
        success: function () {
            $("#prog_status").text("Registro guardado.");
        },
        error: function () {
            $("#prog_status").text("Error al guardar.");
        }
    });
}

function loadProgramSlot() {
    let id = parseInt($("#prog_id").val(), 10); // 1..3

    $("#prog_status").text("Leyendo...");

    $.ajax({
        url: "/get_program.json?id=" + id,
        method: "GET",
        dataType: "json",
        success: function (data) {
            $("#prog_active").prop("checked", data.active === 1);
            $("#prog_h_start").val(data.h_start);
            $("#prog_m_start").val(data.m_start);
            $("#prog_h_end").val(data.h_end);
            $("#prog_m_end").val(data.m_end);
            $("#prog_t0").val(data.t0);
            $("#prog_t100").val(data.t100);
            $("#prog_status").text("Registro cargado.");
        },
        error: function () {
            $("#prog_status").text("No existe el registro.");
        }
    });
}

function eraseProgramSlot() {
    let id = parseInt($("#prog_id").val(), 10); // 1..3

    $("#prog_status").text("Borrando...");

    $.ajax({
        url: "/erase_program.json",
        method: "POST",
        contentType: "application/json",
        data: JSON.stringify({ id: id }),
        success: function () {
            $("#prog_status").text("Registro borrado.");
            $("#prog_active").prop("checked", false);
        },
        error: function () {
            $("#prog_status").text("Error al borrar.");
        }
    });
}


/* ============================================================
 *        SELECTOR DE MODO (SOLO VISUAL / ESTÉTICO)
 * ============================================================ */

function showModeCards(mode) {
    currentMode = mode;

    $("#card_manual").toggleClass("hidden", mode !== 0);
    $("#card_auto").toggleClass("hidden",  mode !== 1);
    $("#card_prog").toggleClass("hidden",  mode !== 2);
}

function applyModeSelection() {
    const val = parseInt($("input[name='mode_select']:checked").val(), 10);

    if (isNaN(val)) {
        $("#mode_status").text("Selecciona un modo primero.");
        return;
    }

    showModeCards(val);

    let txt =
        val === 0 ? "Manual" :
        val === 1 ? "Automático" :
        "Programado";

    $("#mode_status").text("Mostrando controles del modo " + txt + ".");

    // Nota: el modo real del firmware se sigue cambiando
    // al guardar PWM manual, configuración automática o registros.
}
