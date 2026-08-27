-- =============================================================================
-- Migration 0001 — Schema inicial
-- =============================================================================
-- ERP Distribuidora de bebidas (loja única). SQLite, offline-first.
--
-- CONVENÇÕES (ver docs/modelo-de-dados.md e CLAUDE.md):
--   * Valores monetários: INTEGER em CENTAVOS (nunca float/REAL). Ex.: R$ 12,50
--     é gravado como 1250. Evita erro de arredondamento no fechamento de caixa.
--   * Quantidades de estoque: INTEGER, SEMPRE na unidade base do produto.
--   * Datas/datetime: TEXT em ISO 8601 (ex.: "2026-08-19T14:30:00").
--   * Chaves primárias: INTEGER PRIMARY KEY (rowid) salvo onde indicado.
--   * Foreign keys dependem de `PRAGMA foreign_keys = ON` na conexão (Database.cpp).
-- =============================================================================

-- -----------------------------------------------------------------------------
-- BLOCO 1 — Produtos, estoque e conversão de embalagem
-- -----------------------------------------------------------------------------

CREATE TABLE categorias (
    id    INTEGER PRIMARY KEY,
    nome  TEXT NOT NULL UNIQUE
);

CREATE TABLE marcas (
    id    INTEGER PRIMARY KEY,
    nome  TEXT NOT NULL UNIQUE
);

CREATE TABLE fornecedores (
    id       INTEGER PRIMARY KEY,
    nome     TEXT NOT NULL,
    cnpj     TEXT,
    contato  TEXT,
    telefone TEXT,
    email    TEXT,
    endereco TEXT
);

CREATE TABLE produtos (
    id              INTEGER PRIMARY KEY,
    nome            TEXT NOT NULL,
    categoria_id    INTEGER REFERENCES categorias(id)   ON DELETE SET NULL,
    marca_id        INTEGER REFERENCES marcas(id)        ON DELETE SET NULL,
    fornecedor_id   INTEGER REFERENCES fornecedores(id)  ON DELETE SET NULL,
    unidade_base    TEXT NOT NULL DEFAULT 'unidade',  -- ex.: "long neck", "garrafa"
    estoque_minimo  INTEGER NOT NULL DEFAULT 0,        -- em unidade base
    localizacao     TEXT,                              -- posição física no estoque
    foto            TEXT,                              -- caminho da imagem
    taxa_manutencao INTEGER NOT NULL DEFAULT 0,        -- centavos; compõe cálculo de preço
    ativo           INTEGER NOT NULL DEFAULT 1,        -- 0/1 (soft delete)
    criado_em       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_produtos_nome      ON produtos(nome);
CREATE INDEX idx_produtos_categoria ON produtos(categoria_id);

-- Cada nível de embalagem de um produto (genérico: 2, 3 ou mais níveis).
-- O fator_conversao diz quantas UNIDADES BASE essa embalagem representa.
-- Ex.: caixa de Heineken -> fator 12.
CREATE TABLE produto_embalagens (
    id              INTEGER PRIMARY KEY,
    produto_id      INTEGER NOT NULL REFERENCES produtos(id) ON DELETE CASCADE,
    nome_embalagem  TEXT NOT NULL,                       -- "unidade", "caixa", "fardo"...
    fator_conversao INTEGER NOT NULL CHECK (fator_conversao > 0),
    codigo_barras   TEXT,                                -- código específico da embalagem
    preco_venda     INTEGER NOT NULL DEFAULT 0,          -- centavos, neste nível
    custo_compra    INTEGER,                             -- centavos, opcional (derivável)
    UNIQUE (produto_id, nome_embalagem)
);

-- Código de barras é a chave de busca no PDV: único global (quando informado).
CREATE UNIQUE INDEX idx_embalagens_codigo_barras
    ON produto_embalagens(codigo_barras)
    WHERE codigo_barras IS NOT NULL;

-- Uma linha por produto: o número único de estoque, em unidade base.
CREATE TABLE estoque (
    produto_id           INTEGER PRIMARY KEY REFERENCES produtos(id) ON DELETE CASCADE,
    quantidade_atual     INTEGER NOT NULL DEFAULT 0,   -- unidade base
    custo_medio_unitario INTEGER NOT NULL DEFAULT 0    -- centavos, por unidade base
);

-- Controle por lote/validade (opcional por produto).
CREATE TABLE lotes (
    id            INTEGER PRIMARY KEY,
    produto_id    INTEGER NOT NULL REFERENCES produtos(id) ON DELETE CASCADE,
    codigo_lote   TEXT,
    data_validade TEXT,                                 -- ISO date
    quantidade    INTEGER NOT NULL DEFAULT 0            -- unidade base
);

CREATE INDEX idx_lotes_produto  ON lotes(produto_id);
CREATE INDEX idx_lotes_validade ON lotes(data_validade);

-- Histórico de TODAS as movimentações de estoque (auditoria).
CREATE TABLE movimentacoes_estoque (
    id         INTEGER PRIMARY KEY,
    produto_id INTEGER NOT NULL REFERENCES produtos(id) ON DELETE CASCADE,
    tipo       TEXT NOT NULL CHECK (tipo IN
                   ('entrada','saida_venda','ajuste','inventario','devolucao')),
    quantidade INTEGER NOT NULL,                        -- unidade base; +entra / -sai
    origem     TEXT,                                    -- ref. à venda/compra/ajuste
    usuario_id INTEGER REFERENCES usuarios(id) ON DELETE SET NULL,
    data       TEXT NOT NULL DEFAULT (datetime('now')),
    observacao TEXT
);

CREATE INDEX idx_mov_estoque_produto ON movimentacoes_estoque(produto_id);
CREATE INDEX idx_mov_estoque_data    ON movimentacoes_estoque(data);

-- -----------------------------------------------------------------------------
-- BLOCO 3 (parte) — Usuários e permissões
-- Declarado antes do Bloco 2 porque vendas/caixa referenciam usuarios.
-- -----------------------------------------------------------------------------

CREATE TABLE perfis (
    id         INTEGER PRIMARY KEY,
    nome       TEXT NOT NULL UNIQUE,                    -- Administrador, Funcionário
    permissoes TEXT NOT NULL DEFAULT '{}'               -- JSON: pode_dar_desconto, etc.
);

CREATE TABLE usuarios (
    id         INTEGER PRIMARY KEY,
    perfil_id  INTEGER NOT NULL REFERENCES perfis(id) ON DELETE RESTRICT,
    nome       TEXT NOT NULL,
    login      TEXT NOT NULL UNIQUE,
    senha_hash TEXT NOT NULL,
    ativo      INTEGER NOT NULL DEFAULT 1,
    criado_em  TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE log_operacoes (
    id         INTEGER PRIMARY KEY,
    usuario_id INTEGER REFERENCES usuarios(id) ON DELETE SET NULL,
    acao       TEXT NOT NULL,
    entidade   TEXT,
    data       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_log_data ON log_operacoes(data);

-- -----------------------------------------------------------------------------
-- BLOCO 3 (parte) — Clientes
-- Declarado antes do Bloco 2 porque vendas referenciam clientes.
-- -----------------------------------------------------------------------------

CREATE TABLE clientes (
    id           INTEGER PRIMARY KEY,
    nome         TEXT NOT NULL,
    telefone     TEXT,
    cpf          TEXT,
    endereco     TEXT,
    aniversario  TEXT,                                  -- ISO date
    observacoes  TEXT,
    limite_fiado INTEGER NOT NULL DEFAULT 0,            -- centavos
    ativo        INTEGER NOT NULL DEFAULT 1,
    criado_em    TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_clientes_nome ON clientes(nome);

-- -----------------------------------------------------------------------------
-- BLOCO 2 — Vendas, caixa e pagamentos
-- -----------------------------------------------------------------------------

-- Uma sessão = um turno de caixa aberto/fechado.
CREATE TABLE sessoes_caixa (
    id                 INTEGER PRIMARY KEY,
    usuario_abertura   INTEGER NOT NULL REFERENCES usuarios(id) ON DELETE RESTRICT,
    usuario_fechamento INTEGER REFERENCES usuarios(id) ON DELETE RESTRICT,
    valor_abertura     INTEGER NOT NULL DEFAULT 0,      -- centavos (troco inicial)
    aberta_em          TEXT NOT NULL DEFAULT (datetime('now')),
    fechada_em         TEXT,
    valor_esperado     INTEGER,                         -- centavos (calculado)
    valor_informado    INTEGER,                         -- centavos (contado)
    diferenca          INTEGER,                         -- centavos (sobra/falta)
    status             TEXT NOT NULL DEFAULT 'aberta'
                           CHECK (status IN ('aberta','fechada'))
);

CREATE INDEX idx_sessoes_status ON sessoes_caixa(status);

-- Sangrias e suprimentos durante o turno.
CREATE TABLE mov_caixa (
    id         INTEGER PRIMARY KEY,
    sessao_id  INTEGER NOT NULL REFERENCES sessoes_caixa(id) ON DELETE CASCADE,
    tipo       TEXT NOT NULL CHECK (tipo IN ('sangria','suprimento')),
    valor      INTEGER NOT NULL,                        -- centavos
    motivo     TEXT,
    usuario_id INTEGER REFERENCES usuarios(id) ON DELETE SET NULL,
    data       TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX idx_mov_caixa_sessao ON mov_caixa(sessao_id);

CREATE TABLE vendas (
    id         INTEGER PRIMARY KEY,
    sessao_id  INTEGER NOT NULL REFERENCES sessoes_caixa(id) ON DELETE RESTRICT,
    cliente_id INTEGER REFERENCES clientes(id) ON DELETE SET NULL,
    total      INTEGER NOT NULL DEFAULT 0,              -- centavos
    desconto   INTEGER NOT NULL DEFAULT 0,              -- centavos (no total)
    status     TEXT NOT NULL DEFAULT 'concluida'
                   CHECK (status IN ('concluida','cancelada','troca')),
    usuario_id INTEGER REFERENCES usuarios(id) ON DELETE SET NULL,
    data       TEXT NOT NULL DEFAULT (datetime('now')),
    observacao TEXT                                     -- promoção manual, comanda...
);

CREATE INDEX idx_vendas_sessao ON vendas(sessao_id);
CREATE INDEX idx_vendas_data   ON vendas(data);
CREATE INDEX idx_vendas_status ON vendas(status);

CREATE TABLE venda_itens (
    id               INTEGER PRIMARY KEY,
    venda_id         INTEGER NOT NULL REFERENCES vendas(id) ON DELETE CASCADE,
    produto_id       INTEGER NOT NULL REFERENCES produtos(id) ON DELETE RESTRICT,
    embalagem_id     INTEGER REFERENCES produto_embalagens(id) ON DELETE SET NULL,
    qtd_unidade_base INTEGER NOT NULL,                  -- JÁ convertida (baixa do estoque)
    preco_unit       INTEGER NOT NULL,                  -- centavos (preço aplicado)
    desconto         INTEGER NOT NULL DEFAULT 0         -- centavos (nesta linha)
);

CREATE INDEX idx_venda_itens_venda   ON venda_itens(venda_id);
CREATE INDEX idx_venda_itens_produto ON venda_itens(produto_id);

-- Separada de `vendas`: permite múltiplas formas de pagamento na mesma venda.
CREATE TABLE pagamentos (
    id        INTEGER PRIMARY KEY,
    venda_id  INTEGER NOT NULL REFERENCES vendas(id) ON DELETE CASCADE,
    forma     TEXT NOT NULL CHECK (forma IN ('pix','dinheiro','debito','credito','fiado')),
    valor     INTEGER NOT NULL,                         -- centavos
    status    TEXT NOT NULL DEFAULT 'aprovado'
                  CHECK (status IN ('aprovado','pendente','estornado')),
    dados_tef TEXT                                      -- retorno da maquininha (futuro)
);

CREATE INDEX idx_pagamentos_venda ON pagamentos(venda_id);

-- Uma venda pode ter entrega (opcional, fase posterior).
CREATE TABLE delivery (
    id            INTEGER PRIMARY KEY,
    venda_id      INTEGER NOT NULL REFERENCES vendas(id) ON DELETE CASCADE,
    taxa_entrega  INTEGER NOT NULL DEFAULT 0,           -- centavos (calculada por KM)
    endereco      TEXT,
    situacao      TEXT NOT NULL DEFAULT 'recebido'
                      CHECK (situacao IN ('recebido','separando','saiu_entrega','entregue')),
    atualizado_em TEXT NOT NULL DEFAULT (datetime('now'))
);

-- -----------------------------------------------------------------------------
-- BLOCO 3 (parte) — Compras e financeiro
-- -----------------------------------------------------------------------------

CREATE TABLE compras (
    id            INTEGER PRIMARY KEY,
    fornecedor_id INTEGER REFERENCES fornecedores(id) ON DELETE SET NULL,
    total         INTEGER NOT NULL DEFAULT 0,           -- centavos
    data          TEXT NOT NULL DEFAULT (datetime('now')),
    status        TEXT NOT NULL DEFAULT 'pedido'
                      CHECK (status IN ('pedido','recebida','cancelada')),
    origem        TEXT NOT NULL DEFAULT 'manual'
                      CHECK (origem IN ('nota','manual'))
);

CREATE INDEX idx_compras_fornecedor ON compras(fornecedor_id);
CREATE INDEX idx_compras_data       ON compras(data);

CREATE TABLE compra_itens (
    id               INTEGER PRIMARY KEY,
    compra_id        INTEGER NOT NULL REFERENCES compras(id) ON DELETE CASCADE,
    produto_id       INTEGER NOT NULL REFERENCES produtos(id) ON DELETE RESTRICT,
    embalagem_id     INTEGER REFERENCES produto_embalagens(id) ON DELETE SET NULL,
    qtd_unidade_base INTEGER NOT NULL,                  -- entra no estoque (unidade base)
    custo_unit       INTEGER NOT NULL                   -- centavos, unidade base (custo médio)
);

CREATE INDEX idx_compra_itens_compra  ON compra_itens(compra_id);
CREATE INDEX idx_compra_itens_produto ON compra_itens(produto_id);

-- O que se deve ao fornecedor (nasce das compras ou despesa avulsa).
CREATE TABLE contas_pagar (
    id         INTEGER PRIMARY KEY,
    compra_id  INTEGER REFERENCES compras(id) ON DELETE SET NULL,
    descricao  TEXT,                                    -- despesa avulsa sem compra
    valor      INTEGER NOT NULL,                        -- centavos
    vencimento TEXT,                                    -- ISO date
    status     TEXT NOT NULL DEFAULT 'aberta'
                   CHECK (status IN ('aberta','paga','vencida')),
    pago_em    TEXT
);

CREATE INDEX idx_contas_pagar_status     ON contas_pagar(status);
CREATE INDEX idx_contas_pagar_vencimento ON contas_pagar(vencimento);

-- O que o cliente deve (fiado / venda a prazo).
CREATE TABLE contas_receber (
    id         INTEGER PRIMARY KEY,
    cliente_id INTEGER NOT NULL REFERENCES clientes(id) ON DELETE RESTRICT,
    venda_id   INTEGER REFERENCES vendas(id) ON DELETE SET NULL,
    valor      INTEGER NOT NULL,                        -- centavos
    vencimento TEXT,                                    -- ISO date
    status     TEXT NOT NULL DEFAULT 'aberta'
                   CHECK (status IN ('aberta','paga','vencida')),
    pago_em    TEXT
);

CREATE INDEX idx_contas_receber_cliente ON contas_receber(cliente_id);
CREATE INDEX idx_contas_receber_status  ON contas_receber(status);
