-- =============================================================================
-- Migration 0004 — Composição por categoria (escolha do produto na venda)
-- =============================================================================
-- Reprojeta a composição do produto composto (copão/drink/shot):
-- cada linha da receita referencia uma CATEGORIA (ex.: Destilados, Gelo,
-- Energético, Descartáveis) + a UNIDADE de consumo (ml, unidade, g, litro) +
-- a quantidade. O produto específico (qual vodka, qual gelo) é escolhido na
-- HORA DA VENDA, quando a categoria tem variações.
--
-- A tabela anterior (por produto-insumo) é recriada. Composições de teste
-- criadas antes desta migration são descartadas.
-- =============================================================================

DROP TABLE IF EXISTS produto_composicao;

CREATE TABLE produto_composicao (
    id                  INTEGER PRIMARY KEY,
    produto_composto_id INTEGER NOT NULL REFERENCES produtos(id) ON DELETE CASCADE,
    categoria_id        INTEGER NOT NULL REFERENCES categorias(id) ON DELETE RESTRICT,
    unidade             TEXT NOT NULL DEFAULT 'unidade',  -- ml | unidade | g | litro...
    quantidade          INTEGER NOT NULL CHECK (quantidade > 0)  -- por 1 unidade do composto
);

CREATE INDEX idx_composicao_composto ON produto_composicao(produto_composto_id);
