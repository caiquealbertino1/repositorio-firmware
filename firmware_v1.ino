/*
 * S2-CP02 - Projeto Motiva | Firmware 1.0
 * Atualizacao remota de firmware (OTA)
 *
 * ESP32 DevKit + Wokwi-GUEST
 *
 * Fluxo:
 *   1. Executa 5 leituras pseudoaleatorias entre 10 e 20 cm.
 *   2. Faz uma leitura a cada 2 segundos.
 *   3. Fecha a sessao e calcula a media.
 *   4. Uma nova sessao comeca 48 segundos apos o inicio da anterior.
 *   5. Depois de 3 sessoes, consulta version.json no GitHub.
 *   6. Se houver uma versao maior, baixa firmware_v2.bin e executa OTA.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

// ---------------------------------------------------------------------
// CONFIGURACAO DO OTA
// ---------------------------------------------------------------------
const char *VERSAO = "1.0";
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";
const int WIFI_CHANNEL = 6;

const char *URL_MANIFESTO =
    "https://raw.githubusercontent.com/caiquealbertino1/repositorio-firmware/main/version.json";

// ---------------------------------------------------------------------
// LED RGB - catodo comum
// ---------------------------------------------------------------------
const int LED_VERMELHO = 25;
const int LED_VERDE = 26;
const int LED_AZUL = 27;

// ---------------------------------------------------------------------
// SESSAO
// ---------------------------------------------------------------------
const int TOTAL_LEITURAS = 5;
const unsigned long INTERVALO_LEITURA = 2000UL;
const unsigned long INTERVALO_SESSAO = 48000UL;

int leituras[TOTAL_LEITURAS];
int contadorLeituras = 0;
int sessoesConcluidas = 0;
unsigned long inicioSessao = 0;
bool verificacaoConcluida = false;

// ---------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------
void mostrarFirmware1() {
  digitalWrite(LED_VERMELHO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, HIGH);
}

// ---------------------------------------------------------------------
// SESSAO / LEITURAS
// ---------------------------------------------------------------------
void iniciarSessao() {
  inicioSessao = millis();
  contadorLeituras = 0;

  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");
}

void realizarLeitura() {
  leituras[contadorLeituras] = random(10, 21); // 10..20 inclusive

  Serial.print("Leitura ");
  Serial.print(contadorLeituras + 1);
  Serial.print(": ");
  Serial.print(leituras[contadorLeituras]);
  Serial.println(" cm");

  contadorLeituras++;
}

float calcularMedia() {
  int soma = 0;

  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    soma += leituras[i];
  }

  return soma / 5.0f;
}

void finalizarSessao() {
  Serial.print("Media da sessao: ");
  Serial.print(calcularMedia(), 1);
  Serial.println(" cm");

  Serial.println("Proxima sessao em 48 segundos.");
  Serial.println();

  sessoesConcluidas++;
}

// ---------------------------------------------------------------------
// PARSING SIMPLES DO version.json
// Espera:
// {"version":"2.0","url":"https://.../firmware_v2.bin"}
// ---------------------------------------------------------------------
bool extrairCampoJson(const String &json, const String &campo, String &valor) {
  String chave = "\"" + campo + "\"";
  int posicaoChave = json.indexOf(chave);

  if (posicaoChave < 0) {
    return false;
  }

  int posicaoDoisPontos = json.indexOf(':', posicaoChave + chave.length());
  if (posicaoDoisPontos < 0) {
    return false;
  }

  int inicioValor = json.indexOf('"', posicaoDoisPontos + 1);
  if (inicioValor < 0) {
    return false;
  }

  int fimValor = json.indexOf('"', inicioValor + 1);
  if (fimValor < 0) {
    return false;
  }

  valor = json.substring(inicioValor + 1, fimValor);
  return valor.length() > 0;
}

// ---------------------------------------------------------------------
// WIFI
// ---------------------------------------------------------------------
bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("Conectando ao Wi-Fi Wokwi-GUEST...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);

  const int maxTentativas = 20;
  for (int tentativa = 1; tentativa <= maxTentativas; tentativa++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Wi-Fi conectado.");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }

    delay(500);
  }

  Serial.println("Erro: sem conexao Wi-Fi/internet.");
  return false;
}

// ---------------------------------------------------------------------
// OTA
// ---------------------------------------------------------------------
void procurarAtualizacao() {
  Serial.println("----------------------------------------");
  Serial.println("Verificando atualizacao OTA...");

  if (!conectarWiFi()) {
    Serial.println("OTA cancelada: Wi-Fi indisponivel.");
    return;
  }

  WiFiClientSecure cliente;
  cliente.setInsecure(); // apropriado para a simulacao da CP2

  HTTPClient http;
  if (!http.begin(cliente, URL_MANIFESTO)) {
    Serial.println("Erro: nao foi possivel iniciar a requisicao do manifesto.");
    return;
  }

  int codigoHttp = http.GET();

  if (codigoHttp != HTTP_CODE_OK) {
    Serial.print("Erro: manifesto inacessivel. HTTP ");
    Serial.println(codigoHttp);
    http.end();
    return;
  }

  String manifesto = http.getString();
  http.end();

  Serial.println("Manifesto recebido com sucesso.");

  String versaoDisponivel;
  String urlFirmware;

  if (!extrairCampoJson(manifesto, "version", versaoDisponivel) ||
      !extrairCampoJson(manifesto, "url", urlFirmware)) {
    Serial.println("Erro: manifesto invalido ou incompleto.");
    return;
  }

  Serial.print("Versao instalada: ");
  Serial.println(VERSAO);
  Serial.print("Versao disponivel: ");
  Serial.println(versaoDisponivel);
  Serial.print("URL do firmware: ");
  Serial.println(urlFirmware);

  if (versaoDisponivel.toFloat() <= String(VERSAO).toFloat()) {
    Serial.println("A versao instalada ja e a mais recente.");
    verificacaoConcluida = true;
    return;
  }

  Serial.println("Nova versao encontrada!");
  Serial.println("Baixando firmware_v2.bin e iniciando OTA...");

  t_httpUpdate_return resultado = httpUpdate.update(cliente, urlFirmware);

  switch (resultado) {
    case HTTP_UPDATE_FAILED:
      Serial.print("Erro OTA: ");
      Serial.println(httpUpdate.getLastErrorString());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("OTA: servidor informou que nao ha atualizacao.");
      break;

    case HTTP_UPDATE_OK:
      // Normalmente o ESP32 reinicia automaticamente apos sucesso.
      Serial.println("OTA concluida. Reiniciando para o Firmware 2.0...");
      break;
  }
}

// ---------------------------------------------------------------------
// SETUP / LOOP
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  mostrarFirmware1();

  randomSeed(analogRead(0));

  Serial.println();
  Serial.println("Rodando o FIRMWARE 1.0");
  iniciarSessao();
}

void loop() {
  unsigned long agora = millis();

  // Leitura 1 em 0 s, leitura 2 em 2 s ... leitura 5 em 8 s.
  if (contadorLeituras < TOTAL_LEITURAS &&
      agora - inicioSessao >= (unsigned long)contadorLeituras * INTERVALO_LEITURA) {
    realizarLeitura();

    if (contadorLeituras == TOTAL_LEITURAS) {
      finalizarSessao();

      // O enunciado exige que a consulta aconteca apos pelo menos 3 ciclos.
      if (sessoesConcluidas >= 3 && !verificacaoConcluida) {
        procurarAtualizacao();
      }
    }
  }

  // Nova sessao 48 s apos o INICIO da sessao anterior.
  if (agora - inicioSessao >= INTERVALO_SESSAO) {
    iniciarSessao();
  }
}
