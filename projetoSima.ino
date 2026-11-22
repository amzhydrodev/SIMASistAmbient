#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// =====================================================
// 1. CONFIGURACOES DO USUARIO
// =====================================================

const char* ssid = "PPGEEL";
const char* password = "Ppgeel.2k26!!";

const char* mqtt_server = "10.228.197.41";
const int mqtt_port = 1883;
const char* mqtt_user = "sima_hardware";
const char* mqtt_pass = "simaprojetc01";
const char* mqtt_topic = "simahugo/SIMA/proj";

// =====================================================
// 2. PINOS E SENSORES
// =====================================================

#define DHTPIN 4
#define DHTTYPE DHT11

#define BUZZER_PIN 25

const int ldrPin = 34;
const int mq135Pin = 35;

// 0 = buzzer desativado
// 1 = buzzer ativado
#define BUZZER_ATIVO 0

// =====================================================
// 3. LIMITES DO SISTEMA
// =====================================================

#define TEMP_MAX 35
#define TEMP_MIN 18
#define UMIDADE_MIN 30
#define GAS_MAX 2000
#define LUZ_MIN 20
#define LUZ_MAX 90

// =====================================================
// 4. OBJETOS E VARIAVEIS GLOBAIS
// =====================================================

DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);

// Estrutura dos dados compartilhados
struct DadosSIMA {
  float temperatura;
  float umidade;
  int luminosidade;
  int gas;
  bool alerta;
  bool leituraValida;
};

DadosSIMA dados = {
  NAN, NAN, 0, 0, false, false
};

// Mutex para proteger os dados
SemaphoreHandle_t mutexDados = NULL;

// =====================================================
// 5. CONEXAO WIFI
// =====================================================

void setup_wifi() {

  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    vTaskDelay(pdMS_TO_TICKS(500));

    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi conectado!");

  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// 6. FUNCAO PARA COPIAR DADOS COM SEGURANCA
// =====================================================

bool obterDados(DadosSIMA &copia) {

  if (xSemaphoreTake(mutexDados, pdMS_TO_TICKS(100))
      == pdTRUE) {

    copia = dados;

    xSemaphoreGive(mutexDados);

    return true;
  }

  return false;
}

// =====================================================
// 7. TAREFA 1 - LEITURA DOS SENSORES
// =====================================================

void tarefaSensores(void *parameter) {

  TickType_t instanteAnterior = xTaskGetTickCount();

  while (true) {

    // Leitura do LDR
    int ldrRaw = analogRead(ldrPin);

    int luminosidade =
      map(ldrRaw, 0, 4095, 100, 0);

    // Leitura do MQ-135
    int gas = analogRead(mq135Pin);

    // Leitura do DHT11
    float umidade = dht.readHumidity();

    float temperatura = dht.readTemperature();

    if (isnan(umidade) || isnan(temperatura)) {

      Serial.println("[SENSORES] Falha na leitura do DHT11!");

    } else {

      bool alerta = false;

      // Verificacao da temperatura
      if (temperatura > TEMP_MAX ||
          temperatura < TEMP_MIN) {

        alerta = true;
      }

      // Verificacao da umidade
      if (umidade < UMIDADE_MIN) {

        alerta = true;
      }

      // Verificacao do gas
      if (gas > GAS_MAX) {

        alerta = true;
      }

      // Verificacao da luminosidade
      if (luminosidade < LUZ_MIN ||
          luminosidade > LUZ_MAX) {

        alerta = true;
      }

      // Atualizacao protegida pelo mutex
      if (xSemaphoreTake(mutexDados, portMAX_DELAY)
          == pdTRUE) {

        dados.temperatura = temperatura;
        dados.umidade = umidade;
        dados.luminosidade = luminosidade;
        dados.gas = gas;
        dados.alerta = alerta;
        dados.leituraValida = true;

        xSemaphoreGive(mutexDados);
      }

      Serial.println("[SENSORES] Leituras atualizadas.");

    }

    // Intervalo de leitura: 5 segundos
    vTaskDelayUntil(
      &instanteAnterior,
      pdMS_TO_TICKS(5000)
    );

  }
}

// =====================================================
// 8. TAREFA 2 - COMUNICACAO MQTT
// =====================================================

void tarefaMQTT(void *parameter) {

  client.setServer(mqtt_server, mqtt_port);

  TickType_t ultimaPublicacao = xTaskGetTickCount();

  TickType_t ultimaTentativa = 0;

  while (true) {

    // Verifica conexao Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {

      Serial.println("[MQTT] Wi-Fi desconectado!");

      WiFi.reconnect();

      vTaskDelay(pdMS_TO_TICKS(2000));

      continue;
    }

    // Reconexao MQTT
    if (!client.connected()) {

      TickType_t agora = xTaskGetTickCount();

      if ((agora - ultimaTentativa) >=
          pdMS_TO_TICKS(3000)) {

        ultimaTentativa = agora;

        Serial.println("[MQTT] Conectando ao broker...");

        if (client.connect(
              "ESP32_SIMA",
              mqtt_user,
              mqtt_pass)) {

          Serial.println("[MQTT] Conectado!");

        } else {

          Serial.print("[MQTT] Falha. Estado: ");

          Serial.println(client.state());

        }
      }

      vTaskDelay(pdMS_TO_TICKS(100));

      continue;
    }

    // Mantem a conexao MQTT ativa
    client.loop();

    // Copia os dados protegidos pelo mutex
    DadosSIMA copia;

    if (obterDados(copia) && copia.leituraValida) {

      TickType_t agora = xTaskGetTickCount();

      // Publicacao a cada 5 segundos
      if ((agora - ultimaPublicacao) >=
          pdMS_TO_TICKS(5000)) {

        ultimaPublicacao = agora;

        // Montagem do JSON
        String payload = "{";

        payload += "\"temperatura\":";
        payload += String(copia.temperatura, 1);

        payload += ",\"umidade\":";
        payload += String(copia.umidade, 1);

        payload += ",\"luminosidade\":";
        payload += String(copia.luminosidade);

        payload += ",\"gas\":";
        payload += String(copia.gas);

        payload += ",\"alerta\":";
        payload += (copia.alerta ? "true" : "false");

        payload += "}";

        // Envio MQTT
        if (client.publish(
              mqtt_topic,
              payload.c_str())) {

          Serial.println("[MQTT] Dados enviados:");

          Serial.println(payload);

        } else {

          Serial.println("[MQTT] Erro ao publicar!");

        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));

  }
}

// =====================================================
// 9. TAREFA 3 - CONTROLE DO ALARME
// =====================================================

void tarefaAlarme(void *parameter) {

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  bool alertaAnterior = false;

  while (true) {

    DadosSIMA copia;

    if (obterDados(copia) && copia.leituraValida) {

      if (copia.alerta != alertaAnterior) {

        if (copia.alerta) {

          Serial.println(
            "[ALARME] PARAMETROS FORA DOS LIMITES!"
          );

        } else {

          Serial.println(
            "[ALARME] Parametros normalizados."
          );

        }

        alertaAnterior = copia.alerta;
      }

#if BUZZER_ATIVO

      if (copia.alerta) {

        tone(BUZZER_PIN, 1000);

      } else {

        noTone(BUZZER_PIN);

      }

#else

      digitalWrite(BUZZER_PIN, LOW);

#endif

    }

    // Verificacao a cada 200 ms
    vTaskDelay(pdMS_TO_TICKS(200));

  }
}

// =====================================================
// 10. SETUP - INICIALIZACAO DO SISTEMA
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(500);

  // Configuracao dos pinos
  pinMode(ldrPin, INPUT);
  pinMode(mq135Pin, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // Inicializacao do DHT11
  dht.begin();

  // Inicializacao do Wi-Fi
  setup_wifi();

  // Criacao do mutex
  mutexDados = xSemaphoreCreateMutex();

  if (mutexDados == NULL) {

    Serial.println("Erro ao criar mutex!");

    while (true) {
      delay(1000);
    }
  }

  Serial.println();
  Serial.println("Iniciando tarefas FreeRTOS...");

  // ===========================================
  // TAREFA 1 - SENSORES
  // Nucleo 1
  // ===========================================

  BaseType_t tarefa1 = xTaskCreatePinnedToCore(

    tarefaSensores,

    "TarefaSensores",

    4096,

    NULL,

    2,

    NULL,

    1
  );

  // ===========================================
  // TAREFA 2 - MQTT
  // Nucleo 0
  // ===========================================

  BaseType_t tarefa2 = xTaskCreatePinnedToCore(

    tarefaMQTT,

    "TarefaMQTT",

    6144,

    NULL,

    2,

    NULL,

    0
  );

  // ===========================================
  // TAREFA 3 - ALARME
  // Nucleo 1
  // ===========================================

  BaseType_t tarefa3 = xTaskCreatePinnedToCore(

    tarefaAlarme,

    "TarefaAlarme",

    2048,

    NULL,

    1,

    NULL,

    1
  );

  // Verificacao da criacao das tarefas
  if (tarefa1 == pdPASS &&
      tarefa2 == pdPASS &&
      tarefa3 == pdPASS) {

    Serial.println(
      "Todas as tarefas FreeRTOS iniciadas!"
    );

  } else {

    Serial.println(
      "Erro ao criar uma ou mais tarefas!"
    );

  }

}

// =====================================================
// 11. LOOP PRINCIPAL
// =====================================================

void loop() {

  // As tarefas FreeRTOS executam o trabalho principal.
  vTaskDelay(pdMS_TO_TICKS(1000));

}
