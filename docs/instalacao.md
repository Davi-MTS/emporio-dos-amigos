# Instalação no computador da loja

Guia completo para colocar o **Empório dos Amigos** rodando no PC da
distribuidora. Duas partes: o que **você** faz antes (gerar o pacote) e o que é
feito **na loja** (instalar e configurar).

---

## Parte 1 — Gerar o pacote (na sua máquina)

> Só precisa refazer isto quando houver mudança no sistema.

**Requisitos na sua máquina:** Qt 6.8 (MinGW), CMake e Ninja — todos vêm no
instalador do Qt.

Na raiz do projeto, rode:

```bash
powershell -ExecutionPolicy Bypass -File .\deploy\empacotar.ps1
```

O script compila em modo de produção, junta o Qt e gera:

| Saída | O que é |
| --- | --- |
| `deploy/pacote/` | Pasta pronta para copiar (~1.300 arquivos) |
| `deploy/Emporio-dos-Amigos.zip` | A mesma coisa compactada (~26 MB) |

Se o Qt estiver em outro caminho:

```bash
powershell -ExecutionPolicy Bypass -File .\deploy\empacotar.ps1 -Qt "D:\Qt\6.8.3\mingw_64" -MinGW "D:\Qt\Tools\mingw1310_64\bin"
```

**O pacote é autossuficiente:** leva o Qt junto. No PC da loja **não precisa
instalar Qt, .NET, Java nem nada**.

---

## Parte 2 — Instalar na loja

### Requisitos do computador
- **Windows 10 ou 11**, 64 bits
- ~100 MB livres para o programa (mais espaço para os backups)
- **Não precisa de internet** para vender. Internet só é usada para enviar o
  resumo no Telegram.

### Passo a passo

1. **Copie a pasta** `pacote` (ou descompacte o zip) para o computador.
   Sugestão de local: `C:\Emporio dos Amigos`
   > Evite deixar na Área de Trabalho ou em pasta sincronizada com nuvem.

2. **Renomeie** a pasta para `Emporio dos Amigos`, se quiser.

3. Dentro dela, clique com o **botão direito** em **`Emporio dos Amigos.exe`**
   → **Enviar para** → **Área de trabalho (criar atalho)**.

4. Pronto. Use o atalho para abrir o sistema. **Não abre janela preta de
   comando** — é aplicativo normal.

### Primeiro acesso

Na primeira vez o sistema pede para **criar o administrador**:

- **Nome** — quem é (ex.: Davi)
- **Login** — o que digita para entrar (ex.: `davi`)
- **Senha** — mínimo 4 caracteres

> ⚠️ **Guarde essa senha.** É ela que libera relatórios, backup, cancelamento de
> venda e cadastro de usuários. **Não há como recuperar** se perder — só
> restaurando um backup antigo ou começando do zero.

### Configuração inicial (nesta ordem)

1. **Usuários** — crie um login de *Funcionário* para quem opera o caixa.
   O perfil Funcionário **não** vê financeiro, **não** altera preço e **não**
   cancela venda.

2. **Produtos** — cadastre o que vende. Para cada produto:
   - **Dados**: nome, categoria, unidade base
   - **Embalagens**: unidade e, se vender fechado, a caixa/fardo com o **fator**
     (ex.: caixa = 12) e o **código de barras** de cada uma
   - **Composição**: só para copão/drink/dose

3. **Compras** — registre a entrada da mercadoria **com o custo real**.
   É isso que faz o **lucro** aparecer certo nos relatórios.
   > Produto sem compra registrada fica com custo zero e o sistema mostra a
   > venda inteira como lucro.

4. **Clientes** — quem compra fiado, com o **limite** de crédito.

5. **Telegram** (opcional, recomendado) — ver abaixo.

### Uso no dia a dia

| Momento | O que fazer |
| --- | --- |
| Abrir a loja | **PDV** → informar o troco inicial → *Abrir caixa* |
| Vender | Bipar o código de barras (ou digitar o nome) → escolher pagamento → *Finalizar* (F12) |
| Tirar dinheiro | **PDV** → *Sangria* |
| Errou uma venda | **Vendas** → *Ver* → *Cancelar esta venda* (devolve estoque e fiado) |
| Fechar a loja | **PDV** → *Fechar caixa* → contar o dinheiro e informar |

**Ao fechar o caixa**, o sistema faz sozinho: **backup**, **relatório
atualizado** e **envia no Telegram** o resumo, o relatório completo e a
**cópia de segurança do banco**.

---

## Receber o resumo no celular (Telegram)

Feito uma vez só, leva ~5 minutos.

1. No celular, abra o Telegram e procure **@BotFather**.
2. Envie `/newbot`, escolha um nome e um usuário terminado em `bot`.
   Ele responde com o **token** (algo como `8123456789:AAH...`). Copie.
3. **Se for para vários donos:** crie um **grupo**, adicione o seu bot nele.
4. No sistema: **Backup** → *Aviso no celular (Telegram)* → cole o **token**.
5. No grupo (ou na conversa com o bot), mande uma mensagem **começando com
   barra**, por exemplo `/oi`.
   > Em grupos o bot só enxerga mensagens que começam com `/`. É por isso que
   > "oi" sozinho não funciona.
6. No sistema, clique em **Descobrir** → o chat é preenchido → **Salvar** →
   **Enviar teste agora**.

Dali em diante, ao fechar o caixa chega no celular o resumo do dia
(faturamento, lucro, conferência da gaveta, alertas), o **relatório completo**
e a **cópia de segurança do banco** em anexo.

> **Deixe ligado o "Enviar também a cópia de segurança".** É o que faz os dados
> da loja existirem em outro lugar além deste computador: com o arquivo `.db`
> guardado na conversa do Telegram, um HD queimado ou um PC roubado não levam
> junto o histórico. Para recuperar, basta baixar o `.db` do Telegram, colocá-lo
> na pasta de backups e usar **Backup** → *Restaurar*.
>
> Arquivos acima de **45 MB** o Telegram não aceita; nesse caso o sistema avisa
> na tela (o backup local continua sendo feito normalmente).

---

## Onde ficam os dados

```
%APPDATA%\Distribuidora\Distribuidora\
    distribuidora.db      <- banco: vendas, produtos, clientes, tudo
    logs\sistema.log      <- registro de erros e eventos
    Relatorio\            <- relatório enviado no Telegram
```

Backups automáticos:

```
Documentos\Empório dos Amigos\Backups\
```

> Os dados **não** ficam na pasta do programa. Isso é de propósito: permite
> **atualizar o sistema trocando a pasta**, sem perder nada.

---

## Atualizar o sistema

1. Feche o sistema.
2. **Renomeie** a pasta antiga (ex.: `Emporio dos Amigos-antigo`) — não apague
   ainda.
3. Copie a pasta nova no lugar.
4. Abra. Os dados continuam lá, e o banco se atualiza sozinho se preciso.
5. Confirmando que está tudo certo, apague a pasta antiga.

---

## Backup e restauração

**Automático:** a cada fechamento de caixa, guardando as **5 cópias mais
recentes**.

**Manual:** **Backup** → *Fazer backup agora*.

**Restaurar:** **Backup** → escolher a cópia → *Restaurar* → confirmar →
**fechar e abrir o sistema**. Antes de trocar, o sistema guarda uma cópia do
estado atual, então dá para voltar atrás.

> ⚠️ **O backup fica no mesmo computador.** Se o HD queimar ou o PC for
> roubado, perde-se tudo. **Recomendo copiar a pasta de backups para um pen
> drive** de vez em quando, ou combinar o envio automático para fora.

### Levar os dados para outro computador

1. No PC antigo: **Backup** → *Fazer backup agora*.
2. Copie o arquivo `.db` da pasta de backups (pen drive, e-mail).
3. No PC novo: instale o sistema, coloque o arquivo na pasta de backups e use
   **Backup → Restaurar**.

---

## Quando algo der errado

O sistema grava tudo que acontece em:

```
%APPDATA%\Distribuidora\Distribuidora\logs\sistema.log
```

Na tela **Backup**, seção *Registro do sistema*, dá para ver as últimas linhas
(erros em vermelho) e clicar em **Abrir pasta** para pegar o arquivo e enviar
para quem dá suporte.

### Problemas comuns

| Sintoma | Causa provável / o que fazer |
| --- | --- |
| O sistema não abre | Faltou copiar a pasta inteira. Copie tudo, não só o `.exe`. |
| "Produto não encontrado" ao bipar | O código de barras não está cadastrado. **Produtos** → editar → aba **Embalagens** → preencher *Cód. barras*. |
| Lucro parece alto demais | Produtos com custo zero. Registre as entradas em **Compras** com o custo real. |
| Fechamento não bate | Confira sangrias e pagamentos de contas em dinheiro. Pix e cartão **não** entram na gaveta. |
| Não chega mensagem no Telegram | Sem internet no PC, ou o bot foi removido do grupo. Teste em **Backup** → *Enviar teste agora*. |
| Vendeu no fiado e recusou | Cliente sem limite ou limite estourado. **Clientes** → ajustar o limite. |

---

## O que o sistema **não** faz

Para não haver surpresa na loja:

- **Não emite nota fiscal** (foi decidido que a emissão é por fora).
- **Não imprime cupom** — ainda não implementado.
- **Não integra com maquininha** (TEF): o pagamento é registrado no sistema,
  mas a maquininha é operada à parte.
- **Não tem delivery nem integração com WhatsApp.**
- **Não sincroniza entre computadores** — é um PC só, com o banco local.
