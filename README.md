# SIMA — Sistema Inteligente de Monitoramento Ambiental

Sistema de monitoramento ambiental desenvolvido com ESP32 e Orange Pi, destinado à aquisição, transmissão, processamento, armazenamento e visualização de variáveis ambientais em tempo real.

O SIMA utiliza sensores físicos conectados a uma placa eletrônica para monitorar temperatura, umidade, luminosidade e qualidade do ar. Os dados são transmitidos por uma rede Wi-Fi utilizando o protocolo MQTT até um servidor local baseado em Orange Pi, responsável por gerenciar a comunicação, armazenar os registros e disponibilizar uma dashboard acessível pela internet.

O projeto foi desenvolvido com uma arquitetura distribuída, dividida em duas camadas principais: aquisição de dados (ESP32) e processamento e disponibilização (Orange Pi).

---

# 1. Visão geral da arquitetura

O funcionamento do SIMA é baseado na comunicação entre dispositivos embarcados e serviços executados em um servidor Linux.

A arquitetura pode ser compreendida a partir de duas camadas principais.

**Camada de aquisição — ESP32:** responsável pela leitura dos sensores, processamento inicial das informações, verificação dos limites ambientais, acionamento do alarme sonoro e transmissão dos dados por Wi-Fi utilizando o protocolo MQTT.

**Camada de processamento — Orange Pi:** responsável por receber e distribuir as mensagens MQTT, executar o backend de armazenamento, manter os registros históricos e disponibilizar a dashboard para visualização das informações.

O funcionamento integrado acontece da seguinte maneira: os sensores realizam as medições ambientais e enviam os sinais ao ESP32. O microcontrolador processa essas informações e publica os resultados no broker MQTT, hospedado no Orange Pi. O broker recebe as mensagens e as encaminha aos serviços inscritos nos respectivos tópicos. O backend Python registra os dados recebidos, enquanto a dashboard apresenta as informações em tempo real e permite consultar os registros históricos.

**Importante:** o Mosquitto é o broker responsável por receber e encaminhar as mensagens MQTT aos clientes inscritos nos respectivos tópicos. O armazenamento histórico é realizado pelo backend Python, enquanto a dashboard apresenta os dados ao usuário.

O Orange Pi concentra esses serviços, mas cada um possui uma responsabilidade independente.

# 2. Camada de aquisição — ESP32

## 2.1. Hardware utilizado

A primeira camada do projeto é formada pela placa eletrônica (PCB), responsável por realizar a leitura das condições ambientais.

O componente central dessa camada é o ESP32 DevKit V1, que possui conectividade Wi-Fi integrada e capacidade de processamento suficiente para executar tarefas simultâneas utilizando o FreeRTOS.

Os sensores utilizados são:

- **DHT11:** responsável pela medição da temperatura e da umidade do ambiente, conectado ao GPIO 4.
- **LDR:** responsável pela medição da luminosidade, conectado ao GPIO 34.
- **MQ-135:** utilizado para detectar a presença de determinados gases e fornecer uma indicação relacionada à qualidade do ar, conectado ao GPIO 35.
- **Buzzer:** responsável pela emissão de alertas sonoros, conectado ao GPIO 25.

O DHT11 fornece informações digitais de temperatura e umidade. Já o LDR e o MQ-135 fornecem sinais analógicos, que são convertidos pelo conversor analógico-digital (ADC) do ESP32.

## 2.2. Processamento utilizando FreeRTOS

O firmware do ESP32 utiliza o FreeRTOS para dividir as atividades em tarefas independentes.

Essa organização evita que a comunicação Wi-Fi e MQTT prejudique a aquisição dos dados ou o funcionamento do alarme local.

As tarefas são distribuídas entre os dois núcleos do ESP32.

**Núcleo 1 — Sensores e alarme**

- Realiza a leitura dos sensores a cada 5 segundos.
- Processa os valores adquiridos.
- Verifica se as medições estão dentro dos limites estabelecidos.
- Atualiza a estrutura compartilhada de dados.
- Verifica as condições de acionamento do buzzer a cada 200 ms.

**Núcleo 0 — Comunicação**

- Gerencia a conexão Wi-Fi.
- Estabelece a comunicação com o broker MQTT.
- Realiza a autenticação no servidor.
- Publica as informações coletadas.
- Tenta restabelecer a conexão quando ocorre uma falha de comunicação.

Para evitar conflitos entre as tarefas durante o acesso às informações compartilhadas, o firmware utiliza um Mutex (mecanismo de exclusão mútua).

Dessa maneira, uma tarefa não modifica os dados enquanto outra está realizando sua leitura de forma protegida.

## 2.3. Limites de monitoramento

O firmware também realiza verificações locais para identificar condições ambientais fora dos parâmetros estabelecidos.

Os limites configurados são:

- Temperatura mínima de 18 °C e máxima de 35 °C.
- Umidade mínima de 30%.
- Gases com limite máximo de 2000.
- Luminosidade com faixa de referência entre 20% e 90%.

Quando uma condição ultrapassa os limites definidos, o sistema pode acionar o buzzer para sinalizar uma situação que necessita de atenção.

Essas verificações acontecem diretamente no ESP32. Portanto, o funcionamento do alarme local não depende da disponibilidade da dashboard ou da conexão com a internet.

## 2.4. Transmissão dos dados

Depois de realizar a leitura e o processamento, o ESP32 organiza as informações em uma mensagem no formato JSON.

Essa mensagem contém os campos temperatura, umidade, luminosidade e gas, acompanhados de seus respectivos valores numéricos.

Por exemplo, uma medição pode apresentar temperatura de 28,5 °C, umidade de 65%, luminosidade de 72 e leitura de gases de 850.

O dispositivo publica essa mensagem por meio do protocolo MQTT, utilizando a rede Wi-Fi e as credenciais de autenticação configuradas no firmware.

A publicação é direcionada a um tópico específico, configurado tanto no ESP32 quanto nos serviços que recebem os dados.

O MQTT foi escolhido por ser um protocolo leve, adequado à comunicação entre dispositivos IoT e servidores com recursos computacionais limitados.

# 3. Camada de processamento — Orange Pi

## 3.1. O papel do Orange Pi

O Orange Pi funciona como o servidor central do SIMA.

Diferentemente do ESP32, que é especializado na interação direta com os sensores, o Orange Pi possui recursos computacionais para executar diversos serviços simultaneamente.

O equipamento utiliza o sistema operacional DietPi, uma distribuição Linux minimalista baseada em Debian.

Sua responsabilidade é manter os serviços do projeto funcionando continuamente, incluindo:

- Gerenciamento do broker MQTT.
- Recebimento e encaminhamento das mensagens.
- Execução do backend responsável pelo histórico.
- Hospedagem dos arquivos da dashboard.
- Disponibilização do acesso externo.
- Organização e armazenamento dos registros ambientais.

A proposta é manter o servidor em funcionamento contínuo, permitindo que os dados sejam recebidos mesmo quando nenhum usuário estiver acessando a dashboard.

## 3.2. Broker MQTT — Mosquitto

O Mosquitto é o serviço responsável por gerenciar a comunicação MQTT dentro do servidor.

Ele atua como intermediário entre os dispositivos que publicam informações e as aplicações que precisam recebê-las.

O funcionamento acontece da seguinte maneira:

1. O ESP32 estabelece uma conexão com o Mosquitto.
2. O dispositivo realiza a autenticação utilizando usuário e senha.
3. O ESP32 publica uma mensagem no tópico configurado.
4. O Mosquitto recebe a mensagem.
5. O broker identifica os clientes inscritos naquele tópico.
6. A mensagem é encaminhada aos clientes autorizados.

O broker não precisa conhecer o funcionamento interno dos sensores nem interpretar os valores ambientais. Sua principal responsabilidade é receber, controlar e distribuir as mensagens.

### Organização por tópicos MQTT

O protocolo MQTT utiliza tópicos para organizar as mensagens.

Um tópico funciona como um endereço lógico que identifica determinado fluxo de informações.

No SIMA, o tópico utilizado pelo projeto é `simahugo/SIMA/proj`.

Nesse caso, o ESP32 publica as medições nesse tópico, enquanto o backend Python e a aplicação de visualização podem se inscrever nele para receber os dados.

É possível criar outros tópicos para separar diferentes dispositivos, ambientes ou categorias de informações, desde que essa organização seja implementada também nos respectivos códigos.

### Segurança e controle de acesso

O Mosquitto utiliza autenticação e regras de controle de acesso (ACL).

Essas regras permitem determinar quais clientes podem publicar mensagens e quais podem apenas recebê-las.

Uma organização possível é:

- **ESP32:** publicar dados no tópico autorizado.
- **Backend Python:** assinar o tópico para registrar os dados.
- **Dashboard:** assinar o tópico para receber as medições.

Essa separação reduz o risco de que clientes não autorizados publiquem informações ou acessem tópicos indevidos.

# 4. Backend Python — armazenamento dos dados

O MQTT é responsável pela comunicação em tempo real, mas não funciona, por padrão, como um banco de dados histórico das medições.

Por esse motivo, o SIMA utiliza um serviço desenvolvido em Python para registrar as informações recebidas.

Esse serviço funciona como um cliente MQTT independente.

Seu funcionamento ocorre da seguinte maneira:

1. O backend inicia no Orange Pi.
2. Estabelece uma conexão com o Mosquitto utilizando a rede local (`localhost`).
3. Realiza a autenticação necessária.
4. Inscreve-se no tópico de monitoramento.
5. Aguarda as mensagens publicadas pelo ESP32.
6. Recebe os dados em JSON.
7. Extrai os valores das variáveis ambientais.
8. Registra as medições em arquivos CSV, incluindo data e horário.

O armazenamento em CSV permite manter um histórico das condições ambientais e consultar informações anteriores sem depender de que a dashboard esteja aberta.

Cada registro contém a data, o horário, a temperatura, a umidade, a luminosidade e a leitura de gases.

Por exemplo, uma medição registrada pode apresentar a data 22/11/2026, horário 17:00:00, temperatura de 28,5 °C, umidade de 65%, luminosidade de 72 e leitura de gases de 850.

Os arquivos podem ser utilizados posteriormente para gerar gráficos, analisar variações ambientais e exportar os dados para outras ferramentas.

# 5. Dashboard — interface de visualização

A dashboard é a interface responsável por apresentar as informações coletadas pelo sistema.

Ela é hospedada no Orange Pi e pode ser acessada por meio de um navegador, utilizando o domínio configurado para o projeto.

Sua estrutura é dividida em três arquivos principais:

**index.html:** define a estrutura e organização dos elementos da interface.

**style.css:** controla a aparência visual, incluindo cores, espaçamentos, dimensões e responsividade.

**script.js:** contém a lógica de comunicação MQTT, atualização dos dados, gráficos e demais funcionalidades.

## 5.1. HTML — estrutura da aplicação

O arquivo `index.html` define a estrutura da dashboard.

É nele que são organizados os elementos que aparecem na tela, como:

- Título e identificação do SIMA.
- Indicador de conexão.
- Campos de temperatura e umidade.
- Indicadores de luminosidade e gases.
- Áreas destinadas aos gráficos.
- Seções de consulta histórica.
- Botões e opções de exportação.

O HTML fornece a estrutura que será preenchida e atualizada pelos demais arquivos.

## 5.2. CSS — apresentação visual

O arquivo `style.css` controla a aparência da aplicação.

Ele define o tema visual, a organização dos componentes, as cores, os tamanhos, os espaçamentos e a adaptação da interface a diferentes resoluções de tela.

A separação do CSS permite modificar o visual da dashboard sem precisar alterar a lógica de comunicação ou processamento dos dados.

## 5.3. JavaScript — funcionamento e comunicação

O arquivo `script.js` é responsável pela lógica de funcionamento da dashboard.

Entre suas principais responsabilidades estão:

- Estabelecer a conexão MQTT por WebSocket.
- Realizar a autenticação no broker.
- Inscrever-se no tópico de monitoramento.
- Receber as mensagens publicadas pelo ESP32.
- Interpretar os dados JSON.
- Atualizar os indicadores da interface.
- Atualizar os gráficos em tempo real.
- Controlar o estado da conexão.
- Disponibilizar funcionalidades de consulta e exportação, conforme implementadas.

A comunicação entre o navegador e o broker utiliza MQTT sobre WebSocket, permitindo que a dashboard receba as informações diretamente pelo navegador.

É importante diferenciar as duas formas de comunicação:

- **ESP32 → Orange Pi:** MQTT pela conexão de rede do dispositivo.
- **Navegador → Orange Pi:** MQTT sobre WebSocket, utilizando o serviço WebSocket disponibilizado pelo broker.

Dessa forma, o navegador não precisa acessar diretamente os pinos ou sensores do ESP32. Ele recebe as informações já transmitidas pelo dispositivo e encaminhadas pelo broker.

# 6. Visualização em tempo real e histórico

O SIMA possui duas funcionalidades principais de visualização.

## 6.1. Modo Ao Vivo

O modo Ao Vivo permite acompanhar as condições ambientais enquanto o ESP32 realiza novas medições.

Sempre que uma mensagem é recebida pelo JavaScript, os valores apresentados na interface são atualizados.

O usuário consegue acompanhar:

- Temperatura atual.
- Umidade relativa do ar.
- Luminosidade.
- Nível de gases indicado pelo MQ-135.
- Estado da conexão com o servidor.
- Evolução das medições nos gráficos.

A atualização ocorre por meio das mensagens recebidas pelo MQTT, sem necessidade de atualizar manualmente a página.

## 6.2. Modo Cientista — histórico e análise

O modo Cientista permite consultar os registros armazenados pelo backend Python.

Enquanto o modo Ao Vivo utiliza as mensagens recebidas em tempo real, o modo histórico utiliza os dados registrados nos arquivos CSV.

A dashboard pode disponibilizar consultas por períodos, como:

- Últimas 24 horas.
- Última semana.
- Último mês.

Também permite exportar os registros para uma planilha CSV, facilitando análises externas e a utilização dos dados em relatórios técnicos ou científicos.

**Diferença fundamental:** o MQTT transporta os dados, o backend Python registra o histórico e a dashboard apresenta as informações.

# 7. Acesso externo — Cloudflare e domínio

Para permitir o acesso à dashboard fora da rede local, o projeto utiliza um domínio próprio integrado ao Cloudflare.

Essa estrutura permite disponibilizar a aplicação hospedada no Orange Pi sem precisar expor diretamente todos os serviços internos do servidor à internet.

O Cloudflare funciona como intermediário no acesso externo, encaminhando as solicitações para os serviços configurados no Orange Pi.

A comunicação com a dashboard utiliza HTTPS, enquanto a conexão MQTT do navegador utiliza WebSocket seguro (WSS).

No projeto, o endereço da dashboard é `https://slides.meusprojetos.app.br/`, enquanto o endereço de conexão WebSocket é `wss://mqtt.meusprojetos.app.br`.

Os endereços acima são exemplos da configuração do projeto. Ao reproduzir o SIMA, o usuário deverá utilizar seu próprio domínio e configurar os respectivos serviços.

# 8. Como reproduzir o projeto

Esta seção apresenta a sequência geral para quem deseja montar uma instalação própria do SIMA.

## 8.1. Materiais necessários

**Hardware:**

- 1 ESP32 DevKit V1.
- 1 Orange Pi compatível com DietPi.
- 1 sensor DHT11.
- 1 sensor MQ-135.
- 1 sensor LDR.
- 1 buzzer.
- Resistores e componentes necessários para os circuitos.
- Placa PCB ou protoboard para montagem.
- Sistema de alimentação adequado.
- Bateria recarregável e circuito de carregamento, conforme o projeto eletrônico.

**Software:**

- Arduino IDE.
- Bibliotecas `WiFi.h`, `PubSubClient.h` e `DHT.h`.
- DietPi instalado no Orange Pi.
- Mosquitto MQTT Broker.
- Python 3.
- Bibliotecas Python necessárias para o backend.
- Navegador compatível com WebSocket.
- Cloudflare e domínio próprio, caso deseje acesso externo.

## 8.2. Preparação do ESP32

1. Instale a Arduino IDE.
2. Instale o suporte à placa ESP32.
3. Instale as bibliotecas utilizadas pelo firmware.
4. Abra o código do ESP32.
5. Configure o nome e a senha da rede Wi-Fi.
6. Configure o endereço IP do broker MQTT.
7. Configure a porta, o usuário e a senha MQTT.
8. Confira o tópico de publicação.
9. Verifique os pinos utilizados pelos sensores.
10. Compile e envie o firmware para o ESP32.

O endereço do broker deve apontar para o Orange Pi, considerando a rede utilizada pelo projeto.

## 8.3. Preparação do Orange Pi

1. Instale o DietPi no Orange Pi.
2. Configure o acesso SSH.
3. Atualize os pacotes do sistema.
4. Instale o Mosquitto e suas ferramentas.
5. Configure o usuário e a senha MQTT.
6. Configure as regras ACL.
7. Habilite o serviço MQTT.
8. Instale Python 3 e as dependências do backend.
9. Configure o serviço responsável pelo armazenamento CSV.
10. Prepare o servidor web para disponibilizar a dashboard.

Os serviços devem ser configurados para iniciar automaticamente com o sistema, garantindo que o monitoramento continue funcionando após uma reinicialização do Orange Pi.

## 8.4. Configuração do broker MQTT

Depois de instalar o Mosquitto, configure a autenticação e as permissões de acesso.

É necessário garantir que:

- O ESP32 consiga publicar no tópico definido.
- O backend Python consiga receber as mensagens.
- A dashboard consiga estabelecer uma conexão WebSocket autenticada.
- Cada cliente tenha somente as permissões necessárias.

A porta MQTT convencional é `1883`, enquanto a porta WebSocket depende da configuração do Mosquitto.

Não utilize as credenciais de exemplo de outra instalação. Crie usuários e senhas próprios para o seu servidor.

## 8.5. Instalação da dashboard

Coloque os três arquivos da interface no diretório configurado para hospedagem web.

A organização recomendada é manter os arquivos `index.html`, `style.css` e `script.js` em uma pasta própria da dashboard, acompanhados dos recursos necessários, como as bibliotecas MQTT e Chart.js.

Confira no JavaScript:

- Endereço do broker WebSocket.
- Porta configurada.
- Credenciais de acesso.
- Tópico MQTT.
- Estrutura dos campos JSON.
- Configurações dos gráficos.

Depois, configure o servidor web para disponibilizar a página.

## 8.6. Configuração do histórico

O backend Python deverá ser executado no Orange Pi e configurado para iniciar automaticamente.

Verifique se:

- O serviço consegue acessar o broker.
- As credenciais estão corretas.
- O tópico está configurado corretamente.
- Os dados JSON são interpretados sem erros.
- Os registros são gravados com data e horário.
- Os arquivos CSV possuem permissões de escrita.
- Existe espaço de armazenamento disponível.

A política de organização, rotação e retenção dos arquivos CSV deve ser definida de acordo com a capacidade do armazenamento e o período de histórico desejado.

## 8.7. Disponibilização externa

Caso queira acessar a aplicação pela internet:

1. Configure um domínio.
2. Configure o Cloudflare.
3. Crie o encaminhamento para o servidor web do Orange Pi.
4. Configure o acesso HTTPS.
5. Configure o acesso WSS para o MQTT.
6. Verifique se as autenticações continuam funcionando.
7. Teste o acesso utilizando uma rede externa.

O acesso externo deve ser configurado com segurança. Não é recomendado expor diretamente o broker MQTT sem autenticação e controle de permissões.

# 9. Organização recomendada do repositório GitHub

Para facilitar a reprodução e a manutenção do projeto, recomenda-se organizar os arquivos da seguinte maneira:

**Pasta principal — SIMA:** contém o README.md, que apresenta a documentação geral do projeto.

**Pasta ESP32:** contém o firmware responsável pela aquisição dos dados e comunicação com o servidor.

**Pasta ORANGE_PI:** reúne os arquivos utilizados no servidor, incluindo a dashboard, o backend Python e os arquivos de configuração de exemplo do Mosquitto.

**Pasta HARDWARE:** pode conter os esquemáticos eletrônicos, arquivos da PCB e informações sobre as conexões dos componentes.

**Pasta DOCUMENTACAO:** pode reunir documentos complementares sobre a arquitetura, instalação e funcionamento do sistema.

Essa estrutura é uma sugestão. Os nomes e a quantidade de arquivos devem corresponder aos códigos efetivamente disponibilizados.

**Atenção:** arquivos de configuração podem conter informações sensíveis. Nunca publique senhas Wi-Fi, credenciais MQTT, tokens do Cloudflare ou chaves privadas. Utilize arquivos de exemplo com valores fictícios e mantenha as credenciais reais fora do GitHub.

# 10. Funcionamento completo do sistema

Para compreender como todas as partes trabalham em conjunto, podemos acompanhar uma medição desde sua aquisição até sua apresentação ao usuário.

1. O DHT11, o LDR e o MQ-135 realizam a captação das condições ambientais.
2. Os sinais são enviados ao ESP32.
3. O ESP32 processa as informações e verifica os limites ambientais.
4. Caso seja identificada uma condição fora dos parâmetros, o sistema pode acionar o buzzer.
5. Os dados são organizados em uma mensagem JSON.
6. O ESP32 utiliza a rede Wi-Fi para publicar essa mensagem no tópico MQTT.
7. O Mosquitto, executado no Orange Pi, recebe a publicação e verifica as permissões.
8. O broker encaminha a mensagem aos clientes inscritos e autorizados.
9. O backend Python recebe os dados e registra as medições nos arquivos CSV.
10. A dashboard recebe as informações por MQTT sobre WebSocket e atualiza os indicadores e gráficos.
11. Quando o usuário consulta o histórico, a aplicação utiliza os registros armazenados.
12. O usuário pode acompanhar as medições em tempo real ou consultar os dados anteriores pela interface web.

Assim, o sistema consegue realizar aquisição, comunicação, processamento, armazenamento e visualização de dados ambientais de maneira integrada.

# 11. Considerações sobre a arquitetura

A arquitetura do SIMA foi desenvolvida para separar as responsabilidades entre os dispositivos, tornando o projeto mais organizado e facilitando sua manutenção e expansão.

O ESP32 executa as atividades relacionadas ao ambiente físico, utilizando o FreeRTOS para organizar as tarefas de aquisição, alarme e comunicação.

O Orange Pi, por sua vez, atua como servidor central, executando os serviços responsáveis pela distribuição das mensagens, armazenamento dos dados e disponibilização da aplicação web.

A utilização do MQTT permite uma comunicação leve e eficiente entre os componentes, enquanto o backend Python garante a persistência das informações em arquivos CSV.

A dashboard completa o sistema, oferecendo uma interface para acompanhamento das variáveis ambientais, visualização gráfica e consulta dos registros históricos.

Essa estrutura permite que o SIMA seja reproduzido e adaptado para diferentes ambientes, possibilitando a inclusão de novos sensores, dispositivos, tópicos MQTT e funcionalidades de análise.

**SIMA — Sistema de Monitoramento Ambiental**

Da captação física à visualização dos dados: uma arquitetura embarcada integrada para monitoramento ambiental em tempo real.
