-- =============================================================================
-- Migration 0002 — Troco na venda
-- =============================================================================
-- Guarda o troco (em centavos) devolvido em cada venda. Necessário para calcular
-- o dinheiro esperado no fechamento de caixa:
--   dinheiro_esperado = abertura + vendas_dinheiro - troco + suprimentos - sangrias
-- =============================================================================

ALTER TABLE vendas ADD COLUMN troco INTEGER NOT NULL DEFAULT 0;
