-- =============================================================================
-- 0013 — Dose como produto próprio, ligado à garrafa
-- =============================================================================
-- Antes, vender dose só era possível pelo produto COMPOSTO: o operador escolhia
-- a categoria e o produto exato na hora da venda, com diálogo e tudo. Para
-- "dose de whisky" isso é burocracia — a garrafa é sempre a mesma.
--
-- Agora a dose é um produto normal: tem nome, código de barras e preço, aparece
-- na busca do PDV e vende com um bipe. O que ela tem de especial é apontar para
-- a garrafa de onde sai e dizer quanto consome dela.
--
--   dose_de_produto_id  -> o produto de origem (a garrafa)
--   dose_quantidade     -> quanto consome, na UNIDADE BASE da origem
--                          (garrafa em ml, dose de 50 ml => 50)
--
-- Estoque: a dose NÃO tem estoque próprio. O disponível é calculado a partir da
-- garrafa (quantos cabem), e a venda baixa a garrafa. Assim não existe o risco
-- de o estoque da dose e o da garrafa discordarem.
-- =============================================================================

ALTER TABLE produtos ADD COLUMN dose_de_produto_id INTEGER REFERENCES produtos(id);
ALTER TABLE produtos ADD COLUMN dose_quantidade INTEGER NOT NULL DEFAULT 0;

-- Achar rápido "quais doses saem desta garrafa" (usado ao calcular disponível).
CREATE INDEX IF NOT EXISTS idx_produtos_dose_origem
    ON produtos(dose_de_produto_id) WHERE dose_de_produto_id IS NOT NULL;
