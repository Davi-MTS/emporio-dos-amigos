# Plano — Visualização no celular (relatórios, vendas, estoque)

> Planejamento. **Nada implementado ainda.**
> Decisão do dono: **acesso de qualquer lugar** + **atualização periódica** (não precisa ser ao vivo).

## 1. Abordagem escolhida

Gerar, de tempos em tempos, um **relatório-resumo** do sistema e salvá-lo numa **pasta
do OneDrive** da conta do dono. O OneDrive sincroniza sozinho para a nuvem; no celular,
o dono abre o arquivo pelo **app do OneDrive**, de qualquer lugar — mesmo com o PC depois
desligado (ele vê a última atualização).

- ✅ **Sem servidor, sem hospedagem, sem mensalidade.** Aproveita o OneDrive existente.
- ✅ Dados ficam na conta **privada** do dono (não em um servidor público).
- ⚠️ É uma **"foto" periódica** (não tempo real) — que foi exatamente o pedido.
- ⚠️ Só **leitura** (o celular não altera nada no sistema).

## 2. Formato do relatório

Recomendo **PDF** como formato principal: abre **na hora e em qualquer celular** pelo
app do OneDrive (iOS/Android previsualizam PDF nativamente, sem passos extras). Bonito,
imprimível, universal.

Alternativa/complemento: **HTML** (uma página, com abas e filtro de período) — mais
interativo, mas abrir HTML pelo OneDrive no celular costuma exigir 1–2 toques a mais
("abrir no navegador"). Dá para gerar **os dois**.

> **A confirmar:** PDF, HTML, ou os dois.

## 3. Conteúdo (o que aparece no celular)

- **Cabeçalho:** nome da loja + "atualizado em DD/MM HH:MM".
- **Resumo** (Hoje / 7 dias / 30 dias): faturamento, lucro, nº de vendas, ticket médio.
- **Formas de pagamento** do período (dinheiro, pix, cartão, fiado).
- **Mais vendidos** (top N).
- **Estoque:** lista com quantidade e custo, **destacando os baixos/em falta**.
- **Fiado (a receber):** total e por cliente.

## 4. Quando atualizar (gatilhos)

- **Ao fechar o caixa** (fim de expediente) — igual ao backup. **[base]**
- Botão **"Atualizar relatório agora"** na tela (para forçar a qualquer momento).
- (Opcional) a cada X horas com o app aberto.

> **A confirmar:** manter só "ao fechar caixa + botão", ou incluir o periódico?

## 5. Onde salvar

- Pasta padrão: `%OneDrive%/Empório dos Amigos/Relatório/` (detecta a pasta do OneDrive
  pela variável de ambiente `OneDrive`; se não houver, cai para Documentos e avisa).
- Nome fixo (ex.: `relatorio.html` / `relatorio.pdf`) — **sobrescreve** a cada atualização,
  para o dono ter sempre "o atual" no mesmo lugar (sem acumular arquivos).
- Caminho **configurável** (o dono pode apontar para outra pasta do OneDrive).

## 6. Acesso no celular (primeira vez)

1. Instalar o app **OneDrive** no celular e entrar na mesma conta Microsoft.
2. Abrir a pasta `Empório dos Amigos/Relatório` → tocar no arquivo.
3. (Dica) Marcar como favorito para achar rápido depois.
A tela de configuração no PC mostrará o **caminho** e um passo a passo curto.

## 7. Segurança

- A proteção real é a pasta ser **privada na conta do dono** (só quem tem a conta acessa).
- PDF: o Qt não cifra PDF facilmente → **sem senha no PDF** (documentar essa limitação).
- HTML: dá para colocar um **PIN leve** na abertura da página (barra acesso casual; não é
  criptografia).
- Geração e configuração **restritas ao Administrador** no app.

## 8. Arquitetura no código

| Peça | Papel |
|---|---|
| `src/services/relatoriomobile/RelatorioMobileService.{h,cpp}` | Consulta os repositórios (RelatorioRepository, Estoque, Financeiro) e monta os dados; gera o(s) arquivo(s). PDF via `QPdfWriter`/`QTextDocument` (HTML→PDF); HTML como arquivo único (CSS/JS embutidos, dados em JSON). |
| `AppBackend` | `Q_INVOKABLE gerarRelatorioCelular()` (retorna { ok, caminho, erro }) + status/config; gatilho dentro de `fecharCaixa`. |
| `qml/screens/config/...` | Seção "Relatório no celular": botão "Atualizar agora", caminho, instruções, (config da pasta). Pode ficar junto de Configurações/Backup. |
| Config | `QSettings`: pasta destino, formato(s), gatilho periódico on/off. |

Reaproveita os cálculos já existentes de relatório/estoque/financeiro — é uma camada de
**exportação**, não uma nova regra de negócio.

## 9. Testes

- Geração cria o arquivo, não vazio, no caminho esperado.
- HTML: contém os números-chave esperados (faturamento/lucro/contagens) — checagem por string.
- PDF: existência + tamanho > 0 (conteúdo binário não é inspecionado no teste).
- Robustez: se a pasta do OneDrive não existir, cai para o fallback sem quebrar.

## 10. Fora do escopo (por ora)

- Acesso **ao vivo** / servidor próprio / app nativo (Android/iOS).
- Escrever/alterar dados pelo celular (é só visualização).
- Notificações push.

## 11. Fases

| Fase | Entrega | Esforço |
|---|---|---|
| **1** | `RelatorioMobileService` gerando o arquivo (formato escolhido) + `gerarRelatorioCelular()` + botão "Atualizar agora" + testes. | ~1 dia |
| **2** | Gatilho automático ao fechar caixa + detecção/config da pasta OneDrive + tela com instruções. | ~0,5 dia |
| **3 (opcional)** | Segundo formato (HTML+PDF) e/ou periódico a cada X horas. | ~0,5 dia |

## 12. Decisões (fechadas)

1. **Formato:** **HTML** (arquivo único, autossuficiente). ✅
2. **Publicação:** **OneDrive** (pasta privada do dono) — descartado Vercel/nuvem pública
   por expor dados financeiros; ver conversa. Cloudflare Pages+Access fica como evolução. ✅
3. **Gatilhos:** **ao fechar o caixa** + botão **"Atualizar agora"** (sem periódico por ora). ✅
4. **Pasta:** `%OneDrive%/Empório dos Amigos/Relatório/relatorio.html` (fallback Documentos). ✅
5. **Seções:** resumo (Hoje/7/30), formas, mais vendidos, estoque (baixos em destaque), fiado. ✅
