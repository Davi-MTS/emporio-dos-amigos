# Design de UI — Distribuidora

Referência de design das telas. O protótipo visual navegável está em
`docs/mockup-ui.html` (abra no navegador ou veja o Artifact publicado). Este
documento registra as decisões que a implementação em QML deve seguir.

## Contexto e restrições

- **Plataforma:** desktop Windows (PC do caixa). Distância de uso ~60 cm.
- **Input:** mouse + teclado. **Sem touchscreen.** O PDV é *keyboard-first*.
- **Resolução:** responsivo, piso **1366×768**, testar em 1920×1080.
- **Locale:** pt-BR, sem RTL. Datas/números via `QLocale`.
- **Estilo base:** Qt Quick Controls **Fusion** + tokens do `qml/theme/Theme.qml`.

## Identidade

**Empório dos Amigos — Bebidas & Conveniência · 062.** Cor primária **preto**, secundária **laranja**.
Tom **premium e sóbrio** (nada infantilizado): sem emojis na navegação (ícones de
linha), neutros quentes, tipografia com caráter.

## Sistema de design (tokens)

Fonte única de verdade: `qml/theme/Theme.qml`. O mockup usa os mesmos valores.

- **Preto de marca (ink):** `#14120C` — ação primária, headings, sidebar.
- **Acento (laranja queimado):** `#E2611E` (hover `#C24E12`) — usado com restrição:
  total do PDV, botão finalizar, indicador ativo, destaques de embalagem.
- **Superfícies (neutros quentes):** ground `#F5F3F0`, surface `#FFFFFF`,
  borda `#E7E2DB`.
- **Texto:** `#1E1B15`, secundário `#736C61`.
- **Semânticas** (sempre cor + ícone + texto, nunca só cor): sucesso `#2E7D51`,
  alerta `#A9741A`, perigo `#BC3B2A`.
- **Ícones:** linha (SVG stroke), monocromáticos — **sem emoji**.
- **Tipografia:** wordmark, títulos de página e nomes em **Fraunces** (serifada
  refinada); UI, corpo e números em **Archivo** (grotesco preciso). No app QML,
  o corpo usa a fonte do sistema (Segoe UI) por fidelidade ao Fusion; Fraunces
  fica reservada ao wordmark/títulos. Dinheiro sempre com `tabular-nums`.
- **Tema claro e escuro** definidos desde o início via tokens.

## Casca (app shell)

Sidebar fixa de 240px (navegação = pastas de `qml/screens/`) + área de conteúdo
com cabeçalho (título + status do caixa) e a tela ativa. Já esboçada na fase 0.

## Telas

### PDV (frente de caixa) — a mais crítica

Layout em duas colunas:

- **Esquerda (~63%):** campo de scan/busca no topo (foco automático, leitor de
  código de barras age como teclado; Enter adiciona) + tabela do carrinho
  (Produto, Embalagem, Qtd, Preço un., Desc., Subtotal, remover). A embalagem
  vendida aparece como badge — badge de *pack* (ex.: "Caixa ×24") destaca venda
  por embalagem, tornando visível a conversão.
- **Direita (~37%):** cliente (opcional, p/ fiado), **TOTAL** grande, subtotal e
  desconto, botões de pagamento (múltiplas formas: Dinheiro, Pix, Débito,
  Crédito, Fiado), lista dos pagamentos lançados, troco/falta, e o botão
  primário **Finalizar venda (F12)**.
- **Rodapé:** legenda de atalhos (F2 buscar, F4 desconto, F8 cliente, F9
  pagamento, F12 finalizar, Esc cancelar, ↑↓ navegar, Del remover).

Princípios aplicados: reconhecimento sobre memorização (atalhos sempre visíveis),
feedback imediato, prevenção de erro (confirmar cancelamento), von Restorff (só o
total e o botão finalizar usam o acento).

### Produtos

Lista (Nome, Categoria, Estoque, Preço, Status) + painel de detalhe/edição com a
sub-tabela de **embalagens** (Nome, Fator, Código de barras, Preço) — mostra a
conversão explicitamente ("vender 1 caixa baixa 12 da unidade base").

### Estoque

Tabela: Produto, Localização, Qtd atual (unidade base), Mínimo, Custo médio,
Status (OK / Baixo / Zerado — cor + ícone + texto). Ações: entrada, ajuste,
inventário.

### Dashboard

Cartões de KPI (vendas hoje, ticket médio, a receber, produtos em falta) +
gráfico de vendas por hora + ranking de mais vendidos. Ligados a
`src/domain/relatorios/` na fase 2.

### Demais (fase 2+)

Compras, Clientes, Financeiro, Relatórios, Usuários — placeholders consistentes
por enquanto.

## Acessibilidade

- Navegação completa por teclado; foco sempre visível (`activeFocus`).
- Contraste ≥ 4,5:1 para texto; estados nunca só por cor.
- Testar com fonte grande do SO e simulação de daltonismo.
