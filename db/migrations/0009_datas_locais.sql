-- =============================================================================
-- Migration 0009 — Datas em HORA LOCAL (não UTC)
-- =============================================================================
-- O SQLite grava datetime('now') em UTC. Numa distribuidora que vende à noite
-- (UTC-3), toda venda a partir das 21h local era carimbada com a data do DIA
-- SEGUINTE — então o relatório "Hoje", o dashboard e o faturamento do dia saíam
-- errados (a venda das 23:16 do dia 25 aparecia no dia 26).
--
-- Passamos a gravar e comparar SEMPRE em hora local. Esta migration converte o
-- que já existe (UTC -> local). Roda uma única vez (schema_migrations).
-- =============================================================================

UPDATE vendas                SET data       = datetime(data, 'localtime');
UPDATE movimentacoes_estoque SET data       = datetime(data, 'localtime');
UPDATE compras               SET data       = datetime(data, 'localtime');
UPDATE mov_caixa             SET data       = datetime(data, 'localtime');
UPDATE sessoes_caixa         SET aberta_em  = datetime(aberta_em, 'localtime');
UPDATE sessoes_caixa         SET fechada_em = datetime(fechada_em, 'localtime')
 WHERE fechada_em IS NOT NULL;
