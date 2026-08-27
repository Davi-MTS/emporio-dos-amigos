-- =============================================================================
-- Migration 0006 — Custo unitário na movimentação (COGS travado na venda)
-- =============================================================================
-- O lucro precisa usar o custo do PRODUTO NO MOMENTO DA VENDA, não o custo médio
-- atual (que muda a cada nova compra). Sem isso, vendas antigas têm o lucro
-- recalculado retroativamente quando o custo sobe.
--
-- Guardamos o custo médio unitário vigente na hora da saída em cada linha de
-- movimentação. Colunas antigas ficam NULL; o relatório cai para o custo médio
-- atual nesses casos (compatibilidade com dados anteriores).
-- =============================================================================

ALTER TABLE movimentacoes_estoque ADD COLUMN custo_unit INTEGER;  -- centavos, unidade base
