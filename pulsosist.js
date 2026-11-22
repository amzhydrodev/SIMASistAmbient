const srvStatus = document.getElementById('srvStatus');
let chartTempHum, chartLux, chartAirBar;
let dadosGlobaisServidor = null;

// ==========================================
// UX DOS CARDS: SISTEMA DE PULSO (GLOW)
// ==========================================
function animarCard(idCard, classeAlerta = "updated") {
    const card = document.getElementById(idCard);
    if (card) {
        card.classList.remove("updated", "red-alert");
        void card.offsetWidth; // Força reinício da animação CSS
        card.classList.add(classeAlerta);
    }
}

// ==========================================
// 1. CONEXÃO MQTT (TEMPO REAL)
// ==========================================
const client = mqtt.connect('wss://mqtt.meusprojetos.app.br', {
  username: 'sima_hardware',
  password: 'simaprojetc01'
});

client.on('connect', () => {
  srvStatus.textContent = "● Online";
  srvStatus.className = "badge online";
  console.log("Conectado ao MQTT com sucesso!");
  client.subscribe('simahugo/SIMA/proj');
});

client.on('offline', () => {
  srvStatus.textContent = "● Offline";
  srvStatus.className = "badge offline";
  console.log("MQTT Offline.");
});

client.on('error', (err) => {
  console.error("Erro na conexão MQTT:", err);
});

client.on('message', (topic, message) => {
  try {
    const dados = JSON.parse(message.toString());
    console.log("Mensagem recebida:", dados);
    updateTemperature(parseFloat(dados.temperatura));
    updateHumidity(parseFloat(dados.umidade));
    updateLux(parseFloat(dados.luminosidade));
    updateAir(parseFloat(dados.gas));
  } catch (erro) {
    console.error("Erro no pacote MQTT (JSON inválido):", erro);
  }
});

function updateTemperature(temp) {
  const el = document.getElementById("tempValue");
  const alert = document.getElementById("tempAlert");
  el.textContent = temp.toFixed(1) + " °C";
  if (temp <= 22) { el.className = "qualitative text-blue"; alert.textContent = "❄️ Frio"; animarCard("cardTemp"); }
  else if (temp <= 29) { el.className = "qualitative text-green"; alert.textContent = "✅ Agradável"; animarCard("cardTemp"); }
  else { el.className = "qualitative text-red"; alert.textContent = "⚠️ Alta"; animarCard("cardTemp", "red-alert"); }
}

function updateHumidity(hum) {
  const el = document.getElementById("humValue");
  const alert = document.getElementById("humAlert");
  el.textContent = hum.toFixed(0) + " %";
  if (hum < 40) { el.className = "qualitative text-yellow"; alert.textContent = "⚠️ Baixa"; animarCard("cardHum"); }
  else if (hum <= 65) { el.className = "qualitative text-green"; alert.textContent = "✅ Ideal"; animarCard("cardHum"); }
  else { el.className = "qualitative text-blue"; alert.textContent = "🌧️ Elevada"; animarCard("cardHum"); }
}

function updateLux(lux) {
  const el = document.getElementById("luxValue");
  const alert = document.getElementById("luxAlert");
  if (lux < 200) { el.textContent = "Baixa"; el.className = "qualitative text-blue"; alert.textContent = `🌙 Escuro (${lux.toFixed(0)} lx)`; }
  else if (lux <= 1500) { el.textContent = "Média"; el.className = "qualitative text-green"; alert.textContent = `💡 Ideal (${lux.toFixed(0)} lx)`; }
  else { el.textContent = "Alta"; el.className = "qualitative text-yellow"; alert.textContent = `☀️ Claro (${lux.toFixed(0)} lx)`; }
  animarCard("cardLux");
}

function updateAir(air) {
  const el = document.getElementById("airValue");
  const alert = document.getElementById("airAlert");
  if (air <= 50) { el.textContent = "Boa"; el.className = "qualitative text-green"; alert.textContent = `🍃 Limpo (${air.toFixed(0)} ppm)`; animarCard("cardAir"); }
  else if (air <= 300) { el.textContent = "Atenção"; el.className = "qualitative text-yellow"; alert.textContent = `⚠️ Alterado (${air.toFixed(0)} ppm)`; animarCard("cardAir"); }
  else { el.textContent = "Ruim"; el.className = "qualitative text-red"; alert.textContent = `🚨 Gás/Fuma! (${air.toFixed(0)} ppm)`; animarCard("cardAir", "red-alert"); }
}

// ==========================================
// 2. LOGICA DOS GRAFICOS INTELIGENTES
// ==========================================
function inicializarGraficos(initData) {
    if (!initData || initData.temp.length === 0) return; // Trava de segurança

    Chart.defaults.color = '#94a3b8';
    Chart.defaults.font.family = "'Segoe UI', Tahoma, sans-serif";

    // 1. Temp e Umid
    chartTempHum = new Chart(document.getElementById('chartTempHum').getContext('2d'), {
        type: 'line',
        data: {
            labels: initData.labels,
            datasets: [
                { label: 'Temp (°C)', data: initData.temp, borderColor: '#10b981', borderWidth: 3, tension: 0.4, pointBackgroundColor: '#10b981', fill: false },
                { label: 'Umid (%)', data: initData.hum, borderColor: '#3b82f6', borderWidth: 3, tension: 0.4, pointBackgroundColor: '#3b82f6', fill: false }
            ]
        },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { position: 'bottom' } }, scales: { y: { grid: { color: 'rgba(71, 85, 105, 0.12)' } }, x: { grid: { display: false } } } }
    });

    // 2. Luz
    const ctxLux = document.getElementById('chartLux').getContext('2d');
    const gradientLux = ctxLux.createLinearGradient(0, 0, 0, 250);
    gradientLux.addColorStop(0, 'rgba(245, 158, 11, 0.38)');
    gradientLux.addColorStop(1, 'rgba(245, 158, 11, 0)');

    chartLux = new Chart(ctxLux, {
        type: 'line',
        data: {
            labels: initData.labels,
            datasets: [{ label: 'Luminosidade (lux)', data: initData.lux, borderColor: '#f59e0b', borderWidth: 3, tension: 0.4, fill: 'start', backgroundColor: gradientLux, pointRadius: 4, pointBackgroundColor: '#0f172a', pointBorderColor: '#f59e0b', pointBorderWidth: 2 }]
        },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { y: { grid: { color: 'rgba(71, 85, 105, 0.12)' } }, x: { grid: { display: false } } } }
    });

    // 3. Ar
    chartAirBar = new Chart(document.getElementById('chartAirBar').getContext('2d'), {
        type: 'bar',
        data: {
            labels: initData.labels,
            datasets: [{ label: 'Gás Máximo (ppm)', data: initData.gas, backgroundColor: 'rgba(239, 68, 68, 0.6)', borderColor: '#ef4444', borderWidth: 1, borderRadius: 6 }]
        },
        options: { responsive: true, maintainAspectRatio: false, plugins: { legend: { display: false } }, scales: { y: { grid: { color: 'rgba(71, 85, 105, 0.12)' } }, x: { grid: { display: false } } } }
    });
}

function mudarPeriodo(escala) {
    if (!dadosGlobaisServidor || !dadosGlobaisServidor.payloads[escala]) return;

    document.querySelectorAll('.btn-time').forEach(b => b.classList.remove('active'));
    document.getElementById(`btn-${escala}`).classList.add('active');

    const novosDados = dadosGlobaisServidor.payloads[escala];

    chartTempHum.data.labels = novosDados.labels;
    chartTempHum.data.datasets[0].data = novosDados.temp;
    chartTempHum.data.datasets[1].data = novosDados.hum;
    chartTempHum.update();

    chartLux.data.labels = novosDados.labels;
    chartLux.data.datasets[0].data = novosDados.lux;
    chartLux.update();

    chartAirBar.data.labels = novosDados.labels;
    chartAirBar.data.datasets[0].data = novosDados.gas;
    chartAirBar.update();
}

async function carregarDadosDoServidor() {
    try {
        const resposta = await fetch('./dados_graficos.json?nocache=' + new Date().getTime());
        if (!resposta.ok) throw new Error("JSON inexistente");

        dadosGlobaisServidor = await resposta.json();

        if (dadosGlobaisServidor && dadosGlobaisServidor.botoesHabilitados) {
            dadosGlobaisServidor.botoesHabilitados.forEach(escala => {
                const btn = document.getElementById(`btn-${escala}`);
                if (btn) {
                    btn.removeAttribute('disabled');
                    btn.classList.remove('disabled');
                }
            });

            const escalaInicial = dadosGlobaisServidor.botoesHabilitados[0];
            const btnInicial = document.getElementById(`btn-${escalaInicial}`);
            if (btnInicial) btnInicial.classList.add('active');

            inicializarGraficos(dadosGlobaisServidor.payloads[escalaInicial]);

            // Rede de segurança preenchendo os cards
            const ultimosDados = dadosGlobaisServidor.payloads[escalaInicial];
            if (ultimosDados && ultimosDados.temp.length > 0) {
                const ultimoIndice = ultimosDados.temp.length - 1;
                if (document.getElementById("tempValue").textContent === "--") {
                    updateTemperature(parseFloat(ultimosDados.temp[ultimoIndice]));
                    updateHumidity(parseFloat(ultimosDados.hum[ultimoIndice]));
                    updateLux(parseFloat(ultimosDados.lux[ultimoIndice]));
                    updateAir(parseFloat(ultimosDados.gas[ultimoIndice]));
                }
            }
        }
    } catch (err) {
        console.log("Aguardando JSON do servidor...");
        setTimeout(carregarDadosDoServidor, 5000); // Tenta novamente em 5 segundos
    }
}

document.addEventListener("DOMContentLoaded", carregarDadosDoServidor);

function baixarPlanilha() {
  window.location.href = './dados_atuais.csv?nocache=' + new Date().getTime();
}
