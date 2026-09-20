-- =============================================================================
-- Migration 0018 — Data de cadastro em HORA LOCAL
-- =============================================================================
-- A 0009 passou vendas, compras, caixa e movimentações para hora local, mas
-- esqueceu a data de cadastro de produtos, clientes e usuários: essas seguiam
-- no DEFAULT do schema, datetime('now') — UTC. Um produto cadastrado às 22h
-- aparecia como cadastrado no dia seguinte, 3 horas à frente.
--
-- Daqui em diante os INSERTs gravam datetime('now','localtime') explicitamente
-- (o DEFAULT da coluna não muda: mexer nele exigiria recriar a tabela).
-- Esta migration converte o que já existe (UTC -> local), uma única vez.
-- =============================================================================

UPDATE produtos SET criado_em = datetime(criado_em, 'localtime');
UPDATE clientes SET criado_em = datetime(criado_em, 'localtime');
UPDATE usuarios SET criado_em = datetime(criado_em, 'localtime');
