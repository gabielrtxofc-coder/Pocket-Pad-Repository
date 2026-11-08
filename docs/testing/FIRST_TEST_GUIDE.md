# Guia simplificado de instalação e teste — Pocket Pad / Mousedroid 1.6

**Aparelho-alvo:** Samsung Galaxy A12 (Android 9 ou superior).

**Computador:** Windows x64.

**Conexão suportada nesta versão:** USB/ADB. Wi-Fi está desativado até existir pareamento/autenticação segura.

**Validação física:** ainda pendente; os testes automatizados não substituem o teste no Galaxy A12.

> **Segurança:** o servidor agora escuta somente em `127.0.0.1` e encaminha USB por ADB. Não é necessário abrir porta no roteador/Firewall nem permitir acesso à rede. Não crie exceção de Firewall, não desative o Windows Defender e não tente usar Wi-Fi como contorno. O encerramento do Mousedroid deixa o ADB global ativo para não interromper Android Studio ou outros clientes ADB. A autorização RSA normal do Android continua obrigatória.

## 1. Baixar os artefatos e conferir os hashes

1. Abra a execução aprovada do GitHub Actions indicada na seção **Build atual** abaixo.
2. Em **Artifacts**, baixe `Pocket-Pad-Android-Debug.apk` e `Pocket-Pad-Windows-Server.zip`.
3. O GitHub entrega cada artifact dentro de um ZIP externo. Extraia esse envelope. Você obterá:
   - `Pocket-Pad-Android-Debug.apk` e seu arquivo `.sha256`;
   - `Pocket-Pad-Windows-Server.zip` e seu arquivo `.sha256`.
4. Confirme os dois hashes SHA-256 abaixo. Eles devem coincidir com os arquivos `.sha256` entregues no mesmo artifact.

| Arquivo | SHA-256 |
|---|---|
| `Pocket-Pad-Android-Debug.apk` | `bfe2a0a971d09adffe4cbf5b0bdc07e7dca27c82e1fb933d7ebbe04cfcd3c5f2` |
| `Pocket-Pad-Windows-Server.zip` | `d672408b7ab142f8688363872d9f9d1f8baec292359bc4b18c3906b78e9069a5` |

No Windows, abra PowerShell na pasta correspondente e execute:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath .\Pocket-Pad-Android-Debug.apk
Get-FileHash -Algorithm SHA256 -LiteralPath .\Pocket-Pad-Windows-Server.zip
```

Compare o campo `Hash` com a tabela e com os sidecars `.sha256`.

## 2. Preparar o Galaxy A12

1. Abra **Configurações → Sobre o telefone → Informações do software** e toque sete vezes em **Número da versão/compilação** para habilitar Opções do desenvolvedor.
2. Em **Configurações → Opções do desenvolvedor**, ligue **Depuração USB**.
3. Desbloqueie o telefone e conecte-o ao PC com um cabo USB que transfira dados.
4. Quando o Android exibir **Permitir depuração USB?**, confira que o computador é o seu e toque **Permitir**. Marcar **Sempre permitir deste computador** é opcional e deve ser usado apenas em um PC confiável.
5. Se não aparecer a solicitação RSA, verifique o status no servidor Windows. `unauthorized` exige aprovação no telefone; o aplicativo não ignora nem contorna esse passo.

## 3. Instalar e iniciar

1. Transfira `Pocket-Pad-Android-Debug.apk` para **Downloads** no Galaxy (ou abra-o no telefone) e instale. Se solicitado, autorize instalação desta fonte somente para o aplicativo usado para abrir o APK.
2. Extraia `Pocket-Pad-Windows-Server.zip` para qualquer pasta, inclusive um caminho com espaços, por exemplo `C:\Users\Public\Pocket Pad`.
3. Abra a pasta extraída e dê dois cliques em **`Start-Server.bat`**. Não é necessário instalar ADB, CMake ou vcpkg separadamente.
4. Na aba **Status** do Windows, confira a indicação de ADB:
   - **No ADB device**: confira o cabo e a depuração USB;
   - **unauthorized**: desbloqueie o telefone e aceite o prompt RSA;
   - **offline**: reconecte o cabo ou reinicie a depuração USB;
   - **multiple devices**: selecione o serial do Galaxy na lista;
   - **USB device ready**: o reverse serial-específico está ativo.
5. Abra Mousedroid no telefone. O modo USB inicia automaticamente; não é necessário tocar repetidamente no botão. O Android mostra o estado de espera/conexão e abre o Touchpad quando conectar.

## 4. Teste funcional simples

1. No Windows, abra um Bloco de Notas vazio.
2. Deslize um dedo no Touchpad: o cursor deve se mover.
3. Toque no Touchpad para testar o clique esquerdo. Teste também os botões Left e Right; feche qualquer menu contextual com `Esc`.
4. Para rolagem, use uma página longa sem dados pessoais e deslize dois dedos para cima/baixo.
5. Não teste sobre senhas, documentos ou janelas com informações pessoais. Não use o teste para digitar credenciais.

## 5. Testar recuperação USB

1. Deixe o Touchpad aberto e desconecte o cabo USB por alguns segundos.
2. O aplicativo deve indicar que a conexão foi interrompida e manter uma única rotina de reconexão com espera crescente; não deve abrir popups repetidos nem exigir que você saia do Touchpad.
3. Reconecte o cabo, desbloqueie o telefone e aceite novamente a autorização RSA se o Android solicitar. O Touchpad deve voltar a funcionar automaticamente após o reverse e a conexão serem restaurados.
4. Como verificação adicional, feche e reabra o servidor Windows. Ele deve revalidar/reaplicar o reverse sem encerrar nem reiniciar o ADB global.
5. Para terminar, escolha **Disconnect** no menu do Touchpad; o auto-connect USB fica pausado até tocar novamente em **Connect USB**. Depois feche a janela do servidor e desligue **Depuração USB**. Se desejar, use **Revogar autorizações de depuração USB**.

## 6. Limitações e diagnóstico

- A execução CI valida compilação, unit tests, análise lint, estrutura do APK/ZIP, chamadas simuladas de ADB, hashes e caminhos com espaços (no job Windows). Ela **não** executa o servidor em um PC real nem valida o cabo, RSA, cursor ou recuperação no Galaxy A12.
- Wi-Fi está deliberadamente desativado: não use a lista de dispositivos salvos como alternativa.
- O APK e o ZIP de Actions são artifacts temporários; use o run atual e os hashes que acompanham aquele run.
- Se aparecer erro de ADB/reverse, verifique somente o indicador da aba Status, a autorização RSA e o cabo. Não altere Firewall, roteador ou Defender. Guarde uma captura do estado/log e pare se o problema persistir.

## Build atual

- **GitHub Actions:** [run 42224403618 — todos os jobs aprovados](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42224403618)
- **Commit validado:** `4562fbbaff11fe95012193195cdb404b773faa0b`
- **Artefatos:** `Pocket-Pad-Android-Debug.apk` e `Pocket-Pad-Windows-Server.zip` (com seus sidecars `.sha256`); retenção até **6 de dezembro de 2026**.
- Os dois jobs computaram e conferiram os hashes antes de enviar os artefatos. O job Windows também extraiu e verificou a estrutura do ZIP.
