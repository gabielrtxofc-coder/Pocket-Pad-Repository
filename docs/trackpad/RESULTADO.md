# Missão 03 — resultado parcial e validação

## Comparação

| Área | Antes | Alteração nesta missão | Estado |
|---|---|---|---|
| Cursor | Deltas convertidos para inteiro por evento | Frações acumuladas, sensibilidade e aceleração configuráveis | Implementação e testes matemáticos disponíveis; avaliação física pendente |
| Rolagem | Eixo dominante com ganho fixo | Controle persistente de velocidade | Parcial: conflitos e pequenas quantidades no HID continuam possíveis |
| Clique | Toques habilitados sem preferências | Opção para clique simples e clique direito com dois dedos | Parcial: opção de toque simples não desativa duplo toque; reconhecimento anterior permanece |
| Arrastar | Liberação normal ao levantar dedo | Liberação também ao cancelar ou destruir a tela | Parcial: mecanismo anterior de duplo toque/movimento permanece |
| Configurações | Nenhuma | Ajustes simples persistidos em SharedPreferences; salvar/cancelar | Implementado, sem teste instrumentado |
| Tela cheia | Botão podia ficar atrás do sensor | Botão trazido para a frente | Alterado, requer teste no dispositivo |
| Pinça e conflitos multitouch | Detecção concorrente com scroll e clique | Sem mudança no reconhecimento | Pendente: não declarar resolvido |
| USB | TCP/ADB reverse e reconexão | Transporte e protocolo inalterados | Regressão física pendente |

Configurações: abra **Ajustes** na tela Touchpad, ajuste sensibilidade, aceleração e velocidade de rolagem, ou habilite/desabilite os cliques indicados; toque em Salvar. Os padrões preservam o ganho anterior (100%, aceleração 0%). A opção de toque simples não desativa o duplo toque/arraste existente. Não há mudanças no protocolo; resultados variam com sensibilidade e aceleração do computador.

## Testes executados

- CI [42459733027](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42459733027), commit `0aec21e`: job Android passou `clean testDebugUnitTest lintDebug assembleDebug`, assinatura e checksum. O artefato **Pocket-Pad-Android-Debug.apk** está na página da execução. SHA-256 do APK: `07c2936a2889056946c0c30040ae5b673f6e8bcf1544dcaaede486fc73b32d84`. A máquina local não tem Java/SDK; o download local do artefato foi bloqueado pela restrição de rede ao armazenamento do GitHub Actions. O job Windows ainda estava em andamento quando consultado.
- Testes novos cobrem acumulação fracionária, reset, ganhos e limites de movimento. Não cobrem o reconhecimento de gestos Android ou concorrência de transporte.
- XML da tela validado e `git diff --check` passou. Nenhum teste físico foi executado.

## Pendente no código e em hardware

A missão não está concluída: ainda é necessário reconhecer os gestos de forma mutuamente exclusiva, testar automaticamente conflitos entre rolagem, pinça, toque de dois dedos e arraste, melhorar o tratamento de rolagem pequena em Bluetooth e testar persistência/estado da UI. O algoritmo anterior pode gerar clique direito ao levantar um dedo após pinça, ou combinar rolagem com zoom; não foi comprovada uma experiência de trackpad dedicado.

Validar em telefone Android real e computadores Windows/Linux: conexão USB, desconexão/reconexão e latência; clique simples/duplo, clique com dois dedos, rolagem vertical/horizontal, pinça, arraste e cancelamento; fullscreen e alternância de orientação; diferentes velocidades e densidades; persistência após reiniciar; Bluetooth e diferenças de sensibilidade/rolagem no SO. Confirmar que não há botão preso ao sair da tela ou perder conexão. Nenhum desses testes foi considerado aprovado.
