/*
 * =====================================================================
 *  PROJETO MOTIVA - MONITORAMENTO DE VEGETACAO
 *  FIRMWARE 1.0  (versao simplificada, sem bibliotecas externas)
 *  S2-CP02 - Atualizacao Remota de Firmware (OTA)
 *
 *  - Gera 5 leituras pseudoaleatorias (10-20 cm) por sessao
 *  - Faz 1 leitura a cada 2 segundos
 *  - Calcula a media aritmetica da sessao
 *  - Inicia uma nova sessao a cada 48 segundos, contados a partir do
 *    inicio da sessao anterior (usa millis(), sem delay longo)
 *  - Apos 3 sessoes, conecta ao Wi-Fi, baixa o version.json, compara
 *    a versao e, se houver uma mais nova, baixa o firmware_v2.bin e
 *    executa a atualizacao OTA, reiniciando na versao 2.0
 *  - LED: indica visualmente que a versao 1.0 esta em execucao (AZUL)
 *
 *  Observacao: esta versao NAO usa a biblioteca ArduinoJson. O
 *  version.json e lido "na mao" com indexOf/substring, porque o
 *  arquivo tem so dois campos simples ("version" e "url").
 * =====================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

// ---------------------------------------------------------------------
// CONFIGURACOES GERAIS - AJUSTE PARA O SEU REPOSITORIO
// ---------------------------------------------------------------------
String versao = "1.0";
String linkJson = "https://raw.githubusercontent.com/SEU_USUARIO/repositorio-firmware/main/version.json";

// LED RGB (catodo comum: HIGH = aceso)
int ledVermelho = 25;
int ledVerde    = 26;
int ledAzul     = 27;

// ---------------------------------------------------------------------
// VARIAVEIS DE ESTADO
// ---------------------------------------------------------------------
int leituras[5];
int contador   = 0;
int sessoes    = 0;
unsigned long inicio = 0;
bool atualizado = false;

// =====================================================================
// FUNCOES DE SESSAO / LEITURA
// =====================================================================
void comecarSessao() {
  inicio = millis();
  contador = 0;
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW " + versao);
  Serial.println("========================================");
}

void lerValor() {
  leituras[contador] = random(10, 21); // numero de 10 a 20 (inclusive)
  Serial.println("Leitura " + String(contador + 1) + ": " + String(leituras[contador]) + " cm");
  contador++;
}

float media() {
  int soma = 0;
  for (int i = 0; i < 5; i++) {
    soma = soma + leituras[i];
  }
  return soma / 5.0;
}

// =====================================================================
// PARSING MANUAL DO version.json (sem biblioteca externa)
//   Espera um JSON simples do tipo:
//   { "version": "2.0", "url": "https://.../firmware_v2.bin" }
// =====================================================================
String pegarValor(String texto, String nome) {
  int posicao = texto.indexOf(nome);
  int comeco  = texto.indexOf("\"", texto.indexOf(":", posicao)) + 1;
  int fim     = texto.indexOf("\"", comeco);
  return texto.substring(comeco, fim);
}

// =====================================================================
// VERIFICACAO E EXECUCAO DA ATUALIZACAO OTA
// =====================================================================
void procurarAtualizacao() {
  Serial.println("Procurando atualizacao...");

  // 1) conectar no Wi-Fi da rede do Wokwi
  WiFi.begin("Wokwi-GUEST", "", 6);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    tentativas++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Erro: sem internet");
    return; // tenta de novo na proxima vez que entrar aqui
  }
  Serial.println("Wi-Fi conectado");

  // 2) baixar o version.json
  WiFiClientSecure cliente;
  cliente.setInsecure(); // simulacao: ignora validacao do certificado
  HTTPClient http;
  http.begin(cliente, linkJson);
  int codigoHttp = http.GET();
  if (codigoHttp != 200) {
    Serial.println("Erro: nao consegui abrir o version.json (HTTP " + String(codigoHttp) + ")");
    http.end();
    return;
  }
  String texto = http.getString();
  http.end();

  // 3) comparar as versoes
  String versaoNova = pegarValor(texto, "version");
  String linkBin     = pegarValor(texto, "url");
  Serial.println("Versao atual: " + versao + " | Versao no GitHub: " + versaoNova);

  if (versaoNova.length() == 0 || linkBin.length() == 0) {
    Serial.println("Erro: nao consegui ler o conteudo do version.json");
    return;
  }

  if (versaoNova.toFloat() <= versao.toFloat()) {
    Serial.println("Ja esta na versao mais nova");
    atualizado = true;
    return;
  }

  // 4) baixar o .bin e gravar (OTA)
  Serial.println("Versao nova encontrada! Baixando...");
  t_httpUpdate_return resultado = httpUpdate.update(cliente, linkBin);

  if (resultado != HTTP_UPDATE_OK) {
    Serial.println("Erro ao baixar ou atualizar: " + httpUpdate.getLastErrorString());
  }
  // se der certo, o ESP32 reinicia sozinho ja no Firmware 2.0
}

// =====================================================================
// SETUP / LOOP
// =====================================================================
void setup() {
  Serial.begin(115200);
  pinMode(ledVermelho, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAzul, OUTPUT);
  digitalWrite(ledAzul, HIGH); // azul = versao 1.0

  randomSeed(analogRead(0));

  Serial.println("Rodando o FIRMWARE " + versao);
  comecarSessao();
}

void loop() {
  // uma leitura a cada 2 segundos, contadas a partir do inicio da sessao
  if (contador < 5 && millis() - inicio >= (unsigned long)contador * 2000) {
    lerValor();

    if (contador == 5) {
      Serial.println("Media da sessao: " + String(media(), 1) + " cm");
      Serial.println("Proxima sessao em 48 segundos.");
      sessoes++;

      if (sessoes >= 3 && !atualizado) {
        procurarAtualizacao();
      }
    }
  }

  // nova sessao exatamente 48 segundos depois do inicio da anterior
  if (millis() - inicio >= 48000) {
    comecarSessao();
  }
}
