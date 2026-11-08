# Missão 03 — auditoria anterior às mudanças

Base: d35c3cfbccf3fb6fb89d84947dc075e650d39cef. Inspeção estática, não validação em hardware.

## Arquitetura
`Touchpad` instala `GestureHandler` no sensor e envia `InputEvent` pelo `TouchpadViewModel` ao `ConnectionManager`. USB usa TCP através de adb reverse e do supervisor de conexão; Bluetooth usa relatórios HID. `Translators.kt` transforma eventos em bytes; o servidor desktop injeta eventos no sistema. Nenhuma alteração de transporte é necessária para os controles abaixo.

## Estado anterior
| Recurso | Estado | Evidência e ressalvas |
|---|---|---|
| Um dedo / cursor | Parcial | GestureDetector.onScroll; trunca frações e limita cada evento, sem ajustes. |
| Rolagem vertical/horizontal dois dedos | Parcial | onScroll escolhe um eixo, depende da contagem de ponteiros dos eventos e tem janela residual de 150 ms; HID divide por 10, descartando valores pequenos. |
| Toque esquerdo e toque duplo | Parcial | singleTap atrasado 150 ms; doubleTap envia dois cliques, com atraso adicional. |
| Toque dois dedos direito | Parcial | ACTION_POINTER_UP envia clique se não houve scroll recente, sem avaliar duração/deslocamento; pode conflitar com pinch. |
| Arrastar | Parcial | duplo toque seguido de movimento dentro de 150 ms inicia botão; cancelamento e destruição da view não garantem liberação. |
| Pinça zoom | Parcial | ScaleGestureDetector gera Zoom; pode concorrer com rolagem/clique e é alimentado somente se GestureDetector não consumir o evento. |
| Botões mouse e multimídia | Implementado | layout e TouchpadViewModel; compatibilidade efetiva depende do transporte/host. |
| Área ampliada | Parcial | controle fullscreen existe, mas sensor é desenhado depois do botão e pode interceptar seus toques. |
| Sensibilidade, aceleração, velocidade de rolagem e preferências | Ausente | não há configuração no fluxo do touchpad inspecionado. |

Um trackpad convencional separa gestos por estado, acumula frações, evita cliques após movimento/pinça, libera botões ao cancelar e responde ao toque sem atrasos desnecessários. Resultados reais de latência e suavidade precisam de medição no dispositivo e no host; não podem ser inferidos apenas do código.
