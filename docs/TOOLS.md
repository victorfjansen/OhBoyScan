# As 5 Principais Funcionalidades do OhBoyScan

Considerando os requisitos do artigo acadêmico de cibersegurança e o foco prático em roteadores MikroTik (RouterOS v7) utilizando a Raspberry Pi 3B+ com display de 3.5", estas são as 5 features mais relevantes e estratégicas do projeto:

## 1. Auditoria de Integridade Criptográfica de Scripts RouterOS (ECDSA)
* **Por que é essencial:** É o **diferencial técnico e acadêmico** do seu projeto. Resolve uma brecha real do RouterOS (execução de scripts `.rsc` sem assinatura) e atende às exigências do artigo científico abordando a padronização FIPS 186-5.
* **No display de 3.5":** Valida a assinatura digital $(r, s)$ via API e exibe o badge imediato em verde (`[VALID]`) ou alerta em vermelho caso haja backdoor ou alteração de regras (`[TAMPERED]`).

## 2. Mapeador Rápido de Topologia e Vizinhos (MNDP / LLDP / CDP)
* **Por que é essencial:** É a principal ferramenta de **produtividade em campo** para técnicos de rede.
* **No display de 3.5":** Basta plugar o cabo Ethernet (com PoE ativo) para capturar os pacotes de descoberta de camada 2 e exibir instantaneamente o nome do roteador (*Identity*), modelo da RouterBOARD, versão do RouterOS, IP de gerência e a porta física conectada, dispensando o uso de um notebook de suporte.

## 3. Monitor Contínuo de Envenenamento de Cache ARP (Anti-MitM)
* **Por que é essencial:** Consolida o perfil do dispositivo como um **scanner ativo de cibersegurança** operando na camada de enlace (L2).
* **No display de 3.5":** Escuta o tráfego local via sockets brutos (`libpcap`) e alerta em tempo real se o endereço MAC do gateway MikroTik estiver sendo forjado ou duplicado por um invasor realizando um ataque de *ARP Spoofing/Poisoning*.

## 4. Diagnóstico Físico de Enlace e Qualidade de Cabo (TDR)
* **Por que é essencial:** Traz **utilidade imediata de hardware e infraestrutura** para técnicos atuando diretamente em racks e armários de telecomunicação.
* **No display de 3.5":** Aciona via API o teste de cabo nativo do chip Ethernet MikroTik (`/interface ethernet cable-test`) e indica graficamente se o cabo está rompido ou em curto, exibindo na tela a distância exata da falha em metros.

## 5. Verificador de Exposição de Interfaces de Gerenciamento Inseguras
* **Por que é essencial:** Cobre o teste de **postura básica e higiene de segurança** dos roteadores MikroTik em campo, prevenindo invasões primárias.
* **No display de 3.5":** Executa varreduras rápidas nas portas críticas (WebFig HTTP porta 80, Winbox 8291, API 8728, Telnet/SSH) para identificar credenciais de fábrica padrão ou a ausência de restrição de IPs permitidos (`allowed-address`), alertando imediatamente caso portas administrativas estejam escutando indevidamente na interface WAN.