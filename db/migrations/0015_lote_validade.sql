-- =============================================================================
-- 0015 — Controle de validade por remessa (lotes)
-- =============================================================================
-- A tabela `lotes` existe desde o schema inicial e nunca foi usada. Passa a ser
-- preenchida na entrada de mercadoria, quando o operador informa a validade.
--
-- Por que por REMESSA e não uma data por produto: duas cargas do mesmo doce
-- chegam com validades diferentes. Uma data só no cadastro seria sobrescrita
-- pela carga nova e a mercadoria velha sumiria do radar — justamente a que
-- precisa sair primeiro.
--
-- As saídas consomem FEFO (vence primeiro, sai primeiro), que é o que se faz na
-- prateleira. Índice para achar rápido o próximo a vencer.
-- =============================================================================

CREATE INDEX IF NOT EXISTS idx_lotes_validade
    ON lotes(produto_id, data_validade);

-- Lotes sem saldo não interessam e sujariam a tela de validades.
DELETE FROM lotes WHERE quantidade <= 0;
