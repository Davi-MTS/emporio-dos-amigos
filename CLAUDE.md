# Sistema de Gestão — Empório dos Amigos

Sistema de gestão (ERP) para uma distribuidora de bebidas: estoque, PDV,
financeiro, compras, clientes (fiado) e relatórios. **Loja única.**

**Identidade:** Empório dos Amigos — Bebidas & Conveniência · 062. Cor primária
**preto**, secundária **laranja**; visual premium e sóbrio. Logo em
`resources/images/logo.png` (embutida como `:/images/logo.png`, usada no login e
na sidebar). Ver `docs/design-ui.md` e `docs/mockup-ui.html`.

## Stack e plataforma (decidido)

- **Plataforma:** aplicativo desktop nativo (não é web).
- **Framework:** Qt 6 + QML.
- **Linguagem do backend:** C++.
- **Banco de dados:** SQLite (local, embutido, offline-first — o app precisa
  vender mesmo sem internet).
- **Sistema operacional alvo:** Windows (PC do caixa).
- **Backup:** local (arquivo SQLite) + sincronização na nuvem quando houver
  internet.

## Princípios de arquitetura

- Separar **lógica de negócio** (`src/domain/`) da **interface** (`qml/`).
  As regras de negócio devem ser testáveis isoladamente, sem depender da UI.
- As partes mais críticas e que NÃO podem ter erro silencioso:
  **conversão de embalagem** e **fechamento de caixa** — cobrir com testes
  em `tests/cpp/`.
- Integração com hardware (`src/services/hardware/`) é **plugável**: leitor de
  código de barras e maquininha (TEF). O resto do sistema não depende dela.
- Alterações de schema sempre via **migrations versionadas** em `db/migrations/`
  (arquivos SQL numerados). Nunca alterar o banco "na mão".

## Conceito central — conversão de embalagem

- O estoque guarda SEMPRE um único número, na **unidade base** (ex: long neck).
- Cada produto tem uma lista de embalagens (`produto_embalagens`), cada uma com
  um **fator de conversão** para a unidade base e seu próprio código de barras.
  Ex: caixa de Heineken = fator 12.
- Vender/comprar em caixa ou unidade só multiplica/divide pelo fator; o que é
  gravado (em `venda_itens.qtd_unidade_base` e `compra_itens.qtd_unidade_base`)
  e o que baixa do estoque é SEMPRE em unidade base.
- "Quantas caixas fechadas tenho?" é cálculo (total ÷ fator), não é armazenado.
- Custo também vive na unidade base (`estoque.custo_medio_unitario`); custo da
  caixa é derivado. Fonte única de verdade para custo e margem.
- **Escala do custo por unidade = MILÉSIMOS de centavo (centavos × 1000)** —
  migration `0007`. Vale para `estoque.custo_medio_unitario`, `movimentacoes.
  custo_unit` e `compra_itens.custo_unit`. Motivo: custo por ml/g é fração de
  centavo; em centavos inteiros arredondava para 0 e zerava o lucro. Conversão
  nas bordas: `EstoqueRepository::registrarEntrada` recebe centavos e ×1000;
  `registrarEntradaMilli`/`aplicarEntrada`/`CompraRepository` usam milésimos;
  `ItemEstoque.custoMedio` e o relatório de lucro dividem por 1000. Só valores
  em dinheiro "cheios" (preço, total) seguem em centavos.

## Decisões de negócio importantes

- **Nota fiscal:** FORA do escopo do sistema (emitida por fora).
- **Pagamentos:** múltiplas formas na mesma venda (Pix, dinheiro, débito,
  crédito). Tabela `pagamentos` é separada de `vendas`.
- **Cancelamento/troca:** nunca deletam registro — mudam `status` da venda e
  geram movimentação de estoque de volta (auditoria e fluxo de caixa).
- **Fechamento de caixa preciso:** inclui sangria e suprimento (`mov_caixa`),
  contagem por forma de pagamento, esperado × informado, diferença registrada.
- **Permissões:** campo JSON (`perfis.permissoes`), não colunas fixas. Perfis:
  Administrador (tudo) e Funcionário (vende, mas não altera preço/cadastro, não
  vê financeiro; desconto/cancelamento podem exigir autorização do admin).
- **Maquininha (TEF):** provedor ainda NÃO decidido. Integração fica plugável.
- **WhatsApp/Delivery:** opcional, fase posterior. Tabela `delivery` já prevista.

## Modelo de dados

Ver `docs/modelo-de-dados.md` para o esquema completo das tabelas dos 3 blocos
(produtos/estoque, vendas/caixa/pagamentos, compras/clientes/financeiro/usuários).

## Ordem de construção sugerida (fatia vertical primeiro)

1. Fatia vertical do núcleo: cadastro de produto (com conversão) → estoque →
   PDV → fechamento de caixa. É onde está o maior risco e o maior valor.
2. Compras/fornecedores + clientes/fiado + relatórios.
3. Delivery + WhatsApp + dashboard + permissões avançadas + backup na nuvem.

## Convenção de valores monetários

Dinheiro é armazenado e manipulado SEMPRE como **inteiro em centavos**
(`qint64`), nunca como `float`/`REAL`/`decimal`. R$ 12,50 = `1250`. Isso elimina
erro de arredondamento no fechamento de caixa (requisito crítico). A formatação
para exibição e o parse da entrada do usuário ficam em `src/utils/Money.*`.

## Estado atual

Fundação implementada (fatia inicial da fase 1):

- **Build:** `CMakeLists.txt` (raiz + `src/` + `qml/` + `tests/`) e
  `CMakePresets.json`. Dois alvos: `DistribuidoraCore` (biblioteca estática,
  regra de negócio, sem QML — definida em `src/`) e `distribuidora` (executável +
  módulo QML `Distribuidora` — definido em `qml/CMakeLists.txt`, para que os
  arquivos `.qml` sejam locais e o qmlcachegen não gere caminhos com "..").
  Compilado e testado com Qt 6.8.3 MinGW (build limpo, 4/4 testes passam, app
  inicia OK).
- **Banco:** migration `db/migrations/0001_schema_inicial.sql` (schema completo
  dos 3 blocos) e `db/seed/0001_dados_iniciais.sql` (perfis + categorias). Os
  scripts SQL são embutidos como recursos (`:/db/...`).
- **Camada de banco (`src/database/`):** `Database` (conexão SQLite + PRAGMAs:
  foreign_keys, WAL) e `MigrationRunner` (aplica migrations versionadas via
  tabela `schema_migrations`, e o seed idempotente).
- **Domínio (`src/domain/produtos/`):** `EmbalagemConverter` — conversão de
  embalagem (conceito central), funções puras e testadas.
- **Utils (`src/utils/`):** `Money` (centavos ⇄ texto pt-BR).
- **UI (`qml/`):** `Main.qml` (janela + barra lateral navegável), `theme/Theme`
  (singleton de design tokens), componentes e `DashboardScreen` (placeholder).
- **Testes (`tests/cpp/`):** `tst_money`, `tst_embalagem_converter`,
  `tst_migrations`. Rodam via CTest.

O app já **boota**: abre o banco, aplica migrations + seed e mostra a janela.

**Fase 1.1 — Produtos + embalagens (feita):**

- `src/domain/produtos/`: `Produto`/`Embalagem` (tipos de valor) e
  `ProdutoRepository` (CRUD sobre SQLite: listar com busca por nome/código,
  obter com embalagens, salvar em transação, inativar/soft-delete,
  `buscarPorCodigoBarras` p/ o PDV). Coberto por `tst_produto_repository`.
- `src/models/ProdutosListModel` (QAbstractListModel) e `src/app/AppBackend`
  (fachada QObject exposta ao QML como `App`, sem dependência de QML — fica no
  Core, testável). `AppBackend` também expõe helpers de dinheiro (centavos ⇄ texto).
- `qml/screens/produtos/ProdutosScreen.qml`: lista + editor. Prático — só o nome
  é obrigatório; o produto já nasce com a embalagem base; busca única por nome ou
  código de barras. Estilo Qt Quick Controls **Fusion** (definido no `main.cpp`).
- Componentes `qml/components/StatusBadge.qml` e `FormField.qml`.

**Padrão de wiring:** `main.cpp` cria `AppBackend(db.connection())` e o registra
via `rootContext()->setContextProperty("App", &backend)`. Novos módulos entram
como propriedades/invokables do `AppBackend`.

**Fase 1.2 — Estoque (feita):**

- `src/domain/estoque/EstoqueRepository`: listar, entrada de mercadoria com
  **custo médio ponderado**, e ajuste/inventário. Toda alteração registra
  `movimentacoes_estoque` (auditoria). Coberto por `tst_estoque_repository`.
- `src/models/EstoqueListModel` + métodos no `AppBackend` (`estoque`,
  `registrarEntrada`, `registrarInventario`, `embalagensDe`, `itemEstoque`).
- `qml/screens/estoque/EstoqueScreen.qml`: lista com status + diálogo de
  Entrada (por embalagem, converte p/ unidade base) e Inventário.

**Identidade visual aplicada no app** (igual ao `docs/mockup-ui.html`): fontes
**Fraunces** (títulos/marca) + **Archivo** (UI) embutidas em `resources/fonts/`;
ícones de linha (`AppIcon`, via QtQuick.Shapes) no lugar de emojis; sidebar com
seções e rodapé; topbar com título + status; botões da marca (`AppButton`);
**tema claro/escuro alternável** (padrão escuro), botão na topbar; transição de
página em fade. 5/5 testes passam; app compila e roda (Qt 6.8.3 MinGW).

**Fase 1.3 — PDV (feita):**

- `src/domain/caixa/CaixaRepository`: abrir sessão / consultar sessão aberta
  (fechamento preciso fica na 1.4).
- `src/domain/vendas/VendaRepository`: `registrarVenda` — grava venda, itens e
  pagamentos, **baixa o estoque** em unidade base e registra movimentações
  `saida_venda`; valida pagamento suficiente; calcula troco; fiado gera
  `contas_receber`. Coberto por `tst_venda_repository`.
- `AppBackend`: `caixaAberto`, `abrirCaixa`, `buscarProdutoPorCodigo`,
  `buscarProdutosPorNome` (sugestões), `finalizarVenda`. Cria um **usuário admin
  placeholder** no 1º boot (`garantirUsuarioPadrao`) — a fase 1.5 formaliza.
- `qml/screens/pdv/PdvScreen.qml`: abertura de caixa; campo de scan keyboard-first
  (Enter adiciona; sugestões por nome); carrinho com +/−; múltiplos pagamentos
  com troco/falta; atalhos (F2/F4/F12/Esc); baixa de estoque ao finalizar.

**Componentes de UI padronizados** (identidade da marca): `AppTextField`,
`AppComboBox`, `AppSpinBox`, `AppButton` e `AppDialog` (diálogo com fundo de
superfície arredondado, título em Fraunces e backdrop escurecido). **Use
`AppDialog` no lugar de `Dialog`** em todos os diálogos novos.

**Atenção QML (bug recorrente):** dentro de um `Repeater`, o `onActivated` de um
ComboBox recebe um parâmetro de sinal chamado `index` que SOMBREIA o `index` da
linha. Sempre qualifique com o id do delegate (ex.: `linha.index`) ao gravar no
model, senão grava na linha errada.

**Fase 1.4 — Fechamento de caixa (feita):**

- Migration `0002_venda_troco.sql` (coluna `vendas.troco`) — 1ª migration além do
  schema inicial; o `MigrationRunner` a aplica em bancos existentes.
- `CaixaRepository`: `registrarMovimento` (sangria/suprimento), `resumo` (por
  forma de pagamento, troco, sangria/suprimento) e `fechar` — calcula o
  **dinheiro esperado** (`abertura + vendasDinheiro − troco + suprimentos −
  sangrias`), grava esperado × informado × **diferença** e marca a sessão
  'fechada'. Coberto por `tst_caixa_repository`.
- `AppBackend`: `caixaResumo`, `registrarSangria/Suprimento`, `fecharCaixa`.
- `PdvScreen`: barra do caixa (Sangria / Suprimento / Fechar caixa) + diálogo de
  fechamento com o resumo por forma, contagem e diferença destacada.

**Fase 1.5 — Login e permissões (feita):**

- `src/services/auth/AuthService`: hash de senha **PBKDF2-HMAC-SHA256** (sal
  aleatório). `QPasswordDigestor` vive no **QtNetwork** — por isso o Core linka
  `Qt6::Network`. Coberto por `tst_auth`.
- `src/domain/usuarios/UsuarioRepository`: autenticar, CRUD, perfis,
  `criarPrimeiroAdmin`. Coberto por `tst_usuario_repository`.
- `AppBackend`: estado de sessão (`logado`, `usuarioAtual`, `precisaCriarAdmin`),
  `login`/`logout`/`criarAdmin`, `temPermissao(chave)` e CRUD de usuários. O
  autor de vendas/caixa é o usuário logado (`m_usuarioId`).
- UI: `LoginScreen` (portão; 1º uso cria o admin), `UsuariosScreen` (CRUD, só
  admin). Navegação Financeiro/Usuários e edição de Produtos gated por permissão.
  Topbar mostra o usuário + Sair; rodapé da sidebar mostra o usuário real.

**FASE 1 (núcleo operacional) CONCLUÍDA** — Produtos, Estoque, PDV (venda),
Fechamento de caixa e Login/Permissões, todos funcionais e testados (**9 testes**).
Perfis: Administrador (tudo) e Funcionário (restrito).

**Fase 2.1 — Compras e fornecedores (feita):**

- `EstoqueRepository` refatorado: `aplicarEntrada` (sem transação) reutilizável;
  `registrarEntrada` o envolve numa transação. Assim a compra aplica a entrada
  dentro da própria transação (SQLite não aninha).
- `src/domain/compras/`: `FornecedorRepository` (CRUD) e `CompraRepository`
  (`registrarCompra` — grava compra + itens, dá entrada no estoque/custo médio de
  cada item e, opcional, cria conta a pagar; tudo atômico). Coberto por
  `tst_compra_repository`.
- `AppBackend`: models `fornecedores`/`compras` + CRUD de fornecedor +
  `registrarCompra`.
- `qml/screens/compras/ComprasScreen.qml`: histórico + diálogo de nova compra
  (busca produto, escolhe embalagem/qtd/custo, opção de conta a pagar) + diálogo
  de fornecedores. Nav "Compras" gated por `ve_financeiro`.
- **Entrada por nota fiscal (manual):** migration `0008` adiciona `compras.numero_nota`
  e `compras.data_nota`; `registrarCompra` recebe `numeroNota/dataNota` (opcionais) e
  marca `origem='nota'` quando há número; a conta a pagar vira "NF <numero>". UI: campos
  "Nº da nota" + "Data da nota" na nova compra; o nº aparece na lista. Coberto por
  `tst_compra_repository::compraComNotaFiscal`.
- **Melhorias de operação (feito):** conta a pagar em dinheiro lança **sangria** no
  caixa (`pagarConta(id, forma)`); **quantidade digitável** no PDV (AppSpinBox no
  carrinho); **seletor unidade/caixa** por linha no PDV (embalagens serializadas em
  `embalagensJson`, troca preço/fator); **retirada de estoque** (perda/quebra) via
  `EstoqueRepository::registrarSaida`/`AppBackend::registrarRetirada` (tipo `ajuste`,
  aba "Retirada" no diálogo de estoque); tela de "Venda concluída" maior/legível.
  Cobertos por `tst_estoque_repository::retiradaBaixaEstoque`. Relatório/compra de
  caixas/fechamento verificados OK; desconto do PDV é valor fixo (sem % ainda).
- **Bugs de caixa/relatório corrigidos (auditoria com dados reais):**
  1. **Troco só sobre DINHEIRO** (`qBound(0, pago-total, pagoDinheiro)`): pix/cartão
     lançado a mais gerava "troco" e o fechamento tirava dinheiro que nunca entrou
     (visto em produção: `sessoes_caixa.valor_esperado = -42150`). O PDV também
     limita formas não-dinheiro ao que falta.
  2. **`ResumoCaixa::totalVendas()` = `SUM(vendas.total)`** (campo `totalVendido`),
     não a soma dos pagamentos — esta inflava o valor (mostrava 1.872,50 para
     1.452,50 vendidos, porque o dinheiro entregue inclui o troco).
     `totalRecebidoPorForma()` mantém a soma por forma.
  3. **DATAS EM HORA LOCAL** (migration `0009`): o SQLite grava `datetime('now')`
     em UTC; em UTC-3 toda venda após 21h caía no DIA SEGUINTE nos relatórios
     (venda de 25/08 23:16 aparecia em 26/08). Todos os INSERTs relevantes gravam
     `datetime('now','localtime')` e as consultas comparam com
     `date('now','localtime')`. A migration converte o histórico.
  4. **Compra: escolher "Caixa" não aplicava o fator** — `embList` era array de
     objetos dentro de ListModel (mesma armadilha dos insumos) e `[i]` dava
     undefined. Agora `embListJson` + `JSON.parse`. **REGRA: nunca guardar array
     de objetos em ListModel — serializar para JSON.**
  Cobertos por `tst_venda_repository::excedenteEmPixNaoViraTroco` e asserts novos
  em `tst_caixa_repository`.
### Aviso no celular via Telegram (feito)

Substitui a dependência de "ir olhar o OneDrive": ao fechar o caixa o resumo do
dia é ENVIADO como notificação para o celular dos donos.
- `src/services/telegram/TelegramService`: `sendMessage` (HTML) e `sendDocument`
  (anexa o relatorio.html) via `QNetworkAccessManager` — assíncrono, nunca trava
  a UI nem faz o fechamento falhar. Sem servidor próprio e sem custo.
- **Token e chat_id ficam em `QSettings` (máquina local), NUNCA no repositório** —
  são credenciais do dono; ele cola na tela de Backup.
- `RelatorioMobileService::resumoTexto()` monta a mensagem; `coletarDados()` foi
  extraído para alimentar tanto o HTML quanto o resumo (fonte única).
- `AppBackend`: `configTelegram/salvarConfigTelegram/testarTelegram` + sinal
  `telegramResultado`; envio automático dentro de `fecharCaixa`.
- UI: painel "Aviso no celular (Telegram)" na tela de Backup (só Admin), com
  botão de teste.
- OneDrive segue ativo como reforço (relatório HTML completo).

- **Importador automático de NF-e (XML): PENDENTE** — o dono só tem DANFE em papel/PDF
  hoje. Combinado: construir o leitor de XML (parse fornecedor+itens via `QXmlStreamReader`,
  casar por código de barras, revisar e reaproveitar `registrarCompra`) **quando houver um
  XML real de exemplo** para validar. OCR de PDF foi descartado (frágil p/ dados financeiros).
- **Aprendizado:** role de model chamado `data` colide com a propriedade padrão
  `data` de todo Item QML — renomeado para `dataCompra`. (Evitar roles `data`.)

**Fase 2.2 — Clientes + fiado (feita):**

- `src/domain/clientes/ClienteRepository`: CRUD, `saldoDevedor` (soma de contas a
  receber abertas) e `quitar` (baixa a dívida). 
- `VendaRepository`: venda no **fiado** valida **limite** (saldo + fiado ≤
  limite; limite 0 = sem crédito) antes de gravar.
- `AppBackend`: model `clientes` + CRUD + `clientesLista` + `quitarCliente`.
- PDV: seletor de **cliente** (F8) + botão **Fiado** (só com cliente); o fiado
  gera conta a receber. `ClientesScreen`: CRUD com saldo e "Quitar dívida".
- Coberto por `tst_cliente_repository` (limite dentro/fora, sem limite, quitar).

**Fase 2.3 — Financeiro (feita):**

- `src/domain/financeiro/FinanceiroRepository`: lista contas a pagar/receber
  (só abertas ou todas), `pagar`/`receber` (baixa), `criarDespesa` (conta a pagar
  avulsa) e `resumo` (total a pagar/receber aberto, saldo previsto). "Vencida" é
  derivada (aberta e vencimento < hoje), não é status armazenado.
- `AppBackend`: models `contasPagar`/`contasReceber` + `recarregarFinanceiro`,
  `resumoFinanceiro`, `pagarConta`, `receberConta`, `criarDespesa`.
- `FinanceiroScreen`: cartões de resumo + abas A pagar/A receber com baixa +
  "Nova despesa". Coberto por `tst_financeiro_repository`.

**Fase 2.4 — Relatórios + Dashboard (feita):**

- `src/domain/relatorios/RelatorioRepository`: KPIs do dashboard, faturamento/
  lucro por período, vendas por forma de pagamento, mais vendidos e produtos
  parados. Coberto por `tst_relatorio_repository`.
- **Custo/lucro travado no momento da venda (COGS):** migration `0006` adiciona
  `movimentacoes_estoque.custo_unit`; `VendaRepository` grava nele o custo médio
  vigente na saída. O lucro usa `COALESCE(m.custo_unit, e.custo_medio_unitario)`
  (fallback p/ linhas antigas), então compras futuras não recalculam o lucro de
  vendas passadas. Coberto por `tst_relatorio_repository::lucroTravadoNoMomentoDaVenda`.
- `AppBackend`: `dashboard`, `relatorioFaturamento/Formas/MaisVendidos/ProdutosParados`.
- `DashboardScreen` ligado a dados reais (KPIs + mais vendidos + financeiro).
- `RelatoriosScreen`: seletor de período (Hoje/7/30 dias) + cartões (faturamento,
  lucro, ticket, nº vendas) + painéis (por forma, mais vendidos, parados).

## ✅ PROJETO COMPLETO — todas as fases do plano implementadas

Fase 1 (núcleo): Produtos, Estoque, PDV, Fechamento de caixa, Login/permissões.
Fase 2 (retaguarda): Compras/fornecedores, Clientes/fiado, Financeiro, Relatórios.
**13 testes** cobrindo as partes críticas. App compila limpo e roda (Qt 6.8.3 MinGW).

Fora do escopo original (fase posterior, se desejado): delivery/WhatsApp, backup
na nuvem, empacotamento/instalador Windows (`deploy/`), integração TEF real.

### Backup e restauração (feito)

Ver `docs/plano-backup.md`. `src/services/backup/BackupService`:
- **Cópia** com `VACUUM INTO` (snapshot íntegro, com o banco aberto/WAL, sem travar).
- Pasta: `Documentos/Empório dos Amigos/Backups/`; cada `.db` tem um sidecar `.json`
  com metadados (data, contagens) para a lista.
- **Automático ao fechar o caixa** (`AppBackend::fecharCaixa`, melhor esforço) +
  **retenção 5** (na 6ª, apaga a mais antiga).
- **Restauração** segura em 2 tempos: `agendarRestauracao` faz backup de emergência +
  grava marcador `<db>.restore`; `main.cpp` chama `aplicarRestauracaoPendente()`
  ANTES de abrir o banco, troca o arquivo e as migrations sobem o schema se preciso.
- UI: `qml/screens/config/BackupScreen.qml` (rota `backup`, sidebar só Admin via
  `gerencia_usuarios`): status, "Fazer backup agora", lista com "Restaurar".
- Cópia externa (pen drive/OneDrive) ficou **fora** por escolha do dono (futuro).
- Coberto por `tst_backup_service` (cópia íntegra, retenção 5, round-trip de
  restauração).

### Ver no celular — relatório HTML no OneDrive (feito)

Ver `docs/plano-mobile.md`. Decisão do dono: acesso remoto + atualização periódica,
sem servidor. `src/services/relatoriomobile/RelatorioMobileService`:
- Gera um **HTML único autossuficiente** (CSS/JS/dados embutidos; `<` escapado no
  JSON contra quebra de `</script>`) e grava em `%OneDrive%/Empório dos Amigos/
  Relatório/relatorio.html` (fallback Documentos). O dono abre pelo app do OneDrive
  no celular (privado, só leitura).
- Conteúdo: resumo Hoje/7/30 (faturamento, lucro, nº vendas, ticket), formas de
  pagamento, mais vendidos, estoque (baixos em destaque) e fiado a receber.
- **SEM aba própria** (decisão do dono): é 100% automático, sempre **junto com o
  backup** — no `fecharCaixa` e também no botão "Fazer backup agora". A tela de
  Backup só mostra uma linha com "atualizado em / pasta".
- **Relatório completo**: além do resumo por período (Hoje/7/30 com faturamento,
  lucro, nº vendas, ticket), traz formas de pagamento, mais vendidos, **último
  fechamento de caixa** (esperado × contado × diferença + composição), **estoque**
  com valor imobilizado e itens em falta, **fiado a receber**, **contas a pagar**
  (vencidas destacadas), **últimas compras** e **produtos parados**.
- `AppBackend::gerarRelatorioCelular/statusRelatorioCelular` (sem UI dedicada).
- Descartado Vercel/nuvem pública (exporia finanças); Cloudflare Pages+Access fica
  como evolução futura para "URL com login".
- Coberto por `tst_relatorio_mobile`. **16 executáveis de teste no CTest.**

### Produto composto / "copão" — receita por CATEGORIA (feito)

Um produto de venda (copão/drink/shot) que, ao ser vendido, baixa do estoque os
insumos, não a si mesmo. **A receita é por CATEGORIA** (ex.: Destilados em ml,
Gelo em unidade, Energético em unidade), e o **produto específico é escolhido na
hora da venda** quando a categoria tem mais de um produto. Só baixa os insumos
(sem estoque próprio).

- Migrations `0003` (coluna `produtos.composto`) e `0004` (recria
  `produto_composicao` como `categoria_id + unidade + quantidade`).
- `ProdutoRepository`: carrega/salva a composição por categoria;
  `produtosDaCategoria(catId)` (produtos não compostos da categoria, p/ escolha).
- `VendaRepository`: `LinhaVenda.insumos` (os produtos escolhidos na venda +
  quantidade); ao vender, baixa `qtd_vendida * quantidade_receita` de cada insumo
  e registra as movimentações. Erro se um composto vier sem insumos resolvidos.
- **Armadilha (resolvida):** o `ListModel` do carrinho no PDV NÃO preserva arrays
  de objetos (os insumos se perdiam → o C++ recebia vazio → "Escolha os insumos"
  mesmo já escolhidos). Os insumos do carrinho são guardados como **string JSON**
  na role `insumosJson` (`JSON.stringify`/`JSON.parse` na hora de finalizar).
- Compostos **não exigem estoque** para vender: ficam fora do aviso de estoque do
  PDV (a linha com `insumosJson !== "[]"` é ignorada em `_conferirEstoque`).
- `AppBackend.composicaoParaVenda(produtoId)`: linhas com categoria/unidade/
  quantidade + os produtos de cada categoria (para o PDV montar).
- **Custo/lucro:** calculado pelas movimentações reais de saída (o produto exato
  baixado), valendo p/ normal e composto.
- Compostos ficam fora das telas de estoque e do alerta "produtos em falta".
- UI: editor com **categoria + quantidade + unidade** (unidade/ml/litro/g/kg);
  no PDV, ao adicionar um composto, **sempre abre a tela de montagem** para
  escolher/confirmar o produto exato de cada categoria (combo por categoria;
  categoria sem produto fica em vermelho e bloqueia). O carrinho mostra os
  insumos escolhidos.
- Cada produto escolhe sua **unidade base** (atalhos unidade/ml/litro/g/kg).
- Coberto por `tst_composicao` (categoria, escolha na venda, ml+unidade, custo).

**Fase 2.4 — Ajustes de contabilidade/praticidade (feita):**
- Migration `0005_recebimento_caixa.sql`: `mov_caixa.tipo` passa a aceitar
  `'recebimento'` (recria a tabela; nada a referencia, seguro com FK on).
- **Recebimento de fiado entra no caixa:** ao receber fiado **em dinheiro** com
  caixa aberto, lança um `mov_caixa` tipo `recebimento`; `ResumoCaixa` ganhou
  `recebimentos` e `dinheiroEsperado()` passou a somá-lo (fiado recebido converte
  recebível em dinheiro na gaveta, sem duplicar a venda original).
- **Pagamento parcial de fiado (FIFO):** `ClienteRepository::aplicarRecebimento`
  (abate contas da mais antiga p/ a nova; última coberta em parte tem `valor`
  reduzido) e `FinanceiroRepository::aplicarRecebimentoConta` (uma conta). Ambos
  NÃO abrem transação — `AppBackend` envolve a baixa + o lançamento no caixa numa
  transação só (`receberDeCliente` / `receberContaValor`, forma dinheiro/pix/…).
- UI: Clientes tem **"Receber pagamento"** (valor + forma) e Financeiro→A receber
  idem por conta; o fechamento de caixa mostra "Recebimentos de fiado".
- **Aviso de estoque insuficiente no PDV** (não bloqueia): `estoqueDisponivel` +
  banner que soma a necessidade de cada produto/insumo do carrinho vs. saldo.
- Testes: casos novos em `tst_caixa` (recebimento na gaveta), `tst_cliente`
  (recebimento parcial FIFO) e `tst_financeiro` (parcial por conta). Seguem
  **14 executáveis** no CTest, todos verdes.
