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
| Testes | **28 executáveis** no CTest, todos verdes: 327 casos de regra + `tst_qml` (156 casos de interface) |
| Migrations | **0001–0018** aplicadas |
| Entrega | `deploy/empacotar.ps1` → pasta autossuficiente + zip, sem console e sem os extras do Qt |
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
- **O fator vem SEMPRE do cadastro**, nunca da tela: compra, venda, entrada e
  retirada recebem a embalagem escolhida e leem o fator de `produto_embalagens`
  (`ProdutoRepository::fatorDaEmbalagem`). Embalagem de outro produto é recusada.
- **O cadastro confere o fator** (`ProdutoRepository::salvarEmbalagens`): fator
  0 ("não informado", como nasce a linha nova) é recusado, e duas embalagens com
  o **mesmo fator e preços diferentes** também — foi assim que a loja vendeu
  caixinha baixando 1 lata. Mesmo fator e mesmo preço é aceito (dois códigos
  de barras para a mesma unidade).
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
| `ve_financeiro` | ❌ | rotas `compras`/`financeiro` + painel Financeiro do Dashboard + **coluna Margem do Estoque** (na tela, no model e no `itemEstoque`) |
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

### Fotos em lote — a fila de atribuição (feito)

Pôr foto em produto existia desde a migration `0014`, mas só de um jeito: abrir
o produto no cadastro, achar o botão, navegar até a pasta, escolher um arquivo,
fechar. Para os ~200 produtos da loja era inviável, e por isso **nenhum produto
tinha foto**.

O gargalo nunca foi escolher o arquivo — é dizer a QUAL produto ele pertence.
Então o desenho separa **transporte** de **atribuição**:

- **Transporte:** `FileDialog` com seleção múltipla, `FolderDialog` +
  `FolderListModel` (pasta inteira de uma vez) e `DropArea` (arrastar do
  Explorer). As três somam à mesma fila, em vez de trocá-la.
- **Atribuição:** `qml/screens/produtos/FotosEmLoteDialog.qml` — a foto aparece
  grande e um campo único recebe o **código de barras** (o leitor digita e dá
  Enter: `buscarProdutoPorCodigo`, atribui e anda) ou **o nome**
  (`buscarProdutosPorNome`, até 8 candidatos). Atribuiu → grava → anda → limpa o
  campo → devolve o foco. Botão "Fotos em lote" no cabeçalho de Produtos.

Detalhes que não são enfeite:
- Candidato que **já tem foto** vem marcado, e substituir pede confirmação: sem
  isso dá para sobrescrever a foto certa de um produto sem perceber.
- **Desfazer** remove a foto recém-atribuída e volta uma casa. Depois de uma
  SUBSTITUIÇÃO ele fica **desligado** — devolver a foto antiga exigiria tê-la
  guardado, e não guardamos; melhor desligado do que mentindo.
- Filtro **"Sem foto (n)"** na lista de Produtos (`mostrarApenasSemFoto`,
  `contarProdutosSemFoto`): a lista vira a lista do que falta e encurta sozinha.
  O flag é do backend e vale para o model inteiro, então a tela o **desliga no
  `Component.onDestruction`** — senão a próxima visita esconderia produtos sem
  nada na tela explicando por quê.
- **"Colar imagem"** (`colarFotoProduto`, via `QClipboard`) no editor do
  produto: muita foto se pega da internet, e copiar/colar é mais rápido do que
  salvar arquivo e procurar na pasta.

**BUG REAL, achado ao auditar antes de construir:** `temFoto` era **sempre
false**. `ProdutoRepository::listar` lia `value(14)` numa consulta de 14 colunas
(0..13) e `obter` lia `value(17)` numa de 17 — nenhuma das duas selecionava a
coluna `foto`. Como o editor pergunta ao banco por outro caminho
(`produtoTemFoto`), a foto aparecia lá dentro **e em nenhum outro lugar**:
lista de produtos, sugestões do PDV e carrinho mostravam a inicial do nome para
sempre. Só não tinha incomodado ninguém porque nenhum produto tinha foto ainda.
Coberto por `tst_foto_produto::listaEnxergaQuemTemFoto` (verificado: falha com a
consulta antiga, passa com a nova).

**Formatos:** o filtro do seletor oferecia `*.webp`, que este Qt não abre. Os
plugins do pacote são só `qgif/qico/qjpeg/qsvg`; medido em execução, o que entra
é `bmp gif ico jpeg jpg png ppm svg xbm xpm` — **sem webp, sem tiff, sem heic**.
O filtro passou a listar só o que funciona, e arquivo `.heic`/`.heif` agora
recebe a saída prática ("Ajustes → Câmera → Formatos → Mais Compatível") em vez
do erro genérico.

**Regressão pega pelo `tst_telas`:** com o filtro e o botão novos, a barra de
Produtos passou a ter quatro controles e na janela restaurada o último começava
19 px fora da tela. Virou `Flow` (mesma correção do Financeiro).

**Fora do escopo por decisão do dono:** receber as fotos pelo Telegram (o bot já
existe e converteria HEIC sozinho) e girar a foto antes de atribuir.

**Testes novos:** `tests/qml/casos/tst_fotos.qml` (14 casos: a fila anda, pular
não grava, desfazer volta, bipe atribui direto, "já tem foto" avisa, o botão da
tela abre a fila, cabe em 760×560) e quatro casos em `tst_foto_produto`.
**23 executáveis, 70 casos de QML.**

**Regressão pega pelos testes de tela:** a linha de filtros nova do Financeiro
não cabia na janela restaurada e empurrava a lista 79 px para fora — virou
`Flow`, que quebra a linha em vez de estourar.

### Dois bugs de produção, relatados da loja (feito)

**1. O sistema FECHAVA INTEIRO ao pôr foto num produto.** Regressão introduzida
pela correção do `temFoto` (commit `40e440c`): com a lista enxergando as fotos,
cada linha passou a pedir miniatura. `FotoProduto.qml` usa `asynchronous: true`,
e com isso o Qt chama `ProdutoFotoProvider::requestImage` numa **thread
separada** (documentação do `QQuickImageProvider`: *"may be called by multiple
threads"*). O provider usava a conexão de banco da thread principal — e uma
conexão do Qt SQL só pode ser usada pela thread que a criou. Ao gravar a foto, a
lista recarregava na principal enquanto as miniaturas eram lidas na outra: as
duas mexiam juntas no driver e o processo caía (segfault, sem mensagem).
- **Correção:** o provider não guarda conexão, só o caminho do arquivo; cada
  thread abre a sua, **somente leitura**, via `QThreadStorage` (o destrutor fecha
  e remove a conexão quando a thread termina). O banco está em WAL, então leitor
  e escritor convivem.
- **REGRA:** nada fora da thread principal usa `db.connection()`. Hoje o único
  código em outra thread é o image provider.
- Junto: na fila de fotos, o clique no candidato passou a ser `Qt.callLater` —
  `atribuir()` esvazia `candidatos` e destruía o delegate de dentro do próprio
  `onClicked`.
- Coberto por `tst_foto_concorrencia` (duas threads pedindo miniaturas enquanto a
  principal grava) e `tst_fotos.qml::test_gravar_fotos_com_a_lista_carregando_miniaturas`
  (a ProdutosScreen de verdade). **Verificado nos dois: o provider antigo cai 3 de
  3 (exit 139); o novo passa.**

**2. O lucro do Dashboard "caía de uma vez e depois voltava a subir".** O PDV
deixa vender sem estoque (só avisa), e numa loja começando o saldo fica negativo.
`EstoqueRepository::aplicarEntrada` calculava a média ponderada com `qtd_atual`
negativa: vendeu 20, comprou 24 a R$ 3,00 → divisor 4 → cada unidade passava a
"custar" **R$ 18,00**. As vendas seguintes gravavam esse custo (travado em
`movimentacoes_estoque.custo_unit`), davam prejuízo e o lucro despencava; a
compra seguinte diluía a média e ele voltava.
- **Correção:** com saldo ≤ 0, o novo custo é o custo da entrada — as unidades
  negativas já foram vendidas e não carregam custo. Saldo positivo segue a média
  ponderada de sempre. É o único lugar do sistema que recalcula o custo médio
  (compra e entrada manual passam por ele).
- Coberto por `tst_custo_estoque_negativo` (verificado: `1800000` antes,
  `300000` depois).
- **Limite:** corrige daqui para frente. Custos já gravados errados no banco da
  loja continuam lá, e as entradas manuais **não guardam o custo** na
  movimentação — então não dá para recalcular o histórico inteiro; só as
  compras (`compra_itens.custo_unit`) têm o custo de origem.

### Filtro do Estoque por situação (feito)

Barra de filtro na tela de Estoque: **Todos · Zerados · Baixo · OK**, cada botão
com a quantidade de produtos naquela situação ("Zerados 4").
- O filtro mora no **`EstoqueListModel`** (`Q_PROPERTY filtroStatus` +
  `contagem`), não na tela, por dois motivos: usa a MESMA regra do selo da coluna
  Status (`statusDe`), então filtro e selo nunca discordam; e sobrevive às
  recargas — dar entrada num produto "Zerado" recarrega a lista e ele sai do
  filtro sozinho. `setItens` guarda a lista inteira (`m_todos`) e filtra para
  `m_itens`; a contagem é sobre a lista inteira (respeita a busca por nome).
- Regra: zerado = saldo ≤ 0 (inclui **negativo**), baixo = até o mínimo, ok = acima.
- Valor desconhecido vira "todos"; a tela desliga o filtro no
  `Component.onDestruction` (o model é um só para o app).
- A mensagem de lista vazia passou a respeitar o contexto: com filtro ou busca
  ligados dizia "Nenhum produto cadastrado", o que faria o dono achar que perdeu
  o cadastro.
- Barra virou `Flow` (busca 260 + filtro 400 + dica cabem numa linha em 1280 px;
  na janela restaurada quebram a linha).
- Coberto por `tests/qml/casos/tst_estoque_filtro.qml` (contagem, filtro × selo,
  botão da tela, sobrevive à entrada, sair desliga, valor inválido).

### Relatório de um dia específico (feito)

Seletor de Relatórios: **Hoje · 7 dias · 30 dias · Outro dia**. "Outro dia" mostra
um botão com o dia ("seg., 14/09/2026") que abre um **calendário**
(`components/CalendarioPopup.qml`, sobre `MonthGrid` + `DayOfWeekRow` do Qt Quick
Controls, locale pt_BR). Abre em **ontem** — hoje já tem botão próprio — e não
deixa escolher dia futuro nem avançar para um mês inteiro à frente.
- **Datas por inteiros:** o `date` do `MonthGrid` pode vir em UTC e, em hora
  local (UTC−3), vira o dia anterior. O calendário monta o ISO com
  dia/mês/ano do delegate.
- **Dois defeitos pegos pela captura de tela, não pelos testes:** (1) o
  `MonthGrid` não informa a própria altura e as 6 linhas saíam sobrepostas —
  agora `Layout.preferredHeight`; (2) os dias de fora do mês estavam com
  `visible: false`, e a grade do Qt não reserva lugar para item invisível: o mês
  inteiro escorregava de coluna (14/09/2026, segunda, aparecia no sábado). Agora
  `opacity: 0`. Os dois viraram teste (`tst_relatorios_dia.qml`); o da coluna foi
  verificado falhando com o código antigo.
- `RelatorioRepository` ganhou `struct Periodo { int dias; QDate dia; }`: com
  `dia` válido o filtro vira `date(coluna) = 'yyyy-MM-dd'` (a data sai de
  `QDate`, nunca de texto digitado). As funções por `int dias` continuam como
  atalhos — Dashboard, relatório do celular e testes não mudaram.
- O filtro vale para receita (`vendas.data`) **e** custo
  (`movimentacoes_estoque.data`): com só um dos dois, o lucro do dia misturaria
  a receita de um dia com o custo de outro. Coberto por
  `tst_relatorio_repository::diaEspecifico`.
- `AppBackend::relatorio*Dia(isoDia)`; a conversão para QML foi fatorada em
  helpers únicos (`mapaFaturamento`, `listaFormas`...). A tela só recarrega com
  a data completa. Coberto por `tests/qml/casos/tst_relatorios_dia.qml`.

**Aviso de divergência de lote REMOVIDO da aba Vencimento** (decisão do dono:
não quer avisos desse tipo). `LoteRepository::divergencias()` segue existindo e
testado (agora devolve `DivergenciaLote` com a unidade), só não é exibido.

### Venda sem estoque não deixa mais o lucro errado (feito)

Decisão do dono: **o PDV continua vendendo sem estoque** (não trava o balcão).
O problema era o custo dessas unidades: a venda gravava o custo médio do instante
e ele ficava travado para sempre. Produto que nunca tinha entrado saía com custo
**zero** (lucro de 100%) mesmo depois da compra lançada; e com custo anterior, as
unidades a descoberto ficavam com o custo velho em vez do da compra que chegou.

- Migration `0017`: `movimentacoes_estoque.qtd_pendente_custo` — quantas unidades
  de cada saída de venda foram vendidas **além do saldo**.
- `VendaRepository`: lê o saldo antes de baixar e anota a falta. O custo médio de
  agora fica como **provisório** dessas unidades.
- `EstoqueRepository::acertarCustoPendente`, chamado por `aplicarEntrada` (compra
  e entrada manual) quando o saldo está negativo e a entrada tem custo: cobre no
  máximo `min(entrada, −saldo)` unidades, das vendas mais antigas para as novas.
  Saída inteira pendente → troca o custo; parte dela → **divide a linha** (a parte
  acertada vira uma saída própria, mesma venda e mesma data, com o custo da
  compra). Dividir em vez de fazer média evita erro de centavos no lucro. A soma
  das quantidades não muda, então saldo × movimentações continua batendo.
- `cancelarVenda` zera o pendente da venda cancelada: senão a compra seguinte
  acertaria uma venda que não vale e a verdadeira ficaria no provisório.
- **Limites:** entrada **sem custo** e **inventário** não acertam nada (não há
  custo para dar). Vendas anteriores à `0017` ficam com pendente 0 — não dá para
  reconstruir quais saíram sem estoque.
- Coberto por `tst_custo_estoque_negativo` (4 casos novos, verificados falhando
  antes: custo 0 em vez de 15,00; 45,00 em vez de 50,00). Migration testada sobre
  cópia de banco com dados: movimentações e saldo inalterados, `integrity_check` ok.

### Auditoria do banco da loja — Fase A (feito)

Backup real da loja (17/09) analisado **somente leitura**. Os dados da loja
**não** foram corrigidos (decisão do dono); só o código.

**A1 — O fator da embalagem vinha da tela.** `registrarCompra` e
`finalizarVenda` recebiam `embalagemId` **e** `fator` separados e acreditavam
no fator. Na loja: um BOX de 20 palheiros entrou com fator errado (custo por
unidade ×20), uma caixinha de 12 vendida por R$ 48,00 baixou 1 lata. O dono
compensava no inventário (+285, −22), mas o custo ficava errado.
- `ProdutoRepository::fatorDaEmbalagem(produtoId, embalagemId)` — só devolve se
  a embalagem for DESTE produto. `AppBackend::_fatorDoCadastro` usa isso nos dois
  caminhos; `embalagemId = 0` (copão) vale fator 1.
- Fator da tela diferente do cadastro vira **`qWarning` no `sistema.log`**.
- **A3 — a causa NÃO era a tela** (corrigido na auditoria seguinte): o PDV e o
  cadastro foram reproduzidos e gravam o fator certo. Os registros errados
  vieram do **cadastro**: a embalagem nova nascia com fator 1, o dono criava
  "BOX"/"CAIXINHA", punha o preço e salvava sem mudar o fator, vendia/comprava
  e corrigia dias depois (compra do MONSTER 1 minuto após criar o produto). Por
  isso o A1 sozinho não resolvia. A trava está no cadastro (ver "Conceito
  central").
- Coberto por `tst_fator_embalagem` (tela mentindo fator 1 na compra e na venda,
  embalagem de outro produto, sem embalagem).

**A2 — Aviso de custo fora do normal** (Compras e Estoque → Entrada).
`AppBackend::avaliarCusto(produtoId, embalagemId, custoTexto)` compara o custo
digitado com quanto a embalagem **rende** vendida pelo preço da menor
embalagem com preço: acima disso = `alto`, abaixo de 10% = `baixo`. No banco da
loja essa faixa pegou exatamente os custos errados (custo da lata na caixa ou o
contrário) e nenhum certo. **Só avisa**: o primeiro clique mostra o aviso e
troca o botão para "… mesmo assim"; mudar custo/embalagem zera a conferência.
Sem preço de venda ou sem custo, não avisa. Coberto por
`tst_fator_embalagem::avaliarCusto` e `tests/qml/casos/tst_custo_aviso.qml`.

**A5 — Data de cadastro em hora local.** A `0009` esqueceu `criado_em` de
produtos, clientes e usuários (DEFAULT `datetime('now')` = UTC). Migration
`0018` converte o histórico; os INSERTs gravam `datetime('now','localtime')`
explicitamente (o DEFAULT do schema continua UTC — mudar exigiria recriar as
tabelas, então **todo INSERT novo nessas tabelas precisa passar o criado_em**).
Testada sobre cópia do banco da loja: 304 produtos, `integrity_check` ok.
Coberto por `tst_migrations` (conversão e cadastro novo).

**A6 — OpenSSL no pacote (Telegram na loja).** O PC da loja é Windows 10 1709
(build 16299); o TLS do Windows (schannel) do Qt 6.8 não abre conexão lá
("não foi possível criar um canal seguro"). `empacotar.ps1` agora copia
`libssl-3-x64.dll` + `libcrypto-3-x64.dll` (OpenSSL 3, do Git for Windows —
parâmetro `-OpenSSL`), `tls/qopensslbackend.dll` e `LICENSE-OpenSSL.txt`, e
**para com erro** se faltar algo. As DLLs só dependem de DLLs do sistema.
Verificado numa cópia do pacote com PATH limpo: backend `openssl`,
`api.telegram.org` responde 200. **Não verificado no PC da loja** — confirmar lá.
- O script também passou a desligar **explicitamente** testes e a ferramenta do
  manual no Release (o cache de `build\release` guardava `ON`), e
  `DISTRIBUIDORA_BUILD_TOOLS` agora é `OFF` por padrão.

**Fora da Fase A:** contagem cega do caixa (A7) — excluída pelo dono.

### Auditoria completa do sistema (feito)

Leitura de todo o backend e das telas principais, com cada suspeita conferida
no backup da loja (somente leitura) e, quando preciso, reproduzida em teste
descartável. Corrigido **só o que é erro**; regra de negócio ficou como estava.

| Erro | Correção | Teste |
| --- | --- | --- |
| Embalagem nova nascia com **fator 1** e o cadastro aceitava caixinha com fator 1 (4 produtos da loja assim hoje) | Linha nova começa em 0; salvar recusa fator 0 e mesmo fator com preço diferente | `tst_produto_repository::fatorDeEmbalagemConferido`, `tst_correcoes.qml` |
| Compra **sugeria custo errado em produto de ml** (custo por ml truncado em centavos: Black Stone R$ 10,00 × R$ 18,99) | `ItemEstoque.custoMedioMilli` (exato); `custoMedio` agora arredonda e é só exibição | `tst_correcoes.qml`, `tst_compra_repository` |
| Valor do estoque no relatório do celular ~R$ 213 menor, e saldo negativo descontando | Soma em milésimos, só saldo positivo | — |
| **Histórico de fiado nunca aparecia** em Clientes (`vendas.data_hora` não existe) | `vendas.data` | `tst_cliente_repository::historicoFiadoEnxergaAsVendas` |
| Cancelar venda **fiado já paga** deixava o recebido sem devolução (venda nº 3 da loja) | O já pago sai da gaveta como sangria | `tst_venda_repository::cancelamentoDevolveFiadoJaPago` |
| Cancelar venda de outro turno **com o caixa fechado**: dinheiro saía sem registro | Recusa e pede para abrir o caixa (venda só em pix/cartão cancela normal) | `::cancelamentoComCaixaFechado` |
| **Restaurar backup apagava o banco antes de copiar** | Copia ao lado → renomeia o atual → troca; falhou, o atual fica | `tst_backup_service::restauracaoQueFalhaMantemOBanco` |
| Log dizia **"Backup enviado ao Telegram"** no início do envio, mesmo quando falhava | Resultado real no `finished` (enviado / NÃO enviado + motivo) | — |
| Custo médio **0 (desconhecido) entrava na média** e inflava o lucro | Com custo 0, a entrada define o custo | `tst_custo_estoque_negativo::estoqueSemCustoNaoEntraNaMedia` |
| Entrada de estoque: custo ilegível ignorado, embalagem de outro produto virava fator 1, **validade conferida depois de gravar** | Tudo conferido antes; mesma fonte de fator da compra/venda | — |
| Preço ilegível no cadastro virava **R$ 0,00** | Recusado com mensagem | `tst_correcoes.qml` |
| Salvar produto pela tela **apagava o custo de compra** das embalagens | A tela devolve o custo carregado | `tst_correcoes.qml` |
| Desativar cliente sem confirmação (dívida some da tela) | Diálogo de confirmação com o saldo | `tst_correcoes.qml` |
| Dava para **ficar sem nenhum administrador** | Recusa rebaixar/desativar o único admin | `tst_usuario_repository::naoFicaSemAdministrador` |
| "Mais vendidos" **somava ml com unidade** (PARATUDO 1800 em 1º) | Quantidade na menor embalagem | `tst_relatorio_repository::maisVendidosNaoMisturaMlComUnidade` |
| Banco que não abre: **o sistema fechava sem mensagem** | Janela com o motivo e a pasta do log | — |
| Produto desativado **continuava vendendo pelo bipe** e prendia o código | Busca só ativos; desativar libera os códigos | `::inativoNaoVendePeloCodigo` |
| Dashboard contava **dose** como produto em falta | Exclui dose | — |
| Permissões só na tela em compras, fornecedores, pagar conta, despesa, usuários, backup, Telegram | Conferidas também no `AppBackend` | suíte (`tst_permissoes`) |
| Venda aceitava quantidade/valor negativo pelo backend | Recusa | — |

**Não mexido (regra da loja, decisão do dono):** fiado sem vencimento (o
indicador de "atrasado" depende de um prazo que só o dono define); Esc que
cancela a venda no PDV; custo 0 aceito na entrada (pode ser bonificação).
**Sem correção possível:** cancelamento não devolve ao lote de validade — o
sistema não registra de qual lote cada venda saiu.

**Cadastro da loja com fator errado HOJE** (dado, não código — não corrigido):
ORIGINAL 350 ML (CAIXINHA fator 1, R$ 64,00), IMPERIO ULTRA LONG NECK 275 ML,
PALHEIRO TERRA TOMBADA UVA (3 embalagens fator 1), PRESIDENTE 900ML. Com a trava
nova, esses produtos só salvam depois de o fator ser corrigido na tela.

### Bateria de verificação geral (feito)

Pedido do dono depois da auditoria: *"garanta que esteja perfeito, faça 100
testes verificando tudo o que for possível"*. São **104 verificações novas**,
escritas a partir do que foi combinado ao longo do projeto inteiro — não só do
código como ele está hoje.

**`tests/cpp/tst_verificacao_geral.cpp` (98 casos)** passa pelo `AppBackend`, o
mesmo caminho das telas, num banco novo por execução: dinheiro (parse/formato,
"1OO" recusado), usuários e permissões (funcionário barrado no backend em
produto, compra, conta, despesa e backup), cadastro e embalagem (fator
obrigatório, fator ambíguo recusado, código de barras), fator sempre do cadastro
(venda, compra, entrada e retirada, inclusive com a tela mentindo), estoque e
custo (média ponderada, saldo negativo, custo pendente acertado pela compra,
custo 0 = desconhecido, inventário, retirada), custo exato em ml, aviso de custo
fora do normal, venda (troco só em dinheiro, pix a mais, insuficiente, qtd/valor
inválidos, desconto sem permissão, fiado e limite), cancelamento (mesmo turno,
estoque, fiado aberto, fiado já pago, caixa fechado, motivo e permissão), caixa
(abertura ilegível, sangria, suprimento, recebimento de fiado, diferença no
fechamento), financeiro (despesa, sangria ao pagar, estorno, exclusão,
recebimento parcial, conta de compra), clientes (histórico, FIFO, resumo),
relatórios (dia, cancelada fora da conta, dia específico, mais vendidos, dose
fora do "em falta", hora local na venda e no cadastro), copão e dose, validade
com FEFO, backup (criar, recusar arquivo que não é backup, retenção), fotos
(redução a 320 px, HEIC com recado claro) e o registro do cancelamento no log.

**`tests/cpp/tst_banco_real.cpp` (6 casos)** roda as invariantes sobre uma
**cópia** do banco de verdade — o banco da loja **não** entra no repositório:

```
DISTRIBUIDORA_BANCO_REAL=/caminho/para/copia.db ./tst_banco_real.exe
```

Sem a variável, o teste é pulado. Ele aplica as migrations na cópia e confere:
`integrity_check`, `foreign_key_check`, nenhuma data de cadastro no futuro,
**saldo de estoque = soma das movimentações** em todos os produtos, pagamento
nunca menor que o total da venda, troco nunca maior que o dinheiro recebido, e
lista (como aviso) as embalagens de mesmo fator com preços diferentes.
Rodado sobre o backup de 17/09: tudo verde, com os 6 avisos de cadastro
(4 produtos) já conhecidos.

### Faxina do que estava versionado (feito)

Varredura pedida pelo dono depois do commit da auditoria: procurar lixo no que
está no repositório. Nada de banco, backup, zip, cache ou binário indevido; todo
`.cpp`/`.h`/`.qml`/teste está registrado no CMake; sem duplicata, sem arquivo
vazio, sem TODO pendente, sem segredo. O que sobrou foi isto:

**O pacote levava ~15 MB que o sistema nunca usa.** O `windeployqt` copia TODOS
os estilos do Qt Quick Controls, o depurador de QML e todos os drivers de banco.
O sistema usa **Fusion** (`main.cpp`) e **SQLite**. `empacotar.ps1` agora remove,
depois do deploy: os estilos FluentWinUI3/Imagine/Material/Universal/Windows
(pasta QML + `Qt6QuickControls2<estilo>.dll` + `...StyleImpl.dll`), `qmltooling`,
`generic` (toque TUIO) e os drivers psql/odbc/mimer. **Duas conferências** no fim
do script param a entrega se o estilo em uso ou o driver do SQLite sumirem —
erro de digitação na lista só apareceria na loja.
- Medido numa cópia: 78,1 MB → 62,7 MB, e o sistema abre e roda igual.
- Isso pesa duas vezes: a pasta vai versionada, então cada atualização do pacote
  levava esse excesso para o histórico do Git (hoje 39 MB, 15 pacotes commitados).

**Código morto removido** (nenhum uso fora do próprio teste):
`AppBackend::divergenciasDeLote()` (era a ponte para o aviso que o dono mandou
tirar), `ResumoCaixa::totalRecebidoPorForma()` e `LoteRepository::totalEmLotes()`
— este virou helper dentro de `tst_lotes`, onde é a única coisa que pergunta.
A regra `LoteRepository::divergencias()` continua, com teste: ela é o que sabe
dizer que estoque e lotes não batem, e um dia a tela pode voltar a mostrar.

**Documentação que tinha envelhecido:** `docs/modelo-de-dados.md` parava na
migration `0015` (faltavam `0016`, `0017` e `0018`, incluindo a coluna
`qtd_pendente_custo`); `docs/instalacao.md` não dizia que o pacote leva OpenSSL
nem o que o script tira; `CMakePresets.json` dava exemplo de caminho do MSVC num
projeto que compila com MinGW.

**Observação sem conclusão:** numa das medições, abrir o sistema duas vezes matou
a instância que já estava aberta (código `0xC0000602`). Repetido duas vezes
depois, as duas instâncias conviveram. Não há evidência para chamar de defeito —
fica anotado para olhar se acontecer na loja.

### Mensagens curtas (feito)

Pedido do dono, em duas rodadas. Na primeira eu encurtei pela metade e ele
devolveu: *"ao invés de 'Custo pode estar errado: R$ 3,00 por Caixinha (12
unidades), que rende R$ 54,00', eu quero: 'Custo pode estar errado'"*. Ou seja:
**o recado, e nada mais**.

| Onde | Mensagem |
| --- | --- |
| Compra e Entrada | "Custo pode estar errado" · "Confira o custo" |
| Cadastro | "Informe o fator de \"FARDO\"" · "\"Unidade\" e \"FARDO\" com o mesmo fator" · "Preço inválido em \"Unidade\"" |
| Entrada | "Custo inválido" · "Validade inválida" · "Essa embalagem não é deste produto" |
| Caixa | "Troco inicial inválido" · "Valor contado inválido" · "Abra o caixa para cancelar" · "Saíram R$ 18,00 da gaveta" |
| Venda | "Fiado exige cliente" · "Pagamento insuficiente" · "Escolha os insumos" · "Informe o motivo" |
| Permissão | "Sem permissão para registrar compras" (mesmo molde nas 18) |
| Usuários | "É o único administrador" · "Login já existe" |

**A regra:** o aviso diz o que olhar; o valor digitado, a embalagem escolhida e
o botão ("Registrar mesmo assim") já estão na tela, ao lado. Repeti-los na
mensagem é ruído no meio do atendimento. Instrução de formato também sai — o
campo de data já tem máscara `dd/mm/aaaa`.

Onde o valor **não** está na tela, ele fica: "Saíram R$ 18,00 da gaveta" é o
único jeito de o operador saber quanto saiu, e "Ainda deve R$ 45,00" é o que
faz pensar duas vezes antes de desativar o cliente.

Os testes que conferiam o texto antigo foram ajustados (`tst_custo_aviso.qml`
procura "pode estar errado"; `tst_mensagens` compara os textos novos) e as
imagens do relatório foram regeradas — elas mostram as mensagens na tela.

### Margem na tela de Estoque (feito)

Pedido do dono: ver, produto a produto, quanto sobra de cada real vendido.
Ele foi explícito sobre qual das duas contas é: **margem sobre o preço de
venda, NÃO markup**. Custo R$ 10,00 e venda R$ 15,00 → lucro R$ 5,00 e
**margem 33,3%** (o markup, que é sobre o custo, daria 50%). Trocar uma pela
outra faz o dono achar que ganha metade a mais do que ganha.

- `ItemEstoque::margemDecimos()` é a única fonte da conta, em **décimos de por
  cento** (333 = 33,3%) para não espalhar `double`. A conta sai dos dois valores
  em **milésimos de centavo**: em ml o custo é fração de centavo e, pelo custo
  arredondado, a garrafa de R$ 18,99 mostraria 47,3% em vez de 42,1%.
- **Preço de referência = a menor embalagem com preço**, levado para a unidade
  base (`ItemEstoque::precoBaseMilli`, uma subconsulta dentro do `listar` — uma
  por produto seriam 300 idas ao banco a cada recarga). É a **mesma referência
  do aviso de custo** (`avaliarCusto`), de propósito: as duas telas não podem
  discordar sobre "o preço" do mesmo produto. Pela caixa, a margem do exemplo
  daria 25% em vez de 33,3%.
- **Não existe margem** (a tela mostra "—", o `MargemRole` devolve `undefined` e
  a chave `margem` nem entra no mapa do `itemEstoque`) quando o produto não tem
  preço de venda ou quando o **custo é 0**, que aqui significa DESCONHECIDO
  (bonificação, produto que nunca entrou). Dizer "100% de margem" nesse caso
  seria uma mentira com cara de número certo. Margem zero e "não dá para
  calcular" são coisas diferentes e aparecem diferente.
- **Margem negativa aparece**, em vermelho e em negrito: é prejuízo, e é
  justamente a linha que precisa ser olhada hoje.
- O diálogo do produto mostra **preço de venda ao lado da margem** — sozinha, a
  porcentagem é um número sem origem. A linha de resumo virou `Flow`: com quatro
  blocos ela não cabe numa linha na janela restaurada.
- `AppBackend::formatarPercentual(decimos)` escreve em pt-BR ("33,3%",
  "-12,5%"), como o `formatarDinheiro` faz com o dinheiro.

**Role novo entra no FIM do enum do `EstoqueListModel`.** O QML acha os roles
pelo nome, mas os testes chegam neles pelo NÚMERO (`Qt.UserRole + posição`):
inserir no meio renumera tudo o que vem depois e quebra teste que não tem nada
a ver com a mudança (o `tst_estoque_filtro` teria quebrado neste mesmo commit).

**Conferido no banco real da loja (17/09, somente leitura):** dos 280 produtos
com estoque, 237 têm margem, 43 ficam em "—" por custo desconhecido e 5 saem
negativas. Os extremos são todos **cadastro errado, não conta errada** — as
maiores (ORIGINAL 350 ML com 94,3%, IMPERIO ULTRA LONG NECK com 95,1%) são
exatamente os produtos com fator 1 na caixinha já listados na auditoria, e as
piores (PAÇOCA −1099%, PALHEIRO PIRACANJUBA −775%) são custo de caixa lançado
na unidade. Ou seja: a coluna acende sozinha os erros de cadastro que hoje só
apareciam procurando.

**Testes:** 5 casos em `tst_estoque_repository` (o exemplo do dono, referência
pela menor embalagem, sem preço, sem custo, prejuízo e o caso em ml) e
`tests/qml/casos/tst_estoque_margem.qml` (6 casos: o model, a coluna com o
texto formatado, o "—" nos dois casos sem margem, e o diálogo com e sem
margem), mais um caso em `tst_permissoes`. **28 executáveis, 117 casos de QML.**

**Margem é só de quem vê o financeiro** (pedido do dono, logo depois de a
coluna ficar pronta). Segue `ve_financeiro`, a mesma chave que já esconde o
painel de dinheiro do Dashboard — o funcionário recebe mercadoria e consulta
saldo, mas não vê quanto a loja ganha em cada produto. A coluna some INTEIRA,
cabeçalho junto: um "—" no lugar do número anunciaria que há algo escondido
ali. E a trava não é só de tela — `EstoqueListModel::setMostrarMargem` zera o
role e o `itemEstoque` não devolve a chave, senão o número ficaria a uma linha
de QML de distância. O flag é reavaliado em toda troca de usuário (login e
logout), e não só na recarga: a tela não recarrega sozinha ao abrir, então
entrar como funcionário precisa apagar a margem da lista que o admin deixou
carregada.

**O que NÃO foi feito, e o dono precisa decidir:** a coluna **Custo médio**
continua visível para o funcionário — sempre foi. Com o custo e o preço de
prateleira na mão, a margem é uma conta de cabeça. Esconder a margem e deixar o
custo é meia trava; se a intenção é que o balcão não saiba o lucro, o custo
precisa ir junto (e aí a Entrada de mercadoria, que pede o custo da nota,
precisa ser repensada).

### Filtro por faixa de margem, e a lista que estava em escada (feito)

**Faixas de margem (decisão do dono):** abaixo de 35% **baixa**, de 35% a 45%
**boa** (as duas bordas INCLUSIVE), acima de 45% **muito boa**. Filtro igual ao
de situação, na mesma barra: `EstoqueListModel::filtroMargem` +
`contagemMargem` ({ todos, baixa, boa, muitoboa, **sem** }).

- Quem **não tem margem** (sem preço ou custo desconhecido) não entra em faixa
  nenhuma: conta em `sem` e só aparece em "Todas". Jogar esses produtos em
  "baixa" encheria a faixa de gente que ninguém consegue avaliar.
- **Os dois filtros valem juntos** (situação E margem): "o que está acabando e
  ainda por cima vende com margem baixa" é a pergunta que resolve a compra da
  semana.
- **Cada contagem é sobre o que o OUTRO filtro deixou passar.** Contar sobre a
  lista inteira mostraria "Baixa 4" e entregaria lista vazia com o filtro de
  situação ligado.
- **A coluna Margem NÃO usa as cores do selo de Status** (retorno do dono: *"as
  cores estão confundindo, dá para separar para ninguém associar sem querer com
  a coluna ao lado?"*). A primeira versão pintava o número em âmbar/verde — as
  MESMAS três cores do `StatusBadge`, coladas nele. "34,3%" em âmbar ao lado de
  "⚠ Baixo" em âmbar lia-se como uma informação só, e são duas: uma é lucro, a
  outra é quantidade em estoque. Agora:
  - o número sai em texto normal; **só prejuízo é colorido** (vermelho, e já vem
    com sinal de menos);
  - **a faixa vem ESCRITA embaixo do número**, apagada e em minúscula ("baixa",
    "boa", "muito boa", "prejuízo") — mesma regra do filtro, então filtrar
    "Baixa" e ler "boa" na linha é impossível;
  - um **divisor vertical** com folga dos dois lados separa o bloco de números
    do selo de situação.
  No diálogo do produto, onde sobra espaço, a faixa vem na mesma linha
  ("33,3% · baixa") — tira a dúvida de "33,3% é bom?" sem obrigar a decorar os
  limites.
- Filtro de margem **some junto com a coluna** para quem não vê o financeiro, e
  `setMostrarMargem(false)` **desliga o filtro**: senão a lista apareceria
  encurtada, sem nada na tela explicando por quê.
- Faixa desconhecida vira "todas", e sair da tela desliga os dois filtros.

**Defeito visual na lista de Clientes (corrigido).** O selo "Em dia"/"Deve R$ x"
ficava numa posição diferente em cada linha, colado no fim do nome — uma escada
pela lista. Causa: **uma coluna aninhada não cresce além da largura máxima
dela, e essa máxima vem dos filhos**. O `Layout.fillWidth` estava na
`ColumnLayout`, mas os `Text` de dentro não tinham nenhum, então a máxima da
coluna era a largura do nome e o `fillWidth` não tinha para onde crescer. Com
`Layout.fillWidth` nos textos (mais `minimumWidth: 0` e `elide`), a coluna ocupa
o vão e o selo encosta à direita.

Varri as 34 ocorrências do mesmo formato nas telas: a maioria é coluna em pilha
vertical, onde não crescer não aparece. O único outro caso de verdade era a aba
**A receber** do Financeiro, onde o valor e o botão "Receber" andavam conforme o
tamanho do nome do cliente — corrigido igual.

**REGRA:** numa linha de lista, `Layout.fillWidth` na coluna **e** nos textos
dentro dela.

**Testes:** `tst_estoque_margem` foi de 6 para 13 casos (as duas bordas, 35,0% e
45,0%, dentro de "boa"; 34,9% fora; sem margem fora de todas as faixas; os dois
filtros combinados; a cor por faixa) e `tst_clientes_lista.qml` fixa o
alinhamento (mesmo x em toda linha, encostado à direita) — **verificado falhando
antes da correção**: 425 e 443 em vez do mesmo x.

**A coluna gulosa (corrigido no mesmo dia).** Pôr a faixa embaixo do número
transformou a célula da margem numa `ColumnLayout` — e **uma Layout dentro de
outra tem `Layout.fillWidth` TRUE por padrão**, ao contrário de um item comum.
Ela passou a comer a folga da linha (293 px em vez de 104) e empurrou Qtd
atual, Mínimo e Custo médio uns 380 px para longe do próprio cabeçalho. Na
janela estreita mal dava para ver; na tela cheia da loja ficou evidente.
`Layout.fillWidth: false` resolve.

Isso virou `test_colunas_batem_com_o_cabecalho`, que abre a lista a 1660 px e
compara a borda direita de cada célula com a do cabeçalho — **verificado
falhando com a correção desfeita** ("cabQtd termina em 1123 e a linha em 1035").
É o teste que faltava: os de tela conferiam que nada começava fora da janela,
mas nenhum conferia que as colunas batem entre si.

**REGRA:** célula de lista que vira Layout precisa de `Layout.fillWidth: false`
explícito, senão rouba a folga da linha.

**Fio entre as linhas das listas (pedido do dono: "igual tem em compras").**
Compras, Financeiro, Vencimento, Vendas, Backup e PDV já desenhavam um fio de
1 px em `Theme.border` no fim de cada linha; **Estoque, Clientes, Produtos e
Usuários não**. Eram as quatro fora do padrão — agora as quatro têm o mesmo
`Rectangle` ancorado no rodapé da linha.

Junto veio o mesmo defeito da escada na lista de **Usuários**: a coluna do nome
sem `fillWidth` nos textos, e o perfil ("Administrador") mudava de lugar a cada
linha conforme o tamanho do nome. Corrigido igual ao de Clientes — a varredura
anterior tinha marcado este arquivo como falso positivo porque olhou a coluna
de fora, não a da linha.

### Janela pequena: varredura de largura (feito)

Pedido do dono depois de encolher a janela: *"fica esquisito os filtros,
verifique em todo o sistema problemas visuais ao deixar a tela menor"*. Os
testes conferiam só DOIS tamanhos (1160 e 760) e só se algo **começava** fora
da tela. O feio mora no meio do caminho — foi numa janela de ~1400 que os
filtros do Estoque ficaram tortos.

**`tst_telas::test_cabe_em_qualquer_largura`**: as 14 telas em **1600, 1200,
900 e 760 px**, reprovando todo item visível cujo lado direito passa da borda.
Item dentro de pai com `clip` não conta (lista rolável é recortada de
propósito). Sem `waitForRendering` — a geometria já está resolvida no polish, e
esperar o quadro custava 4 minutos de suíte em vez de 60 s.

O que a varredura achou e foi corrigido:

- **`Layout.minimumWidth` é letra morta quando `fillWidth` é falso.** O Qt trava
  o item na largura PREFERIDA, e o mínimo só vale para quem pode ser
  redimensionado. O editor da tela de Produtos declarava `preferredWidth: 520` +
  `minimumWidth: 380` e, a 760 px, ficava nos 520 — **60 px fora da tela**.
  Agora é `fillWidth` + `maximumWidth: 520` + `minimumWidth: 380`: cresce até
  520 e encolhe até 380. Verificado falhando com a correção desfeita.
- **Colunas do Estoque somem por ordem de utilidade** (mesmo recurso que a tela
  de Produtos já tinha): Status > 420, Custo > 560, Margem > 780, Mínimo > 920,
  Localização > 1080. Sem isso, a 760 px o cabeçalho escrevia "Produto" por
  cima de "Localização". Nome e quantidade nunca somem. O **filtro de margem
  anda com a coluna** e é desligado quando ela some, senão a lista ficaria
  filtrada por algo invisível.
- **`SegmentedControl` agora se mede.** Tinha `implicitWidth: 260` fixo e cada
  tela chutava uma largura (400 aqui, 440 ali) — por isso os dois filtros do
  Estoque saíam de tamanhos diferentes, um em cada linha. Agora a largura
  natural sai da MAIOR legenda (`TextMetrics`) e o texto do segmento tem
  `elide`: espremido, encurta em vez de invadir o vizinho.
- **Os dois filtros do Estoque viraram um `Row` só**, então passam para a linha
  de baixo juntos e alinhados, em vez de um em cada linha.

**O arnês de QML passou a carregar as fontes embutidas** (as mesmas do
`main.cpp`). Não muda resultado de teste, mas sem elas a plataforma offscreen
não tem fonte alguma: toda imagem de `grabImage` saía com o texto em
quadradinhos — e, pior, as larguras medidas eram as da fonte de emergência.
Três "transbordamentos" que eu tinha achado no Dashboard, Clientes e Usuários
eram fantasmas disso.

### Embalagem ao lado do nome, no carrinho (feito)

Pedido do dono. O seletor de embalagem ficava EMBAIXO do nome, dentro da mesma
coluna: a linha do carrinho parecia um formulário e o nome era empurrado para
cima. Agora a embalagem tem **coluna própria**, com cabeçalho "Embalagem", na
mesma posição em toda linha — seletor quando o produto tem mais de uma, texto
quando só tem uma. O carrinho virou tabela: Produto · Embalagem · Qtd · Preço ·
Subtotal.

**O ajuste que isso exigiu:** os limites de largura do carrinho foram feitos
quando não existia essa coluna. Com ela, e sem mexer nos limites, quem sumia
primeiro era o NOME do produto (a captura mostrou "Pro…" no cabeçalho e linhas
só com a miniatura). Preço e subtotal passaram a ceder antes: `mostrarPrecoUnit`
> 640 e `mostrarSubtotal` > 520 (eram 470 e 360), e a coluna de embalagem tem
120 px e some abaixo de 430 — aí a embalagem volta como texto embaixo do nome,
para a informação não sumir (trocar, aí sim, só alargando a janela).

**REGRA:** coluna nova numa lista obriga a revisar os limites de "quem some
primeiro" — senão a coluna nova entra empurrando a mais importante para fora.

Coberto por `tst_pdv::test_embalagem_ao_lado_do_nome` (a coluna existe nas duas
linhas e começa no MESMO x, o seletor aparece só para quem tem escolha, e o
seletor está depois do nome, não embaixo). O caso precisa **abrir o caixa**: com
o caixa fechado a tela mostra o painel "Caixa fechado" e o carrinho inteiro fica
invisível — foi o que fez a primeira versão do teste falhar.

### Correção de custo — aba "Custo" no Estoque (feito)

Pergunta do dono: *"se eu colocar o custo errado na compra, consigo mudar
direto pelo estoque?"* Não conseguia: a Entrada exige quantidade > 0, o
Inventário só mexe em quantidade, e **nem cancelar compra existe**. O custo
errado ficava no lucro para sempre. No banco da loja (17/09) eram 8 produtos
com custo implausível — o PALHEIRO PIRACANJUBA com custo de R$ 17,50 vendendo a
R$ 2,00 e 10 vendas registradas com prejuízo que nunca existiu.

**Aba "Custo"** no diálogo do produto (a última, como as outras restritas):
escolhe a embalagem, digita o custo certo dela — igual à Entrada, fator do
cadastro — e a tela mostra ANTES de gravar: custo por unidade antes → depois e
margem antes → depois. `EstoqueRepository::ajustarCusto` troca
`estoque.custo_medio_unitario` **sem mexer na quantidade**.

**As vendas que já saíram com o custo errado** (decisão: ligado por padrão, o
dono pode desligar em cada correção). O botão diz quantas e desde quando
("Corrigir também as 10 vendas feitas desde 02/09/2026"). Regrava o
`custo_unit` TRAVADO das saídas de venda **desde a última entrada** do produto
— o período em que o custo errado valia; antes disso o custo veio de outra
remessa e não é tocado. Produto que nunca teve entrada: todas as vendas.
Só vendas `concluida` (cancelada não entra no lucro). A contagem da tela e a
atualização usam o MESMO filtro (`filtroVendasDesde`), para o número prometido
ser o número gravado.

**O aviso de custo fora do normal vale aqui também**, e aqui ele tem papel a
mais: separa custo errado de **fator errado**. Se o custo "certo" ainda dá uma
margem absurda, o problema está no cadastro da embalagem (é o caso de ORIGINAL
350 ML e IMPERIO: o custo está certo, a caixinha é que tem fator 1) — e mexer no
custo pioraria. Primeiro clique avisa, segundo grava, como na Entrada.

**Rastro, sem migration:** uma movimentação de quantidade 0 (tipo `'ajuste'`,
origem `'ajuste_custo'`) com o custo antigo e o novo na observação e o custo
exato em `custo_unit`. Conferido antes de escolher: o relatório de lucro só lê
`saida_venda` com origem `venda:%`, e nada mais lê `'ajuste'` — a linha não
entra em conta nenhuma, e saldo = soma das movimentações continua valendo. E
uma linha no `sistema.log`: quem mudou, de quanto para quanto, quantas vendas,
motivo.

**Trava:** `ajusta_estoque` (a mesma do Inventário/Retirada) na tela e no
backend; a prévia só entrega margem a quem tem `ve_financeiro`.

**O que NÃO muda:** o registro da compra (`compra_itens`) e a conta a pagar
dela. Pesa pouco na loja: o banco inteiro tem 1 conta a pagar.

**Conferido nos dados reais** (somente leitura, a mesma consulta): PALHEIRO
PIRACANJUBA — última entrada 02/09, **10 vendas (25 un.)** a corrigir, o mesmo
número do diagnóstico. De brinde: há dois produtos de paçoca, "PAÇOCÃO" e
"Paçocao" (este sem entrada nenhuma) — cara de cadastro duplicado.

**Testes:** 9 casos em `tst_verificacao_geral` (`custo97`–`custo105`: só o
custo muda; o lucro do dia muda exatamente 3 × (17,50 − 1,46); **sem corrigir,
o lucro não muda** — controle negativo do anterior; venda de antes da última
entrada intocada; cancelada fora; prévia não grava; funcionário barrado;
custo 0/vazio/"1OO"/negativo recusados; log e auditoria) e
`tests/qml/casos/tst_estoque_custo.qml` (5 casos: caminho feliz com as vendas,
desligar o botão, custo absurdo pede segundo clique, sem vendas não oferece
corrigir, custo inválido). **28 executáveis, 326 + 154 casos.**

**Custo por embalagem na aba Custo (retorno do dono).** *"Ao trocar a embalagem,
mostre o custo da embalagem e não só o da unidade; quero poder trocar o custo
do fardo."* Trocar pelo fardo JÁ funcionava (escolhe Fardo, digita o custo do
fardo) — mas a prévia só mostrava "custo por unidade", e parecia que não. Agora:
- o campo diz de qual embalagem é: **"Novo custo (Fardo)"** (sem artigo — o
  nome da embalagem é livre e "do Caixinha" erraria o gênero);
- vazio, o campo mostra o **custo atual daquela embalagem** (placeholder):
  trocar para o Fardo mostra o do fardo;
- no lugar da linha por unidade, uma **tabela com todas as embalagens, atual →
  novo**, a escolhida em destaque. Por unidade, num produto em ml, a linha dizia
  "R$ 0,02 → R$ 0,02" — por embalagem, diz alguma coisa.
- `previaAjusteCusto` devolve `embalagens` ({nome, fator, atual, novo,
  escolhida}, do menor fator ao maior) e `custoAtualEmbalagem`.

**Por que o fardo não tem custo PRÓPRIO, separado da unidade:** é o mesmo
estoque — a lata do fardo aberto é a lata vendida avulsa, e o estoque guarda um
número só, em unidade base. Um custo por embalagem faria a mesma lata ter dois
custos conforme a forma de venda, e o lucro de cada venda dependeria de uma
escolha arbitrária. Por isso corrigir o fardo corrige a unidade e a caixinha
junto, e a tabela deixa isso visível. O que de fato difere entre embalagens é o
PREÇO (fardo mais barato por unidade) — e com ele, a margem.
`produto_embalagens.custo_compra` existe mas não entra em conta nenhuma (e na
loja está vazio nas 399 embalagens).

Testes: `tst_verificacao_geral::custo106` (a lista por embalagem, o custo atual
da escolhida, e gravar pelo fardo define a unidade: R$ 21,00 / 12 = R$ 1,75) e
dois casos em `tst_estoque_custo.qml` (trocar a embalagem muda rótulo e o custo
mostrado no campo; corrigir pela caixinha define a unidade).
