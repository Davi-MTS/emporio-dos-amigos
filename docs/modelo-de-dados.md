# Modelo de dados — Empório dos Amigos

Esquema das tabelas, dividido em 3 blocos. Este documento acompanha o schema
**realmente aplicado**; a fonte da verdade são as migrations em
`db/migrations/`, aplicadas em ordem ao abrir o app.

## Convenções (valem para todo o schema)

- **Dinheiro: INTEIRO em CENTAVOS.** R$ 12,50 = `1250`. Nunca `float`/`REAL` —
  evita erro de arredondamento no fechamento de caixa.
- **Custo por unidade: INTEIRO em MILÉSIMOS de centavo** (centavos × 1000), em
  `estoque.custo_medio_unitario`, `movimentacoes_estoque.custo_unit` e
  `compra_itens.custo_unit`. Motivo: custo por ml/g é fração de centavo e, em
  centavos inteiros, arredondava para zero e zerava o lucro (migration `0007`).
- **Quantidades SEMPRE na unidade base** do produto (inteiro). Embalagem é
  conversão, não uma unidade de armazenamento.
- **Datas/hora em TEXT ISO 8601, em HORA LOCAL** (migration `0009`). O
  `datetime('now')` do SQLite é UTC e jogava as vendas da noite para o dia
  seguinte nos relatórios.
- Chaves primárias `INTEGER PRIMARY KEY` (rowid). FKs dependem de
  `PRAGMA foreign_keys = ON`, ligado em `Database.cpp`.
- Registros nunca são apagados quando têm valor de auditoria: usa-se `status`
  (venda cancelada, conta cancelada) ou `ativo = 0` (soft delete).

## Migrations aplicadas

| # | O que faz |
| --- | --- |
| `0001` | Schema inicial (os 3 blocos abaixo) |
| `0002` | `vendas.troco` |
| `0003` | `produtos.composto` + tabela `produto_composicao` |
| `0004` | Recria `produto_composicao` como receita **por categoria** |
| `0005` | `mov_caixa.tipo` aceita `'recebimento'` (fiado recebido em dinheiro) |
| `0006` | `movimentacoes_estoque.custo_unit` (custo travado no momento da venda) |
| `0007` | Custo por unidade em **milésimos de centavo** |
| `0008` | `compras.numero_nota` e `compras.data_nota` |
| `0009` | Converte as datas gravadas para **hora local** |
| `0010` | `contas_receber.status` aceita `'cancelada'`; `vendas.cancelada_em` e `vendas.motivo_cancelamento` |

---

## Bloco 1 — Produtos, estoque e conversão

Núcleo do sistema. A conversão de embalagem vive aqui.

### categorias
- id (PK)
- nome  — ex: cerveja, refrigerante, energético, destilados, gelo, carvão, snacks

### marcas
- id (PK)
- nome

### fornecedores
- id (PK)
- nome
- cnpj
- contato
- (detalhado também no Bloco 3)

### produtos
- id (PK)
- nome
- categoria_id (FK → categorias)
- marca_id (FK → marcas)
- fornecedor_id (FK → fornecedores)
- unidade_base  — ex: "unidade", "long neck", "garrafa"
- estoque_minimo (int, em unidade base)
- localizacao  — localização física no estoque
- foto  — caminho/arquivo da imagem
- taxa_manutencao  — taxa de manutenção da loja (compõe o cálculo de preço)
- ativo (bool)  — soft delete
- **composto** (bool)  — produto montado na hora (copão, drink, dose). Não tem
  linha em `estoque`: ao vender, baixa os **insumos**, não a si mesmo.

### produto_composicao
Receita de um produto composto, **por CATEGORIA** (não por produto fixo): na
venda o operador escolhe qual produto de cada categoria vai entrar.
- id (PK)
- produto_composto_id (FK → produtos)
- categoria_id (FK → categorias)  — ex.: Destilados, Gelo, Energético
- unidade  — unidade | ml | litro | g | kg
- quantidade (int)  — por 1 unidade do composto (ex.: 50 ml de destilado)

### produto_embalagens
Cada nível de embalagem de um produto (genérico: 2, 3 ou mais níveis).
- id (PK)
- produto_id (FK → produtos)
- nome_embalagem  — ex: "unidade", "caixa", "fardo", "pallet"
- fator_conversao (int)  — quantas unidades base essa embalagem representa
- codigo_barras  — código de barras específico dessa embalagem
- preco_venda (decimal)  — preço de venda nesse nível
- custo_compra (decimal, opcional)  — custo nesse nível (derivável do base)

### estoque
Uma linha por produto (o número único de estoque).
- produto_id (PK, FK → produtos)
- quantidade_atual (int, unidade base)
- custo_medio_unitario (int, **milésimos de centavo** por unidade base) —
  média ponderada, atualizada só na ENTRADA (compra/entrada manual)

### lotes
Controle por lote/validade.
- id (PK)
- produto_id (FK → produtos)
- data_validade (date)
- quantidade (int, unidade base)
- (opcional) codigo_lote

### movimentacoes_estoque
Histórico de todas as movimentações (entrada, saída, ajuste, inventário/quebra).
- id (PK)
- produto_id (FK → produtos)
- tipo  — entrada | saida_venda | ajuste | inventario | devolucao
- quantidade (int, unidade base; positivo ou negativo)
- origem  — referência ao que gerou: `venda:<id>`, `compra:<id>`,
  `cancelamento:<id>`, `entrada_manual`, `inventario`, `retirada`
- usuario_id (FK → usuarios)  — quem fez (trilha de auditoria)
- data (datetime, hora local)
- observacao
- **custo_unit** (int, milésimos de centavo, opcional)  — nas saídas de venda,
  guarda o custo do produto **no momento da venda**. Sem isto, uma compra mais
  cara depois recalcularia o lucro de vendas passadas.

**Aviso de estoque baixo:** consulta — `estoque.quantidade_atual <= produtos.estoque_minimo`.

---

## Bloco 2 — Vendas, caixa e pagamentos

Operação do balcão. Toda venda pertence a uma sessão de caixa.

### sessoes_caixa
Uma sessão = um turno de caixa aberto/fechado.
- id (PK)
- usuario_abertura (FK → usuarios)
- usuario_fechamento (FK → usuarios)
- valor_abertura (decimal)  — troco inicial
- aberta_em (datetime)
- fechada_em (datetime)
- valor_esperado (decimal)  — calculado
- valor_informado (decimal)  — contado pelo operador
- diferenca (decimal)  — sobra/falta

### mov_caixa
Movimentos de dinheiro na gaveta durante o turno.
- id (PK)
- sessao_id (FK → sessoes_caixa)
- tipo  — **sangria | suprimento | recebimento**
  - `sangria` — dinheiro sai (retirada, pagamento de conta em dinheiro, estorno
    de venda de turno anterior)
  - `suprimento` — dinheiro entra (reforço de troco)
  - `recebimento` — fiado recebido **em dinheiro** (converte recebível em
    dinheiro na gaveta, sem duplicar a venda original)
- valor (int, centavos)
- motivo
- usuario_id (FK → usuarios)
- data (datetime, hora local)

> **Dinheiro esperado no fechamento** =
> `abertura + vendas em dinheiro − troco + suprimentos + recebimentos − sangrias`.
> Pix, cartão e fiado **não** entram na gaveta.

### vendas
- id (PK)
- sessao_id (FK → sessoes_caixa)
- cliente_id (FK → clientes, opcional)
- total (decimal)
- desconto (decimal)  — desconto no total da venda
- **troco** (int, centavos)  — só do excedente pago em DINHEIRO; excesso em
  pix/cartão não vira troco (senão o fechamento tira dinheiro que não entrou)
- status  — concluida | cancelada | troca
- usuario_id (FK → usuarios)
- data (datetime, hora local)
- observacao  — usada para promoção manual, comanda etc.
- **cancelada_em** (datetime, opcional)
- **motivo_cancelamento** (texto, opcional)

> **Cancelar uma venda** não apaga nada: marca `status = 'cancelada'`, devolve
> ao estoque (movimentação `devolucao`, lida das próprias movimentações da
> venda — funciona também para composto), cancela a conta de fiado e, se a
> venda for de turno já fechado, lança sangria no caixa aberto.

### venda_itens
- id (PK)
- venda_id (FK → vendas)
- produto_id (FK → produtos)
- embalagem_id (FK → produto_embalagens)  — qual nível foi vendido
- qtd_unidade_base (int)  — quantidade JÁ convertida (baixa do estoque)
- preco_unit (decimal)  — preço aplicado
- desconto (decimal)  — desconto nesta linha

### pagamentos
Separada de `vendas` para permitir múltiplas formas na mesma venda.
- id (PK)
- venda_id (FK → vendas)
- forma  — pix | dinheiro | debito | credito
- valor (decimal)
- status  — aprovado | pendente | estornado
- (futuro) dados_tef  — retorno da maquininha, quando integrada

### delivery
Opcional, uma venda pode ter entrega.
- id (PK)
- venda_id (FK → vendas)
- taxa_entrega (decimal)  — calculada por KM
- endereco
- situacao  — recebido | separando | saiu_entrega | entregue
- atualizado_em (datetime)

---

## Bloco 3 — Compras, clientes, financeiro e usuários

Retaguarda.

### compras
- id (PK)
- fornecedor_id (FK → fornecedores)
- total (int, centavos)
- data (datetime, hora local)  — quando foi registrada no sistema
- status  — pedido | recebida | cancelada
- origem  — nota | manual  (vira `nota` automaticamente se houver número)
- **numero_nota** (texto, opcional)  — nº da NF-e de origem
- **data_nota** (date, opcional)  — data de emissão impressa na nota

### compra_itens
- id (PK)
- compra_id (FK → compras)
- produto_id (FK → produtos)
- embalagem_id (FK → produto_embalagens)  — nível comprado
- qtd_unidade_base (int)  — entra no estoque em unidade base
- custo_unit (int, **milésimos de centavo** por unidade base)  — alimenta o
  custo médio. Calculado como `custo_da_embalagem × 1000 ÷ fator`, multiplicando
  ANTES de dividir para não perder a fração (ex.: custo por ml)

### clientes
- id (PK)
- nome
- telefone
- cpf (opcional)
- endereco
- aniversario (date)
- observacoes
- limite_fiado (decimal)  — limite de débito fiado

### contas_pagar
Nasce das compras (o que se deve ao fornecedor).
- id (PK)
- compra_id (FK → compras, opcional)
- descricao  — para despesas avulsas sem compra vinculada
- valor (int, centavos)  — em pagamento PARCIAL este valor é REDUZIDO; a conta
  segue aberta com o saldo restante
- vencimento (date)
- status  — aberta | paga | vencida | **cancelada** (venda desfeita)
- pago_em (date)

> **Recebimento de fiado** abate as contas da mais antiga para a mais nova
> (FIFO). Se for em dinheiro e houver caixa aberto, entra como `recebimento`
> em `mov_caixa`, na mesma transação.

### contas_receber
Nasce do fiado do cliente e de vendas a prazo.
- id (PK)
- cliente_id (FK → clientes)
- venda_id (FK → vendas, opcional)
- valor (decimal)
- vencimento (date)
- status  — aberta | paga | vencida
- pago_em (date)

### perfis
- id (PK)
- nome  — ex: Administrador, Funcionário
- permissoes (json)  — chaves como pode_dar_desconto, pode_cancelar_venda,
  ve_financeiro, edita_produto, etc.

### usuarios
- id (PK)
- perfil_id (FK → perfis)
- nome
- login
- senha_hash
- ativo (bool)

### log_operacoes
Registro de quem fez cada operação.
- id (PK)
- usuario_id (FK → usuarios)
- acao  — descrição da operação
- entidade  — tabela/registro afetado
- data (datetime)

---

## Relatórios (sem tabela própria)

Fluxo de caixa, lucro, produtos mais vendidos, produtos parados, faturamento,
ticket médio, vendas por forma de pagamento e estoque atual são **consultas
agregadas** sobre as tabelas acima. Implementados em `src/domain/relatorios/`.

**Como o lucro é calculado:**

```
faturamento = SUM(vendas.total)            -- só status 'concluida', no período
custo       = SUM(-mov.quantidade × COALESCE(mov.custo_unit,
                                             estoque.custo_medio_unitario)) / 1000
lucro       = faturamento − custo
```

O custo vem das **movimentações reais de saída** (`tipo = 'saida_venda'`), que
registram o produto exato baixado — por isso vale igualmente para produto normal
e para os insumos de um composto. O `COALESCE` cobre movimentações antigas,
anteriores à migration `0006`.

## Onde ficam os arquivos

| Caminho | Conteúdo |
| --- | --- |
| `%APPDATA%\Distribuidora\Distribuidora\distribuidora.db` | O banco |
| `…\logs\sistema.log` | Registro de erros e eventos (rotaciona a 2 MB) |
| `…\Relatorioelatorio.html` | Relatório enviado no Telegram |
| `Documentos\Empório dos Amigos\Backups\` | Backups (5 mais recentes) |

Configurações que não são do negócio (token do Telegram, chat de destino) ficam
em `QSettings`, **fora do banco** — assim continuam válidas mesmo restaurando um
backup, e nunca entram no repositório.
