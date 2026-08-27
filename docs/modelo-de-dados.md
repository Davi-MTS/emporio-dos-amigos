# Modelo de dados — Distribuidora

Esquema conceitual das tabelas, dividido em 3 blocos. Chaves e tipos são um
ponto de partida; ajustes finos (índices, constraints, nomes exatos de colunas)
serão feitos ao escrever as migrations em `db/migrations/`.

Convenção: identificadores como `uuid`, datas como ISO 8601, valores monetários
como `decimal`. Quantidades de estoque SEMPRE em unidade base (inteiro).

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
- custo_medio_unitario (decimal, unidade base)

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
- origem  — referência à venda/compra/ajuste que gerou (opcional)
- usuario_id (FK → usuarios)
- data (datetime)
- observacao

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
Sangrias e suprimentos durante o turno.
- id (PK)
- sessao_id (FK → sessoes_caixa)
- tipo  — sangria | suprimento
- valor (decimal)
- motivo
- usuario_id (FK → usuarios)
- data (datetime)

### vendas
- id (PK)
- sessao_id (FK → sessoes_caixa)
- cliente_id (FK → clientes, opcional)
- total (decimal)
- desconto (decimal)  — desconto no total da venda
- status  — concluida | cancelada | troca
- usuario_id (FK → usuarios)
- data (datetime)
- observacao  — usada para promoção manual, comanda etc.

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
- total (decimal)
- data (date)
- status  — pedido | recebida | cancelada
- origem  — nota | manual

### compra_itens
- id (PK)
- compra_id (FK → compras)
- produto_id (FK → produtos)
- embalagem_id (FK → produto_embalagens)  — nível comprado
- qtd_unidade_base (int)  — entra no estoque em unidade base
- custo_unit (decimal, unidade base)  — alimenta o custo médio

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
- valor (decimal)
- vencimento (date)
- status  — aberta | paga | vencida
- pago_em (date)

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

Fluxo de caixa, lucro diário/mensal, produtos mais vendidos, produtos parados,
faturamento (diário/semanal/mensal), ticket médio, vendas por forma de pagamento
e estoque atual são **consultas agregadas** sobre as tabelas acima
(vendas, venda_itens, pagamentos, sessoes_caixa, estoque, movimentacoes_estoque).
Implementados em `src/domain/relatorios/`.
