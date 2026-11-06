# Guia do primeiro teste — Pocket Pad / Mousedroid

**Para:** Galaxy A12 com Android 9 ou mais recente e computador Windows x64.<br>
**Build de teste:** [abrir a execução aprovada no GitHub Actions](https://github.com/gabielrtxofc-coder/Pocket-Pad-Repository/actions/runs/42183313698).<br>
**Importante:** estes arquivos são builds de teste, não uma versão de produção. O teste físico no Galaxy A12 e a execução do servidor no seu Windows ainda precisam ser feitos.

## 1. Segurança: leia antes de abrir o servidor

- O servidor atual recebe comandos de mouse **sem senha ou autenticação** e escuta em todas as interfaces de rede, na porta **6969**. Isso continua valendo mesmo quando o telefone se conecta por USB.
- **Não abra o servidor em Wi-Fi público ou compartilhado** (hotel, café, escola, trabalho ou rede com pessoas desconhecidas). Use somente USB/ADB e, se for necessária uma rede para o servidor iniciar, use uma rede privada e isolada com apenas o seu computador e o Galaxy A12.
- Não abra portas no roteador e não permita acesso em redes públicas no Firewall do Windows. Se aparecer uma pergunta do Firewall sobre permitir acesso à rede, escolha **Cancelar**. Se o servidor não iniciar sem rede, pare e guarde uma captura de tela; não tente contornar isso usando uma rede pública.
- Feche Android Studio, emuladores e outros programas que usam ADB antes do teste. Ao fechar, Mousedroid pode desligar o serviço ADB global do Windows e interromper essas outras sessões.
- O pacote não tem assinatura digital. Se o Windows ou o antivírus bloquear o arquivo, **não desative a proteção**; pare e guarde uma captura de tela.

## 2. Baixar os arquivos e conferir os códigos SHA-256

1. Abra o link do **Build de teste** acima e entre no GitHub com uma conta que tenha acesso ao repositório.
2. Na seção **Artifacts**, baixe os dois itens:
   - `Pocket-Pad-Android-Debug.apk`
   - `Pocket-Pad-Windows-Server.zip`
3. O GitHub baixa cada artefato dentro de um ZIP externo. Extraia esse ZIP externo:
   - No artefato Android, você verá `Pocket-Pad-Android-Debug.apk` e `Pocket-Pad-Android-Debug.apk.sha256`.
   - No artefato Windows, você verá o arquivo real `Pocket-Pad-Windows-Server.zip` e `Pocket-Pad-Windows-Server.zip.sha256`. Esse ZIP interno é o servidor que será extraído para uso.
4. Os códigos conferidos pelo processo de build são:

   | Arquivo                         | SHA-256                                                            |
   | ------------------------------- | ------------------------------------------------------------------ |
   | `Pocket-Pad-Android-Debug.apk`  | `163e5f7a73f1dd47027374fd7ccb10dc2b0bbb32e3bc04ceea3f4a621a7d7878` |
   | `Pocket-Pad-Windows-Server.zip` | `4dba4d1b68d6df475ed1cc0da2076cd2542ce4a10d8300120a826e88f0fe795d` |

   Cada arquivo `.sha256` vem junto do seu arquivo. O build verificou os dois códigos antes de enviar os artefatos.

**Conferência opcional no Windows:** abra a pasta extraída que contém o arquivo e o respectivo `.sha256`. Na barra de endereço do Explorador de Arquivos, digite `powershell` e pressione Enter. Na janela que abrir, rode o comando correspondente e compare o campo `Hash` com a tabela acima:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath .\Pocket-Pad-Android-Debug.apk
Get-FileHash -Algorithm SHA256 -LiteralPath .\Pocket-Pad-Windows-Server.zip
```

O artefato fica disponível por 30 dias, até **6 de dezembro de 2026**. Os hashes acima valem somente para esta execução. Se o link já tiver expirado, a execução pode ser refeita na aba **Actions** do repositório, escolhendo o fluxo “Pocket Pad baseline build artifacts” e a opção **Run workflow** na branch Arena; nesse caso, compare com os arquivos `.sha256` do novo run, pois os hashes podem mudar.

## 3. Ativar a depuração USB no Galaxy A12

1. Confira em **Configurações → Sobre o telefone → Informações do software → Versão do Android** que o aparelho tem Android 9 ou mais recente.
2. Para mostrar as opções de desenvolvedor, abra **Configurações → Sobre o telefone → Informações do software** e toque **7 vezes** em **Número da versão** (ou **Número de compilação**). Digite o PIN do aparelho se for pedido.
3. Volte às Configurações, abra **Opções do desenvolvedor** e ligue **Depuração USB**. Ative-a só para este teste.
4. Conecte o Galaxy ao computador com um cabo USB que transfira dados e deixe o telefone desbloqueado. Se aparecer a pergunta **Permitir depuração USB?**, confira que o computador é o seu; marque **Sempre permitir deste computador** somente se for um computador pessoal confiável e toque **Permitir**.
5. No Windows, escolha **Transferência de arquivos** na notificação USB, se necessário. Copie `Pocket-Pad-Android-Debug.apk` para a pasta **Download** do telefone.
6. No Galaxy, abra **Meus Arquivos → Downloads**, toque no APK e escolha **Instalar**. Se o Android pedir autorização, permita instalar aplicativos desta fonte apenas para **Meus Arquivos**; depois do teste, você pode desligar essa permissão.

Se o Android disser que o aplicativo não pode ser instalado por já existir uma cópia ou por conflito de assinatura, não desinstale uma versão antiga sem querer apagar os dados dela. Guarde uma captura da mensagem e peça ajuda.

## 4. Preparar e iniciar o servidor Windows

Faça esta etapa somente depois de ler o aviso de segurança e preparar a conexão privada descrita acima.

1. Extraia o ZIP real `Pocket-Pad-Windows-Server.zip` para uma pasta cujo **caminho completo não tenha espaços**. Use, por exemplo, `C:\Users\Public\MousedroidTest`. Se o Windows não permitir criar essa pasta, escolha outra pasta sem espaços no caminho.
2. Não mova nem apague os arquivos extraídos. O pacote inclui `Mousedroid.exe`, `config.ini`, `Start-Server.bat`, arquivos ADB e DLLs de apoio. Os arquivos ADB aparecem ao lado do servidor e também dentro da pasta `adb` porque esta versão do programa procura nos dois lugares.
3. Dê dois cliques em **`Start-Server.bat`**. Ele abre o servidor usando a pasta correta como local de trabalho. Não precisa instalar CMake, vcpkg ou ADB separadamente.
4. Na janela Mousedroid, abra a aba **Status** e confira se aparece **ADB On**. O servidor tenta configurar o ADB automaticamente ao iniciar. Se a pergunta de autorização USB aparecer agora no telefone, desbloqueie-o e toque **Permitir**. Se aparecer **ADB Off**, a janela fechar, ou surgir um erro, não abra o Firewall nem mude configurações do roteador: tire uma captura da tela e pare aqui.

## 5. Conectar o telefone e testar

1. Confirme que o servidor Windows continua aberto, que o telefone está desbloqueado e conectado por USB, e que a depuração USB foi autorizada.
2. Abra **Mousedroid** no Galaxy A12 e toque em **Server (USB/WIFI)**. Com o cabo conectado, o aplicativo tenta usar USB/ADB automaticamente. Para este primeiro teste, não escolha Wi-Fi nem Bluetooth.
3. Aguarde a mensagem **Connecting...**. Ao conectar, deve aparecer a tela **Touchpad**. Na janela Windows, confira se o aparelho aparece na lista de dispositivos.
4. Para testar sem risco, abra um **Bloco de Notas vazio** no Windows:
   - Arraste um dedo pelo touchpad do telefone: o cursor deve se mover.
   - Toque uma vez numa área vazia do Bloco de Notas: deve ocorrer um clique esquerdo.
   - Experimente os botões **Left** e **Right** do aplicativo. O botão direito pode abrir um menu no Windows; feche-o com `Esc` no teclado do computador. O botão **Middle** pode não produzir um efeito visível no Bloco de Notas.
   - Para testar rolagem, abra no Windows uma página longa e sem dados pessoais e deslize **dois dedos** para cima e para baixo no touchpad do telefone.
5. Não digite senhas nem use o teste sobre janelas com informações pessoais.

## 6. Reconectar e encerrar

- Se aparecer **Connection to Mousedroid server failed** ou **Disconnected**, toque **OK**. Confira o cabo de dados, se o telefone está desbloqueado, se a autorização USB ainda está ativa, se a janela do servidor está aberta e se ela mostra **ADB On**. Volte à tela principal do aplicativo e toque novamente em **Server (USB/WIFI)**.
- Se a janela do servidor fechou, inicie outra vez `Start-Server.bat` e só então tente conectar no telefone. Não altere o Firewall para tentar corrigir uma falha USB.
- Para terminar, no telefone abra o menu lateral do touchpad (ícone de três linhas) e escolha **Disconnect**. Depois feche a janela do servidor e confirme o encerramento se o Windows perguntar.
- Volte às **Opções do desenvolvedor** e desligue **Depuração USB**. Se quiser, use também **Revogar autorizações de depuração USB**. Desligue a rede privada/hotspot temporário usado no teste.

## 7. O que fazer se algo der errado

| O que aparece                                                    | O que fazer com segurança                                                                                                                                                                |
| ---------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **ADB Off** na janela Windows                                    | Deixe o telefone desbloqueado; confira cabo de dados e Depuração USB. Aceite a pergunta USB no telefone, feche e abra o servidor uma vez. Se continuar, pare e capture a aba Status/Log. |
| **Connection to Mousedroid server failed**                       | Confira que o servidor foi aberto primeiro, que mostra **ADB On**, e que o USB está conectado e autorizado. Tente novamente uma vez. Não libere acesso público no Firewall.              |
| O servidor fecha logo ao iniciar, especialmente sem rede         | Pode haver uma falha na descoberta da rede local. Não use uma rede pública/compartilhada como contorno. Pare e capture a mensagem.                                                       |
| O Windows mostra aviso de Firewall ou de aplicativo desconhecido | Não desative o Defender e não permita acesso em redes públicas. Cancele e guarde uma captura para análise.                                                                               |
| O cursor não se move ou a conexão cai                            | Confira se a tela Touchpad aparece e se o servidor continua aberto. Desconecte e reconecte uma vez; se persistir, pare e registre o erro.                                                |
| O Android não instala o APK                                      | Confirme Android 9 ou mais recente e que o arquivo foi extraído/copiado inteiro. Não remova uma instalação antiga sem antes decidir sobre os dados dela.                                 |

## 8. Capturas de tela e resultado

Guarde estas capturas na pasta **Imagens** ou **Documentos**, sem mostrar senhas, nomes de redes Wi-Fi, mensagens privadas ou outros dados pessoais:

1. Galaxy A12 com a tela principal do Mousedroid.
2. Windows com Mousedroid aberto e o estado **ADB On**.
3. Galaxy A12 conectado na tela **Touchpad**.
4. Cada erro, se ocorrer.

No Galaxy, pressione **Ligar/Desligar + Diminuir volume** ao mesmo tempo para tirar uma captura. No Windows, pressione **Win + Shift + S**, selecione a área e salve a imagem. Se enviar o resultado para análise, informe também a versão do Android e do Windows e marque cada item como **Funcionou**, **Não funcionou** ou **Não testado**:

| Teste                               | Resultado |
| ----------------------------------- | --------- |
| Instalação do APK                   |           |
| Autorização USB/ADB e estado ADB On |           |
| Conexão USB até a tela Touchpad     |           |
| Movimento do cursor                 |           |
| Clique esquerdo/direito             |           |
| Rolagem com dois dedos              |           |
| Desconectar e conectar novamente    |           |

**Este guia não certifica que o aplicativo é seguro nem que foi testado em hardware.** O build de CI compilou o APK, executou os testes unitários Android e o lint, compilou o servidor Windows x64 Release e verificou os arquivos e SHA-256 do ZIP. A execução do servidor num Windows real e o teste no Galaxy A12 continuam pendentes.
