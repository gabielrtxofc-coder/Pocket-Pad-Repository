# Revisão de encerramento — Missão 03

> Registro histórico. A revisão atualizada está em [REMEDIACAO_03_1.md](REMEDIACAO_03_1.md).

## CI e artefatos

A execução [42459733027](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42459733027) concluiu com sucesso **ambos** os jobs para `0aec21e`: Android (testes unitários, lint, APK, verificação de assinatura e checksum) e Windows x64 (compilação, testes fake-ADB/processo, ZIP). A revisão posterior altera documentação apenas. Não houve teste físico.

Os artefatos não expirados na consulta de 2026-11-08 estão na página da execução: `Pocket-Pad-Android-Debug.apk` e `Pocket-Pad-Windows-Server.zip`; expiram em 2026-12-08. As anotações do CI registram SHA-256 do APK `07c2936a2889056946c0c30040ae5b673f6e8bcf1544dcaaede486fc73b32d84` e do ZIP Windows `70c04f6621b96f2fda43f8054b3a0a9b81e8c7226d90b4203d829e4d3601de05`. O acesso ao armazenamento externo de artefatos e logs é bloqueado neste ambiente; não se trata de verificação independente dos bytes ou do conteúdo dos arquivos. Os hashes acima são os dos arquivos internos conforme as anotações, não dos envelopes de download de artifacts do GitHub.

## Escopo e histórico

Contra a base solicitada `d35c3cf`, a Missão 03 altera a tela do touchpad, seu handler de gestos, matemática do cursor, testes unitários, documentação e filtro de branch do workflow; não altera transporte USB ou servidor. `git diff --check` passou. Entretanto, a `main` ainda está no commit inicial `caac5a5`: o diff do PR contra `main` inclui também a importação completa do Mousedroid e o trabalho das missões anteriores (223 arquivos antes desta revisão), não apenas a Missão 03. Isso decorre da base expressamente solicitada; a inclusão integral dessa base precisa de aprovação consciente. Não se pode afirmar que o diff contra `main` está limitado à Missão 03.

## Impedimentos para aprovação

- A própria missão permanece parcial: detecção de pinça, rolagem e clique de dois dedos pode conflitar, sem testes automatizados de interação; Bluetooth pode descartar rolagens pequenas. A opção de toque simples não desabilita toque duplo/arraste.
- `PointerMotion` retém deslocamentos acima de 127 até outro evento: após um movimento muito grande, um evento posterior na direção oposta pode continuar transmitindo movimento anterior; ao terminar o gesto, o restante é descartado. Teste atual verifica a retenção, não a naturalidade ou reversão. Requer correção e testes antes de declarar precisão/consistência aprovadas.
- Ausência de testes instrumentados para configurações, lifecycle e cancelamento; nenhuma verificação física de USB, gestos, ergonomia e comportamento em Windows/Linux. Consulte a matriz em `RESULTADO.md`.
- A opção foi renomeada para “Toque simples para clicar”; duplo toque/arraste continua habilitado, conforme documentado. A confirmação visual ainda depende de teste instrumentado ou físico.
- Não há teste novo para demonstrar ausência de regressões de gestos simultâneos; sucesso do CI não equivale à conclusão dos critérios de experiência. Recomenda-se manter o PR em rascunho até resolver esses pontos e revisar também a base herdada.

Nenhum merge foi feito ou autorizado. A revisão não declara a missão pronta para merge.

## Atualização após correção do rótulo

No commit `a363ef7`, o rótulo foi esclarecido. A execução [42465674887](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42465674887) para esse commit passou no Android e gerou um novo APK (SHA-256 anotado pelo CI: `e088f9b4eeddaf00af2811a0ffe5ec294551afc29a9baeeab32d92d35c8e63bb`). O Windows falhou na etapa **Bootstrap pinned vcpkg and install dependencies**, tanto na primeira tentativa quanto na reexecução do job. A anotação mostra apenas exit code 1; os logs detalhados continuam inacessíveis devido ao bloqueio de rede. Portanto não é possível identificar ou corrigir com segurança a causa. A execução anterior passou em ambos os jobs, mas não substitui um CI verde no código atual. Corrigir/diagnosticar essa falha é outro impedimento para merge.
