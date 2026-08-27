-- =============================================================================
-- Migration 0003 — Produto composto ("copão")
-- =============================================================================
-- Um produto composto (ex.: copão/drink) é vendido como um produto normal, mas
-- ao ser vendido baixa do estoque VÁRIOS produtos-insumo diferentes (a receita).
-- Decisão: o composto NÃO tem estoque próprio — só baixa os insumos.
--
-- Diferente da conversão de embalagem (mesmo produto em pacotes diferentes):
-- aqui os insumos são produtos distintos.
-- =============================================================================

ALTER TABLE produtos ADD COLUMN composto INTEGER NOT NULL DEFAULT 0;  -- 0/1

CREATE TABLE produto_composicao (
    id                  INTEGER PRIMARY KEY,
    produto_composto_id INTEGER NOT NULL REFERENCES produtos(id) ON DELETE CASCADE,
    insumo_produto_id   INTEGER NOT NULL REFERENCES produtos(id) ON DELETE RESTRICT,
    quantidade          INTEGER NOT NULL CHECK (quantidade > 0)  -- unidade base do insumo
                                                                 -- por 1 unidade do composto
);

CREATE INDEX idx_composicao_composto ON produto_composicao(produto_composto_id);
