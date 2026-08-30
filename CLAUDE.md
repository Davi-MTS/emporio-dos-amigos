# Sistema de Gestão — Empório dos Amigos

Sistema de gestão (ERP) para uma distribuidora de bebidas: estoque, PDV,
financeiro, compras, clientes (fiado) e relatórios. **Loja única.**

**Identidade:** Empório dos Amigos — Bebidas & Conveniência · 062. Cor primária
**preto**, secundária **laranja**; visual premium e sóbrio. Logo em
`resources/images/logo.png` (embutida como `:/images/logo.png`, usada no login e
na sidebar). Ver `docs/design-ui.md` e `docs/mockup-ui.html`.

## Estado atual (resumo)

> Sistema **completo e empacotado**. O histórico detalhado de cada fase está no
> fim deste arquivo; leia esta seção primeiro.

| | |
| --- | --- |
| Telas | Dashboard, PDV, **Caixa**, Produtos, Estoque, **Vencimento**, Vendas, Compras, Clientes, Financeiro, Relatórios, Usuários, Backup |
| Testes | **21 executáveis** no CTest, todos verdes (20 de regra + `tst_qml`, com 56 casos de interface) |
| Migrations | **0001–0015** aplicadas |
| Entrega | `deploy/empacotar.ps1` → pasta autossuficiente + zip (~26 MB), sem console |
| Repositório | `github.com/Davi-MTS/emporio-dos-amigos` (público; pacote pronto versionado em `deploy/pacote/`) |

**Fora do PDV/estoque, o que existe:** produto composto (copão) com receita por
categoria; **dose como produto próprio ligado à garrafa**; **foto do produto**
(no banco, reduzida); **validade por remessa** com FEFO; cancelamento de venda
com devolução de estoque/fiado/dinheiro; **estorno de pagamento de conta**;
backup automático (5 cópias) e restauração; relatório HTML **e a cópia do
banco** enviados por **Telegram** ao fechar o caixa; registro do sistema em
arquivo.

**O que NÃO existe (decidido ou pendente):** emissão de NF-e, impressão de
cupom, integração TEF/maquininha, delivery/WhatsApp, multi-PC; importador de
NF-e por XML (aguardando um XML real de exemplo); desconto em %.

**Instalação na loja:** ver `docs/instalacao.md`.

## Stack e plataforma (decidido)

- **Plataforma:** aplicativo desktop nativo (não é web).
- **Framework:** Qt 6 + QML.
- **Linguagem do backend:** C++.
- **Banco de dados:** SQLite (local, embutido, offline-first — o app precisa
  vender mesmo sem internet).
- **Sistema operacional alvo:** Windows (PC do caixa).
- **Backup:** local (arquivo SQLite), automático ao fechar o caixa. Sem
  sincronização em nuvem, mas a cópia **sai do PC**: o `.db` vai anexado na
  mensagem do Telegram (opção "Enviar também a cópia de segurança", ligada por
  padrão). Sem isso, um HD queimado levaria junto o histórico da loja.

## Princípios de arquitetura

- Separar **lógica de negócio** (`src/domain/`) da **interface** (`qml/`).
  As regras de negócio devem ser testáveis isoladamente, sem depender da UI.
- As partes mais críticas e que NÃO podem ter erro silencioso:
  **conversão de embalagem** e **fechamento de caixa** — cobrir com testes
  em `tests/cpp/`.
- Integração com hardware fica **plugável** e isolada em `src/services/`: o
  resto do sistema não depende dela. Hoje não existe nenhuma — o leitor de
  código de barras funciona como teclado (não precisa de código) e a maquininha
  (TEF) está fora do escopo. A pasta vazia que reservava esse lugar foi
  removida; quando houver integração de verdade, ela nasce aqui.
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

- **Nota fiscal:** a EMISSÃO está fora do escopo (feita por fora). O sistema
  apenas REGISTRA o nº/data da nota na compra (migration `0008`).
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

## Histórico de implementação (fase a fase)

> Registro de como o sistema foi construído e **por que** cada decisão foi
> tomada. Os números citados (ex.: "4/4 testes") são do momento de cada fase.

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
- O HTML completo vai ANEXADO na mensagem (não há mais canal separado).

### Histórico de vendas e cancelamento (feito)

- Migration `0010`: `contas_receber.status` aceita `'cancelada'`; `vendas` ganha
  `cancelada_em` e `motivo_cancelamento`.
- `VendaRepository::cancelarVenda(vendaId, motivo, usuarioId, sessaoAbertaId)`:
  devolve ao estoque **lendo as movimentações da venda** (pega o produto real,
  inclusive insumos de composto) com tipo `devolucao`; cancela a conta de fiado;
  marca a venda como `cancelada` — **nunca apaga** (auditoria).
  **Dinheiro:** na MESMA sessão o esperado cai sozinho (o resumo só conta vendas
  `concluida`); em sessão JÁ FECHADA registra **sangria** (`dinheiro - troco`),
  senão a gaveta de hoje não bate.
- `VendaRepository::listar(dias)` / `itens(vendaId)` + `VendasListModel`.
- `AppBackend`: `recarregarVendas/itensDaVenda/cancelarVenda` — cancelamento
  exige permissão **`pode_cancelar_venda`** (já existia no seed) e motivo.
- UI: `qml/screens/vendas/VendasScreen.qml` (rota `vendas`, em OPERAÇÃO) com
  período, detalhe da venda e cancelamento; canceladas ficam riscadas.
- Coberto por `tst_venda_repository::cancelamentoDevolveEstoqueEFiado` e
  `::cancelamentoSaiDoCaixaEDoHistorico`.

### Registro do sistema — log em arquivo (feito)

Sem console no build de produção, um erro não deixaria rastro nenhum na loja.
`src/services/log/LogService`:
- `instalar()` no `main.cpp` (logo após nome/versão) captura tudo que passa por
  `qDebug/qWarning/qCritical/qFatal` — **inclusive os erros de QML** — e delega
  ao handler anterior (console segue funcionando em Debug).
- Arquivo: `<AppDataLocation>/logs/sistema.log`, **com BOM UTF-8** (sem ele o
  Bloco de Notas antigo mostra acento trocado — e é o dono que vai abrir).
- **Rotação** aos 2 MB, mantendo `sistema.1.log`..`.3.log`. Mutex (o Qt loga de
  várias threads). Filtra o ruído do QFontDatabase.
- `registrar()` grava eventos do negócio: **cancelamento de venda** (quem, qual,
  motivo) e **fechamento de caixa** (esperado × contado × diferença).
- UI: painel "Registro do sistema" na tela de Backup — últimas 80 linhas
  coloridas por nível + botão "Abrir pasta" (`Qt.openUrlExternally`, evita
  depender de QtGui no Core). `AppBackend::statusLog/ultimasLinhasLog`.

### Empacotamento para a loja (feito)

`deploy/empacotar.ps1` gera pasta autossuficiente + zip (~26 MB): compila em
Release, roda `windeployqt --qmldir qml`, copia o runtime do MinGW e escreve um
LEIA-ME. **Verificado rodando com PATH limpo (sem Qt instalado).**
Nota PS 5.1: stderr de .exe vira erro — o script usa `Exec{}` conferindo
`$LASTEXITCODE` em vez de `ErrorActionPreference=Stop`.
Inno Setup não está instalado; o pacote é "copiar a pasta e criar atalho".
**Sem janela de console:** `WIN32_EXECUTABLE $<CONFIG:Release>` em
`qml/CMakeLists.txt` — o exe era `Windows CUI` e abria um cmd preto junto (se o
operador fechasse, matava o sistema no meio da venda). Debug segue CUI.

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

### Ver no celular — relatório HTML (feito; entregue via Telegram)

Ver `docs/plano-mobile.md`. Decisão do dono: acesso remoto + atualização periódica,
sem servidor. `src/services/relatoriomobile/RelatorioMobileService`:
- Gera um **HTML único autossuficiente** (CSS/JS/dados embutidos; `<` escapado no
  JSON contra quebra de `</script>`) numa **pasta LOCAL do app**
  (`AppDataLocation/Relatorio/relatorio.html`).
- **OneDrive foi REMOVIDO como canal de entrega** (decisão do dono): dependia de o
  dono lembrar de olhar a pasta, com a conta certa, e falhava em silêncio quando a
  sincronização do PC estava parada — foi o que aconteceu no teste real. A entrega
  agora é **só pelo Telegram**, que anexa este HTML na mensagem.
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

### Riscos de operação — cópia fora do PC e testes de interface (feito)

Dois riscos que sobreviviam a tudo: o backup nunca saía do computador e nenhum
teste abria uma tela.

**1. A cópia do banco sai do PC.** `TelegramService` ganhou `enviaBackup()`
(`QSettings` `telegram/enviaBackup`, **ligada por padrão**) e `salvarConfig` passou
a receber esse quarto parâmetro. No `AppBackend::fecharCaixa` e no botão "Fazer
backup agora", depois do resumo e do relatório HTML, o `.db` do backup vai
anexado (`enviarArquivo`) com legenda "Backup do sistema — <resumo>", e a linha
fica no log. `enviarArquivo` recusa acima de **45 MB** (limite do Bot API) com
mensagem clara em vez de erro de rede. A tela de Backup tem o botão e, quando
ele está desligado, um aviso de que a cópia fica só neste computador.

**2. Testes de interface (`tst_qml`).** O módulo QML saiu do executável e virou a
biblioteca **`distribuidora_ui`** (+ `distribuidora_uiplugin`); o `distribuidora`
passa a linkar as duas. Sem isso os `.qml` ficavam presos dentro do `.exe` e
nenhum outro binário conseguia abrir as telas. `tests/qml/arnes_qml.cpp`
(`QUICK_TEST_MAIN_WITH_SETUP`) sobe um `AppBackend` real sobre banco em
`QTemporaryDir`, com `QStandardPaths` em modo de teste, cria e loga um admin, e
registra o contexto `App` — as telas rodam sem saber que estão num teste. Casos
em `tests/qml/casos/`:
- `tst_telas.qml` — as 12 telas abrem em 1160×700 e em 760×560 (janela
  restaurada), sem aviso de QML, e nada pode começar fora da largura da tela;
- `tst_pdv.qml` — total, agrupamento, composto sem estoque e a receita
  sobrevivendo ao `ListModel` (o defeito que travava a venda);
- `tst_componentes.qml` — diálogo nunca maior que a janela, toggle, segmented,
  campo que não corta a data/valor digitado;
- `tst_backup.qml` — o botão de enviar a cópia existe, reflete o que está salvo
  e o aviso aparece quando está desligado.

**Bug real que esses testes acharam:** em `RelatoriosScreen`, o título do painel
("Vendas por forma de pagamento") virava a **largura mínima** do painel; com a
janela restaurada o painel encolhia abaixo disso e a lista de dentro ficava 45 px
mais larga que o cartão, invadindo o painel vizinho. Corrigido com
`Layout.fillWidth` + `Layout.minimumWidth: 0` + `elide` no título.

Nota de arnês: `PdvScreen` expõe `linhaCarrinho(i)`/`itensNoCarrinho()` porque o
`ListModel` do carrinho é interno e o teste não o enxerga.

Ficam **17 executáveis** no CTest (`tst_qml` roda em `offscreen`, ~68 s).

### Perfil "Funcionário" — permissões estruturadas (feito)

Regra: **toda chave declarada no perfil é lida em algum lugar do sistema.**
Antes havia chave decorativa — o perfil dizia `pode_dar_desconto: false` e o
funcionário dava desconto à vontade, porque ninguém lia a chave. `edita_preco`
era do mesmo tipo e foi removida (preço faz parte de `edita_produto`).

Migration `0011_perfil_funcionario.sql` (UPDATE no perfil 2, para bancos que já
existem) + o mesmo JSON no seed (para banco novo — migrations rodam **antes** do
seed, com `perfis` ainda vazia, então os dois precisam estar iguais).

| Chave | Vale para | Onde é aplicada |
| --- | --- | --- |
| `vende` | ✅ | rota `pdv` na Sidebar |
| `consulta_produtos` | ✅ | rota `produtos` na Sidebar (só consulta) |
| `recebe_mercadoria` | ✅ | aba Entrada + `AppBackend::registrarEntrada` |
| `atende_cliente` | ✅ | rota `clientes` na Sidebar |
| `edita_produto` | ❌ | `ProdutosScreen.podeEditar` + `salvarProduto` / `inativarProduto` |
| `pode_dar_desconto` | ❌ | campo F4 + atalho no PDV + `finalizarVenda` (recusa a venda) |
| `ajusta_estoque` | ❌ | abas Inventário/Retirada + `registrarInventario` / `registrarRetirada` |
| `ve_relatorios` | ❌ | rota `relatorios` na Sidebar |
| `ve_financeiro` | ❌ | rotas `compras`/`financeiro` + painel Financeiro do Dashboard |
| `pode_cancelar_venda` | ❌ | `VendasScreen` + `cancelarVenda` |
| `gerencia_usuarios` | ❌ | rotas `usuarios`/`backup` na Sidebar |

Administrador continua com `{"tudo": true}`, que atropela qualquer chave
(`temPermissao` devolve `true` de saída).

**Trava na tela E no backend.** A tela é conveniência (não mostra o que não dá
para usar); a recusa que vale está no `AppBackend`. Antes só `cancelarVenda`
fazia isso. No desconto a recusa é **explícita** — a venda não passa, em vez de
passar com o desconto zerado em silêncio, que faria o operador cobrar errado
sem entender o motivo.

Decisões de escopo: Entrada de mercadoria fica **liberada** (é balcão, o
funcionário recebe carga); Inventário e Retirada não, porque reescrevem saldo
sem nota — é por onde mercadoria some sem rastro. No `EstoqueScreen` as abas
restritas são as **últimas** da lista de propósito: quando somem, os índices das
que sobram continuam válidos.

Coberto por `tests/qml/casos/tst_permissoes.qml`: entra de fato como
funcionário e confere as chaves, as telas, a Sidebar e a recusa do backend
(inclusive forçando um desconto por fora da tela). São **52 testes** no `tst_qml`.

### Retorno da loja — 13 ajustes de uso (feito)

Lista trazida pelo dono depois de operar o sistema. Agrupados em fases; cada
uma foi commitada e testada separadamente.

**Atrito diário.**
- **Botão ＋ ao lado da categoria** no cadastro (`ProdutoRepository::criarCategoria`,
  reaproveita nome existente sem diferenciar maiúsculas). Antes só existiam as 12
  do seed: chegando um produto que não se encaixa, o cadastro parava.
- **Cartões do Dashboard viraram atalho**: "Produtos em falta" → Estoque,
  "A receber (fiado)" → Clientes. `Main.qml` ganhou `irPara(rota)`, que também
  acende o item certo na barra lateral (as telas pedem por um `signal navegar`).
- **"Restaurar de um arquivo…"** na tela de Backup — a lista só enxergava a pasta
  de backups, então a cópia vinda do Telegram não tinha como ser restaurada,
  justo o caso de HD queimado. Junto veio `BackupService::validarArquivoBackup`
  (SQLite íntegro + tabelas deste sistema + versão não mais nova): sem isso,
  escolher por engano o `relatorio.html` trocaria o banco por lixo.
- **PDV: pagamento não fica mais preso.** A tela guarda o total de quando o
  pagamento foi lançado; se o carrinho muda, os pagamentos saem sozinhos com
  aviso, em vez de exigir caçar o ✕.
- **Aba própria do Caixa** (`CaixaScreen`, rota `caixa`, permissão `vende`).
  Abertura, sangria, suprimento e fechamento moravam numa barra de 46 px dentro
  do PDV; o caixa é a prestação de contas do dia e precisa de espaço para
  conferir. O PDV ficou só com a venda (−200 linhas).

**Dinheiro.**
- **Financeiro explícito:** botões (Gaveta/Pix/Débito/Crédito) no lugar do combo
  e, ao vivo, de onde o dinheiro sai, quanto tem na gaveta **agora** e quanto
  fica **depois** (`AppBackend::efeitoDoPagamento`). Alerta quando a gaveta
  ficaria negativa e quando o **caixa está fechado** — caso em que a conta é
  quitada mas a saída não entra na conferência de turno nenhum (acontecia calado).
- **Estorno de pagamento** (despesas e compras): migration `0012` grava
  `contas_pagar.forma_pagamento`; `estornarPagamento` reabre a conta e devolve o
  dinheiro à gaveta como **suprimento**, na mesma transação. Pagamento de outro
  dia entra no caixa de hoje, com o motivo escrito — não reabre turno fechado.
  A lista ganhou o filtro **"Mostrar já pagas"**, sem o qual a conta paga por
  engano sumia da tela.
- **Fiado na tela de Clientes:** faixa com o que está na rua, o **atrasado**, o
  maior devedor e quantos passaram do limite; no cliente aberto, quanto ainda
  cabe no limite, última compra × último pagamento e a conta mais antiga em
  aberto (`ClienteRepository::resumoFiado` / `historicoFiado`).
- **Cancelamento achável:** a lógica já existia, ninguém encontrava. O botão
  saiu de dentro do detalhe e foi para a **linha** da venda, e o aviso de venda
  concluída no PDV ganhou **"Errei — cancelar esta venda"**. `AppButton` ganhou
  o kind `perigo`.

**Cadastro e venda.**
- **Dose como produto próprio** (migration `0013`): `dose_de_produto_id` +
  `dose_quantidade`. Vende com um bipe; não tem estoque próprio (não aparece no
  Estoque); `estoqueDisponivel` = estoque da garrafa ÷ quantidade da dose; a
  venda baixa a garrafa. `VendaRepository` passou a baixar por insumos sempre
  que a linha tiver insumos (antes só quando o produto fosse composto).
- **Unidade base explicada:** a escolha (unidade/ml/litro/g/kg) agora mostra o
  que significa, com exemplo; o cabeçalho do fator diz a unidade do produto
  ("Fator (ml)"). Era o ponto que confundia na entrada de estoque.

**Novidades.**
- **Foto do produto** (migration `0014`): guardada **no banco** — o backup é um
  arquivo só e é ele que sai pelo Telegram; em pasta, restaurar perderia as
  imagens. Reduzida a 320 px/JPEG (20–30 KB). `ProdutoFotoProvider`
  (`image://produto/<id>?v=App.versaoFotos`, o `?v=` fura o cache do Qt) vive na
  biblioteca de interface. Miniatura em Produtos, Estoque, sugestões do PDV e
  carrinho; sem foto, mostra a inicial do nome.
- **Vencimento por remessa** (migration `0015`, tabela `lotes` que existia sem
  uso): validade opcional na entrada, saída consome o lote que vence primeiro
  (**FEFO**) na venda e na retirada, lote zerado é apagado, e `divergencias()`
  aponta quando estoque e lotes não batem (entrada sem validade ou ajuste de
  inventário) em vez de fingir que fecham. Aba **Vencimento** com cartões,
  filtro e o prazo em português.

**Testes novos:** `tst_estorno`, `tst_dose`, `tst_foto_produto`, `tst_lotes`,
mais Caixa e Vencimento no `tst_telas`. **21 executáveis, 56 casos de QML.**

**Regressão pega pelos testes de tela:** a linha de filtros nova do Financeiro
não cabia na janela restaurada e empurrava a lista 79 px para fora — virou
`Flow`, que quebra a linha em vez de estourar.
